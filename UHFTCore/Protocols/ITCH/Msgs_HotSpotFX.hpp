// vim:ts=2:et
//===========================================================================//
//                   "Protocols/ITCH/Msgs_HotSpotFX.hpp":                    //
//                 ITCH Msg Types and Parsers for HotSpotFX                  //
//===========================================================================//
// XXX: Currently, this Protocol Engine supports the Connector (Client) Side
// only:
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Protocols/ITCH/Features.h"
#include <utxx/config.h>
#include <utxx/enumu.hpp>
#include <utxx/error.hpp>
#include <spdlog/spdlog.h>
#include <cstdlib>

//===========================================================================//
// Macros:                                                                   //
//===========================================================================//
//===========================================================================//
// "GENERATE_SUBSCR_MSG_HSFX":                                               //
//===========================================================================//
#ifdef  GENERATE_SUBSCR_MSG_HSFX
#undef  GENERATE_SUBSCR_MSG_HSFX
#endif
#define GENERATE_SUBSCR_MSG_HSFX(MsgType) \
  struct MsgType \
  { \
    /* Data Flds: */        \
    char    m_MsgType;      \
    char    m_CcyPair[7];   \
    char    m_TLF;          \
    \
    /* Non-Default Ctor: */ \
    MsgType(SymKey const& a_symbol) \
    : m_MsgType(ReqT::MsgType),     \
      m_TLF    ('\n')               \
    { StrCpySP(m_CcyPair, a_symbol.data()); } \
  } \
  __attribute__((packed));

//===========================================================================//
// "GENERATE_0ARG_MSG_HSFX":                                                 //
//===========================================================================//
#ifdef  GENERATE_0ARG_MSG_HSFX
#define GENERATE_0ARG_MSG_HSFX
#endif
#define GENERATE_0ARG_MSG_HSFX(Prefix, MsgType) \
  struct MsgType \
  { \
    /* Data Flds: */        \
    char    m_MsgType;      \
    char    m_TLF;          \
    \
    /* Default Ctor: */     \
    MsgType()               \
    : m_MsgType(Prefix::MsgType), \
      m_TLF    ('\n')       \
    {} \
  } \
  __attribute__((packed));
//===========================================================================//

namespace MAQUETTE
{
namespace ITCH
{
  namespace HotSpotFX
  {
    //=======================================================================//
    // HotSpotFX ITCH Msg Type Tags:                                         //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // Top-Level Server-to-Client Msg Types:                                 //
    //-----------------------------------------------------------------------//
    // XXX: With CLang, there is currently a strange warning -- in the ENUMU
    // below, _END_ clashes with SequenceData!
#   ifdef __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wduplicate-enum"
#   endif
    UTXX_ENUMU(MsgT, (char, '\0'),
      // Session-Level:
      (LogInAccepted,       'A')
      (LogInRejected,       'J')
      (ServerHeartBeat,     'H')
      (ErrorNotification,   'E')
      // (EndOfSession,     'S')  // Recognized dynamically from SequencedData
      // Appl-Level:
      (SequencedData,       'S')  // This is Envelope; Content: see "PayLoadT"
      (InstrsDir,           'R')
    );
#   ifdef __clang__
#   pragma clang diagnostic pop
#   endif

    //-----------------------------------------------------------------------//
    // Server-to-Client PayLoad Msg Types (encapsulated in "SequencedData"): //
    //-----------------------------------------------------------------------//
    UTXX_ENUMU(PayLoadT, (char, '\0'),
      (NewOrder,            'N')
      (ModifyOrder,         'M')
      (CancelOrder,         'X')
      (MktSnapShot,         'S')
      (Ticker,              'T')
    );

    //-----------------------------------------------------------------------//
    // Client-to-Server Msgs (Requests):                                     //
    //-----------------------------------------------------------------------//
    UTXX_ENUMU(ReqT, (char, '\0'),
      (LogInReq,            'L')
      (ClientHeartBeat,     'R')
      (MktDataSubscrReq,    'A')
      (MktDataUnSubscrReq,  'B')
      (MktSnapShotReq,      'M')
      (TickerSubscrReq,     'T')
      (TickerUnSubscrReq,   'U')
      (InstrsDirReq,        'I')
      (LogOutReq,           'O')
    );

    //=======================================================================//
    // Utils for Msg Generation and Parsing:                                 //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "GetOrderSide": Returns "true" for Bid, "false" for Ask:              //
    //-----------------------------------------------------------------------//
    inline bool GetOrderSide(char a_s)
    {
      return
        (a_s == 'B') ? true : (a_s == 'S') ? false :
        throw utxx::runtime_error("GetOrderSide: Invalid Side=", a_s);
    }

