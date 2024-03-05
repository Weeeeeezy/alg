// vim:ts=2:et
//===========================================================================//
//                   "TradingStrats/MM-NTProgress/Orders.hpp":               //
//                             Order Entry Methods                           //
//===========================================================================//
#pragma  once
#include "MM-NTProgress.h"
#include <utility>
#include <ctime>
#include <cassert>
#include <sstream>

namespace MAQUETTE
{
  //=========================================================================//
  // "GetMainAOS": Controlled access to "m_aosesNTPro":                      //
  //=========================================================================//
  // NB: Returns a Ref to AOS Ptr!
  //
  AOS const*& MM_NTProgress::GetMainAOS(int a_T, int a_I, int a_S, int a_B)
  const
  {
    assert((a_T == TOD || a_T == TOM) && (a_I == AB || a_I == CB) &&
           (a_S == Ask || a_S == Bid) &&
           0 <= a_B && a_B  < m_nBands[a_T][a_I]);

    AOS const*& aos = m_aosesNTPro[a_T][a_I][a_S][a_B];
    // Obviously, if is non-NULL, it must have intrinsic TISB vals as above,
    // and the NTPro OMC:
    DEBUG_ONLY(
    if (aos != nullptr)
    {
      OrderInfo const&  ori =  aos->m_userData.Get<OrderInfo>();

      assert(ori.m_T == a_T && ori.m_I == a_I && aos->m_isBuy == a_S &&
             ori.m_B == a_B && aos->m_omc ==  &m_omcNTPro);
    })
    return aos;
  }

  //=========================================================================//
  // "GetPeggedAOS": Simular to "GentMainAOS" above, but for Pegged MICEX:   //
  //=========================================================================//
  AOS const*& MM_NTProgress::GetPeggedAOS(int a_T, int a_I, int a_S) const
  {
    assert((a_T == TOD || a_T == TOM) && (a_I == AB || a_I == CB) &&
           (a_S == Ask || a_S == Bid));

    AOS const*& aos = m_aosesPegMICEX[a_T][a_I][a_S];
    // Obviously, if is non-NULL, it must have intrinsic TIS vals as above, and
    // the MICEX OMC:
    DEBUG_ONLY(
    if (aos != nullptr)
    {
      OrderInfo const&  ori =  aos->m_userData.Get<OrderInfo>();

      assert(ori.m_T == a_T && ori.m_I == a_I && aos->m_isBuy == a_S &&
             aos->m_omc ==  &m_omcMICEX);
    })
    return aos;
  }

