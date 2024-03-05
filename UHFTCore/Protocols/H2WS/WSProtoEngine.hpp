// vim:ts=2:et
//===========================================================================//
//                     "Protocols/H2WS/WSProtoEngine.hpp":                   //
//===========================================================================//
#pragma once

#include "Basis/Macros.h"
#include "Basis/Base64.h"
#include "Protocols/H2WS/WSProtoEngine.h"
#include <cstring>
#include <strings.h>
#include <sys/random.h>
#include <gnutls/crypto.h>

namespace MAQUETTE
{
namespace H2WS
{
  //=========================================================================//
  // Non-Default Ctor:                                                       //
  //=========================================================================//
  template<typename    Processor>
  inline WSProtoEngine<Processor>::WSProtoEngine(Processor* a_processor)
  : m_processor     (a_processor),
    m_inBuff        (new char[InBuffCap]),
    m_inBuffLen     (0),
    m_outBuff       (new char[OutBuffCap]),
    m_outBuffDataLen(0),
    m_sessKey       (),
    m_beforeResp    (true),
    m_hdrs          ()
  {
    assert(m_processor != nullptr && m_inBuff != nullptr &&
           m_outBuff   != nullptr);
  }

  //=========================================================================//
  // Dtor:                                                                   //
  //=========================================================================//
  template<typename    Processor>
  inline WSProtoEngine<Processor>::~WSProtoEngine() noexcept
  {
    m_processor      = nullptr;
    if (m_inBuff  != nullptr)
      delete[] m_inBuff;
    m_inBuff         = nullptr;
    m_inBuffLen      = 0;

    if (m_outBuff != nullptr)
      delete[] m_outBuff;
    m_outBuff        = nullptr;
    m_outBuffDataLen = 0;
  }

  //=========================================================================//
  // Initial WS HandShake:                                                   //
  //=========================================================================//
  // Re-usable method across multiple WS-based protocols:
  //
  template<typename Processor>
  inline void WSProtoEngine<Processor>::InitWSHandShake(char const* a_path)
  const
  {
    assert(a_path != nullptr);

    // Standard HTTP/WS Preamble: NB: It must be HTTP/1.1. XXX: "a_path" is
    // copied over which is slightly inefficient:
    char*  curr = stpcpy(m_outBuff, "GET ");
    curr = stpcpy(curr,  a_path);
    curr = stpcpy(curr,  " HTTP/1.1\r\nHost: ");
    curr = stpcpy(curr,  m_processor->m_serverName.data());
    curr = stpcpy(curr,  "\r\nConnection: Upgrade"
                         "\r\nUpgrade: websocket"
                         "\r\nSec-WebSocket-Version: 13"
                         "\r\nSec-WebSocket-Key: ");
    // Generate a 128-bit random key to prevent any caching proxies from re-
    // using an existing WS connection. XXX:  The following Linux call uses
    // "/dev/urandom" which is not absolutely secure, but should be suffici-
    // ent for our purposes:
    uint8_t     binKey[16];
    int   rc  = 0;
    do    rc  = int(getrandom(binKey, sizeof(binKey), 0));
    while(rc != int(sizeof(binKey)));

    // Perform Base64-encoding of that key, and save it in "m_sessKey" for fut-
    // ure verification:
    int kLen = Base64EnCodeUnChecked(binKey, sizeof(binKey), curr);
    m_sessKey.assign(curr, size_t(kLen));
    curr    += kLen;

    // Final line:
    curr = stpcpy(curr,  "\r\n\r\n");

    // Check the size written:
    int len = int(curr - m_outBuff);
    assert(len < OutBuffCap);
    // Just for extra safety, 0-terminate the OutBuff:
    *curr   =  '\0';

    // We will now bw awaiting the Resp, so clear the HTTP/WS status:
    m_beforeResp = true;
    m_hdrs.clear();

    // Send the req out:
    m_processor->SendImpl(m_outBuff, len);
  }