    //-----------------------------------------------------------------------//
    // "GetInt":                                                             //
    //-----------------------------------------------------------------------//
    // From a byte array (N is automatically inferred):
    template<typename T, int N>
    inline T GetInt(char const (&a_fld)[N])
    {
      T res = 0;
      // Skip optional spaces on the right:
      (void) utxx::atoi_right<T, N, char>
        (static_cast<char const*>(a_fld), res, ' ');
      return  res;
    }

    // Similar, from a buff ptr (N must be specified explicitly):
    template<typename T, int N>
    inline T GetInt(char const* a_buff)
    {
      assert(a_buff != nullptr);
      T res = 0;
      // Skip optional spaces on the right:
      (void) utxx::atoi_right<T, N, char>(a_buff, res, ' ');
      return  res;
    }

    //-----------------------------------------------------------------------//
    // "GetOrderQty":                                                        //
    //-----------------------------------------------------------------------//
    // NB: Similar to FIX (and unlike FAST), the return type here  comes from
    // the ProtocolFeatures / Dialect configured separately, rather than from
    // the Msg Layer itself.  Thus "GetOrderQty" needs to be templated:
    //
    // (1) From a buff ptr (N must be specified explicitly):
    //
    template<QtyTypeT QT,  typename QR,  int N>
    inline Qty<QT,QR> GetOrderQty(char const* a_buff)
    {
      assert(a_buff != nullptr);
      return
        std::is_floating_point_v<QR>
        ? // Yes, fractional:
          Qty<QT,QR>(strtod(a_buff, nullptr))
        : // Non-fractional: read an integer until the ' ' or the end of fld:
          Qty<QT,QR>(GetInt<long, N>(a_buff));
    }

    // (2) From a byte array (N is automatically inferred):
    //
    template<QtyTypeT QT,  typename QR,  int N>
    inline Qty<QT,QR> GetOrderQty(char const (&a_fld)[N])
      { return GetOrderQty<QT,QR,N>(static_cast<char const*>(a_fld)); }

    //-----------------------------------------------------------------------//
    // "GetOrderPx":                                                         //
    //-----------------------------------------------------------------------//
    // From a buff ptr (N must be specified explicitly):
    template<int N>
    inline PriceT  GetOrderPx (char const* a_buff)
    {
      assert(a_buff != nullptr);
      return PriceT(strtod(a_buff, nullptr));
    }

    // From a byte array (N is automatically inferred):
    template<int N>
    inline PriceT  GetOrderPx (char const (&a_fld)[N])
      { return GetOrderPx <N> (static_cast<char const*>(a_fld)); }

    //-----------------------------------------------------------------------//
    // "StrCpySP":                                                           //
    //-----------------------------------------------------------------------//
    // Copy the Src string into the Targ, set any remaining bytes to ' '  ("SP
    // is "SpacePadding"):
    //
    template<int N>
    inline void StrCpySP(char (&a_targ)[N], std::string const& a_src)
    {
      // Check the sizes:
      size_t len = a_src.size();
      if (utxx::unlikely(len > size_t(N)))
        throw utxx::runtime_error("StrCpySP: Targ size too small");

      // Copy the data over:
      char*  end = stpncpy(a_targ, a_src.data(), len);

      // Install the trailing spaces:
      int    nsp = int((a_targ + N) - end);
      assert(nsp >= 0);

      (void) memset(end, ' ', size_t(nsp));
    }

    //-----------------------------------------------------------------------//
    // "ToCStr": A Quick-and-Dirty way of Zero-Terminating a String:         //
    //-----------------------------------------------------------------------//
    template<int N>
    inline char const* ToCStr(char const(&a_str)[N])
    {
      // XXX: Skip trailing ' 's, replace the last one by '\0'. If there are no
      // trailing spaces at all, will replace the 1st char BEYOND this str  (it
      // should  be '\n' or '\0' -- if '\n' was already replaced by '\0' in the
      // Logger) by '\0':
      char*  cstr    =  const_cast<char*>(a_str);
      assert(cstr[N] == '\n' || cstr[N] == '\0');

      for (int i = N; i > 0; --i)
        if (cstr[i-1] != ' ')
        {
          cstr[i] = '\0';
          return cstr;
        }
      // If we are still here: it will be an empty string:
      cstr[0] = '\0';
      return cstr;
    }

