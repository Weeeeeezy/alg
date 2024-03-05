#pragma once

#include <algorithm>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <valarray>
#include <omp.h>

#include "TradingStrats/BLS/Common/BLSParallelEngine.hpp"
#include "TradingStrats/BLS/Common/Params.hpp"

namespace MAQUETTE {
namespace BLS {

using DPoint = std::valarray<double>;
using IPoint = std::valarray<int>;

namespace {

int64_t Binom(size_t n, size_t k) {
  assert(k <= n);

  // since Binom(n, k) == Binom(n, n-k), pick the smaller of k, n-k as the upper
  // bound for the loop
  size_t K = std::min(k, n - k);

  int64_t numer = 1;
  int64_t denom = 1;
  for (size_t i = 0; i < K; ++i) {
    numer *= (n - i);
    denom *= (i + 1);
  }

  return numer / denom;
}

DPoint MakeMinStep(const std::vector<SearchRange> &search_space) {
  DPoint min_steps(search_space.size());
  for (size_t i = 0; i < min_steps.size(); ++i)
    min_steps[i] = search_space[i].min_step;

  return min_steps;
}

IPoint MakeBounds(const std::vector<SearchRange> &search_space, bool upper) {
  IPoint bounds(search_space.size());
  for (size_t i = 0; i < bounds.size(); ++i)
    bounds[i] =
        int(round((upper ? search_space[i].end : search_space[i].start) /
                  search_space[i].min_step));

  return bounds;
}

} // namespace

// point with attached fitness
struct PointFit {
  DPoint x;
  double fit;

  PointFit() : fit(std::numeric_limits<double>::max()) {}

  PointFit(const DPoint &a_x, double a_fit) : x(a_x), fit(a_fit) {}

  PointFit &Combine(const PointFit &other) {
    if (other.fit < fit) {
      fit = other.fit;
      x = other.x;
    }
    return *this;
  }
};

#pragma omp declare reduction(FitMin:PointFit                                  \
                              : omp_out = omp_out.Combine(omp_in))

struct Particle {
  // position and velocity
  DPoint x, v;
  IPoint x_int;

  // previous and local best positions and fitness values
  PointFit prev_best, local_best;

  // this particle informs a fixed number of other particles plus itself about
  // its best known position
  std::vector<size_t> informees;

