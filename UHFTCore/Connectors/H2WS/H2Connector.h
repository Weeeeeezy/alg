// vim:ts=2:et
//===========================================================================//
//                      "Connectors/H2WS/H2Connector.h":                     //
//                    HTTP/2 Protocol Engine and Connector                   //
//===========================================================================//
// NB:  However, similar to "TCP_Connector", it is NOT an "EConnector" yet!
// XXX: It belongds to both "Connectors" and "Protocols" trees:
//
#pragma once

#include "Basis/BaseTypes.hpp"
#include "Connectors/TCP_Connector.h"
#include <utxx/time_val.hpp>
#include <spdlog/spdlog.h>
#include <boost/property_tree/ptree.hpp>
#include <nghttp2/nghttp2.h>
#include <cstdlib>
#include <cstdint>  // For int types
#include <cstddef>  // For "offsetof"

namespace MAQUETTE
{
  //=========================================================================//
  // "H2Connector" Struct:                                                   //
  //=========================================================================//
  // NB:
  // (*) It can be part of an OMC or an MDC, as specified by the "IsOMC" param;
  // (*) "PSM" is a "Processor-SessionMgr" class (typically an "EConnector*")
  //     which provides the following method:
  //
  // void ProcessH2Msg
  // (
  //   uint32_t       a_atream_id,
  //   char const*    a_msg,
  //   int            a_msg_len,
  //   utxx::time_val a_ts_recv,
  //   utxx::time_val a_ts_handl
  // );
  //
  // XXX: We declare it a "struct" since various "EConnector*" classes must
  // have full access to its internals, and declaring multiple "friend"s is
  // impractical:

  template<bool IsOMC, typename PSM>
  struct H2Connector:  public TCP_Connector<H2Connector<IsOMC, PSM>>
  {
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // HTTP/2 Frame Types:                                                   //
    //-----------------------------------------------------------------------//
    enum class H2FrameTypeT: uint8_t
    {
      Data         = '\x00',
      Headers      = '\x01',
      Priority     = '\x02',
      RstStream    = '\x03',
      Settings     = '\x04',
      PushPromise  = '\x05',
      Ping         = '\x06',
      GoAway       = '\x07',  // Disconnect
      WindowUpdate = '\x08',  // Flow Ctl
      Continuation = '\x09',  // Extended Headers
      AltSVC       = '\x0A',  // Obscure extention
      Origin       = '\x0C'   // Similar
    };

    //-----------------------------------------------------------------------//
    // HTTP/2 Frame:                                                         //
    //-----------------------------------------------------------------------//
    template<unsigned MaxDataLen>
    struct H2Frame
    {
      // Data Flds:
      uint8_t      m_dataLen[3];  // In Network Byte Order
      H2FrameTypeT m_type;
      uint8_t      m_flags;
      uint32_t     m_streamID;    // In Network Byte Order, highest bit = 0
      char         m_data[MaxDataLen];

      // Default Ctor:
      H2Frame()  { memset(this, '\0', sizeof(H2Frame)); }

      // Accessors:
      int        GetDataLen () const; // ActualData
      int        GetFrameLen() const; // Prefix + ActualData
      unsigned   GetStreamID() const;
      constexpr static int  PrefixLen = offsetof(H2Frame, m_data);     // 9

      // Setting the actual Data(PayLoad)Length:
      void SetDataLen (int a_len);

      // Setting the StreamID from  a  ReqID:
      void SetStreamID(uint32_t a_stream_id);

      // Protocol-level logging of H2 Frames, in Hex, as opposed to "LogMsg"
      // which logs the Payload expected to be ASCII:
      template<bool IsSend = true>
      void LogFrameHex(spdlog::logger* a_proto_logger) const;
    }
    __attribute__((packed));

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    // NB: The following ptrs are NOT owned -- they are just aliases to PSM and
    // objs referenced via the PSM (as required by "TCP_Connector"):
    PSM*            const                   m_psm;
    std::string     const                   m_name;
    EPollReactor*   const                   m_reactor;
    int             const                   m_debugLevel;
    spdlog::logger* const                   m_logger;

    // Compressor (Deflater) and Decompressor (Inflater) for HTTP/2 Headers.
    // The ptrs ARE OWNED:
    nghttp2_hd_deflater*                    m_deflater;
    nghttp2_hd_inflater*                    m_inflater;

    // Raw Headers to be put here (before Deflation or after Inflation):
    constexpr static int MaxOutHdrs         = 16;
    mutable nghttp2_nv                      m_outHdrs[MaxOutHdrs];

    constexpr static int MaxInHdrs          = 128;
    mutable nghttp2_nv                      m_inHdrs [MaxInHdrs];

    // The total number of pre-defined OutHdrs (client classes can add more):
    // Currently 4: ":authority", ":scheme", ":method" and ":path":
    //
    constexpr static int NBasicOutHdrs      = 4;

    // Out-Bound Frames (Headers and Data) are assembled here. In both cases,
    // 1K is probably enough for H2-based OMC:
    //
    constexpr static int MaxOutHdrsDataLen  = 512;
    mutable H2Frame<MaxOutHdrsDataLen>      m_outHdrsFrame;

    constexpr static int MaxOutDataDataLen  = 1024;
    mutable H2Frame<MaxOutDataDataLen>      m_outDataFrame;