  //=========================================================================//
  // "CheckPassAOS":                                                         //
  //=========================================================================//
  // (*) Verifies the AOS for an NTProgress Quote or MICEX Pegged Order;
  // (*) outputs  the the attributes (T, I, S, B(NTPro only));
  // (*) returns  "true" iff the corresp AOS was indeed found on in the corresp
  //     Main Slot (Quote on NTPro, Pegged on MICEX) OR in the CxlPending list;
  // (*) in the latter case, "o_was_cp" flag is set in addition:
  // (*) if "false" is returned, then an incosistency was encountered and Stop
  //     was already initiated:
  //
  inline bool MM_NTProgress::CheckPassAOS
  (
    char const*     a_where,
    AOS const*      a_aos,
    OrderID         a_req_id,
    char const*     a_err_msg, // Iff Failed, otherwise NULL
    int*            o_T,       // SettlDate  (TOD or TOM)
    int*            o_I,       // Instrument (AB  or CB)
    int*            o_S,       // Side       (Ask or Bid)
    int*            o_B,       // Band       [0.. m_nBands[T][I]-1]: NTPro only
    bool*           o_was_cp,
    utxx::time_val  a_ts_strat
  )
  {
    assert(a_where != nullptr  &&  o_T != nullptr  &&  o_I      != nullptr  &&
           o_S     != nullptr  &&  o_B != nullptr  &&  o_was_cp != nullptr  &&
          (a_aos->m_omc == &m_omcNTPro || a_aos->m_omc == &m_omcMICEX));

    // Which OMC this Order has come from?
    bool        isNTPro = (a_aos->m_omc == &m_omcNTPro);
    char const* omcStr  = isNTPro ? "NTProgress" : "MICEX";

    //-----------------------------------------------------------------------//
    // Get the OrderInfo and Indices:                                        //
    //-----------------------------------------------------------------------//
    OrderInfo const& ori = a_aos->m_userData.Get<OrderInfo>();

    // NB: The following indices characterise the "intrinsic" AOS properties,
    // even if the AOS itself is no longer in the corresp Main Slot:
    int  T  = ori.m_T;
    int  I  = ori.m_I;
    int  S  = a_aos->m_isBuy;
    int  B  = ori.m_B;
    DEBUG_ONLY(bool isPegged = ori.m_isPegged;)

    // The following invariants must hold:
    assert((T == TOD || T == TOM) && (I == AB || I == CB) &&
           (S == Bid || S == Ask));

    // For NTPro only, check the Band:
    assert((!isNTPro && B == -1)  || (isNTPro && 0 <= B && B < m_nBands[T][I]));

    // Pegged orders are only available on MICEX: isPegged => !isNTPro;
    // furthermore, this method is ONLY called for Passive Orders, ie if it is
    // MICEX, it MUST be Pegged: Thus:
    assert(!isNTPro == isPegged);

    // Set the "o_TISB" outputs to the above "intrinsic" vals, no matter whether
    // the AOS is found in the corresp Main Slot (see below) and remains there:
    *o_T = T;
    *o_I = I;
    *o_S = S;
    *o_B = B;

    // Try to find this AOS in the Main Quoting Slot or in a Pegged Slot:
    // NB: Ref to Ptr!
    AOS const*& mainAOS =
      isNTPro ? GetMainAOS(T, I, S, B) : GetPeggedAOS(T, I, S);

    // And at the same time, search the CxlPending list:
    auto  cpIt    = std::find(m_cpAOSes.begin(), m_cpAOSes.end(), a_aos);

    //-----------------------------------------------------------------------//
    // Is our AOS in the "mainAOS" Slot?                                     //
    //-----------------------------------------------------------------------//
    if (mainAOS == a_aos)
    {
      // Then, it should normally NOT be "CxlPending", because once we success-
      // fully issue a CancelReq for it, it is moved from the Main Slot into the
      // CxlPending List. (Counter-examples may theoretically be possible, but
      // not constructed here). So produce a warning if it is CxlPending:
      //
      if (utxx::unlikely(a_aos->m_isCxlPending != 0))
        LOG_WARN(2,
          "MM-NTProgress::CheckPassAOS({}): {}: AOSID={}, ReqID={}: "
          "Inconsistency: IsCxlPending={}, but Order still in the Main Slot: "
          "T={}, I={}, IsBid={}, Band={}",
          a_where, omcStr, a_aos->m_id, a_req_id, a_aos->m_isCxlPending,
          T, I, S, B)

      // Check that it is NOT on the CxlPending List -- if it is found in both
      // locations, it is a very serious error condition:
      if (utxx::unlikely(cpIt != m_cpAOSes.end()))
      {
        DelayedGracefulStop hmm
          (this, a_ts_strat,
           "MM-NTProgress::CheckPassAOS(", a_where,
           "):   ",    omcStr,    ": Invariant Failure: AOSID=",  a_aos->m_id,
           ", ReqID=", a_req_id,  ": Found in the Main Slot: T=", T,   ", I=",
           I, ", S=",  S, ", Band=", B, ", and also in CxlPending @ ",
           (cpIt - m_cpAOSes.begin()));
        return false;
      }

      // Indicate that the AOS was found in the Main Slot, NOT on CxlPending:
      *o_was_cp = false;

      // If the AOS is Inactive, remove it from the Main Slot ("o_{TISB} out-
      // puts remain unchanged):
      if (utxx::unlikely(a_aos->m_isInactive))
      {
        mainAOS = nullptr;

        if (utxx::likely(a_err_msg == nullptr))
        {
          // Fill or Cancel:
          LOG_INFO(3,
            "{}: AOSID={}, T={}, I={}, IsBid={}, Band={}: Removed as {}: {}"
            ", ReqID={}",     omcStr, a_aos->m_id, T, I, S, B,
            a_aos->IsFilled() ? "Filled" : "Cancelled", a_where, a_req_id)
        }
        else
          // Failure:
          LOG_WARN(3,
            "{}: AOSID={}, T={}, I={}, isBid={}, Band={}: Removed as FAILED"
            ": {}, ReqID={}", omcStr, a_aos->m_id, T, I, S, B, a_err_msg,
            a_req_id)
      }
    }
    else
    //-----------------------------------------------------------------------//
    // AOS NOT in the Main Slot -- then it must be in the CxlPending List:   //
    //-----------------------------------------------------------------------//
    {
      if (utxx::unlikely(cpIt == m_cpAOSes.end()))
      {
        // Not found in CxlPending List either. This is actually OK. Eg:
        // (*) a NewOrder was issued;
        // (*) then immediately modified;
        // (*) then the orig order was rejected and the AOS totally removed;
        // (*) then "Modify" failure arrives -- but no  AOS anymore:
        // Still, produce a warning in this case:
        //
        LOG_WARN(3,
          "MM-NTProgress::CheckPassAOS({}): {}: AOSID={}, ReqID={}: Not "
          "Found AnyWhere: Intrinsic T={}, I={}, IsBid={}, Band={}, "
          "IsInactive={}, IsCxlPending={}; AOSID in Main Slot: {}: "
          "Ignoring...",
          a_where,     omcStr, a_aos->m_id, a_req_id, T, I, S, B,
          a_aos->m_isInactive, a_aos->m_isCxlPending,
          (mainAOS == nullptr) ? 0 : mainAOS->m_id)

        // Nothing more to do with this update:
        return false;
      }

      // Otherwise: OK: AOS found on the CxlPending List:
      *o_was_cp = true;

      // If the AOS is Inactive, remove it from there. NB: The AOS status may
      // of course not be "CxlPending" anymore:
      if (a_aos->m_isInactive)
      {
        m_cpAOSes.erase(cpIt);

        if (utxx::likely(a_err_msg == nullptr))
        {
          // Fill or Cancel:
          LOG_INFO(3,
            "{}: AOSID={}: Removed from CxlPending as {}: {}, ReqID={}",
            omcStr,  a_aos->m_id,
            a_aos->IsFilled() ? "Filled" : "Cancelled", a_where, a_req_id)
        }
        else
          // Failure:
          LOG_WARN(3,
            "{}: AOSID={}: Removed from CxlPending as FAILED: {}, ReqID={}",
            omcStr,  a_aos->m_id, a_err_msg, a_req_id)
      }
    }
    //-----------------------------------------------------------------------//
    // If we got here: All is OK:                                            //
    //-----------------------------------------------------------------------//
    return true;
  }

