// vim:ts=2:et:sw=2
//===========================================================================//
//                          "StratMSEnvTypes.h":                             //
//              Common Types used by "StratEnv" and "MSEnv":                 //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_STRATMSENVTYPES_H
#define MAQUETTE_STRATEGYADAPTOR_STRATMSENVTYPES_H

#include "Common/Maths.h"
#include "StrategyAdaptor/EventTypes.hpp"
#include "StrategyAdaptor/OrderBooksShM.h"

#include <utxx/rate_throttler.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>

namespace MAQUETTE
{
  // Fwd declataion of AOS -- it is only accessible via a ptr here:
  struct AOS;
}

namespace StratMSEnvTypes
{
  //=========================================================================//
  // "QSym":                                                                 //
  //=========================================================================//
  struct QSym
  {
    //-----------------------------------------------------------------------//
    // Data Flds:                                                            //
    //-----------------------------------------------------------------------//
    Events::SymKey  m_symKey;   // Exchange-specific Symbol
    int             m_mdcN;     // MDC (by number) which defines this Symbol
    int             m_omcN;     // OMC (by number) used      for this Symbol

    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    QSym()
    : m_symKey(),
      m_mdcN  (-1),
      m_omcN  (-1)
    {
      m_symKey.fill('\0');
    }

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    QSym(char const* symbol, int mdcN, int omcN)
    : m_symKey(Events::MkFixedStr<Events::SymKeySz>(symbol)),
      m_mdcN  (mdcN),
      m_omcN  (omcN)
    {}

    //-----------------------------------------------------------------------//
    // Equality:                                                             //
    //-----------------------------------------------------------------------//
    // XXX: Why is there no auto-generated equality operator for this struct?
    //
    bool operator==(QSym const& right) const
    {
      return m_symKey == right.m_symKey && m_mdcN == right.m_mdcN &&
             m_omcN   == right.m_omcN;
    }

    bool operator!=(QSym const& right) const { return !(*this == right); }

    //-----------------------------------------------------------------------//
    // Comparison:                                                           //
    //-----------------------------------------------------------------------//
    bool operator<(QSym const& right) const
    {
      return (m_symKey <  right.m_symKey) ||
             (m_symKey == right.m_symKey  &&
               (m_mdcN <  right.m_mdcN    ||
               (m_mdcN == right.m_mdcN    && m_omcN <  right.m_omcN)));
    }

    //-----------------------------------------------------------------------//
    // Symbol extraction:                                                    //
    //-----------------------------------------------------------------------//
    char const* GetSymbol() const
      { return Events::ToCStr(m_symKey); }

    //-----------------------------------------------------------------------//
    // Conversion to "std::string":                                          //
    //-----------------------------------------------------------------------//
    std::string ToString() const
    {
      return "("  + Events::ToString(m_symKey) +", " + std::to_string(m_mdcN) +
             ", " + std::to_string  (m_omcN)   + ")";
    }
  };

  //=========================================================================//
  // ShM Allocators and ShM Data Types:                                      //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // Main "void" ShM Allocator:                                              //
  //-------------------------------------------------------------------------//
  // (Can be cast into any other Allocator type declared below):
  //
  typedef
    BIPC::allocator<void, BIPC::fixed_managed_shared_memory::segment_manager>
    ShMAllocator;

  //-------------------------------------------------------------------------//
  // "StrVec":                                                               //
  //-------------------------------------------------------------------------//
  typedef
    BIPC::allocator<char, BIPC::fixed_managed_shared_memory::segment_manager>
    CharAllocator;

  typedef
    BIPC::basic_string<char, std::char_traits<char>, CharAllocator>
    ShMString;

  typedef
    BIPC::allocator
      <ShMString, BIPC::fixed_managed_shared_memory::segment_manager>
    StringAllocator;

  typedef BIPC::vector<ShMString, StringAllocator>   StrVec;

  //-------------------------------------------------------------------------//
  // "QSymVec":                                                              //
  //-------------------------------------------------------------------------//
  typedef
    BIPC::allocator<QSym, BIPC::fixed_managed_shared_memory::segment_manager>
    QSymAllocator;

  typedef BIPC::vector<QSym, QSymAllocator>          QSymVec;

  //-------------------------------------------------------------------------//
  // "AOSMapQPV":                                                            //
  //-------------------------------------------------------------------------//
  // For each "QSym", we maintain a vector of AOS Ptrs corresponding to that
  // "QSym":
  typedef
    BIPC::allocator<MAQUETTE::AOS*,
                    BIPC::fixed_managed_shared_memory::segment_manager>
    AOSPAllocator;

  typedef
    BIPC::vector<MAQUETTE::AOS*, AOSPAllocator>      AOSPtrVec;

  typedef
    BIPC::allocator<std::pair<QSym const, AOSPtrVec>,
                    BIPC::fixed_managed_shared_memory::segment_manager>
    AOSQPVAllocator;

  typedef
    BIPC::map<QSym, AOSPtrVec, std::less<QSym>, AOSQPVAllocator>
    AOSMapQPV;

