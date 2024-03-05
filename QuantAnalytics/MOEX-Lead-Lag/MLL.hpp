// vim:ts=2
//===========================================================================//
//                                 "MLL.hpp":                                //
//    Time Series Representation, Types and Consts for Lead-Lag Analysis:    //
//===========================================================================//
#pragma  once
#include <string>
#include <stdexcept>
#include <cstring>

namespace MLL
{
  //-------------------------------------------------------------------------//
  // "MDEntryT": Tick over non-uniform time:                                 //
  //-------------------------------------------------------------------------//
  // (*) Trading time is from  10:00 to 23:50 MSK   (for Futures, with clearing
  //     gaps in 14:00-14:04, 18:45-19:00; for Spot with SettlDate=TOD, closing
  //     time is eg 15:00 or 17:15);
  // (*) MktData are provided with millisecond-level resolution;
  // (*) so we represent all data as fixed-size array where each slot  corresps
  //     to a particular millisecond within the over-all trading period;   from
  //     10:00 to 23:50 there are 830 min = 49,800 sec = 49,800,000 msec --  so
  //     the data array easily fits in memory;
  // (*) for this analysis, we are only interested in Pxs, not Qtys;
  // (*) we can consider (separately) Bid, Ask and Mid-Point Pxs:
  // (*) XXX: "m_prev" is not actually used by the Hayashi-Yoshida estimator:
  //
  struct MDEntryT
  {
    double  m_px;
    int     m_curr;     // Idx when      this px first appeared
    int     m_prev;     // Idx when the  prev px first appeared (or -oo)
    int     m_next;     // Idx when the  next px will  appear   (or +oo)

    MDEntryT()
    : m_px  (0.0),
      m_curr(0),
      m_prev(0),
      m_next(0)
    {}

    bool IsEmpty() const
      { return m_px == 0.0 && m_curr == 0 && m_prev == 0 && m_next == 0; }
  };

  // Max Number of MilliSecond Ticks from 10:00 to 23:50:
  // 830 min = 49'800 sec = 49'800'000 msec:
  //
  constexpr int MaxTicks  = 49'800'000;

  //-------------------------------------------------------------------------//
  // "FormatT": Supported MktData File Formats:                              //
  //-------------------------------------------------------------------------//
  enum class FormatT
  {
    FORTS   = 0,  // Official FORTS MktData distribution fmt
    MICEX   = 1,  // Official MICEX MktData distrivution fmt
    MQT     = 2,  // Our own recorded fmt
    ICE     = 3   // ICE from TickDataMerket(?)
  };

  // "GetExch": string -> FormatT:
  //
  inline FormatT GetExch(std::string const& a_str)
  {
    return
      (a_str == "FORTS")
      ?  FormatT::FORTS :
      (a_str == "MICEX")
      ?  FormatT::MICEX :
      (a_str == "MQT"  )
      ?  FormatT::MQT   :
      (a_str == "ICE"  )
      ?  FormatT::ICE   :
         throw std::invalid_argument("Invalid Exchange=" + a_str);
  }

  //-------------------------------------------------------------------------//
  // "PxSideT": Side of Px we are intertested in:                            //
  //-------------------------------------------------------------------------//
  enum class PxSideT
  {
    Bid = 0,
    Ask = 1,
    Mid = 2
  };

  // "GetPxSide": string -> PxSide:
  //
  inline PxSideT GetPxSide(std::string const& a_str)
  {
    char const* cstr = a_str.data();
    return
      (strcmp(cstr, "Bid") == 0)
      ? PxSideT::Bid
      :
      (strcmp(cstr, "Ask") == 0)
      ? PxSideT::Ask
      :
      (strcmp(cstr, "Mid") == 0)
      ? PxSideT::Mid
      : throw std::invalid_argument("GetPxSide: Invalid Side=" + a_str);
  }


  //-------------------------------------------------------------------------//
  // "StatsT": Some TimeSeries Statistics:                                   //
  //-------------------------------------------------------------------------//
  struct StatsT
  {
    double  m_retRate;  // Trend (return rate per 1 msec)
    double  m_inter;    // Avg interval (in msec) between ticks
    double  m_var;      // Quadratic variance (non-normalised)
  };

  //-------------------------------------------------------------------------//
  // "MethodT": Computational Methods:                                       //
  //-------------------------------------------------------------------------//
  enum class MethodT
  {
    HY        = 0,      // Hayashi-Yoshida Cross-Corr
    VAR       = 1,      // Vector Auto-Regression
    Uniform   = 2,      // Classical Cross-Corr over uniform intervals
    CondProbs = 3       // Apparently unstable, NOT RECOMMENDED
  };

  struct MethodInfo
  {
    MethodT   m_method;
    int       m_winSz;  // Window Size in msec (for VAR and Uniform)
  };

  // "GetMethod": string -> "MethodInfo":
  //
  inline MethodInfo GetMethod(std::string const& a_method_str)
  {
    char const* cstr = a_method_str.data();
    if (strcmp(cstr, "HY") == 0)
      return { MethodT::HY, 0 };        // No WindowSz for HY
    else
    if (strcmp(cstr, "CondProbs") == 0)
      return { MethodT::CondProbs, 0 }; // No WindowSz either

    // In all other cases, WindowSz is required:
    char const* dash = strchr(cstr, '-');
    if (dash  == nullptr)
      throw std::invalid_argument
            ("GetMethod: Invalid Method=" + a_method_str);
    int winSz  = atoi(dash + 1);
    if (winSz <= 0)
      throw std::invalid_argument
            ("GetMwethod: Invalid WinSz=" + std::to_string(winSz));

    int n = dash - cstr;
    if (strncmp(cstr, "Uniform", n) == 0)
      return { MethodT::Uniform, winSz };
    else
    if (strncmp(cstr, "VAR",     n) == 0)
      return { MethodT::VAR,     winSz };
    else
      throw std::invalid_argument
            ("GetMethod: Invalid Method=" + a_method_str);
  }
}
// End namespace MLL
