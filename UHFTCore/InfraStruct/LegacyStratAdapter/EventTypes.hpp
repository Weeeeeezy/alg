// vim:ts=2:et:sw=2
//===========================================================================//
//                              "EventTypes.hpp":                            //
//                     Msgs from Connectors to Strategies                    //
//===========================================================================//
#pragma once

#include <StrategyAdaptor/EventTypes.h>
//#include <Connectors/AOS.h>
#include <utxx/compiler_hints.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <stdexcept>

// XXX: Searching for a better place for the following declaration:
namespace BIPC = boost::interprocess;

namespace MAQUETTE
{
  //=========================================================================//
  // Fwd Declaration of MktData Classes:                                     //
  //=========================================================================//
  class OrderBookSnapShot;
  class TradeEntry;
  class SecDef;
  class AOS;
  class AOSReq12;
  class AOSReqFill;
  class AOSReqErr;
}

namespace Events
{
  //=========================================================================//
  // "EventT": Event Tag:                                                    //
  //=========================================================================//
  // XXX: In "EventMsg", we encode "EventT" using 4 bits, so the max number of
  // different variants is 16; of them, 14 are currently used (END is not coun-
  // ted) and only 2 remain unallocated:
  //
  enum class EventT
  {
    Undefined         = 0,
    OrderBookUpdate   = 1,
    TradeBookUpdate   = 2,
    SecDefUpdate      = 3,  // Eg ImpliedVol update for options...
    ConnectorStop     = 4,  // Connector has been stopped
    ConnectorStart    = 5,  // Connector is  back on-line
    StratExit         = 6,  // Strategy Exit Flag
    RequestError      = 7,  // Client has submitted an Erroneous Request
    OrderFilled       = 8,  // Order filled    and removed from Active Orders
    OrderPartFilled   = 9,  // Order partially filled and remains Active
    OrderCancelled    = 10, // Order cancelled and removed from Active Orders
    MiscStatusChange  = 11, // Minimal status changes (Exchange confirm...)
    MassCancelReport  = 12,
    END               = 13
  };

  inline char const* ToString(EventT tag)
  {
    switch (tag)
    {
    case EventT::OrderBookUpdate : return "OrderBookUpdate";
    case EventT::TradeBookUpdate : return "TradeBookUpdate";
    case EventT::SecDefUpdate    : return "SecDefUpdate";
    case EventT::ConnectorStop   : return "ConnectorStop";
    case EventT::ConnectorStart  : return "ConnectorStart";
    case EventT::RequestError    : return "RequestError";
    case EventT::OrderFilled     : return "OrderFilled";
    case EventT::OrderPartFilled : return "OrderPartFilled";
    case EventT::OrderCancelled  : return "OrderCancelled";
    case EventT::MiscStatusChange: return "MiscStatusChange";
    case EventT::MassCancelReport: return "MassCancelReport";
    case EventT::StratExit       : return "StratExit";
    default                      : return "Undefined";
    }
  }

  //=========================================================================//
  // "EventMsg": Notification Msgs for the Clients:                          //
  //=========================================================================//
  // "EventMsg" is actually a 64-bit value with the following layout:
  // bit 63..60: Tag
  // bit 59..56: ClientRef
  // bit 55..48: RESERVED
  // bit 47.. 0: MktData*, AOS*, AOS::ReqErr*, AOSReqFill*
  //
  class EventMsg
  {
  protected:
    //-----------------------------------------------------------------------//
    // Contains just a 64-bit encoded value:                                 //
    //-----------------------------------------------------------------------//
    static_assert(sizeof(void*) == sizeof(unsigned long) && sizeof(void*) == 8,
                  "Events::EventMsg: Ptr and Long sizes must be 64 bit");
    unsigned long m_val;

    //-----------------------------------------------------------------------//
    // "SetTag":                                                             //
    //-----------------------------------------------------------------------//
    // Tags currently fit into 4 bits. They are installed in the highest bits
    // of "m_val" (which would not interfere with OrderID or Ptr values):
    //
    void SetTag(EventT tag)
    {
      unsigned long ti = (unsigned long)(tag);
      assert(ti < 16);
      ti    <<=   60;  // 4 bits used, 60 zero bits
      m_val  &=   0x0fffffffffffffff;
      m_val  |=   ti;
    }

    //-----------------------------------------------------------------------//
    // "GetPtr":                                                             //
    //-----------------------------------------------------------------------//
    // Extract an untyped ptr. Under ABI-64, bit 47 is to be propagated to the
    // higher bits:
    void* GetPtr() const
    {
      // Most likely, bit 47 will be clear, but check it anyway:
      unsigned long pi = m_val & 0x0000ffffffffffff;
      if (utxx::unlikely(   pi & 0x0000800000000000))
        pi |= 0xffff000000000000;
      // Cast it into a ptr:
      return (void*)(pi);
    }