  //=========================================================================//
  // "DoQuotes":                                                             //
  //=========================================================================//
  // NB: Will do both Bid and Ask sides as they may be inter-related:
  //
  inline void MM_NTProgress::DoQuotes
  (
    int            a_T,           // TOD or TOM
    int            a_I,           // AB  or CB
    utxx::time_val a_ts_exch,
    utxx::time_val a_ts_recv,
    utxx::time_val a_ts_strat
  )
  {
    //-----------------------------------------------------------------------//
    // Get the Instrument and OrderBook:                                     //
    //-----------------------------------------------------------------------//
    assert((a_T == TOD || a_T == TOM) && (a_I == AB || a_I == CB));

    // A Quote in (a_T, a_I) is allowed IFF this Instr is currently Enabled,
    // AND we have nt reached the max number of rounds:
    if (utxx::unlikely
       (!m_isInstrEnabled[a_T][a_I] || m_roundsCount >= m_maxRounds))
      return;

    // If this Instr is Enabled, it must have a valid number of configured
    // Bands:
    int        nBands  = m_nBands[a_T][a_I];
    assert(0 < nBands && nBands <= MaxBands);

    // NB: "instr" is the one to be quoted on NTProgress;
    //     "ob"    is the BasePx OrderBook on MICEX:
    SecDefD   const* instr = m_instrsNTPro[a_T][a_I];
    OrderBook const* ob    = m_obsMICEX   [a_T][a_I];
    assert(instr  != nullptr && ob != nullptr);

    // The OrderBook must be fully Initialised and Ready:
    if (utxx::unlikely(!(ob->IsReady())))
      throw utxx::logic_error
            ("MM_NTProgress::DoQuotes: NTProgress: ",
            (a_T == TOM ? "TOD"     : "TOM"),     ": ",
            (a_I == AB  ? "EUR/RUB" : "USD/RUB"),
            ": OrderBook is not Ready");

    //-----------------------------------------------------------------------//
    // Initialise the Quote Params:                                          //
    //-----------------------------------------------------------------------//
    // New QuotePxs -- initially all NaN:
    PriceT newPxs[2][MaxBands];

    //-----------------------------------------------------------------------//
    // Get curr AOSes and CurrPxs (may be NaN):                              //
    //-----------------------------------------------------------------------//
    for (int S = 0; S < 2;      ++S)
    for (int B = 0; B < nBands; ++B)
    {
      // Get the AOS (NB: Ref to a Ptr!):
      AOS const*& aos = GetMainAOS(a_T, a_I, S, B);

      // If there is no AOS yet, this will normally cause a "NewOrder" (unless
      // NewQuotePx could not be determined); CurrPx remains NaN:
      if (aos == nullptr)
        continue;

      // Generic Case: AOS exists:
      // Obviously, the Side as recorded in the AOSes themselves must be consi-
      // stent with our notation:
      assert((S == Bid && aos->m_isBuy) || (S == Ask && !(aos->m_isBuy)));

      // If this AOS is already Inactive, set the corresp ptr to NULL -- and do
      // not try to get the CurrPx, of course:
      if (utxx::unlikely(aos->m_isInactive))
      {
        aos = nullptr;
        continue;
      }
      // Also, if the AOS is CxlPending, we do not try to get its CurrPx anyway.
      // XXX: But in most cases, such an order would already be removed from its
      // Main Slot and moved into the "CxlPending" List:
      if (utxx::unlikely(aos->m_isCxlPending != 0))
        continue;

      // Otherwise: AOS is fully Active. Produce a warning if there is no CurrPx
      // for it (this should be extremely unlikely). In that case, we would ra-
      // ther Cancel such a strange order:
      if (utxx::unlikely(!IsFinite(m_currQPxs[a_T][a_I][S][B])))
      {
        LOG_WARN(2,
          "MM-NTProgress::DoQuotes: {}_{}, SettlDate={}, {}, AOSID={}: "
          "Could not get CurrQuotePx, will Cancel that Quote...",
          instr->m_Symbol.data(), (a_T == TOD) ? "TOD" : "TOM",
          instr->m_SettlDate,     (S == 0)     ? "Ask" : "Bid", aos->m_id)
        continue;
      }
    }
    //-----------------------------------------------------------------------//
    // Construct and Verify the "newPxs":                                    //
    //-----------------------------------------------------------------------//
    // In particular, this  will result in cancelling those existing Quotes for
    // which New QuotePx cannot be determined (because of insufficient liquidi-
    // ty on MICEX):
    if (utxx::unlikely(!MkQuotePxs(a_T, a_I, a_ts_strat, newPxs)))
    {
      // This means that an unrecoverable inconsistency in QuotePxs was encoun-
      // tered (error msg already logged), so must shut donw:
      DelayedGracefulStop hmm(this, a_ts_strat, "MkQuotePx FAILED");
      return;
    }

    //-----------------------------------------------------------------------//
    // Ready to submit our Quotes:                                           //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) all submitted orders are first Buffered, then Flushed at the end;
    // (*) some QuotePxs may be empty, in which case the corresp Quotes are
    //     cancelled:
    SubmitQuotes(a_T, a_I, newPxs, a_ts_exch, a_ts_recv, a_ts_strat);
  }

