// vim:ts=2:et
//===========================================================================//
//                           "VolEstimatorRS.hpp":                           //
//    Range-Based Volatility Estimator using the Rogers-Satchell Method      //
//===========================================================================//
#pragma once
#include "Basis/Maths.hpp"
#include <utxx/time_val.hpp>
#include <utxx/compiler_hints.hpp>

namespace MAQUETTE
{
namespace Predictors
{
  //=========================================================================//
  // "VolEstimatorRS" Class:                                                 //
  //=========================================================================//
  class VolEstimatorRS
  {
  private:
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    long const  m_periodSec;   // Interval duration (seconds)

    long        m_currInt;     // Current interval number
    double      m_openPx;      // Opening Px for the curr interval
    double      m_closePx;     // Closing Px for the curr interval
    double      m_minPx;       // Min     Px for the curr interval
    double      m_maxPx;       // Max     Px for the curr interval
    double      m_vol;         // Estimated Volatility for the prev interval

    // Default Ctor is deleted:
    VolEstimatorRS() = delete;

  public:
    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    VolEstimatorRS(long a_periodSec)
    : m_periodSec (a_periodSec),
      m_currInt   (0),
      m_openPx    (NaN<double>),
      m_closePx   (NaN<double>),
      m_minPx     (NaN<double>),
      m_maxPx     (NaN<double>),
      m_vol       (NaN<double>)
    {
      assert(m_periodSec > 0);
    }

    //-----------------------------------------------------------------------//
    // Updating the Vol on a MktData tick:                                   //
    //-----------------------------------------------------------------------//
    // "a_ts": curr TimeStamp;
    // "a_px": the corresp price, normally a mid-point price:
    //
    void Update(utxx::time_val a_ts, double a_px)
    {
      // Calculate the Interval Number for "a_ts":
      long intN = a_ts.seconds() / m_periodSec;

      if (utxx::likely(intN == m_currInt))
      {
        // We are still within the same Interval as on the prev invocation:
        m_minPx   = std::min<double>(m_minPx, a_px);
        m_maxPx   = std::max<double>(m_maxPx, a_px);
        m_closePx = a_px;
      }
      else
      {
        // A new Interval is commencing now. Obviously, its number must move
        // forward only:
        assert(intN > m_currInt);

        // FIXME: At the moment, we do not care for empty intervals (ie gaps in
        // MktDdata), ie we implicitly assume that in this case,
        // intN == m_currInt + 1
        // This assumption is valid for sufficiently liquid markets and long in-
        // tervals.
        // Then "a_px" is the Closing Px for the prev interval, and the Opening
        // Px for the nex interval.
        // Thus the RS Quadratic Variance Estimation for the PREVIOUS interval
        // is:
        double var2 = log(m_maxPx / m_openPx) * log(m_maxPx / m_closePx) +
                      log(m_minPx / m_openPx) * log(m_minPx / m_closePx);
        // If "var2" is Finite, it must be non-negative:
        assert(!Finite(var2) || var2 >= 0.0);

        // Then the (relative) volatility for the interval is:
        m_vol = sqrt(var2);

        // Initialise the new Interval:
        m_currInt = intN;
        m_openPx  = m_minPx = m_maxPx = m_closePx = a_px;
      }
    }

    //-----------------------------------------------------------------------//
    // Accessor:                                                             //
    //-----------------------------------------------------------------------//
    double GetVol() const { return m_vol; }
  };
} // End namespace Predictors
} // End namespace MAQUETTE
