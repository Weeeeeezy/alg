// vim:ts=2
//===========================================================================//
//                             "TickDataReader.hpp":                         //
//    Reading and Verifying the Time Series Data for Lead-Lag Analysis       //
//===========================================================================//
#pragma  once
#include "QuantAnalytics/MOEX-Lead-Lag/MLL.hpp"
#include <climits>
#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cmath>

namespace MLL
{
  //=========================================================================//
  // "TickDataReader" Class:                                                 //
  //=========================================================================//
  class TickDataReader
  {
  private:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    int     const m_date;       // As YYYYMMDD
    int     const m_timeFrom;   // As HHMMSSsss
    int     const m_timeTo;     // Same
    PxSideT const m_pxSide;
    int     const m_debugLevel;

    mutable int   m_prev;
    mutable int   m_idxFrom;    // Lowest  idx in the target just filled in
    mutable int   m_idxTo;      // Highest idx in the target just filled in

    // Default Ctor is deleted:
    TickDataReader() = delete;

  public:
    // Accessors:
    int GetIdxFrom   () const { return m_idxFrom;    }
    int GetIdxTo     () const { return m_idxTo;      }
    int GetDebugLevel() const { return m_debugLevel; }

  private:
    //=======================================================================//
    // "TickProcessor":                                                      //
    //=======================================================================//
    template<int N>
    void TickProcessor
    (
      int         a_time,      // As HHMMSSuuuuuu
      double      a_px,
      MDEntryT  (&a_targ)[N]   // Target to be filled in
    )
    {
      // First of all, convert "a_time" (which is HHMMSSsss) into the number of
      // millieconds since 10:00:00.000 MSK:
      assert(a_time > 0);

      int  msec = a_time % 1000;
      a_time   /= 1000;

      int  sec  = a_time % 100;
      a_time   /= 100;

      int  min  = a_time % 100;
      int  hour = a_time / 100;

      // So the number of milliseconds since start of the day's trading:
      int curr = 1000 * ((hour - 10) * 3600 + min * 60 + sec) + msec;

      if (curr < 0 || curr > N)
        throw std::invalid_argument
              ("TickProcessor: Invalid Time=" + std::to_string(a_time));

      // The prev valid "MDEntryT" (remembered from the prev invocation -- to
      // avoid going back to find it):
      assert(m_prev < N);

      // NB: Obviously, (prev > curr) is not allowed, but it may happen (due to
      // a limited resolution of time stamps) that prev == curr;   in that case
      // we simply over-write that entry with a new px:
      if (m_prev > curr)
        throw std::invalid_argument
              ("TickProcessor: Non-Monotonic TimeStamps: Prev=" +
               std::to_string(m_prev) + ", Curr=" + std::to_string(curr));

      // So initialise the "curr" entry:
      MDEntryT& mde = a_targ[curr];
      mde.m_px      = a_px;
      mde.m_curr    = curr;
      if (m_prev < curr)
        mde.m_prev  = m_prev;   // Otherwise, keep the original "mde.m_prev"!
      mde.m_next    = N;        // As yet, we don't know the next one, even if
                                //   curr == m_prev
      // Propagate the prev px for all intermediate entries (if any):
      if (0 <= m_prev && m_prev < curr)
      {
        MDEntryT& mdp = a_targ[m_prev];
        double prevPx = mdp.m_px;
        mdp.m_next    = curr;

        for (int i = m_prev + 1; i < curr; ++i)
        {
          MDEntryT& mdi = a_targ[i];
          mdi.m_px      = prevPx;
          mdi.m_curr    = m_prev;       // When that px was set
          mdi.m_prev    = mdp.m_prev;
          mdi.m_next    = curr;
        }
      }
      // Update the range of indices (each of them is actually updated only
      // once):
      m_idxFrom = std::min<int>(m_idxFrom, curr);
      m_idxTo   = std::max<int>(m_idxTo,   curr);

      // For the next iteration:
      m_prev = curr;
    }

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    TickDataReader
    (
      int          a_date,        // As YYYYMMDD
      int          a_time_from,   // As HHMM -> convert into HHMMSSsss
      int          a_time_to,     // As above
      PxSideT      a_px_side,
      int          a_debug_level
    )
    : m_date      (a_date),
      m_timeFrom  (a_time_from * 100'000), // 5 zeros: SSsss
      m_timeTo    (a_time_to   * 100'000), // Same
      m_pxSide    (a_px_side),
      m_debugLevel(a_debug_level),
      m_prev      (-1),
      m_idxFrom   (INT_MAX),
      m_idxTo     (INT_MIN)
    {
      if (m_date   <  20150101    || m_timeFrom < 10'00'00'000 ||
          m_timeTo <= m_timeFrom  || m_timeTo   > 23'50'00'000 ||
         (m_pxSide != PxSideT::Bid && m_pxSide != PxSideT::Ask &&
          m_pxSide != PxSideT::Mid))
        throw std::invalid_argument("TickDataReader::Ctor: Invalid arg(s)");
    }

    //=======================================================================//
    // "GetTickData":                                                        //
    //=======================================================================//
    template<int N>
    void GetTickData
    (
      char const* a_file,
      FormatT     a_fmt,
      char const* a_symbol,
      MDEntryT  (&a_targ)[N]    // Target to be filled in
    )
    {
      //---------------------------------------------------------------------//
      // Reset the transient data:                                           //
      //---------------------------------------------------------------------//
      m_prev    = -1;
      m_idxFrom = INT_MAX;
      m_idxTo   = INT_MIN;
      memset(a_targ, '\0', N * sizeof(MDEntryT));

      //---------------------------------------------------------------------//
      // Open the file:                                                      //
      //---------------------------------------------------------------------//
      if (a_file    == nullptr || *a_file   == '\0' ||
          a_symbol  == nullptr || *a_symbol == '\0')
        throw std::invalid_argument("GetTickData: Invalid arg(s)");

      // NB: If it is MICEX, then file name must contain the date (there are no
      // dates inside) -- check it: It must end with "YYYYMMDD.txt":
      if (a_fmt == FormatT::MICEX)
      {
        int date    = 0;
        int fileLen = strlen(a_file);

        if (fileLen < 12 ||
            sscanf(a_file + fileLen - 12, "%8d.txt", &date) != 1 ||
            date != m_date)
          throw std::invalid_argument
                ("GetTickData(MICEX): Invalid EmbeddedDate=" +
                 std::to_string(date) + ": Must be " + std::to_string(m_date));
      }
      FILE* f = fopen(a_file, "r");
      if (f == nullptr)
        throw std::invalid_argument
                   ("GetTickData: Cannot open MktData File: " +
                    std::string(a_file));

      // Input Buffer:
      char line[256];  // Should be enough

      double bidPx   = 0.0;
      double askPx   = 0.0;
      double prevPx  = 0.0;  // Prev Bid, Ask or MidPx -- depends on the call
      int    symLen  = strlen(a_symbol);

      // Symbol Pos: It's 0 for all fmts exceot MQT; for MQT (OrderBook updates
      // only -- we are not interested in other lines), it is 52:
      int   symPos   =
              (a_fmt == FormatT::MQT  ) ? 52 : 0;
      // Buy/Sell Flag Pos: For MICEX and FORTS only:
      int    bsPos   = symLen  +
             ((a_fmt == FormatT::FORTS) ?  3 :
              (a_fmt == FormatT::MICEX) ?  1 :
              INT_MIN);
      // TimePos (but the Time formats are different in all those cases):
      int    timePos =
             ((a_fmt == FormatT::FORTS) ? (symLen + 13) :
              (a_fmt == FormatT::MICEX) ? (symLen +  3) :
              (a_fmt == FormatT::MQT  ) ? 11 :
              (a_fmt == FormatT::ICE  ) ? (symLen + 12) :
              INT_MIN);
      // CommasPos: Not applicable to MQT:
      int    ccPos   =
             ((a_fmt == FormatT::FORTS || a_fmt == FormatT::MICEX)
              ? (timePos +  9) :
              (a_fmt == FormatT::ICE  )
              ? (timePos + 19) :
              INT_MIN);
      // PricePos: For MICEX and FORTS only:
      int    pxPos   = ccPos   +
             ((a_fmt == FormatT::FORTS || a_fmt == FormatT::MICEX) ?  2 :
              INT_MIN);

      int    nLines  = 0;     // All valid px lines
      int    nTicks  = 0;     // Non-zero  px ticks in the "exec" direction
      int    nOpp    = 0;     // Non-zero  px ticks in the opposite dir
      double avgSprd = 0.0;
      double avgTick = 0.0;
      double avgOpp  = 0.0;

      //---------------------------------------------------------------------//
      // Read the data in:                                                   //
      //---------------------------------------------------------------------//
      while (true)
      {
        //-------------------------------------------------------------------//
        // Get the line:                                                     //
        //-------------------------------------------------------------------//
        if (fgets(line, int(sizeof(line)), f) == nullptr)
          // We assume this is an EOF, not an error:
          break;

        //------------------------------------------------------------------//
        // Trade or Quote?                                                  //
        //------------------------------------------------------------------//
        switch (a_fmt)
        {
          case FormatT::FORTS:
          case FormatT::MICEX:
            // Then there must be ",," at "ccPos" -- otherwise it is a Trade,
            // not a Quote: we need the latter:
            if (strncmp(line + ccPos, ",,", 2) != 0)
              continue;
            break;
          case FormatT::MQT:
            // Need an explicit "OrderBook" designation:
            if (strncmp(line + 42, "OrderBook", 9) != 0)
              continue;
            break;
          case FormatT::ICE:
            // For ICE, there would be explicit "Quote" or "Trade" designation:
            if (strncmp(line + timePos + 12, ",Quote,", 7) != 0)
              continue;
            break;
          default:
            assert(false);
        }

        //-------------------------------------------------------------------//
        // Filter by Symbol:                                                 //
        //-------------------------------------------------------------------//
        if (strncmp(line + symPos, a_symbol, symLen) != 0)
          continue;

        //-------------------------------------------------------------------//
        // Filter by Date:                                                   //
        //-------------------------------------------------------------------//
        int date = 0;
        switch (a_fmt)
        {
          case FormatT::FORTS:
          {
            // FORTS: Date format is: "YYYYMMDD":
            int datePos = symLen + 5;
            if (sscanf(line + datePos, "%8d", &date) != 1)
              continue;
            break;
          }
          case FormatT::MICEX:
            // There is no explicit date in MICEX format, because there is 1
            // file per date:
            date = m_date;
            break;

          case FormatT::MQT:
          {
            // MQT: Date format is: YYYY-MM-DD, DatePos=0 always:
            int YYYY = 0, MM = 0, DD = 0;
            if (sscanf(line, "%04d-%02d-%02d", &YYYY, &MM, &DD) != 3)
              continue;
            date = 100 * (100 * YYYY + MM) + DD;
            break;
          }
          case FormatT::ICE:
          {
            // ICE: Date format is: "YYYY/MM/DD":
            int YYYY = 0, MM = 0, DD = 0;
            int datePos = symLen + 1;
            if (sscanf(line + datePos, "%04d/%02d/%02d", &YYYY, &MM, &DD) != 3)
              continue;
            date = 100 * (100 * YYYY + MM) + DD;
            break;
          }
          default:
            assert(false);
        }
        // So:
        if (date < m_date)
          continue;
        if (date > m_date)
          break;
        assert(date == m_date);

        //-------------------------------------------------------------------//
        // Filter by Time:                                                   //
        //-------------------------------------------------------------------//
        // "time" is in microseconds in the following format: HHMMSSuuuuuu:
        int time = 0;
        switch (a_fmt)
        {
          case FormatT::FORTS:
          case FormatT::MICEX:
            // Orig time format is: "HHMMSSsss", so use it "as is":
            if (sscanf(line + timePos, "%09d", &time) != 1)
              continue;
            break;

          case FormatT::MQT:
          {
            // Orig time format is: "HH:MM:SS.uuuuuu", already in MSK, so
            // convert microseconds into milliseconds:
            int hh = 0, mm = 0, ss = 0, u6 = 0;
            if (sscanf(line + timePos, "%02d:%02d:%02d.%06d",
                       &hh, &mm, &ss, &u6) != 4)
              continue;
            time = 1000 * (100 * (100 * hh + mm) + ss) + (u6 / 1000);
            break;
          }

          case FormatT::ICE:
          {
            // Time format is: "HH:MM:SS.sss":
            int hh = 0, mm = 0, ss = 0, sss = 0;
            if (sscanf(line + timePos, "%02d:%02d:%02d.%03d",
                       &hh, &mm, &ss, &sss) != 4)
              continue;
            // Furthermore, this time is LDN (not UTC!), convert it to MSK:
            // FIXME: For the moment, using the Summer-Time Offset of 2 hours:
            if (hh <= 21)
              hh += 2;
            else
              // Otherwise, simply skip this entry:
              continue;
            time = 1000 * (100 * (100 * hh + mm) + ss) + sss;
            break;
          }
          default:
            assert(false);
        }

        // Check the over-all limits:
        if (time  < m_timeFrom)
          continue;
        if (time >= m_timeTo)
          break;

        // Also, for FORTS, there is no trading between 14:00:00--14:04:00 and
        // 18:45:00-19:05:00 (or 19:00 -- use the later time for safety);  any
        // quotes which may be recorded during those periods are SPURIOUS, and
        // we will remove them:
        // XXX: Same for MQT, because FORTS data are currently present in  all
        // MQT files:
        if ((a_fmt == FormatT::FORTS  || a_fmt == FormatT::MQT)   &&
           ((14'00'00'000 <= time && time <= 14'04'00'000) ||
            (18'45'00'000 <= time && time <= 19'05'00'000)) )
          continue;

        //-------------------------------------------------------------------//
        // Ge the Px(s); we are NOT using Qtys:                              //
        //-------------------------------------------------------------------//
        bool isBid = false;
        if (a_fmt != FormatT::MQT)
        {
          //-----------------------------------------------------------------//
          // Bid or Ask Side?                                                //
          //-----------------------------------------------------------------//
          if (a_fmt == FormatT::FORTS || a_fmt == FormatT::MICEX)
          {
            // Get the explicit Buy/Sell (Bid/Ask) flag:
            char bs  = line[bsPos];
            if  (bs != 'B' && bs != 'S')
              continue;
            isBid = (bs == 'B');
            // Then use the constant "pxPos" (does not depend on "isBid")...
          }
          else
          {
            assert(a_fmt == FormatT::ICE);
            // Then the Side is determined by commas: If Bid slot (left-most) is
            // empty (",,"), then it must be Ask:
            isBid = (strncmp(line + ccPos + 2, ",,", 2) != 0);

            // If "isBid" is set, then the BidPx is available (rather than ",,")
            // at [ccPos + 2]; otherwise, the AskPx is available at [ccPos + 4],
            // so "pxPos" is variable:
            pxPos = ccPos + (isBid ? 2 : 4);
          }

          //-----------------------------------------------------------------//
          // Now get the Price:                                              //
          //-----------------------------------------------------------------//
          double px = 0.0;

          if (sscanf(line + pxPos, "%lf", &px) != 1 || !(px > 0.0))
            continue;
          // If OK: assign "px" to the corresp Side:
          (isBid ? bidPx : askPx) = px;
        }
        else
        {
          //-----------------------------------------------------------------//
          // MQT Fmt: Get both Bid and Ask Pxs from the same Line:           //
          //-----------------------------------------------------------------//
          assert(a_fmt == FormatT::MQT);

          // There must be "Bids:" at the following location:
          char const* bids = line + symPos + symLen;
          assert(strncmp(bids, " Bids: ", 7) == 0);
          bids += 7;

          // For Asks, need to find the pos dynamically:
          char const* asks = strstr(bids, " Asks: ");
          assert(asks != nullptr);
          asks += 7;

          if (sscanf(bids, "%lf", &bidPx) != 1 ||
              sscanf(asks, "%lf", &askPx) != 1)
            continue;
        }

        //-------------------------------------------------------------------//
        // Verify the Px:                                                    //
        //-------------------------------------------------------------------//
        // If either side is not available yet, skip this entry:
        if (!(0.0 < bidPx && bidPx < askPx))
          continue;

        // Calculate the Bid-Ask Spread:
        double sprd = askPx - bidPx;
        assert(sprd > 0.0);

        //-------------------------------------------------------------------//
        // Ready to Process:                                                 //
        //-------------------------------------------------------------------//
        // The tick is OK: update the stats:
        ++nLines;
        avgSprd += sprd;

        // Process the curr Tick:
        // MidPx?
        if (m_pxSide == PxSideT::Mid)
        {
          double midPx = 0.5 * (bidPx + askPx);
          if (fabs(midPx - prevPx) > 1e-7)
          {
            TickProcessor(time, midPx, a_targ);
            prevPx = midPx;
            ++nTicks;
          }
        }
        else
        // The requested side has been updated?
        if (m_pxSide == PxSideT::Bid && fabs(bidPx - prevPx) > 1e-7)
        {
          TickProcessor(time, bidPx, a_targ);

          // XXX: For the Bid Px, we are mostly integrested in Down Ticks:
          if (bidPx < prevPx)
          {
            avgTick += (prevPx - bidPx);
            ++nTicks;
          }
          else
          {
            avgOpp +=  (bidPx  - prevPx);
            ++nOpp;
          }
          prevPx = bidPx;
        }
        else
        if (m_pxSide == PxSideT::Ask && fabs(askPx - prevPx) > 1e-7)
        {
          TickProcessor(time, askPx, a_targ);

          // XXX: For the Ask Px, we are mostly interested in Up Ticks:
          if (askPx > prevPx)
          {
            avgTick += (askPx  - prevPx);
            ++nTicks;
          }
          else
          {
            avgOpp  += (prevPx - askPx);
            ++nOpp;
          }
          prevPx = askPx;
        }
      }
      // End of Read-And-Process Loop

      //---------------------------------------------------------------------//
      // Reading Done:                                                       //
      //---------------------------------------------------------------------//
      fclose(f);
      f = nullptr;

      // Output the stats:
      if (nLines > 0)
        avgSprd /= double(nLines);

      if (nTicks > 0)
        avgTick /= double(nTicks);

      if (nOpp  >  0)
        avgOpp  /= double(nOpp);

      std::cerr << "# "    << a_symbol   << ":\tAvgBidAskSprd="
                << avgSprd << "\t(over " << nLines    << " entries)\n"
                   "\tAvgTickSize="      << avgTick   << "\t(over "
                << nTicks  << " directional ticks)\n"
                   "\tAvgOppSize ="      << avgOpp    << "\t(over"
                << nOpp    << " opposite    ticks)\n" << std::endl;
    }
  };
  // End of "TickDataReader" class

  //=========================================================================//
  // "VerifyTickData":                                                       //
  //=========================================================================//
  // There is no need to make this function a member of "TickDataReader":
  //
  template<int N>
  void VerifyTickData
  (
    MDEntryT const (&a_instr)[N],
    char     const*  a_where,
    int              a_debug
  )
  {
    assert(a_where != nullptr);
    // NB: The following indices are not yet known; "prev" and "curr" set set
    // to (-1) consistently with Mode0 (see below); "next" is  simply unknown
    // yet:
    int prev = -1, curr = -1, next = -1;

    // Mode: 0: before 1st valid interval;
    //       1: main data body;
    //       2: after last valid interval:
    int mode = 0;

    for (int i = 0; i < N; ++i)
    {
      MDEntryT const& mde = a_instr[i];

      //---------------------------------------------------------------------//
      // Emptiness Test and Mode-Switching:                                  //
      //---------------------------------------------------------------------//
      if (mde.IsEmpty())
      {
        //-------------------------------------------------------------------//
        // Empty MDE: This is only possible in Mode0 or Mode2:               //
        //-------------------------------------------------------------------//
        // If encountered in Mode1, it means that Mode1 has ended: Got Mode2:
        if (mode == 1)
        {
          mode = 2;

          if (a_debug >= 2)
            std::cerr << "VerifyTickData: " << a_where << ": Entered Mode="
                      << mode << " @ i="    << i << std::endl;
        }
        // In any mode, there is nothing more to do with an Empty MDE:
        continue;
      }
      else
      {
        //-------------------------------------------------------------------//
        // Other way round: Non-Empty MDE:                                   //
        //-------------------------------------------------------------------//
        if (mode == 0)
        {
          mode = 1;           // Non-Empty MDE: Mode0 -> Mode1:

          // This (and only this) MDE has no previous one:
          if (mde.m_prev != -1 || mde.m_curr != i)
            throw std::runtime_error
                  ("VerifyTickData: " + std::string(a_where) +
                   ": Entering Mode=" + std::to_string(mode) + " @ i=" +
                   std::to_string(i)  + " but m_prev="       +
                   std::to_string(mde.m_prev) + ", m_curr="  +
                   std::to_string(mde.m_curr));
          // If OK:
          if (a_debug >= 2)
            std::cerr << "VerifyTickData: " << a_where << ": Entered Mode="
                      << mode << " @ i="    << i << std::endl;
        }
        else
        if (mode == 2)
          // This is an error: once we have reached Mode2, all MDEs must remain
          // Empty to the end of the array:
          throw std::runtime_error
                ("VerifyTickData: "   + std::string(a_where)    + ": Mode=" +
                 std::to_string(mode) + ": Found EmptyMDE @ i=" +
                 std::to_string(i));
      }
      //---------------------------------------------------------------------//
      // Mode1 (Main Mode):                                                  //
      //---------------------------------------------------------------------//
      // If we got here, we must be in Mode1 with a non-Empty MDE:
      assert(mode == 1 && !mde.IsEmpty());

      // The following invariants must hold:
      if (!(mde.m_prev  < mde.m_curr && mde.m_curr <= i && i < mde.m_next &&
            mde.m_next <= N))
        throw std::runtime_error
              ("VerifyTickData: "     + std::string(a_where) +
               ": Inconsistency @ i=" + std::to_string(i)    + ": m_prev=" +
               std::to_string(mde.m_prev) + ", m_curr="      +
               std::to_string(mde.m_curr) + ", m_next="      +
               std::to_string(mde.m_next) + ", N="    + std::to_string(N));

      if (mde.m_curr == i)
      {
        // This is a tick time, update the indices:
        // Old "curr" must now be "m_prev", old "next" must now be "m_curr":
        if (mde.m_prev != curr || (next != -1 && mde.m_curr != next))
          throw std::runtime_error
                ("VerifyTickData: " +  std::string(a_where) + ": NewTick @ i=" +
                 std::to_string(i)          +  ": Inconsistency: m_prev="      +
                 std::to_string(mde.m_prev) + ", expected=" +
                 std::to_string(curr)       + "; m_curr="   +
                 std::to_string(mde.m_curr) + ", expected=" +
                 std::to_string(next));
        // If OK: Adjust the indices:
        prev = mde.m_prev;
        curr = mde.m_curr;
        next = mde.m_next;
      }
      else
      {
        // Not a NewTick: simply propagating the prev MDE, so all indices must
        // be as expected:
        if (mde.m_prev != prev || mde.m_curr != curr || mde.m_next != next)
          throw std::runtime_error
                ("VerifyTickData: "         + std::string(a_where) +
                 ": Unchanged @ i="         + std::to_string(i)    +
                 ": Inconsistency: m_prev=" + std::to_string(mde.m_prev) +
                 ", expected=" + std::to_string(prev) +
                 "; m_curr="   + std::to_string(mde.m_curr) + ", expected="   +
                 std::to_string(curr)       + "; m_next="   +
                 std::to_string(mde.m_next) + ", expected=" +
                 std::to_string(next));
      }
    }
  }
  // End of "VerifyTickdata"
}
// End namespace MLL