  //=========================================================================//
  // "SubmitQuotes":                                                         //
  //=========================================================================//
  // Used by "DoQuotes":
  //
  inline void MM_NTProgress::SubmitQuotes
  (
    int             a_T,
    int             a_I,
    PriceT const    (&a_new_pxs)[2][MaxBands],
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    // If this Instr is Enabled, it must have a valid number of configured
    // Bands:
    int        nBands  = m_nBands[a_T][a_I];
    assert(0 < nBands && nBands <= MaxBands);

    // Again, the args are not checked here -- this has already been done by
    // the Caller:
    SecDefD const*  instr = m_instrsNTPro[a_T][a_I];
    assert(instr != nullptr);

    //-----------------------------------------------------------------------//
    // Any specific order of Quotes submission?                              //
    //-----------------------------------------------------------------------//
    // The order of Quotes submission must be such that they do not cross with
    // existing ones. Crossing is only possible in the following 2 cases (inds
    // are Bands):
    // (*) NewBid[0] >= CurrAsk[0]: Do Ask first (Modify or Cancel)
    // (*) NewAsk[0] <= CurrBid[0]: Do Bid first (Modify or Cancel)
    // NB:
    // (*) These conds are only sufficient, not necessary, for a  crossing  to
    //     occur, because it might be (though unlikely)  that  the   offending
    //     New Quote would actually be skipped due to the corresp Order is not
    //     to be done (eg no New QuotePx); nevertheless, for safety, we ALWAYS
    //     take action on such conds:
    // (*) If the corresp "currPx" is NaN, both conds are "false",  which is a
    //     correct behaviour:
    //
    PriceT currBAsk = m_currQPxs[a_T][a_I][Ask][0];
    PriceT currBBid = m_currQPxs[a_T][a_I][Bid][0];
    bool   askFirst = (a_new_pxs[Bid][0] >= currBAsk);
    bool   bidFirst = (a_new_pxs[Ask][0] <= currBBid);

    // Obiously, "askFirst" and "bidFirst" can both be unset, but they  cannot
    // both be set, because if the curr and new quotes are consistent, then we
    // would have:
    // NewAsk[0] > NewBid[0],  and thus, if "askFirst" is set,
    // NewAsk[0] > NewBid[0] >= CurrAsk[0] > CurrBid[0],   ie
    // NewAsk[0] > CurrBid[0], ie "bidFirst" cannot be set!
    // But verify this invariant explicitly. If it fails, it is a severe error
    // condition -- yet we do NOT terminate the Strategy, just Cancel all Quo-
    // tes in that case:
    if (utxx::unlikely(askFirst && bidFirst))
    {
      LOG_ERROR(2,
        "MM-NTProgress::SubmitQuotes: Symbol=",  instr->m_Symbol.data(),
        ", SettlDate=", instr->m_SettlDate,   ": Inconsistent QuotePxs: "
        "Old=", double(currBBid),          " .. ", double(currBAsk), ", "
        "New=", double(a_new_pxs[Bid][0]), " .. ", double(a_new_pxs[Ask][0]))

      // However, do NOT cancel the Pegged Orders (if any):
      CancelAllOrders<false>(a_ts_exch, a_ts_recv, a_ts_strat);
      return;
    }

    // So we will generally use "Bid 1st, Ask 2nd" submission order, unless com-
    // pelled to do otherwise:
    int sides[2] { Bid, Ask };
    if (askFirst)
    {
      sides[0] = Ask;
      sides[1] = Bid;
    }
    //-----------------------------------------------------------------------//
    // Proceed with Submissions:                                             //
    //-----------------------------------------------------------------------//
    QStatusT stats[2][MaxBands];
    bool allUnchgd = true;

    for (int side:  sides)
    for (int B = 0; B < nBands; ++B)
    {
      stats[side][B] =
        Submit1Quote(a_T,  a_I, side,   B, a_new_pxs[side][B],
                     a_ts_exch, a_ts_recv, a_ts_strat);

      allUnchgd &= (stats[side][B] == QStatusT::PxUnchgd);
    }
    // Grand Flush:
    (void) m_omcNTPro.FlushOrders();

    //-----------------------------------------------------------------------//
    // Log the Quotes Submitted:                                             //
    //-----------------------------------------------------------------------//
    // But do not display any output if all states are "PxUnchgd":
    //
    if (m_debugLevel >= 3 && !allUnchgd)
    {
      long lat_usec = utxx::time_val::now_diff_usec(a_ts_recv);

      char  buff[1024];
      char* curr = stpcpy(buff, "Updated: Symbol=");
      curr       = stpcpy(curr, instr->m_Symbol.data());
      curr       = stpcpy(curr, ", SettlDate=");
      (void)utxx::itoa(instr->m_SettlDate, curr);
      curr       = stpcpy(curr, ", Latency="  );
      (void)utxx::itoa(lat_usec, curr);

      // Do it Bids-First:
      for (int S = 1; S >= 0; --S)
      {
        curr = stpcpy(curr, (S == 1) ? "\nBids: " : "\nAsks: ");

        for (int B = 0; B < nBands; ++B)
        {
          curr  =  LogQuote(a_T, a_I, S, B, stats[S][B], curr);
          if (B != nBands-1)
            curr = stpcpy(curr, ", ");
        }
      }
      // For reference, output the curr MOEX pxs as well:
      OrderBook const* ob = m_obsMICEX[a_T][a_I];
      curr  = stpcpy(curr, "\nMICEX: ");
      curr += utxx::ftoa_left(double(ob->GetBestBidPx()), curr, 20, 5);
      curr  = stpcpy(curr, " .. ");
      curr += utxx::ftoa_left(double(ob->GetBestAskPx()), curr, 20, 5);

      // We must still be well within the buffer:
      assert(size_t(curr - buff) < sizeof(buff));

      // Output this line into log:
      m_logger->info(buff);
      // No flush here -- this output is very frequent!
    }
  }

  //=========================================================================//
  // "Submit1Quote":                                                         //
  //=========================================================================//
  inline MM_NTProgress::QStatusT MM_NTProgress::Submit1Quote
  (
    int             a_T,
    int             a_I,
    int             a_S,
    int             a_B,
    PriceT          a_new_px,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    // NB: All Indices are assumed to be verified by the Caller. Get the AOS
    // (Ref to a Ptr!):
    AOS const*& aos = GetMainAOS(a_T, a_I, a_S, a_B);

    //-----------------------------------------------------------------------//
    // Special Cases:                                                        //
    //-----------------------------------------------------------------------//
    // No New QuotePx? -- Then we must Cancel an existing Quote, if any:
    if (utxx::unlikely(!IsFinite(a_new_px)))
    {
      // Buffered=true:
      CancelQuoteSafe<true>
        (a_T, a_I, a_S, a_B, a_ts_exch, a_ts_recv, a_ts_strat);
      return QStatusT::NoQuotePx;
    }

    if (aos != nullptr)
    {
      if (utxx::unlikely(aos->m_isInactive))
        // If this AOS is Inactive, just reset it to NULL, so it will be re-qu-
        // oted (if throttling is OK) as a "NewOrder". But normally, this would
        // not be the case -- Inactive AOS would already be reset to NULL. Then
        // fall though:
        aos = nullptr;
      else
      if (utxx::unlikely(aos->m_isCxlPending != 0))
        // Normally, this would not be the case either: When we successfully is-
        // sue a CancelReq, the AOS is moved to the CxlPending List and its Main
        // Slot is NULLified immediately.   If it did not happen for any reason,
        // we cannot proceed with a Quote until the Slot is available again:
        return QStatusT::MustWait;
    }

    //-----------------------------------------------------------------------//
    // Generic Case:                                                         //
    //-----------------------------------------------------------------------//
    // NB: "currPx" may exist even if "aos" is NULL -- it remains from a previ-
    // ous Quote, and can be re-used (in the absence of MktData updates):
    //
    PriceT& currPx = m_currQPxs[a_T][a_I][a_S][a_B];  // NB: Ref!

    if (utxx::likely(aos != nullptr))
    {
      //---------------------------------------------------------------------//
      // Modify an existing Quote:                                           //
      //---------------------------------------------------------------------//
      // NB: If we got here, this Quote must have a valid CurrPx:
      assert(IsFinite(currPx));

      // Proceed with modification only if the Px has really changed (the Qty
      // is always unchanged):
      if (a_new_px != currPx)
      {
        bool ok =
          ModifyPassOrderSafe<true>
            (aos,       a_new_px,  m_qtys[a_T][a_I][a_B],
             a_ts_exch, a_ts_recv, a_ts_strat);

        if (utxx::likely(ok))
        {
          // Memoise the New Quote Px, but NOT Quote Time -- the latter is only
          // for New Quotes (to control their rate):
          currPx = a_new_px;
          return QStatusT::Done;
        }
        else
          return QStatusT::Failed;
      }
      else
        return   QStatusT::PxUnchgd;
    }
    else
    {
      //---------------------------------------------------------------------//
      // Enter a New Quote:                                                  //
      //---------------------------------------------------------------------//
      // HERE we perform throttling check:
      bool intervalOK =
        (a_ts_strat - m_lastQuoteTimes[a_T][a_I][a_S][a_B]).milliseconds() >=
        m_minInterQuote;

      if (utxx::likely(intervalOK))
      {
        SecDefD const* instr = m_instrsNTPro[a_T][a_I];
        assert(instr != nullptr);

        // NB: IsBid = side, Buffered=true:
        aos =
          NewPassOrderSafe<true>
            (*instr,    bool(a_S), a_new_px,   m_qtys[a_T][a_I][a_B],
             a_ts_exch, a_ts_recv, a_ts_strat, a_T,   a_I, a_B);

        if (utxx::likely(aos != nullptr))
        {
          // Memoise the Quote Px and Time:
          currPx                               = a_new_px;
          m_lastQuoteTimes[a_T][a_I][a_S][a_B] = a_ts_strat;
          return QStatusT::Done;
        }
        else
          return QStatusT::Failed;
      }
      else
        return   QStatusT::Throttled;
    }
    //-----------------------------------------------------------------------//
    // XXX: This point is not reachable:                                     //
    //-----------------------------------------------------------------------//
    __builtin_unreachable();
  }

