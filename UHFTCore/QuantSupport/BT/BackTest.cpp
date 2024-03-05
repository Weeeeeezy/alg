
#include "BackTest.h"

#include "QuantSupport/BT/DataProvider.h"

#include "QuantSupport/BT/SecDefs.h"

#include "Connectors/OrderBook.h"
#include "Basis/ConfigUtils.hpp"

#include <boost/ptr_container/ptr_deque.hpp>
#include <boost/algorithm/string.hpp>

#include <fstream>
#include <list>

namespace MAQUETTE
{
  namespace
  {
    time_t parse_time(const std::string& s)
    {
      tm    t = tm();
      char* p = strptime(s.c_str(), "%Y-%m-%dT%H:%M:%S", &t);
      if (p != s.c_str() + s.size())
        throw utxx::runtime_error("parsing time error: ", s);
      return timegm(&t);
    }

  } // namespace

  using namespace History;

  struct BackTesting
  {
    private:
      // Next fields are mandotory for using LOG_ macros
      spdlog::logger* m_logger;
      int             m_debugLevel;
      TimeRange       m_period;
      uint32_t        cur_window = 0;

    public:
      DataProvider dm;


    public:
    /**
     * @param pt - back test properties in boost property tree
     * @param logger - pointer to logger instance
     * @param tickers_s - comma-separated tickers (coin\@market)
     */
    BackTesting(
        const ptree&       pt,
        spdlog::logger*    logger,
        std::string const& tickers_s) :
      m_logger{logger},
      m_debugLevel{pt.get<int>("DebugLevel")},
      m_period{
          parse_time(pt.get<std::string>("From")),
          parse_time(pt.get<std::string>("To")),
          pt.get<uint32_t>("Window", 24) * 3600},
      dm{SecDefs(pt.get<std::string>("Exchange")),
         tickers_s,
         pt.get<std::string>("LoadData"),
         pt.get<std::string>("DataFolder", ".") + "/",
         pt.get<uint32_t>("MktDepth", 10),
         m_period,
         logger,
         m_debugLevel}
    {
    }

    struct Consumer
    {
      uint16_t        id; // index in secs
      const SecDefD*  s;
      HistoricalData* c;
    };

    std::map<Strategy*, std::list<Consumer>> m_consumers;
    std::mutex                               mutex;

    void Set(
        Strategy*       strategy,
        HistoricalData* consumer,
        SecDefD const   a_instr,
        bool            order_mgmt)
    {
      auto [id, instrument] = dm.GetInstrument(a_instr.m_SecID);

      Consumer cr{id, instrument, consumer};
      std::unique_lock<std::mutex> lock(mutex);
      std::list<Consumer>&         cons = m_consumers[strategy];
      lock.unlock();

      if (order_mgmt)
        cons.push_front(cr);
      else
        cons.push_back(cr);
    }

    void Run(Strategy* s)
    {
      if (!m_period.inside(cur_window))
      {
        return;
      }

      auto                 it   = dm.begin();
      auto                 ie   = dm.end();
      std::list<Consumer>& cons = m_consumers[s];
      for (; it != ie; ++it)
      {
        auto const& data = *it;
        std::list<Consumer>::iterator i = cons.begin(), e = cons.end();
        for (; i != e; ++i)
        {
          if (i->id == it->id)
          {
            if (it->type == 1)
            {
              i->c->SetBook(
                  *(i->s),
                  data.time,
                  data.asks_size,
                  data.quotes[0],
                  data.bids_size,
                  data.quotes[1]);
            }
            else
            {
              i->c->SetTrade(
                  *(i->s),
                  data.time,
                  data.type == 3,
                  data.trade.price,
                  data.trade.count);
            }
          }
        }
      }
    }

    std::map<Strategy*, std::list<Consumer>>::iterator m_consumers_it;

    void RunImpl()
    {
      for (;;)
      {
        std::unique_lock<std::mutex> lock(mutex);
        if (m_consumers_it == m_consumers.end())
          return;
        Strategy* s = m_consumers_it->first;
        ++m_consumers_it;
        lock.unlock();
        try
        {
          Run(s);
        }
        catch (std::exception& e)
        {
            LOG_ERROR(1, "BackTesting::RunImpl error: {}", e.what())
        }
      }
    }

