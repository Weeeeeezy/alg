// vim:ts=2:et
//===========================================================================//
//                        "Tools/PrintOrdStatus.cpp":                        //
//===========================================================================//
#include "Basis/OrdMgmtTypes.hpp"
#include "Connectors/EConnector_OrdMgmt.h"
#include "InfraStruct/PersistMgr.h"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <iostream>
#include <cstring>

using namespace std;
using namespace MAQUETTE;

namespace
{
  //=========================================================================//
  // Operation to be performed:                                              //
  //=========================================================================//
  enum class OpT: int
  {
    UNDEFINED =  0,
    IntLats   = -2, // Must be negative!
    ConfLats  = -1, //
    AOS       =  1,
    Req12     =  2,
    Trade     =  3
  };

  // To simply extract the Qtys, we use "UNDEFINED" QT:
  constexpr QtyTypeT QT =  QtyTypeT::UNDEFINED;

  //=========================================================================//
  // "PrintEntry(Req12)":                                                    //
  //=========================================================================//
  inline int PrintEntry(Req12 const& a_req)
  {
    // Check the "Req12":
    if (utxx::unlikely(a_req.m_aos == nullptr))
    {
      cerr << "ERROR: ReqID=" << a_req.m_id << ": AOSPtr=NULL" << endl;
      return 1;
    }

    //-----------------------------------------------------------------------//
    // OK, Got it: Print all the Flds:                                       //
    //-----------------------------------------------------------------------//
    // The Main Part:
    cout <<   "AOSID     = "   << a_req.m_aos->m_id
         << "\nReqID     = "   << a_req.m_id
         << "\nOrigID    = "   << a_req.m_origID
         << "\nTxSN      = "   << a_req.m_seqNum
         << "\nKind      = "   << a_req.m_kind.to_string()
         << "\nPx        = "   << a_req.m_px
         << "\nQT        = "   << int(a_req.m_qt);  // Actual QT

    if (a_req.m_withFracQtys)
      cout << "\nQty       = " <<  double(a_req.GetQty      <QT,double>())
           << "\nQtyShow   = " <<  double(a_req.GetQtyShow  <QT,double>())
           << "\nQtyMin    = " <<  double(a_req.GetQtyMin   <QT,double>())
           << "\nLeavesQty = " <<  double(a_req.GetLeavesQty<QT,double>())
           << "\nFilledQty = " << (double(a_req.GetQty      <QT,double>()) -
                                   double(a_req.GetLeavesQty<QT,double>()));
    else
      cout << "\nQty       = " << long   (a_req.GetQty      <QT,long>())
           << "\nQtyShow   = " << long   (a_req.GetQtyShow  <QT,long>())
           << "\nQtyMin    = " << long   (a_req.GetQtyMin   <QT,long>())
           << "\nLeavesQty = " << long   (a_req.GetLeavesQty<QT,long>())
           << "\nFilledQty = " << (long  (a_req.GetQty      <QT,long>())   -
                                   long  (a_req.GetLeavesQty<QT,long>()));

    cout << "\nStatus    = "   << a_req.m_status.to_string()
         << "\nWillFail  = "   << a_req.m_willFail
         // ExchOrdIDs are printed here, although they are located at the end
         // of Req12:
         << "\nExchOrdID = "   << a_req.m_exchOrdID.ToString()
         << "\nMDEntryID = "   << a_req.m_mdEntryID.ToString()
         << "\nPrevReqID = "
         << (a_req.m_prev != nullptr ? a_req.m_prev->m_id : 0)
         << "\nNextReqID = "
         << (a_req.m_next != nullptr ? a_req.m_next->m_id : 0);

    // Time Stamps:
    char tsBuff[32]; // Buffer for TimeStamps stringification, with usec

    OutputDateTimeFIX<true>(a_req.m_ts_md_exch,   tsBuff);
    cout << "\nTS_MD_Exch   = " << tsBuff;

    OutputDateTimeFIX<true>(a_req.m_ts_md_conn,   tsBuff);
    cout << "\nTS_MD_Conn   = " << tsBuff;

    OutputDateTimeFIX<true>(a_req.m_ts_md_strat,  tsBuff);
    cout << "\nTS_MD_Strat  = " << tsBuff;

    OutputDateTimeFIX<true>(a_req.m_ts_created,   tsBuff);
    cout << "\nTS_Created   = " << tsBuff;

    OutputDateTimeFIX<true>(a_req.m_ts_sent,      tsBuff);
    cout << "\nTS_Sent      = " << tsBuff;

    OutputDateTimeFIX<true>(a_req.m_ts_conf_exch, tsBuff);
    cout << "\nTS_Conf_Exch = " << tsBuff;

    OutputDateTimeFIX<true>(a_req.m_ts_conf_conn, tsBuff);
    cout << "\nTS_Conf_Conn = " << tsBuff;

    OutputDateTimeFIX<true>(a_req.m_ts_end_exch,  tsBuff);
    cout << "\nTS_End_Exch  = " << tsBuff;

    OutputDateTimeFIX<true>(a_req.m_ts_end_conn,  tsBuff);
    cout << "\nTS_End_Conn  = " << tsBuff;

    // All Done!
    cout << endl;
    return 0;
  }

