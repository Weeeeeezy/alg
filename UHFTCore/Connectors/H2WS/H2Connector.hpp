// vim:ts=2:et
//===========================================================================//
//                     "Connectors/H2WS/H2Connector.hpp":                    //
//                    HTTP/2 Protocol Engine and Connector                   //
//===========================================================================//
#pragma once

#include "Basis/Maths.hpp"
#include "Connectors/H2WS/H2Connector.h"
#include "Connectors/TCP_Connector.hpp"
#include "Basis/IOUtils.hpp"
#include "Basis/OrdMgmtTypes.hpp"
#include "utxx/error.hpp"
#include <arpa/inet.h>   // For "ntoh" etc
#include <cstdlib>

namespace MAQUETTE
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template<bool IsOMC, typename PSM>
  H2Connector  <IsOMC, PSM>::H2Connector
  (
    PSM*                                a_psm,         // Must be non-NULL
    boost::property_tree::ptree const&  a_params,
    unsigned                            a_max_reqs,
    std::string                 const&  a_client_ip
  )
  : //-----------------------------------------------------------------------//
    // "TCPH2" Ctor:                                                         //
    //-----------------------------------------------------------------------//
    TCPH2
    (
      a_psm->m_name + "-H2",            // NB: Available at this point!
      GetH2WSIP(PSM::GetH2Config(a_psm->m_cenv), a_params),
                                        // XXX: May use a Sync DNS Look-Up!
      PSM::GetH2Config(a_psm->m_cenv).m_Port,     // HTTPS port (eg 443)
      16384 * 1024,                     // BuffSz  = 16M (hard-wired)
      65536,                            // BuffLWM = 64k (hard-wired)
      a_client_ip,                      // Passed to TCPH2
      a_params.get<int>        ("MaxConnAttempts", 5),
      10,                               // LogOn  TimeOut, Sec (hard-wired)
      1,                                // LogOut TimeOut, Sec (hard-wired)
      1,                                // Re-Connect Interval, Sec (similar)
      30,                               // PINGs every 30 sec
      a_params.get<bool>       ("UseTLS", true),
      PSM::GetH2Config(a_psm->m_cenv).m_HostName, // Required for TLS
      IO::TLS_ALPN::HTTP2,              // Yes, this is HTTP/2!
      a_params.get<bool>       ("VerifyServerCert", true),
      a_params.get<std::string>("ServerCAFiles",     ""),
      a_params.get<std::string>("ClientCertFile",    ""),
      a_params.get<std::string>("ClientPrivKeyFile", ""),
      a_params.get<std::string>("ProtoLogFile",      "")
    ),
    //-----------------------------------------------------------------------//
    // "H2Connector" Itself:                                                 //
    //-----------------------------------------------------------------------//
    m_psm          (a_psm),
    m_name         (m_psm->m_name),
    m_reactor      (m_psm->m_reactor),
    m_debugLevel   (m_psm->m_debugLevel),
    m_logger       (m_psm->m_logger),
    m_deflater     (nullptr),  // NULL as yet: Created for each new Start
    m_inflater     (nullptr),  // ditto
    m_outHdrs      (),
    m_inHdrs       (),
    m_outHdrsFrame (),
    m_outDataFrame (),
    m_dataBuff     (new char   [MaxDataLen]),
    m_dataBuffLen  (0),
    m_contID       (0),
    m_hdrsBuff     (new uint8_t[MaxHdrsLen]),
    m_hdrsBuffLen  (0),
    m_tmpBuff      (),
    m_logBuff      (),
    m_nextTxStmID  (1),        // NB: NEXT one, hence 1, not 0!
    m_lastRxStmID  (0),
    m_maxIDs       (IsOMC ? a_max_reqs : 0),
    m_cushion      (IsOMC      // Make a sufficient cushion, but not too big:
                    ? std::min<unsigned>
                      (unsigned(Round(double(m_maxIDs) * 0.1)), 1024)
                    : 0),
    m_baseReqID    (-1),       // Important: (<0) if not set
    m_stmIDsMap    (IsOMC ? new uint32_t[m_maxIDs] : nullptr),
    m_reqOffsMap   (IsOMC ? new uint32_t[m_maxIDs] : nullptr)
  {
    assert (m_psm  != nullptr && m_reactor != nullptr && m_logger != nullptr &&
           (!IsOMC || a_max_reqs > 0));

    // The Headers arrays are zeroed out:
    memset(m_outHdrs, '\0', sizeof(m_outHdrs));
    memset(m_inHdrs,  '\0', sizeof(m_inHdrs));

    // The Maps are zeroes out as well:
    if constexpr (IsOMC)
    {
      memset(m_stmIDsMap,   '\0', m_maxIDs * sizeof(uint32_t));
      memset(m_reqOffsMap,  '\0', m_maxIDs * sizeof(uint32_t));
    }

    // Store the constant Headers:
    // [0]: Authority (ServerHostName):
    m_outHdrs[0].name     = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>(":authority"));
    m_outHdrs[0].namelen  = 10;
    m_outHdrs[0].value    = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>
                            (TCPH2::m_serverName.data()));
    m_outHdrs[0].valuelen =  TCPH2::m_serverName.size();

    // [1]: Scheme (also a const):
    m_outHdrs[1].name     = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>(":scheme"));
    m_outHdrs[1].namelen  = 7;
    m_outHdrs[1].value    = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>("https"));
    m_outHdrs[1].valuelen = 5;

    // [2]: Method (Name only -- the Val is dynamic):
    m_outHdrs[2].name     = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>(":method"));
    m_outHdrs[2].namelen  = 7;

    // [3]: Path   (Name only -- the Val is dynamic):
    m_outHdrs[3].name     = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>(":path"));
    m_outHdrs[3].namelen  = 5;

    // Set the "no-copy" flags for all Headers (for inflation and deflation, to
    // improve performance):
    for (int i = 0; i < MaxOutHdrs; ++i)
      m_outHdrs[i].flags  = NGHTTP2_NV_FLAG_NO_COPY_NAME |
                            NGHTTP2_NV_FLAG_NO_COPY_VALUE;

    // NB: Inflater and Defater are NOT created at this point yet -- it will be
    // done in InitSession() after each TCP/TLS connection

    LOG_INFO(2, "H2Connector::Ctor: Binding to IP={}",
                a_client_ip == "" ? "Auto" : a_client_ip)
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  template<bool IsOMC, typename PSM>
  inline H2Connector<IsOMC, PSM>::~H2Connector() noexcept
  {
    // NB: The following calls are "C", so they cannot propagate any exns:
    // Must explicitly de-allocate the Deflater and Inflater:
    if (m_deflater  != nullptr)
    {
      nghttp2_hd_deflate_del(m_deflater);
      m_deflater     = nullptr;
    }
    if (m_inflater  != nullptr)
    {
      nghttp2_hd_inflate_del(m_inflater);
      m_inflater     = nullptr;
    }
    // Also de-allocate the DataBuff and HdrsBuff:
    if (m_dataBuff  != nullptr)
    {
      delete[] m_dataBuff;
      m_dataBuff     = nullptr;
      m_dataBuffLen  = 0;
    }
    if (m_hdrsBuff  != nullptr)
    {
      delete[] m_hdrsBuff;
      m_hdrsBuff     = nullptr;
      m_hdrsBuffLen  = 0;
    }
    if (m_stmIDsMap != nullptr)
    {
      delete[] m_stmIDsMap;
      m_stmIDsMap   = nullptr;
      m_baseReqID   = -1;
    }
    if (m_reqOffsMap != nullptr)
    {
      delete[] m_reqOffsMap;
      m_reqOffsMap    = nullptr;
    }
  }

  //=========================================================================//
  // "H2Frame" Accessors and Setters:                                        //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetDataLen":                                                           //
  //-------------------------------------------------------------------------//
  // Returns the Data Length (w/o the Prefix)!
  //
  template<bool IsOMC, typename PSM>
  template<unsigned    MaxDataLen>
  inline int H2Connector<IsOMC, PSM>::H2Frame<MaxDataLen>::GetDataLen() const
  {
    // DataLen comes in the network byte order:
    return (int(m_dataLen[0]) << 16) +
           (int(m_dataLen[1]) <<  8) + int(m_dataLen[2]);
  }

  //-------------------------------------------------------------------------//
  // "GetFrameLen":                                                          //
  //-------------------------------------------------------------------------//
  // Returns the Total Frame Length (Prefix + Data):
  //
  template<bool IsOMC, typename PSM>
  template<unsigned    MaxDataLen>
  inline int H2Connector<IsOMC, PSM>::H2Frame<MaxDataLen>::GetFrameLen() const
    { return PrefixLen + GetDataLen(); }

  //-------------------------------------------------------------------------//
  // "GetStreamID":                                                          //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename PSM>
  template<unsigned    MaxDataLen>
  inline unsigned H2Connector<IsOMC, PSM>::H2Frame<MaxDataLen>::GetStreamID()
  const
    { return ntohl(m_streamID); }

  //-------------------------------------------------------------------------//
  // "SetDataLen":                                                           //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename PSM>
  template<unsigned    MaxDataLen>
  inline void H2Connector<IsOMC, PSM>::H2Frame<MaxDataLen>::SetDataLen
    (int a_len)
  {
    // XXX: We currently do not allow data len >= 2^14, as that would require
    // specially-negotiated protocol settings.  Consequently, only 2 bytes of
    // DataLen (in BigEndian = Network ByteOrder) need to be written. Another
    // constraint is MaxDataLen of course:
    CHECK_ONLY
    (
      constexpr int Limit = int((MaxDataLen < 16383) ? MaxDataLen : 16383);
      if (utxx::unlikely(a_len > Limit))
        throw utxx::badarg_error
              ("H2Connector::SetDataLen: UnSupported DataLen=",
               a_len, ": Must be in 1..", Limit);
    )
    m_dataLen[0] = 0x00;
    m_dataLen[1] = uint8_t((a_len & 0xFF00) >> 8);
    m_dataLen[2] = uint8_t( a_len & 0x00FF);
    // All Done!
  }

  //-------------------------------------------------------------------------//
  // "SetStreamID":                                                          //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename  PSM>
  template<unsigned    MaxDataLen>
  inline void H2Connector<IsOMC, PSM>::H2Frame<MaxDataLen>::SetStreamID
    (uint32_t a_stream_id)
  {
    assert(a_stream_id > 0);
    // StreamID is to be saved in Network ByteOrder:
    m_streamID = htonl(a_stream_id);
  }

  //-------------------------------------------------------------------------//
  // "LogFrameHex":                                                          //
  //-------------------------------------------------------------------------//
  // NB: This method does something ONLY if ProtoLogger is enabled, and Debug-
  // Level is high enough. And it is only invoked in the Checked mode:
  //
  template<bool IsOMC, typename PSM>
  template<unsigned    MaxDataLen>
  template<bool        IsSend>
  inline void H2Connector<IsOMC, PSM>::H2Frame<MaxDataLen>::LogFrameHex
    (spdlog::logger* a_proto_logger)
  const
  {
    if (a_proto_logger == nullptr)
      return;

    // NB: "dataLen" may exceed MaxDataLen, as we may invoke this method on a
    // generic H2Frame<0>:
    int    frameLen = GetFrameLen();
    assert(frameLen > 0);

    // We reserve 3 output bytes per each Frame byte: 2 hex digits + space:
    char buff[3 * frameLen + 1];
    char const*   frame   = reinterpret_cast<char const*>(this);

    for (int i = 0;  i < frameLen; ++i)
      sprintf(buff + 3*i, "%02hhx ", frame[i]);
    buff[3*frameLen-1] = '\0';

    // Output it into the ProtoLog:
    a_proto_logger->info
      ("{} H2Frame: PrefixLen={}, DataLen={}:\n{}\n",
       IsSend ? "-->" : "<==", PrefixLen, GetDataLen(),
       static_cast<char const*>(buff));
    a_proto_logger->flush();
  }

  //=========================================================================//
  // Session Mgmt Call-Backs for the "TCP_Connector":                        //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "StopNow":                                                              //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename  PSM>
  template<bool        FullStop>
  inline void H2Connector<IsOMC, PSM>::StopNow
  (
    char const*     a_where,
    utxx::time_val  UNUSED_PARAM(a_ts_recv)
  )
  {
    assert(a_where != nullptr);

    // Terminate the TCP/TLS Connection, it will become "InActive":
    TCPH2::template DisConnect<FullStop>(a_where);
    assert(TCPH2::IsInactive());

    // If "H2Connector" stops, we must report this event to the PSM:
    m_psm->template H2ConnStopped<FullStop>
      (static_cast<typename PSM::H2Conn const*>(this), a_where);
  }

  //-------------------------------------------------------------------------//
  // "ServerInactivityTimeOut":                                              //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename  PSM>
  inline void H2Connector<IsOMC, PSM>::ServerInactivityTimeOut
    (utxx::time_val a_ts_recv)
  {
    // Stop the "H2Connector" immediately but try to re-start (FullStop=false);
    // "StopNow" will notify the PSM:
    StopNow<false>("ServerInactivityTimeOut", a_ts_recv);
  }

  //-------------------------------------------------------------------------//
  // "InitGracefulStop":                                                     //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename  PSM>
  inline void H2Connector<IsOMC, PSM>::InitGracefulStop() const
  {
    // XXX: At the moment, this is a NoOp. We do NOT even notify the PSM, beca-
    // use the latter typically contains multiple "H2Conn"s, and there is litt-
    // le point in notifying the PSM right now -- we will call "H2ConnStopped"
    // when the stop really happens!..
  }

  //-------------------------------------------------------------------------//
  // "SendHeartBeat":                                                        //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename  PSM>
  template<bool IsAck>
  inline void H2Connector<IsOMC, PSM>::SendHeartBeat() const
  {
    // Construct the HeartBeat Frame:
    char PingFrame[] =
      "\x00\x00\x08"                      // DataLen:  Must be 8
      "\x06\x00"                          // Type, Flags
      "\x00\x00\x00\x00"                  // StreamID: Must be 0
      "\x00\x00\x00\x00\x00\x00\x00\x00"; // Data: All 0s

    // Possibly set the Ack flag:
    PingFrame[4] = IsAck ? '\x01' : '\x00';

    // Send it out:
    (void) TCPH2::SendImpl(PingFrame, sizeof(PingFrame)-1);
  }

  //-------------------------------------------------------------------------//
  // "InitSession":                                                          //
  //-------------------------------------------------------------------------//
  // HTTP/2 session is initiated here:
  //
  template<bool IsOMC, typename  PSM>
  inline void H2Connector<IsOMC, PSM>::InitSession()
  {
    // (1) (Re-)create the NGHTTP2 Deflater and Inflater NOW: Delete them first
    // if they exist:
    if (m_deflater != nullptr)
    {
      nghttp2_hd_deflate_del(m_deflater);
      m_deflater    = nullptr;
    }
    if (m_inflater != nullptr)
    {
      nghttp2_hd_inflate_del(m_inflater);
      m_inflater    = nullptr;
    }
    // Create new ones:
    if (utxx::unlikely
       (nghttp2_hd_deflate_new(&m_deflater, 4096) != 0 ||
        nghttp2_hd_inflate_new(&m_inflater)       != 0))
      throw utxx::runtime_error
            ("H2Connector::InitSession: Cannot create Inflater/Deflater");
    assert(m_deflater != nullptr && m_inflater != nullptr);

    // (2) Maps etc: All StreamIDs are reset, as they are only valid intra-
    //     session:
    m_nextTxStmID   = 1;                 // NB: 1, not 0!
    m_lastRxStmID   = 0;
    if constexpr (IsOMC)
    {
      m_baseReqID   = -1;
      assert(m_maxIDs > 0);
      memset(m_stmIDsMap,  '\0', m_maxIDs * sizeof(uint32_t));
      memset(m_reqOffsMap, '\0', m_maxIDs * sizeof(uint32_t));
    }

    // (3) Hdrs, Data and Buffers:
    // (*) OutHdrs are to be preserved (their invariable part filled by Ctor);
    // (*) InHdrs, OutHdrsFrame, OutDataFrame are zeroed out;
    // (*) DataBuff,  HdrsBuff,  ContID       are zeroed out:
    //
    memset(m_inHdrs,        '\0', sizeof(m_inHdrs));
    memset(&m_outHdrsFrame, '\0', sizeof(m_outHdrsFrame));
    memset(&m_outDataFrame, '\0', sizeof(m_outDataFrame));
    memset(m_dataBuff,      '\0', MaxDataLen);
    memset(m_hdrsBuff,      '\0', MaxHdrsLen);
    memset(m_tmpBuff,       '\0', sizeof(m_tmpBuff));
    memset(m_logBuff,       '\0', sizeof(m_logBuff));
    m_dataBuffLen = 0;
    m_contID      = 0;
    m_hdrsBuffLen = 0;

    // (4) Send the "Magic PRISM" confirming we are using HTTP/2:
    constexpr char prism[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    (void) TCPH2::SendImpl(prism, sizeof(prism)-1);

    // (5) There will be no confirmation from HTTP/2 Server -- signal completi-
    // on of the H2 Session Init to the PSM right now:
    TCPH2::SessionInitCompleted();

    // This will result in "InitLogOn" invoked, which is forwarded to the "PSM"
    // (see the method below)
  }

  //-------------------------------------------------------------------------//
  // "InitLogOn":                                                            //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename  PSM>
  inline void H2Connector<IsOMC, PSM>::InitLogOn()
  {
    // HTTP/2 by itself does not require any LogOn procedures, as each subsequ-
    // ent req will be signed individually,  so we can signal LogOn completion
    // to the TCP layer:
    TCPH2::LogOnCompleted();

    // We need to notify the PSM, so it will account for this "H2Connector" be-
    // ing operational now:
    m_psm->H2LogOnCompleted(static_cast<typename PSM::H2Conn const*>(this));
  }

  //-------------------------------------------------------------------------//
  // "GracefulStopInProgress":                                               //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename  PSM>
  inline void H2Connector<IsOMC, PSM>::GracefulStopInProgress() const
  {
    // Send a "GoAway" msg to the Server, with ErrorCode=0 and LastStreamID=
    // 2^31-1 (in Network Byte Order):
    constexpr char GoAwayFrame[] =
      "\x00\x00\x08"      // DataLen:  8
      "\x07\x00"          // Type, Flags
      "\x00\x00\x00\x00"  // StreamID: Must be 0
      "\x7F\xFF\xFF\xFF"  // Data: LastStreamID
      "\x00\x00\x00\x00"; // Data: ErrorCode (all 0s)

    (void) TCPH2::SendImpl(GoAwayFrame, sizeof(GoAwayFrame)-1);
 }

  //-------------------------------------------------------------------------//
  // "Update{Tx,Rx}Stats": Forwarded to the "PSM":                           //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename  PSM>
  inline void H2Connector<IsOMC, PSM>::UpdateTxStats
    (int a_len, utxx::time_val a_ts)
  const
  {
    // Update the cumulative stats on the PSM:
    m_psm->UpdateTxStats(a_len, a_ts);
  }

  template<bool IsOMC, typename  PSM>
  inline void H2Connector<IsOMC, PSM>::UpdateRxStats
    (int a_len, utxx::time_val a_ts)
  const
  {
    // Update the cumulative stats on the PSM:
    m_psm->UpdateRxStats(a_len, a_ts);
  }

  //=========================================================================//
  // "ReadHandler":                                                          //
  //=========================================================================//
  template<bool IsOMC, typename PSM>
  inline int H2Connector<IsOMC, PSM>::ReadHandler
  (
    int            DEBUG_ONLY(a_fd),
    char const*    a_buff,    // Points  to the dynamic buffer of Reactor
    int            a_size,    // Size of the data chunk available in Buff
    utxx::time_val a_ts_recv
  )
  {
    assert(a_fd == TCPH2::m_fd && a_buff != nullptr && a_size > 0 &&
          !a_ts_recv.empty());

    //-----------------------------------------------------------------------//
    // Iterate over Frames received:                                         //
    //-----------------------------------------------------------------------//
    // By its operation, "ReadHandler" guarantees that "a_buff" is aligned with
    // a Frame beginning, but it may contain an incomplete Frame,   or multiple
    // Frames:
    utxx::time_val handlTS = utxx::now_utc();
    char const*    curr    = a_buff;
    char const*    end     = curr + a_size;

    while (curr < end)
    {
      //---------------------------------------------------------------------//
      // Frame is Complete?                                                  //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(curr + H2Frame<0>::PrefixLen > end))
        // Certainly not enough space for a (yet another) complete Frame:
        break;

      H2Frame<0> const* frame = reinterpret_cast<H2Frame<0> const*>(curr);

      // Get the Frame size (NB: It is in Big Endian byte order):
      uint32_t streamID = frame->GetStreamID();
      int      dataLen  = frame->GetDataLen ();
      int     frameLen  = dataLen + frame->PrefixLen;
      char const* next  = curr    + frameLen;

      if (utxx::unlikely(next > end))
        // Frame end is not in buff:
        break;

      //---------------------------------------------------------------------//
      // OK, got a complete Frame:
      //---------------------------------------------------------------------//
      // Log the Frame in Hex (IsSend=false):
      CHECK_ONLY
      (
        if (m_debugLevel > 3)
          frame->template LogFrameHex<false>
            // The over-all ProtoLogger is provided by the PSM:
            (m_psm->m_protoLogger);
      )

      if (streamID != m_lastRxStmID)
      {
        if (m_dataBuffLen)
        {
          LOG_WARN(1,
            "H2Connector::ReadHandler: Unconsumed data in buffer: StreamID={}, "
            "Data={}", m_lastRxStmID,
            spdlog::string_view_t(m_dataBuff, size_t(m_dataBuffLen)))
          m_dataBuffLen = 0;
        }
        m_lastRxStmID = streamID;
      }

      // Check the Frame Type:
      switch (frame->m_type)
      {
        //-------------------------------------------------------------------//
        case H2FrameTypeT::Data:
        //-------------------------------------------------------------------//
        // The actual Response Data come here:
        {
          // NB: If the Padding flag is set, there is a PadLength at Offset 0:
          int padLen  = 0;
          int dataOff = 0;
          if (utxx::unlikely((frame->m_flags & 0x08) != 0))
          {
            padLen  = int(frame->m_data[0]);
            dataOff = 1;
          }
          assert(0 <= dataOff && 0 <= padLen && padLen <= dataLen);
          dataLen  -= padLen;

          // In most cases, this Data Frame will be the final one in its Stream,
          // but not always.   In the latter case, merge the data from multiple
          // Frames in the DataBuff:
          bool isFinalFrame = (frame->m_flags & 0x01) != 0;
          if (!isFinalFrame && next + frame->PrefixLen <= end)
          {
            H2Frame<0> const* nextFrame =
              reinterpret_cast<H2Frame<0> const*>(next);
            if(nextFrame->GetStreamID() != streamID)
              isFinalFrame = true;
          }
          char const*  data = nullptr;

          // Append FrameData to DataBuff if the Frame is not final, or the Da-
          // taBuff is not empty:
          if (utxx::unlikely(!isFinalFrame || m_dataBuffLen != 0))
          {
            // Check for DataBuff overflow:
            CHECK_ONLY
            (
              if (utxx::unlikely(m_dataBuffLen + dataLen > MaxDataLen))
              {
                // OverFlow: Better stop permanently:
                LOG_ERROR(2,
                  "H2Connector::ReadHandler: StreamID={}: DataBuff OverFlow "
                  "by {} bytes, STOPPING...",   streamID,
                  (m_dataBuffLen + dataLen - MaxDataLen))

                // Notify the PSM (FullStop=true):
                m_psm->template H2ConnStopped<true>
                  (static_cast<typename PSM::H2Conn const*>(this),
                   "H2 DataBuff OverFlow");
                return -1;
              }
            )
            // If OK: Copy the data to the end of the DataBuff:
            memcpy(m_dataBuff + m_dataBuffLen, frame->m_data + dataOff,
                   size_t(dataLen));
            m_dataBuffLen += dataLen;

            // If the Frame is not final, do NOT process it yet:
            if (!isFinalFrame)
              break;

            // Otherwise, make "data" and "dataLen" point to the DataBuff cont-
            // ent for processing below:
            data    = m_dataBuff;
            dataLen = m_dataBuffLen;

            // Reset "m_dataBuffLen", as the DataBuff will be consumed now:
            m_dataBuffLen = 0;
          }
          else
          {
            // In this (generic) case, we can process FrameData in-place; use
            // "dataLen" as it is:
            assert(isFinalFrame  && m_dataBuffLen == 0);
            data = frame->m_data +  dataOff;
          }
          assert(data != nullptr && dataLen >= 0);

          //-----------------------------------------------------------------//
          // Invoke the Processor:                                           //
          //-----------------------------------------------------------------//
          if (utxx::likely(dataLen > 0))
          {
            // 0-terminate the "data" for extra safety:
            char* dataEnd  = const_cast<char*>(data + dataLen);
            char  was      = *dataEnd;
            *dataEnd       = '\0';

            // Flag indicating whether this is a last Frame in "a_buff". We set
            // it conservatively, only if it is CERTAINLY the last one; it will
            // be False if there is an incomplete Frame after the curr one.  We
            // need it to enable safe restarts of the Connector:
            //
            bool lastFrame = (next == end);

            // XXX: "ProcessH2Msg" is expected to return False in case of any
            // errors, NOT to throw exceptions:
            CHECK_ONLY(bool ok =)
              m_psm->ProcessH2Msg
                (static_cast<typename PSM::H2Conn*>(this),
                 streamID,  data, dataLen, lastFrame, a_ts_recv, handlTS);

            // Restore the orig byte beyond the end of the data processed:
            *dataEnd      = was;

            // Check the Processor return flag. XXX: If "false", it is a signal
            // to stop the whole PSM(!) gracefully,  although  the Reader could
            // technically continue as all Frames are isolated from each other:
            CHECK_ONLY
            (
              if (utxx::unlikely(!ok))
              {
                LOG_WARN(1,
                  "H2Connector::ReadHandler: StreamID={}: STOPPING Gracefully "
                  "at Processor Request",    streamID)

                // XXX: Yes, stop the whole PSM at the Processor's request:
                m_psm->Stop();
                return -1;
              }
            )
          }
          // DataFrame is done!
          break;
        }
        //-------------------------------------------------------------------//
        case H2FrameTypeT::GoAway:
        //-------------------------------------------------------------------//
        // The server is about to close the connection:
        {
          // Get the DataFlds:
          uint32_t lastStreamID =
            utxx::likely(dataLen >= 4)
            ? ntohl(*(reinterpret_cast<uint32_t const*>(frame->m_data)))
            : 0;
          // NB:  "lastReqID" will be 0 if "lastStreamID" is invalid:
          OrderID  lastReqID = ReqIDofStreamID(lastStreamID);

          uint32_t errCode   =
            utxx::likely(dataLen >= 8)
            ? ntohl(*(reinterpret_cast<uint32_t const*>(frame->m_data) + 1))
            : 0;
          // AdditionalDebugData are supposed to be a String which may or may
          // not be 0-terminated. As the GoAway event is presumably rare,  it
          // is OK to create a dynamic string in this case:
          std::string addDebugData =
            (dataLen > 8)
            ? std::string(frame->m_data + 8, size_t(dataLen-8))
            : std::string();

          LOG_WARN(1,
            "H2Connector::ReadHandler: Received GoAway from the Server, "
            "RE-STARTING: LastProcessedReqID={}, ErrCode={}, Info={}",
            lastReqID, errCode, addDebugData)

          // DisConnect (but FullStop=false):
          TCPH2::template DisConnect<false>("H2-ReadHandler(GoAway)");
          assert(TCPH2::IsInactive());

          // No not read any further data until restart:
          return -1;
        }

        //-------------------------------------------------------------------//
        case H2FrameTypeT::RstStream:
        //-------------------------------------------------------------------//
        // The Server asks us to terminate the current Stream,   though not the
        // whole session. Still, there are no legitimate situations as why that
        // could happen, so we better restart:
        {
          uint32_t errCode =
            utxx::likely(dataLen >= 4)
            ? ntohl(*(reinterpret_cast<uint32_t const*>(frame->m_data)))
            : 0;
          LOG_WARN(1,
            "H2Connector::ReadHandler: Received RstStream from the Server: "
            "StreamID={}, ErrCode={}", streamID, errCode)

          // PSM may implement a further action: eg in some cases the whole OMC
          // needs to be restarted after a RstStream:
          m_psm->OnRstStream
            (const_cast <typename PSM::H2Conn*>
            (static_cast<typename PSM::H2Conn const*>(this)),
             streamID, errCode, a_ts_recv);
          break;
        }

        //-------------------------------------------------------------------//
        case H2FrameTypeT::Ping:
        //-------------------------------------------------------------------//
        {
          // If the received Ping was not an Ack by itself, we must respond with
          // a Ping (IsAck=true):
          if ((frame->m_flags & 0x01) == 0) // Not an Ack
            SendHeartBeat<true>();
          break;
        }

        //-------------------------------------------------------------------//
        case H2FrameTypeT::Headers:
        //-------------------------------------------------------------------//
        {
          ProcessHeaderFrame
          (
            streamID,
            m_psm->m_protoLogger,
            m_inflater,
            reinterpret_cast<uint8_t*>(const_cast<char*>(frame->m_data)),
            size_t(frame->GetDataLen()),
            a_ts_recv
          );
          break;
        }

        //-------------------------------------------------------------------//
        case H2FrameTypeT::PushPromise:
        case H2FrameTypeT::Continuation:
        //-------------------------------------------------------------------//
        {
          // XXX: Nothing yet?
          break;
        }

        //-------------------------------------------------------------------//
        case H2FrameTypeT::Priority:
        //-------------------------------------------------------------------//
        // Only logged: Priority is most likely not relevant in our case, it is
        // for receiving large documents with dependencies:
        {
          CHECK_ONLY
          (
          uint32_t streamDep =
            utxx::likely(dataLen >= 4)
            ? ntohl(*(reinterpret_cast<uint32_t const*>(frame->m_data)))
            : 0;
          uint8_t  weight    =
            utxx::likely(dataLen >= 5)
            ? uint8_t(frame->m_data[4])
            : 0;
          LOG_INFO(2,
            "H2Connector::ReadHandler: ConnID={}, Received Priority: "
            "StreamID={}, StreamDep={}, Weight={}: Ignored",
            streamID, streamDep, weight)
          )
          break;
        }

        //-------------------------------------------------------------------//
        case H2FrameTypeT::Settings:
        //-------------------------------------------------------------------//
        // They are logged and acklowledged, but in general we do not expect any
        // non-standard settings:
        {
          CHECK_ONLY
          (
            int nSets = dataLen / 6;  // XXX: Not checking (dataLen % 6 == 0)
            LogSettings(frame->m_data, nSets);
          )
          // We must acknowledge the Settings received.  NB: The Settings may
          // apply to a particular Stream, not all Streams (0), so we need to
          // copy the StreamID into the response:
          //
          char SettingsAckFrame[] =
            "\x00\x00\x00"                      // DataLen:  Must be 0
            "\x04\x01"                          // Type, Flags (ACK=1)
            "\x00\x00\x00\x00";                 // StreamID: Initially 0
          memcpy(SettingsAckFrame + 5, frame + 5, 3);

          (void) TCPH2::SendImpl(SettingsAckFrame, sizeof(SettingsAckFrame)-1);
          break;
        }

        //-------------------------------------------------------------------//
        case H2FrameTypeT::WindowUpdate:
        //-------------------------------------------------------------------//
        // Again, it is only logged at the moment, as buffers / TCP windows are
        // configured statically:
        {
          uint32_t winSzIncr =
            (dataLen >= 4)
            ? ntohl(*(reinterpret_cast<uint32_t const*>(frame->m_data)))
            : 0;
          LOG_INFO(1,
            "H2Connector::ReadHandler: Got WindowUpdate: StreamID={}, "
            "WinSizeIncr={}", streamID, winSzIncr)
          break;
        }

        //-------------------------------------------------------------------//
        default:
        //-------------------------------------------------------------------//
          // This must not happen: Invalid Frame Type:
          LOG_ERROR(1,
            "H2Connector::ReadHandler: StreamID={}: Invalid FrameType={}, "
            "Ignored", streamID, int(frame->m_type))
      }
      //---------------------------------------------------------------------//
      // Next Frame:                                                         //
      //---------------------------------------------------------------------//
      curr = next;
    }
    //-----------------------------------------------------------------------//
    // All Done:
    //-----------------------------------------------------------------------//
    assert (a_buff <= curr && curr <= end);
    int consumed = int(curr - a_buff);

    // For an MDC, we need to invoke a Processor call-back on EndOfDataChunk:
    if constexpr (PSM::IsMDC)
    {
      static_assert(!PSM::IsMultiCast);
      if (consumed > 0)
        // This is a true EndOfDataChunk, invoke the Processor on that:
        // IsMultiCast=false, other flags are taken from the Processor:
        m_psm->template
          ProcessEndOfDataChunk
          <
            false,
            PSM::WithIncrUpdates,
            PSM::WithOrdersLog,
            PSM::WithRelaxedOBs,
            PSM::QT,
            typename PSM::QR
          >
          ();
    }
    // All Done. Return the number of bytes consumed:
    return consumed;
  }

  //=========================================================================//
  // "ProcessHeaderFrame":                                                   //
  //=========================================================================//
  // XXX: Currently only used in Debug Mode -- inflates and logs the Hdrs rece-
  // ived from the Server:
  //
  template<bool IsOMC, typename PSM>
  void H2Connector<IsOMC, PSM>::ProcessHeaderFrame
  (
    uint32_t             a_stream_id,
    spdlog::logger*      a_proto_logger,
    nghttp2_hd_inflater* a_hd_inflater,
    uint8_t*             a_buff,
    size_t               a_len,
    utxx::time_val       a_ts_recv
  )
  {
    ssize_t  rv;
    uint8_t* curr = a_buff;

    char* msg = m_logBuff;
    char const* httpCode;

    for (;;)
    {
      nghttp2_nv nv;
      int inflate_flags = 0;

      rv = nghttp2_hd_inflate_hd2
           (a_hd_inflater, &nv, &inflate_flags, curr, a_len, 1);
      if (rv <= 0)
        break;

      curr += rv;
      a_len -= size_t(rv);

      if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT)
      {
        msg = stpcpy(msg, reinterpret_cast<char*>(nv.name));
        msg = stpcpy(msg, ":");
        msg = stpcpy(msg, reinterpret_cast<char*>(nv.value));
        msg = stpcpy(msg, ",");

        if (memcmp(nv.name, reinterpret_cast<uint8_t const*>(":status"), 3)==0)
        {
          httpCode = reinterpret_cast<char const*>(nv.value);

          if (utxx::likely(strncmp(httpCode, "200", 3) == 0))
            return;

          a_proto_logger->warn("HTTP Status {} received.", httpCode);
        }
      }

      if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL)
      {
        nghttp2_hd_inflate_end_headers(a_hd_inflater);
        break;
      }

      if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && a_len == 0)
        break;
    }

    if (m_debugLevel >= 3)
    {
      a_proto_logger->warn("<== Headers Received: {}", m_logBuff);
      memset(m_logBuff, '\0', sizeof(m_logBuff));
    }

    // Further processing may be done by the PSM:
    m_psm->OnHeaders
      (const_cast <typename PSM::H2Conn*>
      (static_cast<typename PSM::H2Conn const*>(this)),
       a_stream_id, httpCode, m_logBuff, a_ts_recv);
  }

  //=========================================================================//
  // "LogSettings":                                                          //
  //=========================================================================//
  template<bool IsOMC, typename  PSM>
  inline void H2Connector<IsOMC, PSM>::LogSettings
    (char const* a_data, int a_setts_len)
  const
  {
    // Form the Settings String:
    assert(a_data != nullptr);
    char        buff[512];
    char*       currOut = buff;
    char const* currIn  = a_data;

    for (int i = 0; i < a_setts_len; ++i, currIn += 6)
    {
      uint16_t key = ntohs(*(reinterpret_cast<uint16_t const*>(currIn)));
      uint32_t val = ntohl(*(reinterpret_cast<uint32_t const*>(currIn + 2)));

      char const* keyName = "UNKNOWN";
      switch (key)
      {
        case 1: keyName = "HeaderTableSize  ";  break;
        case 2: keyName = "EnablePush       ";  break;
        case 3: keyName = "MaxConcurStreams ";  break;
        case 4: keyName = "InitWindowSize   ";  break;
        case 5: keyName = "MaxFrameSize     ";  break;
        case 6: keyName = "MaxHeaderListSize";  break;
        default: ;
      }
      currOut += sprintf(currOut, "%s: %u\n", keyName, val);
      assert(sizeof(currOut - buff) < sizeof(buff));
    }
    // Output the Settings String:
    LOG_INFO(2, "H2Connector: Received Settings:\n{}", buff)
  }

  //=========================================================================//
  // "SendFrame":                                                            //
  //=========================================================================//
  // All Frame data are already assumed to be in "m_out{Hdrs,Data}Frame":
  //
  template<bool IsOMC, typename PSM>
  template<bool IsData>
  inline utxx::time_val H2Connector<IsOMC, PSM>::SendFrame() const
  {
    // Send the Frame out:
    int frameLen =
      IsData
      ? m_outDataFrame.GetFrameLen()
      : m_outHdrsFrame.GetFrameLen();

    utxx::time_val sendTS   =
      IsData
      ? TCPH2::SendImpl
        (reinterpret_cast<char const*>(&m_outDataFrame), frameLen)
      : TCPH2::SendImpl
        (reinterpret_cast<char const*>(&m_outHdrsFrame), frameLen);

    CHECK_ONLY
    (
      if (m_debugLevel > 3)
      {
        // IsSend=true:
        if (IsData)
          m_outDataFrame.template  LogFrameHex<true>
            (m_psm->m_protoLogger);
        else
          m_outHdrsFrame.template  LogFrameHex<true>
            (m_psm->m_protoLogger);
      }
    )
    // IMPORTANT: For safety, zero-out the Frame after sending:
    if constexpr (IsData)
      memset(&m_outDataFrame, '\0', size_t(frameLen));
    else
      memset(&m_outHdrsFrame, '\0', size_t(frameLen));

    return sendTS;
  }
  //=========================================================================//
  // Helpers: StreamID <-> ReqID Conversion:                                 //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "StreamIDofReqID":                                                      //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename PSM>
  inline uint32_t H2Connector<IsOMC, PSM>::StreamIDofReqID(OrderID a_req_id)
  const
  {
    // The following must always hold:
    assert(m_nextTxStmID % 2 == 1);

    // Special Case: In the MDC mode *OR* if there is no ReqID, there is no
    // mapping, so ignore the ReqID and use a directly-incremented value:
    //
    if (utxx::unlikely(!IsOMC || a_req_id == 0))
    {
      uint32_t autoID = m_nextTxStmID;
      m_nextTxStmID  += 2;
      return   autoID;
    }

    // Generic Case: Got a real ReqID:
    // Set the base if it is not set yet (as ReqIDs may also have a non-trivial
    // base).  XXX: However, if the base is not set yet, we  cannot set it just
    // to "a_req_id", bacause due to Timer-sent Indications, the  Reqs  may not
    // be sent in the increasing order of ReqIDs. So make some allowance for the
    // backlog; the minimal valid ReqID is 1:
    //
    if (utxx::unlikely(m_baseReqID < 0))
    {
      m_baseReqID =
        (a_req_id >= m_cushion)
        ? (long(a_req_id) - long(m_cushion))
        : 0;
    }
    // Compute the offset from the base; it must be STRICTLY POSITIVE:
    assert(m_baseReqID >= 0  &&  a_req_id > unsigned(m_baseReqID));

    unsigned off = unsigned(long(a_req_id) - m_baseReqID);
    assert  (off > 0);

    // Check if this "off" was already mapped to a StreamID:
    CHECK_ONLY
    (
      if (utxx::unlikely(off >= m_maxIDs))
        throw utxx::badarg_error
              ("H2Connector::StreamIDofReqID: ReqID=", a_req_id, ": Limit=",
               m_maxIDs + m_baseReqID);
    )
    uint32_t streamID = m_stmIDsMap[off];

    // If we got 0, this ReqID is not in the map yet. Will need to update the
    // map:
    if (streamID == 0)
    {
      // Use the next StreamID -- so they are monotonically increasing, and they
      // must be odd. Obviously, StreamID=1,3,...":
      streamID         = m_nextTxStmID;
      m_nextTxStmID   += 2;

      assert(streamID >= 1 && (streamID % 2) == 1 && off > 0);
      m_stmIDsMap[off] = streamID;

      // The corresp slot in the inverse map must be empty yet, so update it;
      // however, the slot index is "compressed" (so not necesarily odd). NB:
      // Idx = (StreamID-1)/2 = StreamID >> 1, so it is OK to have Idx==0 (if
      // StreamID==1):
      uint32_t idx   = streamID >> 1;

      // Check for overflow once again:
      CHECK_ONLY
      (
        if (utxx::unlikely(idx >= m_maxIDs))
          throw utxx::badarg_error
                ("H2Connector::StreamIDofReqID: StreamID=", streamID, ", Idx=",
                 idx, ": IdxLimit=", m_maxIDs);
      )
      // Now really update the inverse map. NB: We must have off > 0, otherwise
      // in "ReqIDofStreamID", we would not be able to detect empty entries. But
      // idx==0 is OK:
      assert(m_reqOffsMap[idx] == 0 && idx < m_maxIDs && off > 0);
      m_reqOffsMap       [idx] =  off;
    }

    // In any case: The post-conditions:
    assert(streamID >= 1 && streamID % 2 == 1 && m_stmIDsMap[off] == streamID &&
           m_reqOffsMap[streamID >> 1] == off);
    return streamID;
  }

  //-------------------------------------------------------------------------//
  // "ReqIDofStreamID":                                                      //
  //-------------------------------------------------------------------------//
  template<bool IsOMC, typename PSM>
  inline OrderID H2Connector<IsOMC, PSM>::ReqIDofStreamID(uint32_t a_stream_id)
  const
  {
    // This method is only called in OMC Mode:
    assert(IsOMC);

    // The valid StreamIDs (in particular those generated from our ReqIDs) must
    // be ODD:
    if (utxx::unlikely(a_stream_id % 2 == 0))
      return 0;  // This includes the most common case of StreamID=0

    // Generic Case:
    CHECK_ONLY
    (
      if (utxx::unlikely(a_stream_id >= m_nextTxStmID))
        // This StreamID was not even transmitted yet!
        throw utxx::badarg_error
              ("H2Connector::ReqIDofStreamID: StreamID=", a_stream_id,
               ", LastTxStmID=", m_nextTxStmID-1);
    )
    // Can now perform the map look-up; "idx" is a "compressed" value of Stream-
    // ID (same as in "StreamIDofReqID"):
    uint32_t idx = a_stream_id >> 1;

    // Check the range once again:
    CHECK_ONLY
    (
      if (utxx::unlikely(idx >= m_maxIDs))
        throw utxx::badarg_error
              ("H2Connector::ReqIDofStreamID: StreamID=", a_stream_id, ", Idx=",
               idx, ": IdxLimit=", m_maxIDs);
    )
    uint32_t off = utxx::likely(idx < m_maxIDs) ? m_reqOffsMap[idx] : 0;

    // XXX: It "m_baseReqID" is not set yet, it migtt be because we are respon-
    // ding to an initial no-ReqID request,  so return 0 in that case, and also
    // if "idx" was not on the map:
    if (utxx::unlikely(off == 0 || m_baseReqID < 0))
    {
      LOG_WARN(2,
        "H2Connector::ReqIDofStreamID: StreamID={}: Not in the map: "
        "BaseReqID={}, OffReqID={}",   a_stream_id, m_baseReqID, off)
      return 0;
    }
    // Generic Case:
    return OrderID(m_baseReqID) + off;
  }
} // End namespace MAQUETTE