    void Run(uint32_t threads)
    {
      LOG_INFO(1, "Test run sturted on {} threads", threads)

      while (m_period.inside(cur_window))
      {
        auto [from, to] = m_period.window(cur_window);
        dm.Load(from, to);
        m_consumers_it = m_consumers.begin();
        std::vector<std::thread> thrds;
        for (uint32_t i = 0; i < threads; ++i)
        {
          thrds.push_back(std::thread(&BackTesting::RunImpl, this));
        }
        RunImpl();

        std::for_each(
            thrds.begin(),
            thrds.end(),
            std::bind(&std::thread::join, std::placeholders::_1));
        ++cur_window;
      }

      LOG_INFO(1, "Test run finished")
    }
  };

  static std::unique_ptr<BackTesting> _back_testing;

  namespace History
  {
    void SubscribeMktData(
        Strategy*       s,
        HistoricalData* c,
        const SecDefD&  a_instr,
        bool            order_mgmt)
    {
      assert(s && c);
      _back_testing->Set(s, c, a_instr, order_mgmt);
    }

    struct brute_node
    {
      std::string name;

      struct node
      {
        enum type
        {
          const_t,
          int_t,
          double_t
        };
        type m_type;

        std::string str;
        int         f_i, t_i, s_i, c_i;
        double      f_d, t_d, s_d, c_d;

        node(
            const std::string& _name,
            const std::string& f,
            const std::string& t,
            const std::string& s)
        {
          if (f == t)
          {
            str    = f;
            m_type = const_t;
          }
          else
          {
            m_type = int_t;
            if (std::find(f.begin(), f.end(), '.') != f.end())
              m_type = double_t;
            if (std::find(t.begin(), t.end(), '.') != t.end())
              m_type = double_t;
            if (std::find(s.begin(), s.end(), '.') != s.end())
              m_type = double_t;

            if (m_type == int_t)
            {
              f_i = boost::lexical_cast<int>(f);
              t_i = boost::lexical_cast<int>(t);
              s_i = boost::lexical_cast<int>(s);
              if (s_i < 1)
                throw utxx::runtime_error("step should be positive for ", _name);
              c_i = f_i;
            }

            if (m_type == double_t)
            {
              f_d = boost::lexical_cast<double>(f);
              t_d = boost::lexical_cast<double>(t);
              s_d = boost::lexical_cast<double>(s);
              if (s_d < std::numeric_limits<double>::epsilon())
                throw utxx::runtime_error("step should be positive for ", _name);
              c_d = f_d;
            }
          }
        }
        std::string value() const
        {
          if (m_type == const_t)
            return str;
          if (m_type == int_t)
            return std::to_string(c_i);
          return std::to_string(c_d);
        }
        bool next()
        {
          if (m_type == const_t)
            return false;
          if (m_type == int_t)
          {
            if (c_i >= t_i)
              return false;
            c_i += s_i;
            if (c_i > t_i)
              c_i = t_i;
            return true;
          }
          if (c_d * (1. + std::numeric_limits<double>::epsilon()) > t_d)
            return false;
          c_d += s_d;
          if (c_d > t_d)
            c_d = t_d;
          return true;
        }
      };

      std::vector<node>        nodes;
      std::vector<std::string> bc;

      std::string path;
      brute_node(const std::string& fname, ptree const* p_bt) : cur_node()
      {
        std::string::const_iterator p =
            std::find(fname.begin(), fname.end(), '.');
        if (p != fname.end())
        {
          ++p;
          path = std::string(fname.begin(), p);
          name = std::string(p, fname.end());
        }
        else
        {
          path = "Main.";
          name = fname;
        }

        std::string from = p_bt->get<std::string>(name + "_from");
        std::string to   = p_bt->get<std::string>(name + "_to");
        std::string step = p_bt->get<std::string>(name + "_step");

        std::vector<std::string> bf, bt, bs;
        boost::split(bf, from, [](char c) { return c == ','; });
        boost::split(bt, to, [](char c) { return c == ','; });
        boost::split(bs, step, [](char c) { return c == ','; });

        if (bf.size() != bt.size() || bt.size() != bs.size())
          throw utxx::runtime_error("bad grid params for: ", name);

        for (uint32_t i = 0; i != bf.size(); ++i)
          nodes.push_back(node(name, bf[i], bt[i], bs[i]));

        bc = bf;
        init();
      }