  //=========================================================================//
  // "PrintEntry(AOS)":                                                      //
  //=========================================================================//
  inline int PrintEntry(AOS const& a_aos)
  {
    // XXX: Only the following data are currently printed. In particular,  we
    // cannot print the SecDef (because it is located in a different ShM segm
    // which is not mapped in), or transient data  which are  located in non-
    // shared memory at run-time, or UserData:
    //
    cout <<   "AOSID        = "   << a_aos.m_id
         << "\nSide         = "   << (a_aos.m_isBuy ? "Bid" : "Ask")
         << "\nOrderType    = "   << char(a_aos.m_orderType)
         << "\nTimeInForce  = "   << char(a_aos.m_timeInForce)
         << "\nExpireDate   = "   << a_aos.m_expireDate
         << "\nFrstReqID    = "   << (utxx::likely(a_aos.m_frstReq != nullptr)
                                     ? a_aos.m_frstReq->m_id : 0)
         << "\nLastReqID    = "   << (utxx::likely(a_aos.m_lastReq != nullptr)
                                      ? a_aos.m_lastReq->m_id : 0)
         << "\nLastTrd      = "   << ((a_aos.m_lastTrd != nullptr)
                                     ? a_aos.m_lastTrd->m_id  : 0)
         << "\nIsInactive   = "   << a_aos.m_isInactive
         << "\nIsFilled     = "   << a_aos.IsFilled   ()
         << "\nIsCancelled  = "   << a_aos.IsCancelled()
         << "\nHasFailed    = "   << a_aos.HasFailed  ()
         << "\nIsCxlPending = "   << a_aos.m_isCxlPending
         << "\nNFails       = "   << a_aos.m_nFails
         << "\nCumFilledQty = ";
    if (a_aos.m_withFracQtys)
      cout << double(a_aos.GetCumFilledQty<QT,double>());
    else
      cout << long  (a_aos.GetCumFilledQty<QT,long>  ());

    cout << endl;
    return 0;
  }

  //=========================================================================//
  // "PrintEntry(Trade)":                                                    //
  //=========================================================================//
  inline int PrintEntry(Trade const& a_tr)
  {
    cout <<   "TrdID     = " << a_tr.m_id
         << "\nReqID     = " << ((a_tr.m_ourReq != nullptr)
                                ? a_tr.m_ourReq->m_id : 0)
         << "\nAccountID = " << a_tr.m_accountID
         << "\nExecID    = " << a_tr.m_execID.ToString()
         << "\nPx        = " << double(a_tr.m_px)
         << "\nQty       = ";
    if (a_tr.m_withFracQtys)
      cout << double(a_tr.GetQty<QT,double>());
    else
      cout << long  (a_tr.GetQty<QT,long>  ());

    // XXX: Gee is always assumed to be fractional:
    cout << "\nFee       = " << double(a_tr.GetFee<QT,double>())
         << "\nAggressor = " <<
            ((a_tr.m_aggressor == FIX::SideT::Buy)  ? "Buy"  :
             (a_tr.m_aggressor == FIX::SideT::Sell) ? "Sell" : "UNDEFINED")
         << "\nAccSide   = " <<
            ((a_tr.m_accSide   == FIX::SideT::Buy)  ? "Buy"  :
             (a_tr.m_accSide   == FIX::SideT::Sell) ? "Sell" : "UNDEFINED");

    char tsBuff[32]; // Buffer for TimeStamps stringification, with usec

    OutputDateTimeFIX<true>(a_tr.m_exchTS, tsBuff);
    cout << "\nTS_Exch   = " << tsBuff;

    OutputDateTimeFIX<true>(a_tr.m_recvTS, tsBuff);
    cout << "\nTS_Recv   = " << tsBuff;

    cout << "\nPrevTrdID = " << ((a_tr.m_prev != nullptr)
                                ? a_tr.m_prev->m_id  : 0)
         << "\nNextTrdID = " << ((a_tr.m_next != nullptr)
                                ? a_tr.m_next->m_id  : 0)
         << endl;
    // All Done!
    return 0;
  }