  //=========================================================================//
  // "ReadHandler":                                                          //
  //=========================================================================//
  template<typename Processor>
  inline int WSProtoEngine<Processor>::ReadHandler
  (
    int             DEBUG_ONLY(a_fd),
    char const*     a_buff,    // Points  to the dynamic buffer of Reactor
    int             a_size,    // Size of the data chunk available in Buff
    utxx::time_val  a_ts_recv
  )
  {
    assert(a_fd   >= 0 && a_fd == m_processor->m_fd && a_buff != nullptr &&
           a_size >  0);
    //=======================================================================//
    // Parse the Chunk: It may contain RespLine, Headers and/or WS Frames:   //
    //=======================================================================//
    // XXX: We may need to modify "a_buff" while parsing the data, so make it
    // non-const:
    utxx::time_val handlTS = utxx::now_utc();
    char*          curr    = const_cast<char*>(a_buff);
    char*          end     = curr + a_size;

    while (curr < end)
    {
      char* next = nullptr;
      //=====================================================================//
      // Before the Frames?                                                  //
      //=====================================================================//
      // (LogginOn will be over once we reach the Frames):
      if (utxx::unlikely(m_processor->IsSessionInit()))
      {
        // Parse the initial line and headers.
        // They are separated by "\r\n" strs:
        char* sep  =
          static_cast<char*>(memmem(curr, size_t(end - curr), "\r\n", 2));
        if (sep == nullptr)
          break;   // No (more) separators in the chunk received

        // If found: 0-terminate the "curr" line, and set the "next":
        assert(curr <= sep && sep < end && *sep == '\r');
        *sep = '\0';
        next = sep + 2;

        //-------------------------------------------------------------------//
        // First Response Line?                                              //
        //-------------------------------------------------------------------//
        if (utxx::unlikely(m_beforeResp))
        {
          // This is a very first Response line, we expect
          // "HTTP/1.1 101 {Switching Protocols|Web Socket Protocol Handshake}";
          // in any case, reset the "BeforeResp" flag:
          m_beforeResp = false;

          if (utxx::unlikely(strncmp(curr, "HTTP/1.1 101 ", 13) != 0))
          {
            // XXX: UnExpected response received:
            LOGA_ERROR(m_processor->Processor, 1,
              "H2WS::WSProtoEngine::ReadHandler: UnExpected HTTP Response:\n{}",
               curr)
            // Will have to close the curr session immediately (with a later
            // restart attempt: FullStop=false):
            m_processor->template StopNow<false>
              ("UnExpected HTTP Resp", a_ts_recv);
            return -1;
          }
          // If the RespLine is OK, just continue reading...
        }
        else
        //-------------------------------------------------------------------//
        // End of Headers?                                                   //
        //-------------------------------------------------------------------//
        if (*curr == '\0')
        {
          // Empty line encountered, the Headers are complete:
          CHECK_ONLY
          (
            if (utxx::unlikely(!CheckHeaders()))
            {
              // The actual err msg has already been logged. Again, we have to
              // close the curr session:
              m_processor->template StopNow<false>
                ("UnExpected HTTP/WS Header(s)", a_ts_recv);
              return -1;
            }
          )
          // If Headers are OK: SessionInit is now done:
          m_processor->SessionInitCompleted(a_ts_recv);

          // Continue reading -- we may get Frames after the Headers...
        }
        else
        //-------------------------------------------------------------------//
        // Presumably, got a Header:                                         //
        //-------------------------------------------------------------------//
        {
          // Parse it:
          char* kvSep = strstr(curr, ": ");

          CHECK_ONLY
          (
            if (utxx::unlikely(kvSep == nullptr))
            {
              // Again, cannot continue, will restart:
              LOGA_ERROR(m_processor->Processor, 1,
                "H2WS::WSProtoEngine::ReadHandler: MalFormatted Hdr: {}", curr)

              m_processor->template StopNow<false>
                ("MalFormatted HTTP/WS Hdr", a_ts_recv);
              return -1;
            }
          )
          // If OK: Get Key and Val; remove possible extraneous spaces from the
          // the Val front (but not from other places):
          *kvSep = '\0';
          char const* key = curr;
          char const* val = kvSep + 2;
          for (; val < end && *val == ' '; ++val) ;
          assert(val < end);

          // Save the (Key, Val) pair in Hdrs. XXX: We do not check if the Key
          // was already there:
          m_hdrs[std::string(key)] = std::string(val);
        }
      }
      else
      //=====================================================================//
      // WSFrame = Prefix + PayLoad:                                         //
      //=====================================================================//
      {
        //-------------------------------------------------------------------//
        // Get the 2 bytes of Prefix:                                        //
        //-------------------------------------------------------------------//
        // XXX: Unfortunately, "static_cast" below is not allowed:
        uint8_t const* frame    = reinterpret_cast<uint8_t*>(curr);
        int            remBytes = int(end  - curr);

        // The Frame must always contain at least 2 bytes. If they are not ava-
        // ilable, stop now:
        if (utxx::unlikely(remBytes < 2))
          break;

        // OK: "frame0" is frame[0] with extension bits zeroed-out: OpCode etc;
        //     "frame1" is frame[1] without the Mask Bit:      1st Length byte:
        uint8_t frame0   =  frame[0]  & 0x8F;
        uint8_t frame1   =  frame[1]  & 0x7F;
        bool    isFinal  = (frame[0] >= 128);

        // First of all, data sent from Server to Client should NOT be Masked.
        // If it happens, stop permanently (FullStop=true):
        CHECK_ONLY
        (
          bool    isMasked = (frame[1] >= 128);
          if (utxx::unlikely(isMasked))
          {
            LOGA_ERROR(m_processor->Processor, 1,
              "H2WS::WSProtoEngine::ReadHandler: Received UnExpected Masked "
              "Frame: {}", std::string(curr, size_t(remBytes)))

            m_processor->template StopNow<true>
              ("UnExpected Masked WS Frame", a_ts_recv);
            return -1;
          }
        )
        //-------------------------------------------------------------------//
        // Get the PayLoad (Data) Length:                                    //
        //-------------------------------------------------------------------//
        int  pfxLen   = 0;
        long dataLen  = 0;
        if (frame1 <= 125)
        {
          // DataLen is given by 1 byte (frame1 itself), and it is <= 125:
          pfxLen  = 2;
          dataLen = long(frame1);
        }
        else
        if (frame1 == 126)
        {
          // DataLen is given by 2 bytes (frame[2] and frame[3]), in BigEndian
          // Order, so we need at least 4 bytes remaining in the chunk:
          pfxLen  = 4;
          if (utxx::unlikely(remBytes < pfxLen))
            break;
          dataLen = (long(frame[2]) <<  8) +  long(frame[3]);
        }
        else
        {
          assert(frame1 == 127);
          // DataLen is given by 8 bytes (frame[2..9]) in Big Endian Order, so
          // it may in theory be 64-bit long; we then restrict it to an "int".
          // Again, if "pfxLen" bytes are not available, wait for more data to
          // arrive:
          pfxLen  = 10;
          if (utxx::unlikely(remBytes < pfxLen))
            break;

          dataLen = (long(frame[2]) << 56) + (long(frame[3]) << 48) +
                    (long(frame[4]) << 40) + (long(frame[5]) << 32) +
                    (long(frame[6]) << 24) + (long(frame[7]) << 16) +
                    (long(frame[8]) <<  8) +  long(frame[9]);
        }
        // OK, the PayLoad has been bracketed:  We assume that "dataLen" still
        // fits in the "int" format:
        assert(0 <= dataLen && dataLen <= INT_MAX && pfxLen >= 2);
        int dataLenI = int(dataLen);

        // Check that the whole Frame fits in the curr chunk received.  If not,
        // wait for more data to arrive (XXX: this will result in re-reading of
        // the same Frame Pfx when new data are available, which is inefficient;
        // also, the incoming data will be stored in the Reactor Buffer  in the
        // meantime, and it has finite capacity anyway;  so we will NOT be able
        // to receive huge  (gigabyte-size) Frames with this mechanism!):
        //
        next = curr + (pfxLen + dataLenI);

        if (utxx::unlikely(next > end))
          break;

        //-------------------------------------------------------------------//
        // Now check the OpCode:                                             //
        //-------------------------------------------------------------------//
        switch (frame0)
        {
          //-----------------------------------------------------------------//
          // Generic Case: A Data Frame:                                     //
          //-----------------------------------------------------------------//
          case 0x82: // FINAL Binary       Frame
          case 0x81: // FINAL Text (UTF-8) Frame
          case 0x80: // FINAL Continuation Frame
          case 0x02: // Non-FINAL Binary   Frame (followed by Continuation(s))
          case 0x01: // Non-FINAL Text     Frame (followed by Continuation(s))
          case 0x00: // Non-FINAL Contintn Frame
          {
            // Will invoke the Processor. Here we make no distinction between
            // Text and Binary Frames:
            //
            assert(m_inBuffLen >= 0);
            char const*   data  = nullptr;

            // Append FrameData to InBuff if the Frame is not final, or if the
            // InBuff is not empty:
            if (utxx::unlikely(!isFinal || m_inBuffLen != 0))
            {
              // Check that the Buffer has sufficient remaining capacity:
              CHECK_ONLY
              (
                if (utxx::unlikely(m_inBuffLen + dataLenI > InBuffCap))
                {
                  // OverFlow: Better stop permanently (FullStop=true):
                  LOGA_ERROR(m_processor->Processor, 1,
                    "H2WS::WSProtoEngine::ReadHandler: WS InBuffer OverFlow "
                    "by {} bytes, STOPPING...",
                    (m_inBuffLen + dataLenI - InBuffCap))

                  m_processor->template StopNow<true>
                    ("WS InBuffer OverFlow", a_ts_recv);
                  return -1;
                }
              )
              // If OK: Copy the data to the end of the InBuff:
              memcpy(m_inBuff + m_inBuffLen, curr + pfxLen, size_t(dataLen));
              m_inBuffLen += dataLenI;

              // If the Frame is not final, do NOT process it yet:
              if (!isFinal)
                break;

              // Otherwise, make "data" and "dataLenI" point to the InBuff con-
              // tent for processing below:
              data     = m_inBuff;
              dataLenI = m_inBuffLen;

              // Reset "m_dataBuffLen", as the DataBuff will be consumed now:
              m_inBuffLen = 0;
            }
            else
            {
              // In this (generic) case, we can process FrameData in-place; use
              // "dataLenI" as it is:
              assert(isFinal && m_inBuffLen == 0);
              data = curr + pfxLen;
            }
            assert(data != nullptr && dataLenI >= 0);

            //---------------------------------------------------------------//
            // Invoke the Processor:                                         //
            //---------------------------------------------------------------//
            if (utxx::likely(dataLenI > 0))
            {
              // 0-termin-ate the "data" for safety:
              char* dataEnd = const_cast<char*>(data + dataLenI);
              char  was     = *dataEnd;
              *dataEnd      = '\0';

              // Flag indicating whether this is a last Frame in "a_buff". We
              // set it conservatively, only if it is CERTAINLY the last one;
              // it will be False if there is an incomplete Frame  after  the
              // curr one. We need it to enable safe restarts of the Connector:
              //
              bool lastFrame = (next == end);

              // XXX: "ProcessWSMsg" is expected to return False in case of any
              // errors, NOT to throw exceptions:
              CHECK_ONLY(bool ok =)
                m_processor->ProcessWSMsg
                  (data, dataLenI, lastFrame, a_ts_recv, handlTS);

              // Restore the orig byte beyond the data processed:
              *dataEnd      = was;

              // Check the Processor return flag. XXX: If "false", it is a sig-
              // nal to stop gracefully  (although the Reader could technically
              // continue as all Frames are isolated from each other):
              CHECK_ONLY
              (
                if (utxx::unlikely(!ok))
                {
                  LOGA_WARN(m_processor->Processor, 1,
                    "H2WS::WSProtoEngine::ReadHandler: STOPPING Gracefully at "
                    "Processor Request")
                  m_processor->Stop();
                  return -1;
                }
              )
            }
            // Data Frame Done!
            break;
          }
          //-----------------------------------------------------------------//
          // PING:                                                           //
          //-----------------------------------------------------------------//
          case 0x89:
            // Respond with a 2-byte PONG, with a trivial Mask (all-0s):
            m_processor->SendImpl(PongMsg, sizeof(PongMsg)-1);
            break;
          //-----------------------------------------------------------------//
          // PONG:                                                           //
          //-----------------------------------------------------------------//
          case 0x8a:
            break;
          //-----------------------------------------------------------------//
          // CLOSE:                                                          //
          //-----------------------------------------------------------------//
          case 0x88:
          {
            int         closeCode = 0;
            std::string closeMsg;   // Empty as yet; XXX: Inefficient!

            // PfxLen MUST be 2 for Close, and it must be Final, but we ignore
            // any errors here -- closing the session anyway:
            //
            if (utxx::likely(isFinal && pfxLen == 2  && dataLen >= 2))
            {
              // Extract the 2-byte Termination Code, and possibly the Msg:
              closeCode = (int(frame[2]) << 8) + int(frame[3]);
              closeMsg  = std::string(curr + 4,  size_t(dataLen - 2));
            }
            // Log the event:
            LOGA_INFO(m_processor->Processor, 1,
              "H2WS::WSProtoEngine::ReadHandler: Received WS CLOSE from Server"
              ": Code={}, Msg={}", closeCode, closeMsg)

            // Stop with Restart (FullStop=false):
            m_processor->template StopNow<false>
              ("WS Server Closed Session", a_ts_recv);
            return -1;
          }
          //-----------------------------------------------------------------//
          // Anything Else: UnRecognised:                                    //
          //-----------------------------------------------------------------//
          default:
            // Better close the session:
            LOGA_ERROR(m_processor->Processor, 1,
              "WS::PtotoEngine::ReadHandler: UnExpected WSFrame[0]: {}, "
              "RemBytes={}, STOPPING...", frame0, remBytes)
            m_processor->Stop();
            return -1;
        }
        // End of OpCode switch
      }
      // End of BeforeFrame / Frame
      //---------------------------------------------------------------------//
      // Finally: Adjust "curr":                                             //
      //---------------------------------------------------------------------//
      // If we got here, we must have made some progress:
      assert(next != nullptr && curr < next && next <= end);
      curr = next;
    }
    //=======================================================================//
    // End of parsing loop:                                                  //
    //=======================================================================//
    assert (a_buff <= curr && curr <= end);
    int consumed = int(curr - a_buff);

    // For an MDC, we need to invoke a Processor call-back on EndOfDataChunk:
    if constexpr (Processor::IsMDC)
    {
      static_assert(!Processor::IsMultiCast);
      if (consumed > 0)
        // This is a true EndOfDataChunk, invoke the Processor on that:
        // IsMultiCast=false, other flags are taken from the Processor:
        m_processor->template
          ProcessEndOfDataChunk
          <
            false,
            Processor::WithIncrUpdates,
            Processor::WithOrdersLog,
            Processor::WithRelaxedOBs,
            Processor::QT,
            typename Processor::QR
          >
          ();
    }
    // All Done. Return the number of bytes consumed:
    return consumed;
  }