  // discretize the position of the particle and apply bounds
  template <typename ENGINE> void DiscretizeAndConfine(const ENGINE &engine) {
    x_int = engine.DtoIPoint(x);

    for (size_t k = 0; k < x.size(); ++k) {
      bool confined = false;
      if (x_int[k] < engine.lower_bounds()[k]) {
        x_int[k] = engine.lower_bounds()[k];
        confined = true;
      }
      if (x_int[k] > engine.upper_bounds()[k]) {
        x_int[k] = engine.upper_bounds()[k];
        confined = true;
      }
      if (confined) {
        v[k] *= -0.5;
      }
    }

    x = engine.ItoDPoint(x_int);
  }
};

// The Particle Engine evaluates the fitness function on a set of particles.
//
// The parameter space is discretized in all dimensions by the minimum step size
// of the parameter. At each point, the score of that particular instance is
// given by only the trade history of that instance. The fitness function,
// however, is the minimum score of all points in a neighborhood around the
// instance. The neighborhood is defined as the points obtained by varying at
// most N_vary parameters around the point (if N_vary == N_params we get a dense
// hybercube of size 3^N_params around the point).
//
// The score and fitness function are cached at each point. Evaluations of the
// score are performed in batches.
template <QtyTypeT QT, typename QR> class ParticleEngine {
public:
  using ParallelEngine = BLSParallelEngine<QT, QR>;
  using Qty_t = typename ParallelEngine::Qty_t;
  using RecsT = typename ParallelEngine::RecsT;

  ParticleEngine(const boost::property_tree::ptree &a_pso_params,
                 const boost::property_tree::ptree &a_strategy_params,
                 const ParallelEngine &a_parallel_engine,
                 const RecsT &a_recs, size_t a_meta_idx)
      : m_parallel_engine(a_parallel_engine), m_recs(a_recs),
        m_meta_params(a_strategy_params, true), m_meta_idx(a_meta_idx),
        m_N_vary(a_pso_params.get<size_t>("NeighborhoodDepth")),
        m_N_cycles(a_pso_params.get<size_t>("NumCycles")),
        m_Np(m_meta_params.GetOptimizationSearchSpace().size()),
        m_min_steps(MakeMinStep(m_meta_params.GetOptimizationSearchSpace())),
        m_lower_bounds(
            MakeBounds(m_meta_params.GetOptimizationSearchSpace(), false)),
        m_upper_bounds(
            MakeBounds(m_meta_params.GetOptimizationSearchSpace(), true)),
        m_Ns(m_upper_bounds - m_lower_bounds + 1),
        m_pso_N_iter(a_pso_params.get<size_t>("PSO_NumIter")),
        m_pso_N_particles(a_pso_params.get<size_t>("PSO_NumParticles")),
        m_pso_N_inform(a_pso_params.get<size_t>("PSO_NumInform")),
        m_pso_C(a_pso_params.get<double>("PSO_C")),
        m_pso_W(a_pso_params.get<double>("PSO_W")),
        m_umda_N_iter(a_pso_params.get<size_t>("UMDA_NumIter")),
        m_umda_N_particles(a_pso_params.get<size_t>("UMDA_NumParticles")),
        m_umda_N_select(a_pso_params.get<size_t>("UMDA_NumSelect")),
        m_de_F(a_pso_params.get<double>("DE_F")) {
    for (size_t p = 0; p < m_Np; ++p) {
      assert(m_min_steps[p] > 0.0);
      assert(m_upper_bounds[p] > m_lower_bounds[p]);
    }

    // seed the random number generators
    std::seed_seq meta_seeder{42, 13, 100};
    std::vector<std::uint32_t> meta_seeds(m_meta_params.size());
    meta_seeder.generate(meta_seeds.begin(), meta_seeds.end());

    std::mt19937_64 seeder(meta_seeds[m_meta_idx]);
    for (int i = 0; i < omp_get_max_threads(); ++i) {
      m_rngs.emplace_back(seeder());
    }

    // // Need to make m_Np and m_N_vary non-const for this test
    // // test neighborhood genration
    // for (size_t np = 0; np < 6; ++np) {
    //   m_lower_bounds = std::vector<int>(np, -1);
    //   m_upper_bounds = std::vector<int>(np, 1);

    //   if (np >= 3)
    //     m_lower_bounds[2] = 0;

    //   std::vector<int> center(np, 0);

    //   for (size_t nv = 0; nv <= np; ++nv) {
    //     m_Np = np;
    //     m_N_vary = nv;

    //     std::vector<std::vector<int>> nb;
    //     AppendNeighborhood(center, &nb);

    //     printf("Np = %lu, Nv = %lu, neighborhood size: %lu (== %lu)\n", np,
    //            nv, NeighborhoodSize(), nb.size());

    //     if (NeighborhoodSize() != nb.size())
    //       throw std::runtime_error("Neighborhood size mismatch");

    //     std::unordered_set<int> nums;

    //     for (size_t i = 0; i < nb.size(); ++i) {
    //       int idx = 0;
    //       int n = 1;

    //       for (size_t p = 0; p < np; ++p) {
    //         printf("%2i ", nb[i][p]);
    //         idx += (nb[i][p] + 1) * n;
    //         n *= 5;
    //       }
    //       printf("\n");
    //       if (nums.count(idx) > 0)
    //         throw std::runtime_error("Duplicate found");
    //       else
    //         nums.insert(idx);
    //     }

    //     printf("\n");
    //   }
    // }
  }

  auto lower_bounds() const { return m_lower_bounds; }
  auto upper_bounds() const { return m_upper_bounds; }

  std::vector<double> ComputeScores(
      const std::vector<OverallTradesResults<Qty_t>> &a_results) const {
    std::vector<double> scores(a_results.size());
    for (size_t i = 0; i < scores.size(); ++i) {
      const auto &stats = a_results[i].all;
      // double N = double(stats.all.num_trades);

      // minus sign because algorithm looks for minimum of the score
      scores[i] = -stats.all.profit.avg / stats.all.profit.std_dev;
    }

    return scores;
  }

  void FindBest() {
    DPoint upper_bounds = ItoDPoint(m_upper_bounds);
    DPoint lower_bounds = ItoDPoint(m_lower_bounds);

    // first run over the whole space
    RunUMDA(upper_bounds, lower_bounds);
    InitVelocities();
    auto prev_best = RunParticleSwarm();

    auto search_range = upper_bounds - lower_bounds;

    for (size_t cycle = 0; cycle < m_N_cycles; ++cycle) {
      DPoint bounds;
      double c = double(cycle);
      if (cycle % 2 == 0) {
        // diversification
        bounds = search_range / c;
      } else {
        // intensification
        bounds = search_range / (c * c);
      }

      DPoint new_LB = prev_best - bounds;
      DPoint new_UB = prev_best + bounds;

      for (size_t p = 0; p < m_Np; ++p) {
        new_LB[p] = std::max(new_LB[p], lower_bounds[p]);
        new_UB[p] = std::min(new_UB[p], upper_bounds[p]);
      }

      RunUMDA(new_LB, new_UB);
      InitVelocities();
      prev_best = RunParticleSwarm();
    }
  }

  // Evaluate and cache the scores on a given set of integer parameters
  std::vector<double>
  EvaluateScore(const std::vector<IPoint> &int_points) const {
    std::vector<double> scores(int_points.size());
    std::vector<int64_t> idxs(int_points.size());

    std::vector<std::vector<double>> real_params;
    real_params.reserve(int_points.size());

    std::vector<size_t> new_idx;
    new_idx.reserve(int_points.size());

    for (size_t i = 0; i < int_points.size(); ++i) {
      idxs[i] = Index(int_points[i]);

      if (m_score_cache.count(idxs[i]) > 0) {
        // we already have this score
        scores[i] = m_score_cache.at(idxs[i]);
      } else {
        // we need to evaluate this score
        new_idx.push_back(i);
        auto params = ItoDPoint(int_points[i]);
        real_params.emplace_back(std::begin(params), std::end(params));
      }
    }

    printf("Score cache hit rate: %.2f%%, computing %lu scores...\n",
           100.0 * (1.0 - double(new_idx.size()) / double(int_points.size())),
           int_points.size());

    auto start = std::chrono::high_resolution_clock::now();

    BLSParamSpace param_space(m_meta_params.GetType(), real_params.size());
    for (size_t i = 0; i < param_space.size(); ++i)
      param_space.set(
          i, m_meta_params.GetConcreteParams(m_meta_idx, real_params[i]));

    auto res = m_parallel_engine.RunAndAnalyze(param_space, m_recs/*,
                                               m_write_instance_files,
                                               m_write_instance_files, out_dir*/);
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_s =
        double(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                   .count()) /
        1.0E9;
    printf("Took %.3f seconds\n", elapsed_s);

    auto new_scores = ComputeScores(res);

    // update the cache and results
    for (size_t k = 0; k < new_idx.size(); ++k) {
      scores[new_idx[k]] = new_scores[k];
      m_score_cache.insert({idxs[new_idx[k]], new_scores[k]});

      // printf("[%06lu] Score %14.6f [", new_idx[k], new_scores[k]);
      // auto params = ItoDPoint(int_points[new_idx[k]]);
      // for (size_t k2 = 0; k2 < m_Np; ++k2)
      //   printf(" %8.3f", params[k2]);
      // printf(" ]\n");
    }

    return scores;
  }

  // Evaluate the fitness on a given set of points
  void EvaluateFitness() {
    m_fit.resize(m_particles.size());
    std::vector<int64_t> idxs(m_particles.size());

    // neighborhood size
    size_t nb_size = NeighborhoodSize();

    std::vector<size_t> new_idx;
    new_idx.reserve(m_particles.size());

    std::vector<IPoint> int_params;
    int_params.reserve(10 * m_particles.size() * nb_size);

    for (size_t i = 0; i < m_particles.size(); ++i) {
      idxs[i] = Index(m_particles[i].x_int);

      if (m_fitness_cache.count(idxs[i]) > 0) {
        // we already have this score
        m_fit[i] = m_fitness_cache.at(idxs[i]);
      } else {
        // we need to evaluate this score
        new_idx.push_back(i);
        AppendNeighborhood(m_particles[i].x_int, &int_params);
      }
    }

    printf("Fitness cache hit rate: %.2f%%\n",
           100.0 * (1.0 - double(new_idx.size()) / double(m_particles.size())));

    if (int_params.size() != (nb_size * new_idx.size()))
      throw std::runtime_error("EvaluateFitness: Number of neighbors mismatch");

    std::vector<double> scores = EvaluateScore(int_params);

    // update the cache and results
    for (size_t k = 0; k < new_idx.size(); ++k) {
      double fit = scores[k * nb_size];
      // the fitness of this point is the worst (i.e. max) fitness of all points
      // in the neighborhood
      for (size_t i = 1; i < nb_size; ++i)
        fit = std::max(fit, scores[k * nb_size + i]);

      m_fit[new_idx[k]] = fit;
      m_fitness_cache.insert({idxs[new_idx[k]], fit});
    }

    // print top 10
    printf("Top 10 points:\n");
    auto top_idxs = GetIdxsOfTopN(std::min(size_t(10), m_particles.size()));
    for (auto i : top_idxs) {
      printf("[%06lu] Score %14.6f [", i, m_fit[i]);
      for (size_t k = 0; k < m_Np; ++k)
        printf(" %8.3f", m_particles[i].x[k]);
      printf(" ]\n");
    }

    printf("\n");
  }

  // We are following the Standard Particle Swarm Optimisation 2011 (SPSO 2011)
  // described in:
  // Maurice Clerc. Standard Particle Swarm Optimisation. 15 pages. 2012.
  // <hal-00764996> https://hal.archives-ouvertes.fr/hal-00764996/document

  DPoint RunParticleSwarm() {
    // the particles must already be initialized with position and velocity
    SetInformationLinks();
    EvaluateFitness();

    PointFit overall_best(m_particles[0].x, m_fit[0]);
#pragma omp parallel for reduction(FitMin : overall_best)
    for (size_t i = 0; i < m_particles.size(); ++i) {
      m_particles[i].prev_best = PointFit(m_particles[i].x, m_fit[i]);
      overall_best.Combine(m_particles[i].prev_best);
    }

    for (size_t iter = 0; iter < m_pso_N_iter; ++iter) {
      DoInformationExchange();
#pragma omp parallel for
      for (size_t i = 0; i < m_particles.size(); ++i) {
        auto &p = m_particles[i];

        // compute center of gravity G
        DPoint G;

        // https://stackoverflow.com/a/24629011
        if ((p.prev_best.x == p.local_best.x).min()) {
          // prev_best location is the same as local best location, so we ignore
          // local best
          G = p.x + m_pso_C * 0.5 * (p.prev_best.x - p.x);
        } else {
          // prev best location != local best location
          G = p.x +
              m_pso_C * (p.prev_best.x + p.local_best.x - 2.0 * p.x) / 3.0;
        }

        // Now we need to pick a random point in the hyberball centered at G
        // with radius |G - p.x|. We do this by first selecting a uniform random
        // direction and then a uniform random radius.
        double radius = sqrt(((G - p.x) * (G - p.x)).sum());

        auto &rng = m_rngs[unsigned(omp_get_thread_num())];
        std::normal_distribution<double> norm_dist(0, 1);
        std::uniform_real_distribution<double> uni_dist(0, radius);

        // to pick random direction, we follow
        // https://en.wikipedia.org/wiki/N-sphere#Generating_random_points
        DPoint rand_dir(m_Np);
        for (size_t k = 0; k < m_Np; ++k)
          rand_dir[k] = norm_dist(rng);

        rand_dir /= sqrt((rand_dir * rand_dir).sum());
        DPoint rand_x = uni_dist(rng) * rand_dir;

        p.v = m_pso_W * p.v + rand_x - p.x;
        p.x += p.v;
        p.DiscretizeAndConfine(*this);
      }

      EvaluateFitness();

      PointFit new_overall_best(m_particles[0].x, m_fit[0]);
#pragma omp parallel for reduction(FitMin : new_overall_best)
      for (size_t i = 0; i < m_particles.size(); ++i) {
        m_particles[i].prev_best.Combine(PointFit(m_particles[i].x, m_fit[i]));
        new_overall_best.Combine(m_particles[i].prev_best);
      }

      if (new_overall_best.fit < overall_best.fit) {
        // successful step, keep information links the same
        overall_best = new_overall_best;
      } else {
        // unsuccessful step, randomize information links
        SetInformationLinks();
      }
    }

    printf("\nSwarm finished with best score %14.6f [", overall_best.fit);
    for (size_t k = 0; k < m_Np; ++k)
      printf(" %8.3f", overall_best.x[k]);
    printf(" ]\n\n");

    return overall_best.x;
  }

  void RunUMDA(const DPoint &upper_bounds, const DPoint &lower_bounds) {
    m_particles.resize(m_umda_N_particles);

    // need to initialize valarrays in one thread
    for (size_t i = 0; i < m_particles.size(); ++i) {
      m_particles[i].x = DPoint(m_Np);
      m_particles[i].x_int = IPoint(m_Np);
      m_particles[i].v = DPoint(m_Np);
    }

    auto size = upper_bounds - lower_bounds;

#pragma omp parallel for
    for (size_t i = 0; i < m_particles.size(); ++i) {
      // initialize population with uniform random distribution
      auto &rng = m_rngs[unsigned(omp_get_thread_num())];
      std::uniform_real_distribution<double> uni_dist(0.0, 1.0);

      DPoint rand(m_Np);
      for (size_t p = 0; p < m_Np; ++p)
        rand[p] = uni_dist(rng);

      m_particles[i].x = rand * size + lower_bounds;
      m_particles[i].DiscretizeAndConfine(*this);
    }

    for (size_t iter = 0; iter < m_umda_N_iter; ++iter) {
      EvaluateFitness();
      auto select = GetIdxsOfTopN(m_umda_N_select);

      // for each parameter, compute mean and standard deviation
      DPoint mean(0.0, m_Np);
      DPoint stddev(0.0, m_Np);

      for (auto i : select)
        mean += m_particles[i].x;

      mean /= double(select.size());

      for (auto i : select) {
        auto diff = mean - m_particles[i].x;
        stddev += diff * diff;
      }
      stddev = sqrt(stddev / double(select.size()));

      // sample new population using mean and stddev for each parameter
#pragma omp parallel for
      for (size_t i = 0; i < m_particles.size(); ++i) {
        // initialize population with uniform random distribution
        auto &rng = m_rngs[unsigned(omp_get_thread_num())];
        std::normal_distribution<double> norm_dist(0.0, 1.0);

        DPoint rand(m_Np);
        for (size_t p = 0; p < m_Np; ++p)
          rand[p] = norm_dist(rng);

        m_particles[i].x = rand * stddev + mean;
        m_particles[i].DiscretizeAndConfine(*this);
      }
    }

    // now we select the m_pso_N_particles top particles for the PSO
    EvaluateFitness();
    auto idxs = GetIdxsOfTopN(m_pso_N_particles);

    std::vector<Particle> new_particles(m_pso_N_particles);
    for (size_t i = 0; i < m_pso_N_particles; ++i)
      new_particles[i] = m_particles[idxs[i]];
  }

  void InitVelocities() {
    // we initialize the velocities of the particles using Differential
    // Evolution (DE)
#pragma omp parallel for
    for (size_t i = 0; i < m_particles.size(); ++i) {
      // initialize population with uniform random distribution
      auto &rng = m_rngs[unsigned(omp_get_thread_num())];
      std::uniform_int_distribution<size_t> uni_dist(0, m_particles.size() - 1);

      // we need to random (but distinct) indices
      auto a = uni_dist(rng);
      auto b = uni_dist(rng);
      while (a == b)
        b = uni_dist(rng);

      auto diff = m_particles[a].x - m_particles[b].x;
      m_particles[i].v = m_de_F * diff * (m_fit[a] < m_fit[b] ? 1.0 : -1.0);
    }
  }

  auto GetIdxsOfTopN(size_t N) const {
    std::vector<size_t> idxs(m_particles.size());
    std::iota(idxs.begin(), idxs.end(), 0);

    // do a partial sort to get the N best points
    std::partial_sort(idxs.begin(), idxs.begin() + int(N), idxs.end(),
                      [&](size_t a, size_t b) { return m_fit[a] < m_fit[b]; });

    return std::vector<size_t>(idxs.begin(), idxs.begin() + long(N));
  }

  // Utility functions
  DPoint ItoDPoint(const IPoint &params) const {
    DPoint res(params.size());
    for (size_t p = 0; p < m_Np; ++p)
      res[p] = params[p] * m_min_steps[p];

    return res;
  }

  IPoint DtoIPoint(const DPoint &params) const {
    IPoint res(params.size());
    for (size_t p = 0; p < m_Np; ++p)
      res[p] = int(round(params[p] / m_min_steps[p]));

    return res;
  }

  int64_t Index(const IPoint &params) const {
    int64_t idx = 0;
    int64_t N_tot = 1;

    for (size_t p = 0; p < m_Np; ++p) {
      assert(params[p] >= m_lower_bounds[p]);
      assert(params[p] <= m_upper_bounds[p]);
      int64_t this_idx = params[p] - m_lower_bounds[p];

      idx += this_idx * N_tot;
      N_tot *= m_Ns[p];
    }

    return idx;
  }

private:
  void GenerateNeighbors(size_t start, size_t end, size_t num_vary,
                         const IPoint &center,
                         std::vector<IPoint> *int_params) const {
    // printf("GenNeighbor: %lu - %lu, vary %lu, center: ", start, end,
    // num_vary); for (size_t p = 0; p < center.size(); ++p)
    //   printf("%2i ", center[p]);
    // printf("\n");
    assert(start <= end);

    if (num_vary == 0) {
      int_params->push_back(center);
      return;
    }

    for (size_t i = start; i < end; ++i) {
      auto up = center;
      auto down = center;
      up[i] += 1;
      down[i] -= 1;

      // if we're hitting a boundary, expend by one layer in the other
      // direction
      if (up[i] > m_upper_bounds[i])
        up[i] -= 3;
      if (down[i] < m_lower_bounds[i])
        down[i] += 3;

      GenerateNeighbors(i + 1, end, num_vary - 1, up, int_params);
      GenerateNeighbors(i + 1, end, num_vary - 1, down, int_params);
    }
  }

  // Compute neighborhood size
  size_t NeighborhoodSize() const {
    size_t nb_size = 0;

    // add number of ways we can vary exactly k parameters
    for (size_t k = 0; k <= std::min(m_N_vary, m_Np); ++k) {
      // there are Binom(m_Np, k) ways to choose k parameters from m_Np,
      // and there are 2^k ways to permute each paramter one step up and down
      nb_size += size_t(Binom(m_Np, k)) * (1UL << k);
    }
    return nb_size;
  }

  void AppendNeighborhood(const IPoint &center,
                          std::vector<IPoint> *int_params) const {
    // like above, process one depth (number of params changed) at a time
    for (size_t k = 0; k <= std::min(m_N_vary, m_Np); ++k) {
      GenerateNeighbors(0, m_Np, k, center, int_params);
    }
  }

  void SetInformationLinks() {
#pragma omp parallel for
    for (size_t i = 0; i < m_particles.size(); ++i) {
      auto &rng = m_rngs[unsigned(omp_get_thread_num())];
      std::uniform_int_distribution<size_t> dist(0, m_particles.size() - 1);

      m_particles[i].informees.resize(m_pso_N_inform);
      for (size_t k = 0; k < m_pso_N_inform; ++k)
        m_particles[i].informees[k] = dist(rng);
    }
  }

  void DoInformationExchange() {
    // we don't have atomic minimum, so don't parallelize here
    for (size_t i = 0; i < m_particles.size(); ++i) {
      // each particle informs itself
      auto &me = m_particles[i];
      me.local_best.Combine(me.prev_best);

      // inform other particle
      for (size_t k = 0; k < m_pso_N_inform; ++k) {
        auto &other = m_particles[me.informees[k]];
        other.local_best.Combine(me.prev_best);
      }
    }
  }

  const ParallelEngine &m_parallel_engine;
  const RecsT &m_recs;
  const BLSParamSpace m_meta_params;
  size_t m_meta_idx;

  // number of parameters to vary for fitness neighborhood
  const size_t m_N_vary;

  const size_t m_N_cycles;

  // number of parameters
  const size_t m_Np;

  // Internally, we use integers to represent the parameter values. We search
  // on a grid of m_min_steps. The integers range from m_lower_bounds to
  // m_upper_bounds
  const DPoint m_min_steps;
  const IPoint m_lower_bounds, m_upper_bounds;
  const IPoint m_Ns; // number of points in each dimension

  // PSO constants
  const size_t m_pso_N_iter;
  const size_t m_pso_N_particles;
  const size_t m_pso_N_inform;
  const double m_pso_C;
  const double m_pso_W;

  // UMDA constants
  const size_t m_umda_N_iter;
  const size_t m_umda_N_particles;
  const size_t m_umda_N_select;

  // DE constants
  const double m_de_F;

  // random number generators, one per OMP thread
  std::vector<std::mt19937_64> m_rngs;

  std::vector<Particle> m_particles;
  std::vector<double> m_fit;

  // cache
  mutable std::unordered_map<int64_t, double> m_score_cache, m_fitness_cache;
};

} // namespace BLS
} // namespace MAQUETTE