  //=========================================================================//
  // "PrintsStats(Req12s)":                                                  //
  //=========================================================================//
  inline void PrintStats(Req12 const* a_reqs, OrderID a_n_reqs, long a_op)
  {
    assert(a_reqs != nullptr && a_n_reqs > 0);

    OpT    op =  OpT(a_op);
    assert(op == OpT::IntLats || op == OpT::ConfLats);

    for (OrderID r = 1; r <= a_n_reqs; ++r)
    {
      Req12 const& req = a_reqs[r];
      if (utxx::unlikely(req.m_id != r || req.m_aos == nullptr))
        continue;  // Uninitialised entry -- skip it!

      // Otherwise:
      // Internal Latency?
      if (op == OpT::IntLats &&
          !(req.m_ts_md_conn.empty()   || req.m_ts_sent.empty()))
      {
        long intLat  = (req.m_ts_sent - req.m_ts_md_conn).microseconds();
        cout << req.m_id << '\t' << intLat  << endl;
      }
      else
      if (op == OpT::ConfLats &&
          !(req.m_ts_created.empty()   || req.m_ts_conf_conn.empty()))
      {
        long confLat = (req.m_ts_conf_conn - req.m_ts_created).microseconds();
        cout << req.m_id << '\t' << confLat << endl;
      }
    }
  }

  //=========================================================================//
  // UnImplemented "PrintStats":                                             //
  //=========================================================================//
  inline void PrintStats(AOS   const*, OrderID, long) {}
  inline void PrintStats(Trade const*, OrderID, long) {}

  //=========================================================================//
  // "ProcessEntriesGen": From "FixedShM" or "FixedMMF":                     //
  //=========================================================================//
  // ST:  FixedShM or FixedMMF
  // ART: AOS      or Req12 or Trade
  //
  template<typename ST, typename ART>
  inline int ProcessEntriesGen
  (
    string const& a_seg_name,
    unsigned long a_base_addr,
    long          a_id_op     // ObjID or OpCode (if < 0)
  )
  {
    //-----------------------------------------------------------------------//
    // Map the named ShM or MMF Segment in:                                  //
    //-----------------------------------------------------------------------//
    PersistMgr<ST> PM(a_seg_name, a_base_addr, 0, nullptr);

    //-----------------------------------------------------------------------//
    // Within the Segment, get the AOSes/Req12/Trades and the corresp ID:    //
    //-----------------------------------------------------------------------//
    ART     const* arts  = nullptr;
    OrderID const* artN  = nullptr;

    constexpr bool IsAOS = std::is_same_v<ART, AOS>;
    constexpr bool IsReq = std::is_same_v<ART, Req12>;
    constexpr bool IsTrd = std::is_same_v<ART, Trade>;
    static_assert (IsAOS || IsReq || IsTrd,
                   "ProcessReq12Gen: ART must be AOS or Req12 or Trade");

    // "AOSes" or "Req12s" or "Trades":
    using ECOM =   EConnector_OrdMgmt;
    constexpr char const* artsNm =
      IsAOS ? ECOM::AOSesON() : IsReq ? ECOM::Req12sON() : ECOM::TradesON();

    constexpr char const* cntNm  =
      IsAOS ? ECOM::OrdN_ON() : IsReq ? ECOM::ReqN_ON () : ECOM::TrdN_ON ();

    auto artP    = PM.GetSegm()->template find<ART>(artsNm);
    arts         = artP.first;
    long maxARTs = long(artP.second);

    if (utxx::unlikely(arts == nullptr || maxARTs <= 0))
    {
      cerr << "ERROR: Could not get " << artsNm << endl;
      return 1;
    }

    // "OrdN" or "ReqN" or "TrdN":
    auto artNP   = PM.GetSegm()->template find<OrderID>(cntNm);
    artN         = artNP.first;

    if (utxx::unlikely(artN == nullptr || artNP.second != 1))
    {
      cerr << "ERROR: Could not get " << cntNm << endl;
      return 1;
    }

    // Must have (*artN) in [1 .. Max{AOSes|Reqs|Trds}-1}]
    // (*artN==0 is not used):
    if (utxx::unlikely(*artN == 0 || *artN >= OrderID(maxARTs)))
    {
      cerr << "ERROR: Got invalid " << cntNm << '=' << *artN << " (Max="
           << maxARTs << ')' << endl;
      return 1;
    }
    else
      cout << "Accessing " << cntNm << '=' << *artN << " (Max=" << maxARTs
           << ')' << endl;

    //-----------------------------------------------------------------------//
    // The Action depends on the Op or ID:                                   //
    //-----------------------------------------------------------------------//
    //-----------------------------------------------------------------------//
    // Print and individual "AOS" or "Req12":                                //
    //-----------------------------------------------------------------------//
    if (a_id_op >= 0)
    {
      // Then it is an AOSID or ReqID or TrdID:
      OrderID id = OrderID(a_id_op);

      if (utxx::unlikely(id == 0 || id > *artN))
      {
        cerr << "ERROR: Invalid "
             << (IsAOS ? "AOSID=" : IsReq ? "ReqID=" : "TrdID=") << id
             << ": Must be in [1 .. " << *artN << ']' << endl;
        return 1;
      }

      // Get the Entry -- it must have its own ID installed within it:
      ART const& art = arts[id];
      if (utxx::unlikely(art.m_id != id))
      {
        cerr << "ERROR: Invalid "
             << (IsAOS ? "AOSID=" : IsReq ? "ReqID=" : "TrdID=") << id
             << ": IntrinsicID=" << art.m_id   << endl;
        return 1;
      }
      // OK to proceed:
      return PrintEntry(art);
    }
    else
    //-----------------------------------------------------------------------//
    // Otherwise: Process all valid "Req12"s with the specified Op:          //
    //-----------------------------------------------------------------------//
    if (IsReq)
    {
      PrintStats(arts, *artN, a_id_op);
      return 0;
    }
    else
    {
      cerr << "ERROR: Statistical Ops are implemented for Req12s only" << endl;
      return 1;
    }
    // All Done!
  }

