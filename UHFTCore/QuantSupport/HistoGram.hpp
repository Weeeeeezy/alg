// vim:ts=2:et
//===========================================================================//
//                                "HistoGram.hpp":                           //
//                     Construction of various histograms                    //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/Maths.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <string>
#include <utility>

namespace MAQUETTE
{
namespace QuantSupport
{
  //=========================================================================//
  // "HistoGram" Class:                                                      //
  //=========================================================================//
  // "K" is the number of intervals between "a" and "b"; there are also inter-
  // vals (-oo, a) and (b, +oo), so (K+2) intervals in total:
  //
  template<unsigned K>
  struct HistoGram
  {
  public:
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    static_assert(K >= 1, "HistoGram: Requires K >= 1");

    ObjName const     m_name;
    double  const     m_a;           // The range of poss vals: [a .. b]
    double  const     m_b;           //
    double  const     m_s;           // Step (interval)
    mutable unsigned  m_hist[K + 2]; // Frequency Counters
    mutable unsigned  m_n;           // Total     Counter
    mutable double    m_min;         // Other stats
    mutable double    m_max;         //
    mutable double    m_sum;         //

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    HistoGram()
    : m_name(EmptyObjName),
      m_a   (NaN<double>),
      m_b   (NaN<double>),
      m_s   (NaN<double>),
      m_hist(),
      m_n   (0),
      m_min ( Inf<double>),
      m_max (-Inf<double>),
      m_sum (0.0)
    {
      for (unsigned i = 0; i < K + 2; ++i)
        m_hist[i] = 0;
    }

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    HistoGram
    (
      std::string const&  a_name,
      double              a_left,
      double              a_right
    )
    : m_name(MkObjName(a_name)),
      m_a   (a_left),
      m_b   (a_right),
      m_s   ((m_b - m_a) / double(K)),
      m_hist(),
      m_n   (0),
      m_min ( Inf<double>),
      m_max (-Inf<double>),
      m_sum (0.0)
    {
      if (utxx::unlikely(!(m_s > 0.0)))
        throw utxx::badarg_error("HistoGram::Ctor: Invalid arg(s)");
      assert(m_a < m_b);
    }

    //-----------------------------------------------------------------------//
    // "Update":                                                             //
    //-----------------------------------------------------------------------//
    // Updates the statistics according to "a_val":
    //
    void Update(double a_val) const
    {
      assert (IsFinite(a_val));

      // Get the index of the interval which "a_val" falls into:
      unsigned i =
        utxx::unlikely(a_val <  m_a)
        ? 0       :
        utxx::unlikely(a_val >= m_b)
        ? (K + 1) :
        (unsigned(Floor((a_val - m_a) / m_s)) + 1);

      // Update the stats:
      assert(i <= K + 1);
      m_hist[i]++;
      m_n++;
      m_min  = std::min<double>(m_min, a_val);
      m_max  = std::max<double>(m_max, a_val);
      m_sum += a_val;
    }

    //-----------------------------------------------------------------------//
    // Output:                                                               //
    //-----------------------------------------------------------------------//
    friend inline std::ostream& operator<<
    (
      std::ostream&    a_os,
      HistoGram const& a_hist
    )
    {
      // Title:
      a_os << '\n' << a_hist.m_name.data() << ":\n\n";

      // Bars (normalised to length 50):
      for (unsigned i = 0; i < K + 2; ++i)
      {
        // The upper value bound corresponding to this var, the count and pct:
        double bound = a_hist.m_a  + double(i)  * a_hist.m_s;
        double freq  = double(a_hist.m_hist[i]) / double(a_hist.m_n);
        int    pct   = (a_hist.m_n > 0) ? int(Round(100.0 * freq))  : 0;

        // Legend:
        a_os << bound << ":\t" << a_hist.m_hist[i] << "\t(" << pct << "%)\t";

        // The bar:
        int    len   = int(Round(50.0 * freq));
        for (int j = 0; j < len; ++j)
          a_os << "\u2588";
        a_os << '\n';
      }
      // Summary:
      double avg =
        (a_hist.m_n > 0) ? (a_hist.m_sum / double(a_hist.m_n)) : 0.0;

      a_os << "\nMin   = " << a_hist.m_min << "\nMax   = " << a_hist.m_max
           << "\nAvg   = " << avg          << "\nCount = " << a_hist.m_n
           << std::endl;
      return a_os;
    }
  };
} // End namespace QuantSupport
} // End namespace MAQUETTE