  //=========================================================================//
  // "Do1Quote": As "DoQuotes" but for 1 Quote only, at an existing Px:      //
  //=========================================================================//
  // NB:
  // (*) This method is used when a previous Quote was Filled or Cancelled, and
  //     it needs to be replaced  (only one, as opposed to "DoQuotes" which may
  //     update ALL Quotes on a market move):
  // (*) It in turn invokes "Submit1Quote"  :
  //
  inline void MM_NTProgress::Do1Quote
  (
    int             a_T,
    int             a_I,
    int             a_S,
    int             a_B,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    assert((a_T == TOD || a_T == TOM) && (a_I == AB || a_I == CB) &&
           (a_S == Bid || a_S == Ask) &&
           (0   <= a_B && a_B < m_nBands [a_T][a_I]));

    // IMPORTANT: Are Quotes in this (T,I) currently allowed?
    if (utxx::unlikely
       (!m_isInstrEnabled[a_T][a_I] || m_roundsCount >= m_maxRounds))
      return;

    // The args are assumed to be already verified by the Caller;
    // use "m_currQPxs" for the QuotePx, it MUST be Finite in this case:
    //
    PriceT   px  =  m_currQPxs [a_T][a_I][a_S][a_B];
    if (utxx::unlikely(!IsFinite(px)))
    {
      LOG_ERROR(2,
        "MM-NTProgress::Do1Quote: T={}, I={}, IsBid={}, Band={}: CurrPx is "
        "not Finite",  a_T, a_I, a_S, a_B)
      return;
    }
    // If OK:
    SecDefD const*  instr = m_instrsNTPro[a_T][a_I];
    assert(instr != nullptr);

    // Actually Submit it:
    QStatusT res =
      Submit1Quote(a_T, a_I, a_S,  a_B, px, a_ts_exch, a_ts_recv, a_ts_strat);

    // This is a single order -- Flush it:
    m_omcNTPro.FlushOrders();

    // Log it:
    if (utxx::unlikely(m_debugLevel >= 3))
    {
      char buff[512];
      char* curr = stpcpy(buff, "Re-Quoted: Symbol=");

      curr  = stpcpy(curr, instr->m_Symbol.data());
      curr  = stpcpy(curr, ", SettlDate=");
      (void)utxx::itoa(instr->m_SettlDate, curr);
      curr  = stpcpy(curr, (a_S == 1) ? ": Bid: " : ": Ask: ");

      curr  = LogQuote(a_T, a_I, a_S, a_B, res, curr);

      // We must still be well within the buffer:
      assert(size_t(curr - buff) < sizeof(buff));

      // Output this line into log:
      m_logger->info(buff);
      // No flush here -- this output may be very frequent!
    }
  }