  public:
    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    EventMsg(): m_val(0) {}

    //-----------------------------------------------------------------------//
    // "SetClientRef":                                                       //
    //-----------------------------------------------------------------------//
    // "ClientRef" is the Connector number from the Client's point of view, so
    // it is limited to the total number of Connectors used by that Client (ty-
    // pically 2--3). So XXX we impose the upper limit of 16 here,  and  use 4
    // bits for this number:
    //
    void SetClientRef(int ref)
    {
      if (utxx::unlikely(ref < 0 || ref >= 16))
        throw std::invalid_argument("EventMsg::SetClientRef: Must be in 0..15");

      // Shift it by 52 bit (so it is placed right on top of the 48-bit content
      unsigned long ri = (unsigned long)(ref);
      ri   <<= 56;
      m_val &= 0xf0ffffffffffffff;
      m_val |= ri;
    }

    //=======================================================================//
    // Type-Specific Ctors:                                                  //
    //=======================================================================//
    // NB: They do NOT install the "Ref" because the latter is typically inser-
    // ted when the msg is sent to the Client (see "InstallClientRef"):
    //-----------------------------------------------------------------------//
    // (1) No args, type tag only:                                           //
    //-----------------------------------------------------------------------//
    EventMsg(EventT tag)
    : m_val(0)
    {
      // This is applicable to some tags only:
      if (utxx::unlikely(tag != EventT::ConnectorStop  &&
                         tag != EventT::ConnectorStart &&
                         tag != EventT::StratExit))
        throw std::invalid_argument
              ("EventMsg::Ctor(TagOnly): InAppropriate Tag=" +
               std::string(ToString(tag)));
      SetTag(tag);
    }

    //-----------------------------------------------------------------------//
    // (2) Sending a ShM Ptr (eg on OrderBook, TradeBook or SecDef update):  //
    //-----------------------------------------------------------------------//
    // These Ctors are for communicating MarketData-related Events:
    //
    EventMsg(MAQUETTE::OrderBookSnapShot const* ptr)
    : m_val((unsigned long)(ptr))
    {
      assert(ptr != NULL);
      SetTag(EventT::OrderBookUpdate);
    }

    EventMsg(MAQUETTE::TradeEntry const* ptr)
    : m_val((unsigned long)(ptr))
    {
      assert(ptr != NULL);
      SetTag(EventT::TradeBookUpdate);
    }

    EventMsg(MAQUETTE::SecDef const* ptr)
    : m_val((unsigned long)(ptr))
    {
      assert(ptr != NULL);
      SetTag(EventT::SecDefUpdate);
    }

    //-----------------------------------------------------------------------//
    // (3) Sending an AOS Ptr:
    //-----------------------------------------------------------------------//
    // XXX: Or should we send more specific info in those cases, eg a ptr to a
    // specific Request which caused Cancellation? But this is not always pos-
    // sible (eg not in case when Cancel was caused by a MassCancelRequest):
    //
    EventMsg(EventT tag, MAQUETTE::AOS const* aos)
    : m_val((unsigned long)(aos))
    {
      // Check that the "tag" is valid for sending an AOS:
      if (tag != EventT::OrderCancelled   &&
          tag != EventT::MassCancelReport)
        throw std::invalid_argument
              ("EventMsg::Ctor(AOS*): InAppropriate Tag=" +
               std::string(ToString(tag)));
      SetTag(tag);
    }

    //-----------------------------------------------------------------------//
    // (4) Sending an "AOSReq12" Ptr:                                        //
    //-----------------------------------------------------------------------//
    // The EventType must be "MiscStatusChange", ie "Acknowledge" or "Confirm"
    // (the actual status is recorded inside the "AOSReq12" in question):
    //
    EventMsg(MAQUETTE::AOSReq12 const* req)
    : m_val((unsigned long)(req))
    {
      SetTag(EventT::MiscStatusChange);
    }

    //-----------------------------------------------------------------------//
    // (5) Sending an "AOSReqFill" Ptr:                                      //
    //-----------------------------------------------------------------------//
    EventMsg(EventT tag, MAQUETTE::AOSReqFill const* rf)
    : m_val((unsigned long)(rf))
    {
      if (tag != EventT::OrderFilled   &&
          tag != EventT::OrderPartFilled)
        throw std::invalid_argument
              ("EventMsg::Ctor(AOSReqFill*): InAppropriate Tag=" +
               std::string(ToString(tag)));
      SetTag(tag);
    }