    //-----------------------------------------------------------------------//
    // "MkUniqOrderID":                                                      //
    //-----------------------------------------------------------------------//
    // In HotSpotFX, "OrderID"s are per-CcyPair, whereas "EConnector_MktData"
    // expects them to be over-all global. So modify them before passing them
    // to the generic "EConnector_MktData" layer:
    //
    inline OrderID MkUniqOrderID(OrderID a_orig_id, SymKey const& a_symbol)
    {
      // XXX: Because the HotSpotFX Symbols are always 7-byte long (plus the
      // terminating '\0'), we simply cast them into numbers and add them to
      // the OrigOrderIDs (Can it give rise to large number of collisions?--
      // probably not):
      OrderID fromSym = *(reinterpret_cast<OrderID const*>(a_symbol.data()));
      return  fromSym + a_orig_id;
    }

    //-----------------------------------------------------------------------//
    // "LogMsg":                                                             //
    //-----------------------------------------------------------------------//
    // In HotSpotFX, all msgs are in the ASCII format: can be output directly:
    //
    template<bool IsSend>
    inline void LogMsg
      (spdlog::logger* a_itch_logger, char const* a_data, int a_len)
    {
      assert(a_itch_logger != nullptr && a_data != nullptr && a_len > 0);

      // XXX: A DIRTY HACK: We KNOW that for HotSpotFX, this msg will no longer
      // be needed in its original format, so we can install a '\0' at the end,
      // instead of '\n'. This is why we CANNOT asset anymore that there is an
      // '\n' at the end:
      (const_cast<char*> (a_data))[a_len-1] = '\0';

      // We can now log it:
      a_itch_logger->info("{} {}", IsSend ? "-->" : "<==", a_data);
    }

    //=======================================================================//
    // Session-Level Server-to-Client HotSpotFX Msgs:                        //
    //=======================================================================//
    //=======================================================================//
    // "LogInAccepted":                                                      //
    //=======================================================================//
    struct LogInAccepted
    {
      char    m_MsgType;          // MsgT::LogInAccepted ('A')
      char    m_SeqNum[10];       // XXX: Unused, currenly always 1
      char    m_TLF;              // '\n'

      // XXX: No need for Non-Default Ctor: Not generated at Client Side
    }
    __attribute__((packed));

    //=======================================================================//
    // "LogInRejected":                                                      //
    //=======================================================================//
    struct LogInRejected
    {
      char    m_MsgType;          // MsgT::LogInRejected ('R')
      char    m_Reason[20];
      char    m_TLF;              // '\n'

      // XXX: No need for Non-Default Ctor: Not generated at Client Side
    }
    __attribute__((packed));

    //=======================================================================//
    // "ErrorNotification":                                                  //
    //=======================================================================//
    struct ErrorNotification
    {
      char    m_MsgType;          // MsgT::ErrorNotification ('E')
      char    m_ErrExplanation[100];
      char    m_TLF;              // '\n'

      // XXX: No need for Non-Default Ctor: Not generated at Client Side
    }
    __attribute__((packed));

    //=======================================================================//
    // Session-Level Client-to-Server Msgs:                                  //
    //=======================================================================//
    //=======================================================================//
    // "LogInReq":                                                           //
    //=======================================================================//
    struct LogInReq
    {
      // Data Flds:
      char    m_MsgType;          // ReqT::LogInReq ('L')
      char    m_LogInName[40];
      char    m_Password [40];
      char    m_MktDataUnSubscr;  // 'T' or 'F'
      char    m_Reserved [9];     // Some int
      char    m_TLF;              // '\n'

      // Non-Default Ctor:
      LogInReq(std::string const& a_user_name, std::string const& a_passwd)
      : m_MsgType         (ReqT::LogInReq),
        m_MktDataUnSubscr ('T'),  // Normally terminate existing subscriptions
        m_TLF             ('\n')
      {
        StrCpySP      (m_LogInName, a_user_name);
        StrCpySP      (m_Password,  a_passwd   );
        StrNCpy<false>(m_Reserved,  "        0");
        assert(sizeof (m_Reserved)  == 9);
      }
    }
    __attribute__((packed));

    //=======================================================================//
    // 0-Arg Msgs:                                                           //
    //=======================================================================//
    // Server-to-Client:
    GENERATE_0ARG_MSG_HSFX(MsgT, ServerHeartBeat)