  //=========================================================================//
  // "MayBeDoCoveringOrder":                                                 //
  //=========================================================================//
  inline void MM_NTProgress::MayBeDoCoveringOrder
  (
    int             a_T,
    int             a_I,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    assert((a_T == TOD || a_T == TOM) && (a_I == AB || a_I == CB));

    // In the Dry-Run Mode, we would not even get here -- there is nothing to
    // cover. But still:
    if (utxx::unlikely(m_dryRunMode))
      __builtin_unreachable();

    //-----------------------------------------------------------------------//
    // Analyse the Semi-Netted (per-Tenor, per-CcyPair) Position:            //
    //-----------------------------------------------------------------------//
    // TODO: Multi-Way Optimal Covering:
    //
    QtyO currPos  =  GetSNPos   (a_T, a_I, a_ts_strat);
    QtyO absPos   =  Abs(currPos);
    QtyO maxPos   =  m_posLimits[a_T][a_I];
    assert(maxPos >= 0);

    if (utxx::likely(absPos <= maxPos))
    {
      // This includes the case currPos==absPos==0:
      LOG_INFO(3,
        "MM-NTProgress::MayBeDoCoveringOrder: T={}, I={}: Pos={}, Limit={}: "
        "Nothing to do...", a_T, a_I, QRO(currPos), QRO(maxPos))
      return;   // Nothing to do in this case; ie we also quit in case of an
                // error in "GetSNPos" (yields currPos==0)
    }
    // Otherwise: Yes, got an excessive position!  We may cover it completely or
    // partially; "qty" is the qty of the covering order; also, it is rounded to
    // a multiple of the MICEX LotSize:
    //
    assert (currPos != 0 && absPos > maxPos && absPos > 0);
    bool   isBuy = (currPos < 0);
    QtyO   qty   = m_coverWholePos ? absPos : (absPos - maxPos);
    assert(IsPos(qty) && qty <= absPos);

    SecDefD const* aggrInstr =  m_instrsMICEX[a_T][a_I];
    assert(aggrInstr != nullptr); // Otherwise "GetSNPos" would have failed!

    double lotSz = aggrInstr->m_LotSize;
    qty  = QtyO(long(Round(double(qty) / lotSz)) * long(Round(lotSz)));

    // XXX: Because of "Round", the "qty" can get down to 0, or can now exceed
    // "maxPos"! If it is 0, nothing to do:
    assert(!IsNeg(qty));
    if (utxx::unlikely(IsZero(qty)))
    {
      LOG_INFO(3,
        "MM-NTProgress::MayBeDoCoveringOrder: T={}, I={}: Pos={}, Limit={}: "
        "CoverQty=0, nothing to do...",
        a_T, a_I, QRO(currPos), QRO(maxPos))
      return;
    }
    //-----------------------------------------------------------------------//
    // Generic Case: Proceed with the Covering Order on MICEX:               //
    //-----------------------------------------------------------------------//
    AOS const*       aos = nullptr;
    OrderBook const* ob  = m_obsMICEX[a_T][a_I];
    assert(ob != nullptr);

    // The L1 passive px (if peging is used; but Pegging is itself conditional
    // on this px being Finite). Also, Pegging is NOT used if we are stopping:
    //
    PriceT            oppPx = isBuy ? ob->GetBestBidPx() : ob->GetBestAskPx();
    bool usePegging =
      m_usePegging[a_T][a_I] && IsFinite(oppPx) &&
      !(m_delayedStopInited  || m_nowStopInited);

    // Now issue either a Pegged or a Market Order, or modify an existing Pegged
    // Order. A new order is issued if:
    // (*) Pegging is disabled (then it is a Market Order);
    // (*) Pegging is enabled but there is no existing Pegged order;
    // (*) Pegging is enabled and the existing Pegged order is CxlPending, so we
    //     could not modify it anyway:
    //
    bool useNew    =
      !usePegging                                         ||
      m_aosesPegMICEX[a_T][a_I][isBuy] == nullptr         ||
      m_aosesPegMICEX[a_T][a_I][isBuy]->m_isCxlPending != 0;

    if (utxx::likely(useNew))
    {
      //---------------------------------------------------------------------//
      // Yes, need a new (Market or Pegged) Order:                           //
      //---------------------------------------------------------------------//
      try
      {
        aos = m_omcMICEX.NewOrder
        (
          this,
          *aggrInstr,
          usePegging ? FIX::OrderTypeT::Limit : FIX::OrderTypeT::Market,
          isBuy,
          usePegging ? oppPx : PriceT(),
          !usePegging,            // IsAggr == !Pegged
          QtyAny(qty),
          a_ts_exch,
          a_ts_recv,
          a_ts_strat,
          false,                  // !Buffered
          FIX::TimeInForceT::Day  // Even for market Orders!
        );
      }
      catch (std::exception const& exc)
      {
        // If failed, we better stop the whole Strategy:
        DelayedGracefulStop hmm
          (this, a_ts_strat,
           "MM-NTProgress::MayBeDoCoveringOrder: ",
           (usePegging ? "Pegged" : "Market"), "Order FAILED: Symbol=",
           aggrInstr->m_Symbol.data(),   ", ", isBuy ? "Buy" : "Sell",
           ", Qty=", QRO(qty), ": ", exc.what());
        return;
      }
      assert(aos != nullptr);

      // If it is a Pegged Order, save its AOS ptr (it was either a NULL before,
      // or CxlPending, so we over-write it anyway):
      if (usePegging)
        m_aosesPegMICEX[a_T][a_I][isBuy] = aos;
    }
    else
    {
      //---------------------------------------------------------------------//
      // Modify an existing Pegged Order:                                    //
      //---------------------------------------------------------------------//
      // Qty is increased, but the Px is not modified -- the latter is only done
      // at Mktdata updates. Also, MICEX allows pipelined modifications:
      //
      aos = m_aosesPegMICEX[a_T][a_I][isBuy];
      assert(usePegging && aos != nullptr && aos->m_isCxlPending == 0);

      // Get the curr Px and Qty:
      Req12 const* req     = aos->m_lastReq;
      assert(req != nullptr && req->m_kind != Req12::KindT::Cancel);

      PriceT currPx  = req->m_px;
      assert(IsFinite(currPx));

      QtyO   currQty = req->GetQty<QTO,QRO>();
      assert(IsPos(currQty));
      QtyO   newQty  = currQty + qty;
      assert(newQty  > currQty);

      // Now really try to modify the Pegged Order. Again, the Qtys are already
      // rounded to a LotSz multiple, hence  IsAggr=false, Buffered=false:
      try
      {
        bool ok = m_omcMICEX.ModifyOrder
          (aos,       currPx,    false,      QtyAny(newQty),
           a_ts_exch, a_ts_recv, a_ts_strat, false);

        if (utxx::unlikely(!ok))
          throw utxx::runtime_error("ModifyOrder(Pegged) returned False");
      }
      catch (std::exception const& exc)
      {
        // If failed, we better stop the whole Strategy:
        DelayedGracefulStop hmm
          (this, a_ts_strat,
           "MM-NTProgress::MayBeDoCoveringOrder: Pegged Order Modification "
           "FAILED: Symbol=", aggrInstr->m_Symbol.data(), ", ",
           isBuy ? "Buy" : "Sell",  ", NewQty=",  QRO(newQty), ", CurrPx=",
           currPx, ": ",   exc.what());
        return;
      }
    }
    //-----------------------------------------------------------------------//
    // If we got here: The Order has been successfully issued:               //
    //-----------------------------------------------------------------------//
    // Update the OrderInfo  and   the FlyingDeltas; OrderInfo will allow us to
    // etract (T,I) later, so that the FlyingDeltas can be modified back  when
    // the Order is Filled:
    //
    assert(aos != nullptr && aos->m_isBuy == isBuy);

    OrderInfo ori;
    ori.m_T = a_T;
    ori.m_I = a_I;
    ori.m_B = -1;    // It does NOT corresponds to any particular Band
    ori.m_isPegged = usePegging;
    aos->m_userData.Set(ori);

    QtyO oldFlying = m_flyingCovDeltas[a_T][a_I];
    if (isBuy)
      m_flyingCovDeltas[a_T][a_I] += qty;
    else
      m_flyingCovDeltas[a_T][a_I] -= qty;

    // Log it:
    Req12 const* req = aos->m_lastReq;
    assert(req      != nullptr);

    LOG_INFO(3,
      "MM-NTProgress::MayBeDoCoveringOrder: {} MICEX {} CoveringOrder: "
      "Symbol={}, {}, Qty={}: AOSID={}, ReqID={} : CurrPos={}, Limit={}, "
      "Flying={}",
      useNew ? "New" : "Modified", usePegging ? "Pegged" : "Market",
      aggrInstr->m_Symbol.data(),  isBuy ? "Buy" : "Sell",  QRO(qty),
      aos->m_id,       req->m_id,  QRO(currPos),            QRO(maxPos),
      QRO(oldFlying))
  }