      void apply(ptree& pt, std::string& bt_name)
      {
        uint32_t c_size = 1;
        uint32_t c      = cur_node;
        for (uint32_t i = 0; i != nodes.size(); ++i)
        {
          const std::vector<node>& cn  = all_nodes[i];
          uint32_t                 cur = (c / c_size) % uint32_t(cn.size());
          bc[i]                        = cn[cur].value();
          c_size *= uint32_t(cn.size());
          c -= c % c_size;
        }

        std::string p = boost::algorithm::join(bc, ",");
        pt.put<std::string>(path + name, p);
        bt_name = "{" + name + " = " + p + "}";
      }

      std::vector<std::vector<node>> all_nodes;
      uint32_t                       cur_node, all_size;

      void init()
      {
        all_nodes.resize(nodes.size());
        all_size = 1;
        for (uint32_t i = 0; i != nodes.size(); ++i)
        {
          std::vector<node>& cn = all_nodes[i];
          node&              n  = nodes[i];
          do
          {
            cn.push_back(n);
          } while (n.next());
          all_size *= uint32_t(cn.size());
        }
      }

      bool next()
      {
        if (cur_node + 1 < all_size)
        {
          ++cur_node;
          return true;
        }
        return false;
      }
    };

    struct BruteGenerator
    {
      static const uint32_t        max_brutes = 1000000;
      boost::ptr_deque<StrategyBT> strats;
      std::vector<std::string>     brute_params, bt_names;

      ptree&       pt;
      ptree const* p_bt;

      EPollReactor&   reactor;
      spdlog::logger* logger;
      InitBT          init;
      std::string     omc_name;

      BruteGenerator(
          ptree&             _pt,
          EPollReactor&      _reactor,
          spdlog::logger*    _logger,
          InitBT             _init,
          ptree const*       _p_bt,
          const std::string& _omc_name) :
        pt(_pt),
        p_bt(_p_bt),
        reactor(_reactor),
        logger(_logger),
        init(_init),
        omc_name(_omc_name)
      {
        std::string brute = p_bt->get<std::string>("Brute");
        boost::split(brute_params, brute, [](char c) { return c == ','; });
        if (brute_params.empty())
          throw std::runtime_error("BruteGenerator() nothing for brute");
        bt_names.resize(brute_params.size());
        impl(0);
        logger->info("BruteGenerator(): {} instances created", strats.size());
      }

      void impl(uint32_t p_id)
      {
        brute_node b(brute_params[p_id], p_bt);
        do
        {
          b.apply(pt, bt_names[p_id]);
          if (p_id + 1 != brute_params.size())
            impl(p_id + 1);
          else
          {
            if (strats.size() == max_brutes)
              throw std::runtime_error("max_brutes exceeded");
            pt.put<std::string>(
                omc_name + ".Name",
                boost::algorithm::join(bt_names, ""));
            strats.push_back(init(pt, reactor, logger));
          }
        } while (b.next());
      }
    };

    SecDefD const& FindSecDef(const std::string& ticker)
    {
      return _back_testing->dm.GetSecDef(ticker);
    }
  } // namespace History

  void SetBackTestingEnvironment(
      ptree&          pt,
      EPollReactor&   reactor,
      spdlog::logger* logger,
      InitBT          init)
  {
    ptree const* p = GetParamsOptTree(pt, "BT");
    if (p)
    {
      std::string omc_name = p->get<std::string>("OMC", "OMC");
      {
        std::string   results = pt.get<std::string>(omc_name + ".Result");
        std::ofstream file(results.c_str(), std::ios::trunc);
        if (!file)
          throw utxx::runtime_error("Creating file error: ", results);
      }

      _back_testing.reset(
          new BackTesting(*p, logger, pt.get<std::string>("MDC.Tickers")));

      std::string mode = p->get<std::string>("Mode");
      if (mode == "current")
      {
        pt.put<std::string>(omc_name + ".Name", "current");
        std::unique_ptr<StrategyBT> bt(init(pt, reactor, logger));
        _back_testing->Run(1);
      }
      else if (mode != "grid")
        throw utxx::runtime_error("Unknown BT Mode: ", mode);
      else
      {
        pt.put<std::string>(omc_name + ".Deals", "");
        uint32_t       threads = p->get<uint32_t>("Threads");
        BruteGenerator g(pt, reactor, logger, init, p, omc_name);
        _back_testing->Run(threads);
      }
    }
    else
    {
      std::unique_ptr<StrategyBT> rt(init(pt, reactor, logger));
      rt->Run();
    }
  }
} // namespace MAQUETTE