    // Inbound data are buffered here (if they are spread across multiple Data
    // Frames): the DataBuff ptr IS OWNED!
    constexpr static int MaxDataLen         = 65536 * 4;// 65536;
    mutable char*                           m_dataBuff;
    mutable int                             m_dataBuffLen;
    // Similarly, inbound Headers are buffered here  (until the whole block  is
    // received). According to the HTTP/2 spec, Continuation Frames must NOT be
    // interleaved with Frames of other Streams/Types,   so we memoise the curr
    // stream ID here as well:
    constexpr static int MaxHdrsLen         = 65536;
    mutable uint32_t                        m_contID;
    mutable uint8_t*                        m_hdrsBuff;
    mutable int                             m_hdrsBuffLen;

    // TmpBuff for out-bound signatures etc:
    mutable char                            m_tmpBuff[2048];
    // For header logs
    mutable char                            m_logBuff[4096];

    // StreamIDs: Next Tx and Last Rx:
    mutable uint32_t                        m_nextTxStmID;
    mutable uint32_t                        m_lastRxStmID;

    // Two reciprocal maps, implemented by simple array look-up:
    // ReqID    -> StreamID:
    unsigned const                          m_maxIDs;      // MaxLen of maps
    unsigned const                          m_cushion;     // For StmIDsMap
    mutable  long                           m_baseReqID;   // ditto
    uint32_t*                               m_stmIDsMap;
    // StreamID -> ReqID (InitStmID = 1):
    uint32_t*                               m_reqOffsMap;

  public:
    //-----------------------------------------------------------------------//
    // The Parent Class:                                                     //
    //-----------------------------------------------------------------------//
    using TCPH2 = TCP_Connector<H2Connector<IsOMC, PSM>>;

    //=======================================================================//
    // Ctors, Dtor:                                                          //
    //=======================================================================//
    // Default Ctor is deleted:
    H2Connector() = delete;

    // Non-Default Ctor. NB: ClientIP is made an explicit arg, rather than ent-
    // ries of Params, for ease of use in "EConnector_H2WS*":
    H2Connector
    (
      PSM*                                a_psm,       // Must be non-NULL
      boost::property_tree::ptree const&  a_params,
      uint32_t                            a_max_reqs,
      std::string                 const&  a_client_ip  = ""
    );

    // Dtor (not "virtual" because there are no virtual methods here):
    ~H2Connector() noexcept;

    //=======================================================================//
    // HTTP/2 Protocol Handling:                                             //
    //=======================================================================//
    // The methods below can be used as Call-Backs by "TCP_Connector":
    //-----------------------------------------------------------------------//
    // "ReadHandler":                                                        //
    //-----------------------------------------------------------------------//
    // Returns the number of bytes "consumed" (ie successfully read and process-
    // ed, in particular 0 if the msg was incomplete),  or (-1) if there was an
    // error:
    int ReadHandler
    (
      int                  a_fd,
      char const*          a_buff,  // Points  to the dynamic buffer of Reactor
      int                  a_size,  // Size of the data chunk available in Buff
      utxx::time_val       a_ts_recv
    );

    // Print Header frame
    void ProcessHeaderFrame
    (
      uint32_t             a_req_id,
      spdlog::logger*      a_proto_logger,
      nghttp2_hd_inflater* a_hd_inflater,
      uint8_t*             a_buff,
      size_t               a_len,
      utxx::time_val       a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // HTTP/2 Session Mgmt (other "TCP_Connector" Call-Backs):               //
    //-----------------------------------------------------------------------//
    //  "InitSession":
    //  In HTTP/2, sends the very initial "PRISM" msg to the server:
    void InitSession();

    //  "InitLogOn": Normally a NoOp for H2, but still delegated to the "PSM":
    void InitLogOn();

    // "SendHeartBeat":
    // Typically required in order to avoid connection time-outs in HTTP/2.
    // Will send a "Ping" msg when invoked periodically:
    //
    template<bool IsAck = false>
    void SendHeartBeat() const;

    //  "ServerInactivityTimeOut":
    void ServerInactivityTimeOut(utxx::time_val a_ts_recv);

    //  "InitGracefulStop": Currently a NoOp, actually:
    void InitGracefulStop()       const;

    //  "GracefulStopInProgress": Eg send a "GoAway" Msg to the Server:
    void GracefulStopInProgress() const;

    //  "StopNow": Internally invokes "TCPH2::DisConnect":
    template<bool FullStop>
    void StopNow(char const* a_where, utxx::time_val a_ts_recv);

    // "Update{Tx,Rx}Stats":
    void UpdateTxStats(int a_len,     utxx::time_val a_ts) const;
    void UpdateRxStats(int a_len,     utxx::time_val a_ts) const;

    //=======================================================================//
    // "Start", "Stop", "IsActive" are inherited from "TCP_Connector"        //
    //=======================================================================//
    //=======================================================================//
    // Helpers:                                                              //
    //=======================================================================//
    // "SendFrame": Sends out an HTTP/2 Frame (actual sending occurs via the
    // mechanisms provided by the "PSM"):
    //
    template<bool  IsData>
    utxx::time_val SendFrame () const;

    void LogSettings(char const* a_data, int a_setts_len) const;

    // ReqID <-> StreamID conversion:
    uint32_t StreamIDofReqID (OrderID  a_req_id)    const;
    OrderID  ReqIDofStreamID (uint32_t a_stream_id) const;
  };
} // End namespace MAQUETTE