    //-----------------------------------------------------------------------//
    // (6) Sending an Error Msg:                                             //
    //-----------------------------------------------------------------------//
    // Actually, we send a ptr to that msg within the corresp AOS:
    //
    EventMsg(MAQUETTE::AOSReqErr const* re)
    : m_val((unsigned long)(re))
    {
      // The Tag is "RequestError":
      SetTag(EventT::RequestError);
    }

    //=======================================================================//
    // Extracting Data from "EventMsg":                                      //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "GetTag":                                                             //
    //-----------------------------------------------------------------------//
    EventT GetTag() const
    {
      // Tag is in bits 63..60. The max allowed value is currently 13:
      unsigned long ti = m_val >> 60;
      if (utxx::unlikely(ti == 0 || ti >= (unsigned long)(EventT::END)))
        throw std::invalid_argument
              ("EventMsg::GetTag: Invalid Tag Value: "+ std::to_string(ti));
      return EventT(ti);
    }

    //-----------------------------------------------------------------------//
    // "GetClientRef":                                                       //
    //-----------------------------------------------------------------------//
    int GetClientRef() const
    {
      // ClientRef is in bits 60..56:
      return int((m_val >> 56) & 0x0f);
    }

    //-----------------------------------------------------------------------//
    // "GetOrderBookPtr":                                                    //
    //-----------------------------------------------------------------------//
    MAQUETTE::OrderBookSnapShot const* GetOrderBookPtr() const
    {
      // Tag must be "OrderBookUpdate":
      int ti = int(m_val >> 60);
      return (utxx::likely(ti == int(EventT::OrderBookUpdate)))
           ? (MAQUETTE::OrderBookSnapShot const*)(GetPtr())
           : NULL;
    }

    //-----------------------------------------------------------------------//
    // "GetTradePtr":                                                        //
    //-----------------------------------------------------------------------//
    MAQUETTE::TradeEntry const* GetTradePtr() const
    {
      // Tag must be "TradeBookUpdate":
      int ti = int(m_val >> 60);
      return (utxx::likely(ti == int(EventT::TradeBookUpdate)))
           ? (MAQUETTE::TradeEntry const*)(GetPtr())
           : NULL;
    }

    //-----------------------------------------------------------------------//
    // "GetSecDefPtr":                                                       //
    //-----------------------------------------------------------------------//
    MAQUETTE::SecDef const* GetSecDefPtr() const
    {
      // Tag must be "SecDefUpdate":
      int ti = int(m_val >> 60);
      return (utxx::likely(ti == int(EventT::SecDefUpdate)))
           ? (MAQUETTE::SecDef const*)(GetPtr())
           : NULL;
    }

    //-----------------------------------------------------------------------//
    // "GetAOSPtr":                                                          //
    //-----------------------------------------------------------------------//
    MAQUETTE::AOS* GetAOSPtr() const
    {
      // Tag must be one of the following. In these cases, there is no indivi-
      // dual "Req" -- only the whole AOS makes sense:
      //
      int ti = int(m_val >> 60);
      return (utxx::likely(ti == int(EventT::OrderCancelled)  ||
                           ti == int(EventT::MassCancelReport)))
           ? (MAQUETTE::AOS*)(GetPtr())
           : NULL;
    }

    //-----------------------------------------------------------------------//
    // "GetReqPtr":                                                          //
    //-----------------------------------------------------------------------//
    MAQUETTE::AOSReq12* GetReqPtr() const
    {
      // Tag must be "MiscStatusChange":
      int ti = int(m_val >> 60);
      return (utxx::likely(ti == int(EventT::MiscStatusChange)))
           ? (MAQUETTE::AOSReq12*)(GetPtr())
           : NULL;
    }

    //-----------------------------------------------------------------------//
    // "GetReqFillPtr":                                                      //
    //-----------------------------------------------------------------------//
    MAQUETTE::AOSReqFill* GetReqFillPtr() const
    {
      // Tag must be "OrderFilled" or "OrderPartFilled":
      int ti = int(m_val >> 60);
      return (utxx::likely(ti == int(EventT::OrderFilled)   ||
                           ti == int(EventT::OrderPartFilled)))
           ? (MAQUETTE::AOSReqFill*)(GetPtr())
           : NULL;
    }

    //-----------------------------------------------------------------------//
    // "GetReqErrPtr":                                                       //
    //-----------------------------------------------------------------------//
    // This is only applicable to error msgs:
    //
    MAQUETTE::AOSReqErr* GetReqErrPtr() const
    {
      int ti = int(m_val >> 60);
      return (utxx::likely(ti == int(EventT::RequestError)))
           ? (MAQUETTE::AOSReqErr*)(GetPtr())
           : NULL;
    }
  };
}