  typedef
    utxx::basic_rate_throttler<16, 32>
    PosRateThrottler;

  //-------------------------------------------------------------------------//
  // Risk Limits for a given Instrument (Symbol):                            //
  //-------------------------------------------------------------------------//
  struct RiskLimitsPerSymbol
  {
    long   m_maxAbsPos;          // Applies to both Longs & Shorts
    long   m_maxPosAcqRate;      // Per sec, for both Longs & Shorts
    long   m_posAcqRateInterval; // Per sec, for both Longs & Shorts
    long   m_maxActiveOrdersQty; // Similar

    // Default Ctor: No Limits:
    RiskLimitsPerSymbol()
    : m_maxAbsPos          (LONG_MAX),
      m_maxPosAcqRate      (LONG_MAX),
      m_maxActiveOrdersQty (LONG_MAX)
    {}

    RiskLimitsPerSymbol& operator=(RiskLimitsPerSymbol const&) = default;
  };

  //-------------------------------------------------------------------------//
  // "PositionsMap":                                                         //
  //-------------------------------------------------------------------------//
  struct PositionInfo
  {
    QSym                m_qsym;
    Events::AccCrypt    m_accCrypt;
    long                m_pos;
    // VarMargin data come here as well (currVM is then copied into CashInfo):
    double              m_realizedPnL;      // w/  trans costs
    double              m_avgPosPx0;        // w/o trans costs; 0 if pos==0
    double              m_avgPosPx1;        // w/  trans costs; 0 if pos==0
    double              m_VM;               // Variation Margin
    PosRateThrottler    m_posRateThrottler; // Calculates: pos acquisition rate
    RiskLimitsPerSymbol m_RL;               // Risk Limits for auto-action

    // Non-default Ctor:
    PositionInfo
    (
      QSym const&       a_qsym,
      Events::AccCrypt  a_accCrypt,
      long              a_pos,
      double            a_realizedPnL,
      double            a_avgPosPx0,
      double            a_avgPosPx1,
      double            a_VM,
      int               a_posRateThrWindow
    )
    : m_qsym            (a_qsym),
      m_accCrypt        (a_accCrypt),
      m_pos             (a_pos),
      m_realizedPnL     (a_realizedPnL),
      m_avgPosPx0       (a_avgPosPx0),
      m_avgPosPx1       (a_avgPosPx1),
      m_VM              (a_VM),
      m_posRateThrottler(a_posRateThrWindow),
      m_RL              ()   // USE THE DEFAULTS!
    {}

    // Average Position Px, with or without trans costs:
    double GetAvgPosPx(bool with_trans_costs) const
    {
      // NB: Both "avPosPxs" must be 0 if the pos is 0:
      assert (m_pos != 0 || (m_avgPosPx0 == 0.0 && m_avgPosPx1 == 0.0));
      return (with_trans_costs ? m_avgPosPx1 : m_avgPosPx0);
    }
  };

  typedef
    BIPC::allocator<std::pair<QSym const, PositionInfo>,
                    BIPC::fixed_managed_shared_memory::segment_manager>
    PositionsMapAllocator;

  typedef
    BIPC::map<QSym, PositionInfo, std::less<QSym>, PositionsMapAllocator>
    PositionsMap;

  //-------------------------------------------------------------------------//
  // "OrderBooksMap":                                                        //
  //-------------------------------------------------------------------------//
  typedef
    BIPC::allocator<std::pair<QSym const, MAQUETTE::OrderBookSnapShot const*>,
                    BIPC::fixed_managed_shared_memory::segment_manager>
    OrderBookAllocator;

  typedef
    BIPC::map<QSym, MAQUETTE::OrderBookSnapShot const*, std::less<QSym>,
              OrderBookAllocator>
    OrderBooksMap;

  //=========================================================================//
  // Other Data Types:                                                       //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "MassCancelSideT":                                                      //
  //-------------------------------------------------------------------------//
  enum class MassCancelSideT
  {
    SellT = 0,
    BuyT  = 1,
    BothT = 2
  };

  //-------------------------------------------------------------------------//
  // "AccStateInfo":                                                         //
  //-------------------------------------------------------------------------//
  // "AccStateInfo" provides info on either of the following:
  // (1) securities on a Trading Account, in which case "m_symbol" is non-
  //     empty, and "m_pos" is actually an integer;
  // (2) money (in the specified "m_ccy") on a monetary account, in which
  //     case "m_symbol" is empty, and "m_pos" is the amount (positive or
  //     negative) with 2 decimal places:
  //
  struct AccStateInfo
  {
    // Data Flds:
    std::string     m_accountID;
    std::string     m_symbol;
    std::string     m_ccy;
    double          m_pos;
    std::string     m_comment;

    // Default Ctor:
    AccStateInfo()
    : m_accountID(),
      m_symbol(),
      m_ccy(),
      m_pos(0.0),
      m_comment()
    {}
  };
}
#endif  // MAQUETTE_STRATEGYADAPTOR_STRATMSENVTYPES_H