  //=========================================================================//
  // "NewPassOrderSafe": Exception-Safe Wrapper:                             //
  //=========================================================================//
  // NB: "a_T", "a_I" and "a_B" are installed as UserData in the AOS:
  //
  template<bool Buffered>
  inline AOS const* MM_NTProgress::NewPassOrderSafe
  (
    SecDefD const&  a_instr,
    bool            a_is_buy,
    PriceT          a_px,
    QtyO            a_qty,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat,
    int             a_T,
    int             a_I,
    int             a_B
  )
  {
    assert((a_T == TOD  || a_T == TOM) && (a_I == AB || a_I == CB) &&
           0 <= a_B && a_B < m_nBands [a_T][a_I]);

    DEBUG_ONLY(int       nBands  = m_nBands[a_T][a_I];)
    assert(0 < nBands && nBands <= MaxBands);

    // Stopping or Dry-Run? -- If so, new Quotes are not allowed (we should have
    // already issued Cancellation reqs for all existing Quotes):
    if (utxx::unlikely(IsStopping() || m_dryRunMode))
      return nullptr;

    // Actually issue a new Order. Capture and log possible "NewOrder" except-
    // ions, then terminate the Strategy.
    // NB: IsAggr=false, BatchSend=Buffered:
    AOS const* res = nullptr;
    try
    {
      res = m_omcNTPro.NewOrder
      (
        this,       a_instr,    FIX::OrderTypeT::Limit,
        a_is_buy,   a_px,       false,           QtyAny(a_qty),
        a_ts_exch,  a_ts_recv,  a_ts_strat,      Buffered,
        FIX::TimeInForceT::GoodTillCancel,       0
      );
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated, though it should NOT occur here:
      throw;
    }
    catch (std::exception const& exc)
    {
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "MM-NTProgress::NewPassOrderSafe: Order FAILED: Symbol=",
         a_instr.m_Symbol.data(), ", ", a_is_buy ? "Buy": "Sell",
         ", Band=", a_B, ", Px=", a_px, ", Qty=",  QRO(a_qty), ": ",
         exc.what());
      return nullptr;
    }

    // Install the OrderInfo. This is NOT a Pegged Order, of course (on NTPro):
    assert(res != nullptr);

    OrderInfo ori;
    ori.m_T = a_T;
    ori.m_I = a_I;
    ori.m_B = a_B;
    assert(!ori.m_isPegged);
    res->m_userData.Set(ori);