  //=========================================================================//
  // "CmdArgsHelp":                                                          //
  //=========================================================================//
  inline void CmdArgsHelp()
  {
    cerr << "PARAMETERS: {shm|mmf}:Name" << endl;
    cerr << "            {--r=ReqID|--a=AOSID|--t=TrdID|--int-lats|--conf-lats}"
         << endl;
    cerr << "            [BaseAddr]"     << endl;
  }
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  //-------------------------------------------------------------------------//
  // Get the Command-Line Args:                                              //
  //-------------------------------------------------------------------------//
  if (argc != 3 && argc != 4)
  {
    CmdArgsHelp();
    return 1;
  }

  // Obj Path / SegName:
  char const*   segName  = argv[1];

  // Operation / ObjType we are going to perform / process:
  OpT op =
    (strncmp(argv[2], "--a=", 4) == 0)
    ? OpT::AOS   :
    (strncmp(argv[2], "--r=", 4) == 0)
    ? OpT::Req12 :
    (strncmp(argv[2], "--t=", 4) == 0)
    ? OpT::Trade :
    (strcmp (argv[2], "--int-lats" ) == 0)
    ? OpT::IntLats  :
    (strcmp (argv[2], "--conf-lats") == 0)
    ? OpT::ConfLats
    : OpT::UNDEFINED;

  // The ObjID:
  long id =
    (op == OpT::AOS || op == OpT::Req12 || op == OpT::Trade)
    ? atol(argv[2] + 4)
    : 0;

  // May sometimes need an explicit BaseAddr:
  unsigned long baseAddr =
    (argc == 4)
    ? strtoul(argv[3], nullptr, 16)
    : 0;

  // Is it a ShM Segment Name or a File?
  bool isShM =  (strncmp(segName, "shm:", 4) == 0);
  bool isMMF =  (strncmp(segName, "mmf:", 4) == 0);

  if (utxx::unlikely(!(isShM || isMMF) || op == OpT::UNDEFINED))
  {
    CmdArgsHelp();
    return 1;
  }
  // Remove the URL prefix from "segName":
  segName += 4;

  //-------------------------------------------------------------------------//
  // Now Do It:                                                              //
  //-------------------------------------------------------------------------//
  // XXX: "segName" will be automatically converted to "string":
  try
  {
    switch (op)
    {
      // AOS in ShM or MMF:
      case OpT::AOS:
        if (isShM)
          return ProcessEntriesGen<FixedShM, AOS>  (segName, baseAddr, id);
        else
          return ProcessEntriesGen<FixedMMF, AOS>  (segName, baseAddr, id);

      // Req12 in ShM or MMF:
      case OpT::Req12:
        if (isShM)
          return ProcessEntriesGen<FixedShM, Req12>(segName, baseAddr, id);
        else
          return ProcessEntriesGen<FixedMMF, Req12>(segName, baseAddr, id);

      // Trade in ShM or MMF:
      case OpT::Trade:
        if (isShM)
          return ProcessEntriesGen<FixedShM, Trade>(segName, baseAddr, id);
        else
          return ProcessEntriesGen<FixedMMF, Trade>(segName, baseAddr, id);

      // Otherwise: Stats:
      default:
        if (isShM)
          return ProcessEntriesGen<FixedShM, Req12>
                 (segName, baseAddr, long(op));
        else
          return ProcessEntriesGen<FixedMMF, Req12>
                 (segName, baseAddr, long(op));
    }
  }
  catch (std::exception const& exc)
  {
    cerr << "EXCEPTION: " << exc.what() << endl;
    return 1;
  }
}