    // Client-to-Server:
    GENERATE_0ARG_MSG_HSFX(ReqT, LogOutReq)
    GENERATE_0ARG_MSG_HSFX(ReqT, ClientHeartBeat)
    GENERATE_0ARG_MSG_HSFX(ReqT, InstrsDirReq)

    //=======================================================================//
    // Application-Level Client-to-Server Msgs:                              //
    //=======================================================================//
    // Subscription Mgmt Msgs are in fact all identical(!):
    //
    GENERATE_SUBSCR_MSG_HSFX(MktDataSubscrReq)
    GENERATE_SUBSCR_MSG_HSFX(MktDataUnSubscrReq)
    GENERATE_SUBSCR_MSG_HSFX(MktSnapShotReq)
    GENERATE_SUBSCR_MSG_HSFX(TickerSubscrReq)
    GENERATE_SUBSCR_MSG_HSFX(TickerUnSubscrReq)

    //=======================================================================//
    // Application-Level Server-to-Client Msgs:                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "InstrsDir": XXX: Has a variable layout:                              //
    //-----------------------------------------------------------------------//
    struct InstrsDir
    {
      char    m_MsgType;          // MsgT::InstrsDir ('R')
      char    m_NCcyPairs[4];
      char    m_Var      [0];     // XXX: The rest is unspecified; its length is
    }                             // (7 * NCcyPairs + 1)
    __attribute__((packed));

    //-----------------------------------------------------------------------//
    // "SequencedDataHdr":                                                   //
    //-----------------------------------------------------------------------//
    // "NewOrder", "ModifyOrder" and "CancelOrder" are payloads encapsulated in
    // "SequencedData":
    //
    struct SequencedDataHdr
    {
      char    m_MsgType;          // MsgT::SequencedData ('S')
      char    m_Time     [9];     // HHMMSSsss
      char    m_PayLoadType;
    }
    __attribute__((packed));

    //-----------------------------------------------------------------------//
    // "NewOrder": Is a Payload encapsulated within "SequencedData":         //
    //-----------------------------------------------------------------------//
    struct NewOrder: public SequencedDataHdr
    {
      // PayLoadType: 'N':
      char    m_Side;             // 'B' or 'S'
      char    m_CcyPair  [7];     // Instrument
      char    m_OrderID  [15];
      char    m_Price    [10];
      char    m_Qty      [16];
      // XXX: MinQty[16] and LotSz[16] optionally come here, but skipped anyway!
      char    m_TLF;              // '\n'
    }
    __attribute__((packed));

    //-----------------------------------------------------------------------//
    // "ModifyOrder":                                                        //
    //-----------------------------------------------------------------------//
    struct ModifyOrder: public SequencedDataHdr
    {
      // PayLoadType: 'M':
      char    m_CcyPair  [7];     // Instrument
      char    m_OrderID  [15];
      char    m_Qty      [16];
      // XXX: MinQty[16] and LotSz[16] optionally come here, but skipped anyway!
      char    m_TLF;              // '\n'
    }
    __attribute__((packed));

    //-----------------------------------------------------------------------//
    // "CancelOrder":                                                        //
    //-----------------------------------------------------------------------//
    struct CancelOrder: public SequencedDataHdr
    {
      // PayLoadType: 'X':
      char    m_CcyPair  [7];     // Instrument
      char    m_OrderID  [15];
      char    m_TLF;
    }
    __attribute__((packed));

    //-----------------------------------------------------------------------//
    // "MktSnapShot": XXX: Has a variable layout:                            //
    //-----------------------------------------------------------------------//
    struct MktSnapShot: public SequencedDataHdr
    {
      // PayLoadType: 'S':
      char    m_Length   [6];     // Length of the rest of this msg (Var part)
      char    m_Var      [0];     // XXX: The rest is unspecified...
    }
    __attribute__((packed));

    //-----------------------------------------------------------------------//
    // "Ticker" (Trade; XXX: In HotSpotFX these msgs are distorted):         //
    //-----------------------------------------------------------------------//
    struct Ticker: public SequencedDataHdr
    {
      // PayLoadType: 'T':
      char    m_Aggressor;        // 'B' is Buyer, 'S' if Seller
      char    m_CcyPair  [7];     // Instrument
      char    m_Price    [10];
      char    m_TransDate[8];     // YYYYMMDD
      char    m_TransTime[6];     // HHMMSS
      char    m_TLF;
    }
    __attribute__((packed));
  }
  // End namespace HotSpotFX
} // End namespace ITCH
} // End namespace MAQUETTE
