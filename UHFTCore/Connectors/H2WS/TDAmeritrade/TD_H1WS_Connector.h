#pragma once

#include <stdexcept>
#include <string>

#include <boost/core/noncopyable.hpp>
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include <utxx/time_val.hpp>

#include "Basis/BaseTypes.hpp"
#include "Connectors/H2WS/Configs.hpp"
#include "Connectors/TCP_Connector.hpp"
#include "Protocols/H2WS/WSProtoEngine.hpp"

namespace MAQUETTE {

//=========================================================================//
// "TD_H1WS_Connector" Class:                                              //
//=========================================================================//
//
// TD_H1WS_Connector handles sending HTTP/1.1 requests to the TDAmeritrade
// API and synchronously getting responses back. It also handles the access
// and refresh tokens and automatically updates the access token before it
// expires.
//
// NB: "Derived" class implements the following functionality:
// (*) EPollReactor, Logger, ...
// (*) State / Session Mgmt;
// (*) Application-Level Processing (invoked from ReadHandler):
// (*) In case of multi-level inheritance, "Derived" should normally be the
//     BOTTOM ("final") class in the hierarchy.
//
// More precisely, the following API is assumed from "Derived":
//     m_name
//     m_debugLevel
//     m_logger
//     m_reactor
//==========================================================================//
template <typename Derived>
class TD_H1WS_Connector : public H2WS::WSProtoEngine<Derived>,
                          public TCP_Connector<Derived>
// NB: It must indeed be  TCP_Connector<Derived>,
//                    NOT TCP_Connector<EConnector_WS_OMC<Derived>>,
// because some Call-Backs required by TCP_Connector are defined in the
// ultimate "Derived"!
{
protected:
  //=======================================================================//
  // Types and Consts:                                                     //
  //=======================================================================//
  using ProWS = H2WS::WSProtoEngine<Derived>;
  friend class H2WS::WSProtoEngine<Derived>;

  using TCPWS = TCP_Connector<Derived>;
  friend class  TCP_Connector<Derived>;

  // XXX: We assume that BatchSend functionality  (buffering several raw msgs
  // and sending them out at once) is never available in WS or H2WS OMCs, due
  // due to the structure of H2 and WS Frames, and the presence of TLS:
  //
  constexpr static bool HasBatchSend = false;

  //=======================================================================//
  // Non-Default Ctor, Dtor:                                               //
  //=======================================================================//
  TD_H1WS_Connector(std::string const &a_name,
                    std::string const &a_server_hostname, int a_server_port,
                    boost::property_tree::ptree const &a_params);

  // The Dtor must still be "virtual" -- this class is NOT "final" yet:
  virtual ~TD_H1WS_Connector() noexcept;

  //-----------------------------------------------------------------------//
  // "Start", "Stop", Properties:                                          //
  //-----------------------------------------------------------------------//
  void Start();
  void Stop();
  bool IsActive() const;

  //=======================================================================//
  // Data Flds:                                                            //
  //=======================================================================//
  const std::string m_source_id;  // TD Login name
  const std::string m_account_id; // TD account number
  const std::string m_client_id;  // client ID of the app, not account ID
  const int m_qos_level;          // update frequency (0 fastest, 5 slowest)

  std::string m_subscription_key; // received at init

  // NB: "m_[TR]xSN" (pointing to ShM) are provided by "EConnector_OrdMgmt";
  // but as this class provides  SessionMgmt functionality  required  by the
  // ProtoEngine, we also need to provide similar ptrs "m_[tx]SN" here:
  SeqNum *m_rxSN;
  SeqNum *m_txSN;

  //=======================================================================//
  // Protocol-Level Logging:                                               //
  //=======================================================================//
  // Logger for raw protocol data:
  std::shared_ptr<spdlog::logger> m_protoLoggerShP;
  spdlog::logger *m_protoLogger = nullptr; // Direct raw ptr (alias)

  // Log the Msg content (presumably JSON or similar) in ASCII.  NB: The int
  // val is considered to be part of the comment, so is logged only if it is
  // non-0 and the comment is non-NULL and non-empty:
  //
  template <bool IsSend = true>
  void LogMsg(char const *a_msg, char const *a_comment = nullptr,
              long a_val = 0) const;

protected:
  //=======================================================================//
  // Call-Backs/Forwards for the static interface of "TCP_Connector":      //
  //=======================================================================//
  //  "SendHeartBeat": It might be a good idea to send periodic  "Ping" msgs
  //  to the Server to keep the session alive (in addition to TCP-level Keep-
  //  Alive in "TCP_Connector"):
  void SendHeartBeat() const;

  //  "ServerInactivityTimeOut":
  void ServerInactivityTimeOut(utxx::time_val a_ts_recv);

  //  "StopNow":
  template <bool FullStop>
  void StopNow(char const *a_where, utxx::time_val a_ts_recv);

  //  "SessionInitCompleted":
  void SessionInitCompleted(utxx::time_val a_ts_recv);

  //  "LogOnCompleted": Notifies the Strategies that the Connector is ACTIVE:
  void LogOnCompleted(utxx::time_val a_ts_recv);

  //  "InitGracefulStop": In particular, Cancel all Active Orders:
  void InitGracefulStop();

  //  "GracefulStopInProgress": Sends "Close" to the Server:
  void GracefulStopInProgress() const;

  //  "InitSession":
  void InitSession();

  // "InitLogOn":
  void InitLogOn(); // => InitWSLogOn()

  // NB: "ReadHandler" is implemented in "Proto", and invokes
  // Derived::ProcessWSMsg()

private:
  //=======================================================================//
  // HTTP/1 I/O:                                                           //
  //=======================================================================//
  struct RecvBuffer {
    RecvBuffer() {
      // alloc some initial buffer, may grow
      ptr  = new char[65536];
      size = 0;
    }

    ~RecvBuffer() { delete[] ptr; }

    void Reset() {
      ptr[0] = 0;
      size = 0;
    }

    char *ptr;
    size_t size;
  };

  static size_t RecvBufferCallback(void *contents, size_t size, size_t nmemb,
                                   void *userp);

  static size_t RecvHeaderCallback(char *buffer, size_t size, size_t nitems,
                                   void *userdata);

  CURL *m_curl = nullptr;
  const std::string m_root_url;
  curl_slist *m_out_headers = nullptr;
  curl_slist *m_out_headers_json = nullptr;
  char m_curl_err_buf[CURL_ERROR_SIZE];

  RecvBuffer m_recv_buffer;
  char m_location[CURL_MAX_HTTP_HEADER];
  long m_status_code;

  static int CURLDebug(CURL *handle, curl_infotype type, char *data,
                       size_t size, void *userp);

  // send a prepared request, the response will be in m_recv_buffer, return true
  // if successful (meaning no CURL error and no HTTP error)
  bool SendReq(const char *a_path);

protected:
  constexpr static int MaxH1OutDataDataLen = 65536;
  char m_H1_out_buf[MaxH1OutDataDataLen];

  // Perform HTTP requests, all return true if successful, output data (if any)
  // will be available via GetResponse()

  // POST the data in m_H1_out_buf, optionally using PUT instead of POST
  bool POST(const char *a_path, bool a_use_put = false,
            bool a_not_json = false);
  bool PUT(const char *a_path) { return POST(a_path, true); }
  bool GET(const char *a_path);
  bool DELETE(const char *a_path);

  const char *GetResponse() const { return m_recv_buffer.ptr; }
  auto GetResponseLen() const { return m_recv_buffer.size; }
  const char *GetLocation() const { return m_location; }
  long GetStatusCode() const { return m_status_code; }

private:
  //=======================================================================//
  // Authentication:                                                       //
  //=======================================================================//
  // We get the client id and refresh_token from the input file. With the
  // refresh token we can obtain an access token, that is valid for 30
  // minutes. We need to get a new access token before the current one
  // expires. The refresh token is good for 90 days, for now, we won't
  // bother renewing it, because we don't currently have a mechanism to
  // output the new one.
  int m_access_token_timer_FD = -1;

  const std::string m_refresh_token;
  std::string m_access_token;

  void SetAccessTokenTimer();
  void RemoveAccessTokenTimer();
  void AccessTokenTimerErrHandler(int a_fd, int a_err_code, uint32_t a_events,
                                  char const *a_msg);

  void UpdateAccessToken();

  // DEBUG:
  // int m_inject_fd = -1;
  // std::vector<std::string> m_inject_data;
  // size_t m_inject_idx = 0;

protected:
  //=======================================================================//
  // WS I/O:                                                               //
  //=======================================================================//
  //-----------------------------------------------------------------------//
  //  "SendWSReq": Send a WebSocket request                                //
  //-----------------------------------------------------------------------//
  SeqNum m_ws_req_id = 0;
  void SendWSReq(const char *a_service, const char *a_command,
                 const char *a_params);

  //-----------------------------------------------------------------------//
  //  "ProcessMsg": "H2WS::WSProtoEngine" Call-Back):                      //
  //-----------------------------------------------------------------------//
  // Returns "true" to continue receiving msgs, "false" to stop:
  bool ProcessWSMsg(char const *a_msg_body, int a_msg_len, bool a_last_msg,
                    utxx::time_val a_ts_recv, utxx::time_val a_ts_handl);
};

} // namespace MAQUETTE