    // All Done!
    return res;
  }

  //=========================================================================//
  // "ModifyPassOrderSafe": Exception-Safe Wrapper:                          //
  //=========================================================================//
  template<bool Buffered>
  inline bool MM_NTProgress::ModifyPassOrderSafe
  (
    AOS const*      a_aos,         // Non-NULL
    PriceT          a_px,
    QtyO            a_qty,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    assert(a_aos != nullptr     && IsFinite(a_px)    &&  IsPos(a_qty) &&
         !(a_aos->m_isInactive) && a_aos->m_isCxlPending == 0         &&
          (a_aos->m_omc == &m_omcNTPro  ||  a_aos->m_omc == &m_omcMICEX));

    // Modifications are not allowed (silently ignored) in  the Stopping Mode
    // (because we should have issued Cancellation Reqs for all Quotes before
    // entering that mode anyway):
    //
    if (utxx::unlikely(IsStopping() || m_dryRunMode))
      return false;

    // Generic Case: This may be an NPRo Quote or a MICEX Pegged Order; Still,
    // avoid virtual calls:
    bool        isNTPro = (a_aos->m_omc == &m_omcNTPro);
    char const* omcStr  = isNTPro ? "NTProgress" : "MICEX";
    try
    {
      // NB: IsAggr=false, BatchSend=Buffered:
      DEBUG_ONLY(bool ok =)
        isNTPro
        ? m_omcNTPro.ModifyOrder
          (
            a_aos,     a_px,       false,      QtyAny(a_qty),
            a_ts_exch, a_ts_recv,  a_ts_strat, Buffered
          )
        : m_omcMICEX.ModifyOrder
          (
            a_aos,     a_px,       false,      QtyAny(a_qty),
            a_ts_exch, a_ts_recv,  a_ts_strat, Buffered
          );
      // "ModifyOrder" must not return "false" in this case, because the pre-
      // conds already checked and satisfied:
      assert(ok);
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated, though it should NOT occur here:
      throw;
    }
    catch (std::exception const& exc)
    {
      SecDefD const& instr = *(a_aos->m_instr);
      DelayedGracefulStop hmm
        (this, a_ts_strat,
         "MM-NTProgress::ModifyPassOrderSafe: ", omcStr,  ": FAILED: Symbol=",
         instr.m_Symbol.data(),   ", ",  a_aos->m_isBuy ? "Buy" : "Sell",
         ", Px=", a_px, ", Qty=", QRO(a_qty), ", AOSID=", a_aos->m_id,  ": ",
         exc.what());
      return false;
    }

    // OK:
    return true;
  }

  //=========================================================================//
  // "CancelPassOrderSafe": Exception-Safe Wrapper:                          //
  //=========================================================================//
  // Returns "true" iff the CancelReq was successfully submitted, AND the AOS
  // was successfully placed on the CxlPending list. In all other cases, incl
  // nothing-to-do or errors, returns "false", and may trigger DelayedStop:
  // NB:
  // (*) On NTProgress, we cancel Quotes;
  // (*) On MICEX,      we cancel Pegged Orders (if enabled):
  //
  template<bool Buffered>
  inline bool MM_NTProgress::CancelPassOrderSafe
  (
    AOS const*      a_aos,        // Non-NULL
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    //-----------------------------------------------------------------------//
    // Nothing to do?                                                        //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely
       (a_aos == nullptr || a_aos->m_isInactive || a_aos->m_isCxlPending != 0 ||
        m_dryRunMode))
      // No actual Cancel Req was submitted:
      return false;

    //-----------------------------------------------------------------------//
    // Try to Cancel:                                                        //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) cancellations are always allowed (even if IsStopping() is true);
    // (*) the OMC can be either NTProgress or MICEX;
    // (*) BatchSend=Buffered:
    //
    assert(a_aos->m_omc == &m_omcNTPro || a_aos->m_omc == &m_omcMICEX);
    bool        isNTPro =  (a_aos->m_omc == &m_omcNTPro);
    char const* omcStr  =  (isNTPro ? "NTProgress" : "MICEX");
    try
    {
      // NB: Still, avoid a virtual call:
      DEBUG_ONLY(bool ok =)
        utxx::likely(isNTPro)
        ? m_omcNTPro.CancelOrder
          (a_aos, a_ts_exch, a_ts_recv, a_ts_strat, Buffered)
        : m_omcMICEX.CancelOrder
          (a_aos, a_ts_exch, a_ts_recv, a_ts_strat, Buffered);

      // "CancelOrder" cannot return "false" in this case, because pre-conds
      // were checked and satisfied:
      assert(ok);
    }
    catch (EPollReactor::ExitRun const&)
    {
      // This exception is always propagated -- even for "Cancel", though it
      // should NOT occur here:
      throw;
    }
    catch (std::exception const& exc)
    {
      SecDefD const& instr = *(a_aos->m_instr);
      DelayedGracefulStop hmm
        (this, a_ts_strat,
        "MM-NTProgress::CancelPassOrderSafe(", omcStr,  ":) FAILED: Symbol=",
        instr.m_Symbol.data(),   ", ", a_aos->m_isBuy ? "Buy" : "Sell",
        ", AOSID=", a_aos->m_id, ": ", exc.what());
      return false;
    }

    //-----------------------------------------------------------------------//
    // Manage the CxlPending List:                                           //
    //-----------------------------------------------------------------------//
    // In addition, this AOS will be memoised in the CxlPending List. Because
    // we have just submitted the Cancel Req, the AOS should NOT normally  be
    // on that List already, unless a prev Cancel Req has failed.
    // NB: BOTH NTProgress and MICEX AOSes can be inserted into this list!
    //
    bool ok = false;

    if (utxx::unlikely
       (std::find(m_cpAOSes.begin(), m_cpAOSes.end(), a_aos) !=
        m_cpAOSes.end()))
    {
      LOG_WARN(3,
        "MM-NTProgress::CancelPassOrderSafe({}): AOSID={}: Already on the "
        "CxlPending List", a_aos->m_id)
      // "ok" remains "false"!
    }
    else
    {
      // Not on the List yet:
      // Is the List full? -- That would mean that too many "CxlPending" orders
      // are there -- a very serious error condition:
      //
      if (utxx::unlikely(m_cpAOSes.size() >= m_cpAOSes.capacity()))
      {
        DelayedGracefulStop hmm
          (this, a_ts_strat,
           "MM-NTProgress::CancelPassOrderSafe(", omcStr,
           "): Too many PendingCancel Ords: ",    m_cpAOSes.size());
        return false;
      }

      // OK, really add it:
      m_cpAOSes.push_back(a_aos);
      ok = true;
    }

    //-----------------------------------------------------------------------//
    // Log the CancelReq:                                                    //
    //-----------------------------------------------------------------------//
    SecDefD const*  instr = a_aos->m_instr;
    assert(instr != nullptr);

    LOG_INFO(3,
      "MM-NTProgress::CancelPassOrderSafe({}): Cancelling AOSID={}: Symbol="
      "{}, {}, SettlDate={}",
      omcStr,  a_aos->m_id,           instr->m_Symbol.data(),
      a_aos->m_isBuy ? "Bid" : "Ask", instr->m_SettlDate)

    return ok;
  }

  //=========================================================================//
  // "CancelQuoteSafe" for a Main NTPro Slot:                                //
  //=========================================================================//
  template<bool Buffered>
  inline void MM_NTProgress::CancelQuoteSafe
  (
    int             a_T,
    int             a_I,
    int             a_S,
    int             a_B,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    // The args are assumed to be checked by the Caller:
    AOS const*& aos =
      GetMainAOS(a_T, a_I, a_S, a_B);  // Ref to a Ptr, may be NULL!

    if (CancelPassOrderSafe<Buffered>
       (aos, a_ts_exch, a_ts_recv, a_ts_strat))
    {
      assert(aos != nullptr  &&  aos->m_isCxlPending != 0       &&
             std::find(m_cpAOSes.begin(), m_cpAOSes.end(), aos) !=
             m_cpAOSes.end());
      // Successful CancelReq -- can now NULLify the Main Slot:
      aos = nullptr;
    }
  }

  //=========================================================================//
  // "CancelPeggedSafe" for a Pegged MICEX Order:                            //
  //=========================================================================//
  template<bool Buffered>
  inline void MM_NTProgress::CancelPeggedSafe
  (
    int             a_T,
    int             a_I,
    int             a_S,
    utxx::time_val  a_ts_exch,
    utxx::time_val  a_ts_recv,
    utxx::time_val  a_ts_strat
  )
  {
    // The args are assumed to be checked by the Caller:
    AOS const*& aos =
      GetPeggedAOS(a_T, a_I, a_S); // Ref to a Ptr, may be NULL!

    if (CancelPassOrderSafe<Buffered>(aos, a_ts_exch, a_ts_recv, a_ts_strat))
    {
      assert(aos != nullptr  &&  aos->m_isCxlPending != 0       &&
             std::find(m_cpAOSes.begin(), m_cpAOSes.end(), aos) !=
             m_cpAOSes.end());
      // Successful CancelReq -- can now NULLify the Pegged Slot:
      aos = nullptr;
    }
  }
} // End namespace MAQUETTE