  //=========================================================================//
  // "CheckHeaders":                                                         //
  //=========================================================================//
  // XXX: There are minor inefficiencies here, but they are acceptable as "map"
  // is used for hdrs anyway:
  //
  template<typename Processor>
  inline bool WSProtoEngine<Processor>::CheckHeaders() const
  {
    // Check the common hdrs which indicate (along with the Resp Line) upgrade
    // to the WS protocol:
    std::string const& connHdr = m_hdrs["Connection"];
    std::string const& upgrHdr = m_hdrs["Upgrade"];
    if (utxx::unlikely
       (strcasecmp(connHdr.data(), "Upgrade")   != 0 ||
        strcasecmp(upgrHdr.data(), "WebSocket") != 0))
    {
      LOGA_ERROR(m_processor->Processor, 2,
        "H2WS::WSProtoEngine::CheckHeaders: WS Upgrade Hdr(s) Missing")
      return false;
    }
    // Now check the WebSocket Key (to prevent improper caching). NB: If it was
    // missing altogether, an empty string will be installed now:
    //
    std::string const& wsKey = m_hdrs["Sec-WebSocket-Accept"];

    // Try to re-construct it from the saved SessKey:
    // Append a fixed UUID to SessKey:
    char  appKey[256];
    char* end =
      stpcpy(stpcpy(appKey, m_sessKey.data()),
             "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    int    appKeyLen = int(end  - appKey);
    assert(appKeyLen < int(sizeof(appKey)));

    // Compute the SHA1 hash (160 bit or 20 bytes) of the compound string --
    // use GNUTLS:
    uint8_t appSHA1[20];
    DEBUG_ONLY(int rc =)
      gnutls_hash_fast(GNUTLS_DIG_SHA1, appKey, size_t(appKeyLen), appSHA1);
    assert(rc == 0);

    // Encode the SHA1 hash as Base64:
    char   appB64[2 * sizeof(appSHA1)];
    int    appB64Len  = Base64EnCodeUnChecked(appSHA1, sizeof(appSHA1), appB64);
    assert(appB64Len < int(sizeof(appB64)-1));
    appB64[appB64Len] = '\0';

    // Finally, compare "appB64" with the "wsKey": They must be equal:
    if (utxx::unlikely(strcmp(appB64, wsKey.data()) != 0))
    {
      LOGA_ERROR(m_processor->Processor, 2,
        "H2WS::WSProtoEngine::CheckHeaders: WS Key MisMatch: Got={}, Expected="
        "{}", wsKey, appB64)
     return false;
    }
    // If we got here: All checks passed successfully:
    return true;
  }

  //=========================================================================//
  // "PutTxtData":                                                           //
  //=========================================================================//
  // Append "a_data" (which must be a 0-terminates text) to "m_outBuff", in
  // preparation for Sending the data out:
  //
  template<typename Processor>
  inline void WSProtoEngine<Processor>::PutTxtData(char const* a_data) const
  {
    assert(a_data != nullptr);

    // If OK: Copy the data into "m_outBuff"; beware of the reserved prefix
    // space and the data which might already be in the "m_outBuff" (of len
    // "m_outBuffDataLen"):
    char*  data   = m_outBuff   + OutBuffDataOff;
    char*  end    = stpcpy(data + m_outBuffDataLen, a_data);
    int    totLen =  int(end - data);
    assert(totLen >= 0);

    // The following call checks for buffer overflow and adjusts "m_outBuff-
    // DataLen":
    WrapTxtData(totLen);
  }

  template<typename       Processor>
  inline void WSProtoEngine<Processor>::PutTxtData(char const* a_data, size_t a_len) const
  {
    char*  data   = m_outBuff   + OutBuffDataOff;
    char*  end    = stpncpy(data + m_outBuffDataLen, a_data, a_len);
    int    totLen =  int(end - data);
    assert(totLen >= 0);
    WrapTxtData(totLen);
  }

  //=========================================================================//
  // "WrapTxtData":                                                          //
  //=========================================================================//
  // To be invoked after filling in "m_outBuff". Verifies and sets the buffer
  // length:
  //
  template<typename Processor>
  inline void WSProtoEngine<Processor>::WrapTxtData(int a_len) const
  {
    // XXX: We still require that the new buff len should be no less than the
    // previous one. The buff length is reset when the data are actually sent
    // out:
    assert(0 <= m_outBuffDataLen && m_outBuffDataLen <= a_len);

    // XXX: We check for a potential buffer over-flow a posteriori (after inc-
    // rementing "m_outBuffDataLen"):
    CHECK_ONLY
    (
      if (utxx::unlikely(a_len > 65535))
        throw utxx::runtime_error
              ("H2WS::WSProtoEngine::PutData: OutBuff OverFlow");
    )
    // OK:
    m_outBuffDataLen = a_len;
  }

  //=========================================================================//
  // "MkTxtFrame":                                                           //
  //=========================================================================//
  // Returns the ptr to the Frame beginning and the Frame length:
  //
  template<typename Processor>
  inline std::pair<char const*, int> WSProtoEngine<Processor>::MkTxtFrame()
  const
  {
    assert(m_outBuffDataLen >= 0);

    // The actual size of the prefix depends on DataLen. XXX: For the moment,
    // we do not support DataLen >= 64k. If PfxSz is only 6 (not 8), we start
    // the frame with a 2-byte offset from "m_outBuff":
    int   pfxSz = 6;
    char* frame = m_outBuff + 2;

    if (m_outBuffDataLen >= 126)
    {
      assert(m_outBuffDataLen <= 65535);
      pfxSz = 8;
      frame = m_outBuff;
    }
    // Fill in the Frame Prefix:
    // Byte0 is always the same: (FINAL, TEXT), ie XXX continuation frames are
    // not supported either:
    frame[0] = '\x81';

    if (pfxSz == 6)
      // Byte1 is 1<ActualLen>:
      frame[1] = char(0x80 | m_outBuffDataLen);
    else
    {
      // Byte1 is a fixed one (0x80 + 126):
      frame[1] = '\xFE';

      // Byte2 and Byte3 contain the actual data length in Network (Big Engian)
      // byte order:
      frame[2] = char(m_outBuffDataLen >> 8);    // Hi byte
      frame[3] = char(m_outBuffDataLen &  0xFF); // Lo byte
    }

    // The following 4 bytes (2..5 or 4..7) contain the random Mask:
    char* mask = frame + pfxSz - 4;
    int   rc   = 0;
    do    rc   = int(getrandom(mask, 4, 0));
    while(rc  != 4);

    // Apply the mask to the data:
    char* data = frame + pfxSz;
    for (int i = 0; i < m_outBuffDataLen; ++i)
      data[i] ^= mask[i & 0x3]; // i % 4

    // The frame is now ready for sending:
    return std::make_pair(frame, pfxSz + m_outBuffDataLen);
  }

  //=========================================================================//
  // "SendTxtFrame":                                                         //
  //=========================================================================//
  template<typename Processor>
  inline utxx::time_val WSProtoEngine<Processor>::SendTxtFrame() const
  {
    // Make the WS Frame from the txt data currently in "m_outBuff", with the
    // length given by "m_outBuffDataLen":
    std::pair<char const*, int> res = MkTxtFrame();

    // Actually send the Frame out, if there is anything to send, and memoise
    // the SendTS:
    assert(res.first != nullptr && res.second >= 0);
    utxx::time_val sendTS =
      utxx::likely(res.second > 0)
      ? m_processor->SendImpl(res.first,  res.second)
      : utxx::time_val();

    // Reset the data in "m_outBuff" after sending:
    m_outBuffDataLen = 0;

    // Return the sending TimeStamp:
    return sendTS;
  }
} // End namespace WS
} // End namespace MAQUETTE
