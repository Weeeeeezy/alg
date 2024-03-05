// vim:ts=2:et
//===========================================================================//
//                        "Connectors/H2WS/MDMsg.hpp"                        //
//                  Intermediate Types for H2WS MktData Msgs                 //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Basis/PxsQtys.h"
#include "Connectors/EConnector_MktData.h"
#include "Protocols/FIX/Msgs.h"

namespace MAQUETTE
{
  namespace H2WS
  {
    //=======================================================================//
    // Synthetic "MDEntryST" and "MDMsg" Types:                              //
    //=======================================================================//
    // XXX: They are not strictly necessary,  as we construct OrderBook Snap-
    // Shots and Trades directly from the data  read from incoming JSON objs.
    // However, in order to re-use EConnector_MktData::UpdateOrderBooks<>, we
    // need to use these structs -- the copying overhead is minimal:
    //
    //=======================================================================//
    // "MDEntryST" (SnapShot/Trade) Struct:                                  //
    //=======================================================================//
    // NB: Bids and Offers are stored in the same array of MDEntries, distingu-
    // ished by the MDEntryType:
    template <typename QtyN>
    class MDEntryST
    {
      public:
      // Set trade to MDEntry
      void SetTrade(ExchOrdID a_trade_id, QtyN a_qty, PriceT a_price)
      {
        assert(IsFinite(a_price));
        m_execID    = a_trade_id;
        m_qty       = a_qty;
        m_px        = a_price;
        m_entryType = FIX::MDEntryTypeT::Trade;
      }

      // Set Bid or Ask level
      void SetOrderLevel(bool isBid, QtyN a_qty, PriceT a_price)
      {
        assert(IsFinite(a_price));
        m_qty       = a_qty;
        m_px        = a_price;
        m_entryType = isBid ? FIX::MDEntryTypeT::Bid
                            : FIX::MDEntryTypeT::Offer;
      }
      //---------------------------------------------------------------------//
      // Accessors (CBs for EConnector_MktData::UpdateOrderBooks):           //
      //---------------------------------------------------------------------//
      // "GetEntryType" (returns Bid, Offer or Trade):
      FIX::MDEntryTypeT GetEntryType() const
      {
        return m_entryType;
      }

      // "GetUpdateAction": For SnapShot MDEs and Trades, it should always be
      // "New":
      FIX::MDUpdateActionT GetUpdateAction() const
      {
        return IsZero(m_qty) ? FIX::MDUpdateActionT::Delete :
                               FIX::MDUpdateActionT::New;
      }

      // "GetOrderID": Not available, and not required (currently used for FOL
      // only):
      constexpr static OrderID GetMDEntryID()  { return 0; }

      // "GetEntryRefID": Not applicable to SnapShots or Trades:
      constexpr static OrderID GetEntryRefID() { return 0; }

      // Size (Qty) and Px are supported, of course:
      QtyN GetEntrySize() const                { return m_qty; }

      PriceT GetEntryPx() const                { return m_px;  }

      // "WasTradedFOL" : Not applicable since this is not FOL mode:
      constexpr static bool WasTradedFOL()     { return false; }

      // "IsValidUpdate": Whether this MDEntry is usable
      constexpr static bool IsValidUpdate()    { return true;  }

      // "GetTradeAggrSide":
      // We encode the TradeAggrSide in MDEntryType:
      FIX::SideT GetTradeAggrSide() const
      {
        switch (m_entryType)
        {
          case FIX::MDEntryTypeT::Bid: return FIX::SideT::Buy;
          case FIX::MDEntryTypeT::Offer: return FIX::SideT::Sell;
          default: assert(false); return FIX::SideT::UNDEFINED;
        }
      }

      // "GetTradeExecID":
      ExchOrdID GetTradeExecID() const         { return m_execID; }

      // "GetTradeSettlDate":
      // We can assume that crypto-trades are always settled @ T+0, but can ac-
      // tually ingore this request:
      constexpr static int GetTradeSettlDate() { return 0; }

    private:
      //---------------------------------------------------------------------//
      // "MDEntryST" Data Flds:                                              //
      //---------------------------------------------------------------------//
      ExchOrdID         m_execID    {0}; // For Trades only
      FIX::MDEntryTypeT m_entryType {FIX::MDEntryTypeT::UNDEFINED};
      PriceT            m_px        {NaN<double>};
      QtyN              m_qty       {0.0};
    };

    //=======================================================================//
    // "MDMsg" Struct: Top-Level for SnapShots and Trade Batches:            //
    // Template parameters:                                                  //
    // QtyN type - should match connector QtyN                               //
    // FOB - MDMsg symbol (or altSymbol) should match parameter we pass to   //
    // updateOrderBook method                                                //
    // MAX_MD - maximum number of messages                                   //
    //=======================================================================//
    template
    <typename QtyN, EConnector_MktData::FindOrderBookBy FOBB, int MAX_MD>
    struct MDMsg
    {
      using MDEntry  = MDEntryST<QtyN>;
      constexpr static EConnector_MktData::FindOrderBookBy FindOrderBookBy =
          FOBB;
      //---------------------------------------------------------------------//
      // "MDMsg" Data Flds:                                                  //
      //---------------------------------------------------------------------//
      constexpr int static MaxMDEs = MAX_MD;
      SymKey         m_symbol; // It can be symbol or altSymbol
      utxx::time_val m_exchTS; // XXX: Currently for Trades only
      int            m_nEntrs; // Actual number of Entries, <= MaxMDEs
      MDEntry        m_entries[MaxMDEs];

      //---------------------------------------------------------------------//
      // "MDMsg" Default Ctor:                                               //
      //---------------------------------------------------------------------//
      MDMsg()
      : m_symbol (),
        m_exchTS (),
        m_nEntrs (0),
        m_entries()
      {}

      //---------------------------------------------------------------------//
      // Accessors (CBs for EConnector_MktData::UpdateOrderBooks):           //
      //---------------------------------------------------------------------//
      int GetNEntries() const
      {
        assert(0 <= m_nEntrs && m_nEntrs <= MaxMDEs);
        return m_nEntrs;
      }

      // SubscrReqIDs are not used:
      constexpr static OrderID GetMDSubscrID() { return 0; }

      MDEntry const& GetEntry(int a_i) const
      {
        assert(0 <= a_i && a_i < m_nEntrs);
        return m_entries[a_i];
      }

      // NB: Similar to FIX,  "IsLastFragment" should return "true" for TCP:
      constexpr static bool IsLastFragment()  { return true; }

      //---------------------------------------------------------------------//
      // Accessors using both this "MDMsg" and "MDEntry":                    //
      //---------------------------------------------------------------------//
      // "GetSymbol" (actually, Symbol or AltSymbol):
      char const* GetSymbol(MDEntry const&) const
        { return m_symbol.data(); }

      // "GetSecID" : No SecIDs here:
      constexpr static SecID GetSecID  (MDEntry const&) { return 0; }

      // "GetRptSeq": No RptSeqs here:
      constexpr static SeqNum GetRptSeq(MDEntry const&) { return 0; }

      // "GetEventTS": Exchange-Side TimeStamps are currently only supported
      // for Trades; OrderBook updates are 1-sec SnapShots:
      utxx::time_val GetEventTS(MDEntry const&) const   { return m_exchTS; }
    };
  }
  // End namespace H2WS
}
// End namespace MAQUETTE
