// vim:ts=2:et
//===========================================================================//
//                     "Protocols/H2WS/WSProtoEngine.h":                     //
//                    Generic WebSocket Protocol Support                     //
//===========================================================================//
#pragma once

#include <utxx/time_val.hpp>
#include <boost/core/noncopyable.hpp>
#include <string>
#include <map>

namespace MAQUETTE
{
namespace H2WS
{
  //=========================================================================//
  // "WSProtoEngine" Class:                                                  //
  //=========================================================================//
  // NB: There is no separate "SessMgr" here; "Processor" itself is sufficient.
  // Processor must provide the following methods:
  //
  // void ProcessWSMsg
  // (
  //   char const*    a_msg,
  //   int            a_msg_len,
  //   utxx::time_val a_ts_recv,
  //   utxx::time_val a_ts_handl
  // );
  //
  template<typename Processor>
  class WSProtoEngine: public boost::noncopyable
  {
  protected:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    Processor*           m_processor;      // Ptr NOT owned!
    // Buffers:
    constexpr static int InBuffCap       = 1 << 20;
    mutable char*        m_inBuff;         // Assembling incoming Frames here
    mutable int          m_inBuffLen;      // Length of the curr InBuff content
    // OutBuff: at most 8 bytes pfx + max(64k) data, although using the full 64k
    // data size is undesirable due to IP frame fragmentation:
    constexpr static int OutBuffDataOff  = 8;
    constexpr static int OutBuffCap      = OutBuffDataOff + 65536;
    char*                m_outBuff;
    mutable int          m_outBuffDataLen; // Curr DataLen (w/o pfx) in OutBuff
    mutable std::string  m_sessKey;        // Base64-encoded Session Key

    // HTTP1.1/WS protocol data (ResponseStatus, Headers etc). It is OK to store
    // Hdrs as a map because they are received at the beginning only:
    mutable bool         m_beforeResp;     // Just awaitng the 1st response?
    struct hdrs_cmp : std::binary_function<std::string, std::string, bool>
    {
      bool operator()(const std::string& a, const std::string& b) const
        { return strcasecmp(a.c_str(), b.c_str()) < 0; }
    };
    mutable std::map<std::string, std::string, hdrs_cmp> m_hdrs;

    //-----------------------------------------------------------------------//
    // XXX: Some Elementary WS Msgs:                                         //
    //-----------------------------------------------------------------------//
    // "CLOSE" Msg to the Server with the Code=1000=0x03E8:
    // There is no need for any non-trivial Mask here; use all-0s:
    //
    constexpr static char CloseMsg[] = "\x88\x82\x00\x00\x00\x00\x03\xE8";

    // "PING" and "PONG" Msgs:
    constexpr static char PingMsg [] = "\x89\x80\x00\x00\x00\x00";
    constexpr static char PongMsg [] = "\x8A\x80\x00\x00\x00\x00";

    //=======================================================================//
    // Ctors, Dtor:                                                          //
    //=======================================================================//
    // Default Ctor is deleted:
    WSProtoEngine() = delete;

    // Non-Default Ctor:
    WSProtoEngine(Processor* a_processor);

    // Dtor (not "virtual" because there are no virtual methods here):
    ~WSProtoEngine() noexcept;

    //=======================================================================//
    // WS-Specific Methods:                                                  //
    //=======================================================================//
    void InitWSHandShake(char const* a_path) const;

    //=======================================================================//
    // HTTP/WS Protocol Handling:                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "ReadHandler":                                                        //
    //-----------------------------------------------------------------------//
    // Returns the number of bytes "consumed" (ie successfully read and process-
    // ed, in particular 0 if the msg was incomplete),  or (-1) if there was an
    // error:
    int ReadHandler
    (
      int             a_fd,
      char const*     a_buff,    // Points  to the dynamic buffer of Reactor
      int             a_size,    // Size of the data chunk available in Buff
      utxx::time_val  a_ts_recv
    );

    //-----------------------------------------------------------------------//
    // "PutTxtData":                                                         //
    //-----------------------------------------------------------------------//
    // Append the "a_data" to "m_outBuff"; "a_data" must be a 0-terminated text
    // string. If this function is used (as opposed to direct access to "m_out-
    // Buff", there is no need to invoke "WrapTxtData":
    //
    void PutTxtData(char const* a_data) const;
    void PutTxtData(char const* a_data, size_t a_len) const;

    //-----------------------------------------------------------------------//
    // "WrapTxtData":                                                        //
    //-----------------------------------------------------------------------//
    // Assuming that the data of length "a_len"  is already in the "m_outBuff"
    // (beginnig at the offset "OutBuffDataOff"), checks for "m_outBuff" over-
    // flow and adjusts "m_outBuffDataLen":
    //
    void WrapTxtData(int a_len) const;

    //-----------------------------------------------------------------------//
    // "MkTxtFrame":                                                         //
    //-----------------------------------------------------------------------//
    // Make a Text WS Frame from the data currently in "m_outBuff", ready for
    // being sent out. Returns (FramePtr, FrameLen):
    //
    std::pair<char const*, int> MkTxtFrame() const;

    //-----------------------------------------------------------------------//
    // "SendTxtFrame":                                                       //
    //-----------------------------------------------------------------------//
    // Invokes "MkTxtFrame" and then actually sends the constructed Frame out.
    // Returns the sending TimeStamp:
    //
    utxx::time_val SendTxtFrame() const;

    //=======================================================================//
    // Helpers:                                                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "CheckHeaders": Verifies WSKey etc:                                   //
    //-----------------------------------------------------------------------//
    bool CheckHeaders() const;
  };
} // End namespace WS
} // End namespace MAQUETTE
