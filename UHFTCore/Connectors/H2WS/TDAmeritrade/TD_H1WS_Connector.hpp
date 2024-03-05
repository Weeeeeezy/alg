#pragma once

#include "TD_H1WS_Connector.h"

#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>
#include <string_view>

#include <curl/curl.h>
#include <rapidjson/document.h>
#include <utxx/convert.hpp>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>

#include "Basis/EPollReactor.h"
#include "Basis/IOUtils.hpp"
#include "Basis/Macros.h"
#include "Protocols/JSONParseMacros.h"

//===========================================================================//
// Macros:                                                                   //
//===========================================================================//
// Short-cuts for accessing the Flds and Methods of the "Derived" class:
// Non-Const:
#ifdef DER
#undef DER
#endif
#define DER static_cast<Derived *>(this)->Derived

// Const:
#ifdef CDER
#undef CDER
#endif
#define CDER static_cast<Derived const *>(this)->Derived

namespace MAQUETTE {

//===========================================================================//
// Non-Default Ctor:                                                         //
//===========================================================================//
template <typename Derived>
inline TD_H1WS_Connector<Derived>::TD_H1WS_Connector(
    std::string const &a_name, std::string const &a_server_hostname,
    int a_server_port, boost::property_tree::ptree const &a_params)
    : //---------------------------------------------------------------------//
      // "H2WS::WSProtoEngine":                                              //
      //---------------------------------------------------------------------//
      ProWS::WSProtoEngine(static_cast<Derived *>(this)),
      //---------------------------------------------------------------------//
      // "TCP_Connector":                                                    //
      //---------------------------------------------------------------------//
      TCPWS
      (
        a_name,
        GetH2WSIP(Derived::GetWSConfig(DER::m_cenv), a_params),
        a_server_port, // HTTPS port (normally 443)
        65536 * 1024,  // BuffSz  = 64M (hard-wired)
        65536,         // BuffLWM = 64k (hard-wired)
        a_params.get<std::string>("ClientIP", ""),
        a_params.get<int>("MaxConnAttempts", 5),
        10,            // LogOn  TimeOut, Sec (hard-wired)
        1,             // LogOut TimeOut, Sec (hard-wired)
        1,             // Re-Connect Interval, Sec (SMALL for WS and H2WS!)
        30,            // 30-second periodic "Ping"s
        true,          // Yes, use TLS!
        Derived::GetWSConfig(DER::m_cenv).m_HostName, // Required for TLS
        IO::TLS_ALPN::HTTP11,                         // WS is over HTTP/1.1
        a_params.get<bool>("VerifyServerCert", true),
        a_params.get<std::string>("ServerCAFiles", ""),
        a_params.get<std::string>("ClientCertFile", ""),
        a_params.get<std::string>("ClientPrivKeyFile", ""),
        a_params.get<std::string>("ProtoLogFile", "")
      ),
      m_source_id(a_params.get<std::string>("SourceID")),
      m_account_id(a_params.get<std::string>("AccountID")),
      m_client_id(a_params.get<std::string>("ClientID")),
      m_qos_level(a_params.get<int>("QOSLevel", 0)), m_rxSN(DER::m_RxSN),
      m_txSN(DER::m_TxSN), m_curl(curl_easy_init()),
      m_root_url("https://" + a_server_hostname + ":" +
                 std::to_string(a_server_port)),
      m_refresh_token(a_params.get<std::string>("RefreshToken")) {
  assert(m_rxSN != nullptr && m_txSN != nullptr);

  curl_global_init(CURL_GLOBAL_ALL);

  curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, RecvBufferCallback);
  curl_easy_setopt(m_curl, CURLOPT_WRITEDATA,  &m_recv_buffer);

  curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, RecvHeaderCallback);
  curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &m_location);

  /* require use of SSL for this, or fail */
  curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

  /* enable TCP keep-alive for this transfer */
  curl_easy_setopt(m_curl, CURLOPT_TCP_KEEPALIVE, 1L);

  /* provide a buffer to store errors in */
  curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, m_curl_err_buf);

  //-----------------------------------------------------------------------//
  // Optionally, create the Logger for Raw HTTP Msgs:                      //
  //-----------------------------------------------------------------------//
  auto proto_log_file = a_params.get<std::string>("ProtoLogFile", "");
  if (!proto_log_file.empty()) {
    m_protoLoggerShP =
        IO::MkLogger(a_name + "-Raw-HTTP", proto_log_file + "-HTTP");
    m_protoLogger = m_protoLoggerShP.get();
    assert(m_protoLogger != nullptr);

    curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, CURLDebug);
    curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, m_protoLogger);
  }

  SetAccessTokenTimer();
}

template <typename Derived>
inline TD_H1WS_Connector<Derived>::~TD_H1WS_Connector() noexcept {
  curl_easy_cleanup(m_curl);
  curl_slist_free_all(m_out_headers);
  curl_slist_free_all(m_out_headers_json);
  curl_global_cleanup();
}

//=========================================================================//
// Starting, Stopping and some "TCP_Connector" Call-Backs:                 //
//=========================================================================//
//-------------------------------------------------------------------------//
// "Start":                                                                //
//-------------------------------------------------------------------------//
// It only initiates the TCP connection -- no msgs are sent at this time (the
// latter is done by "Derived::InitLogOn"):
//
template <typename Derived> inline void TD_H1WS_Connector<Derived>::Start() {
  // In case if it is a RE-Start, do a clean-up first (incl the subscr info):
  DER::ResetOrderBooksAndOrders();

  // Start the "TCP_Connector" (which will really initiate a TCP Conn):
  TCPWS::Start();
}

//-------------------------------------------------------------------------//
// "Stop" (graceful, external, virtual):                                   //
//-------------------------------------------------------------------------//
template <typename Derived> inline void TD_H1WS_Connector<Derived>::Stop() {
  // Try to FullStop the "TCP_Connector". Normally, the Stop is graceful (not
  // immediate), but it could sometimes turn into immediate; FullStop=true.
  // TCP_Connector will call back "InitGracefulStop" and "GracefulStopInProg-
  // ress":
  (void)TCPWS::template Stop<true>();
}

//-------------------------------------------------------------------------//
// "IsActive":                                                             //
//-------------------------------------------------------------------------//
template <typename Derived>
inline bool TD_H1WS_Connector<Derived>::IsActive() const {
  return TCPWS::IsActive();
}

//-------------------------------------------------------------------------//
// "LogMsg":                                                               //
//-------------------------------------------------------------------------//
// Logs the Msg content (presumably JSON or similar) in ASCII:
//
template <typename Derived>
template <bool IsSend>
inline void TD_H1WS_Connector<Derived>::LogMsg(char const *a_msg,
                                               char const *a_comment,
                                               long a_val) const {
  if (TCPWS::m_protoLogger != nullptr) {
    // The comment and the int param may or may not be present. The latter,
    // if present, is part of the comment:
    if (a_comment != nullptr && *a_comment != '\0') {
      if (utxx::likely(a_val == 0))
        TCPWS::m_protoLogger->info("{} {} {}", a_comment,
                                   IsSend ? "-->" : "<==", a_msg);
      else
        TCPWS::m_protoLogger->info("{}{} {} {}", a_comment, a_val,
                                   IsSend ? "-->" : "<==", a_msg);
    } else
      TCPWS::m_protoLogger->info("{} {}", IsSend ? "-->" : "<==", a_msg);

    // XXX: Flushing the logger may be expensive, but will ensure that import-
    // ant msgs are not lost in case of a crash:
    TCPWS::m_protoLogger->flush();
  }
}

//-------------------------------------------------------------------------//
// "SendHeartBeat":                                                        //
//-------------------------------------------------------------------------//
template <typename Derived>
inline void TD_H1WS_Connector<Derived>::SendHeartBeat() const {
  // Send the "PING" msg:
  (void)TCPWS::SendImpl(ProWS::PingMsg, sizeof(ProWS::PingMsg) - 1);
}

//-------------------------------------------------------------------------//
// "ServerInactivityTimeOut":                                              //
//-------------------------------------------------------------------------//
template <typename Derived>
inline void
TD_H1WS_Connector<Derived>::ServerInactivityTimeOut(utxx::time_val a_ts_recv) {
  // Stop immediately, but will try to re-start (FullStop=false):
  StopNow<false>("ServerInactivityTimeOut", a_ts_recv);
}

//-------------------------------------------------------------------------//
// "StopNow":                                                              //
//-------------------------------------------------------------------------//
template <typename Derived>
template <bool FullStop>
inline void TD_H1WS_Connector<Derived>::StopNow(char const *a_where,
                                                utxx::time_val a_ts_recv) {
  assert(a_where != nullptr);

  // DisConnect the "TCP_Connector", it should become "Inactive":
  TCPWS::template DisConnect<FullStop>(a_where);
  assert(TCPWS::IsInactive());

  // Remove the PusherTask Timer (NoOp if it is not active):
  DER::RemovePusherTimer();

  // For safety, once again invalidate the OrderBooks (because "StopNow" is
  // not always called sfter "Stop"), incl the subscr info:
  DER::ResetOrderBooksAndOrders();

  // Notify the Strategies (On=false):
  DER::ProcessStartStop(false, nullptr, 0, a_ts_recv, a_ts_recv);

  LOGA_INFO(CDER, 1, "TD_H1WS_Connector({}): Connector STOPPED{}", a_where,
            FullStop ? "." : ", but will re-start")
}

//-------------------------------------------------------------------------//
// "SessionInitCompleted":                                                 //
//-------------------------------------------------------------------------//
template <typename Derived>
inline void TD_H1WS_Connector<Derived>::SessionInitCompleted(
    utxx::time_val UNUSED_PARAM(a_ts_recv)) {
  // HTTP/WS Session Init is done. Notify the "TCP_Connector" about that. It
  // will invoke "InitLogOn" on us:
  TCPWS::SessionInitCompleted();
}

//-------------------------------------------------------------------------//
// "LogOnCompleted":                                                       //
//-------------------------------------------------------------------------//
template <typename Derived>
inline void
TD_H1WS_Connector<Derived>::LogOnCompleted(utxx::time_val a_ts_recv) {
  // Inform the "TCP_Connector" that the LogOn has been successfully comple-
  // ted. This in particular will cancel the Connect/SessionInit/LogOn Time-
  // Out:
  TCPWS::LogOnCompleted();

  // At this stage, the over-all Connector status becomes "Active":
  assert(IsActive());

  // perform WS initialization
  DER::OnTDWSLogOn();

  // Notify the Strategies that the Connector is now on-line. XXX:  RecvTime
  // is to be used for ExchTime as well, since we don't have the exact value
  // for the latter:
  DER::ProcessStartStop(true, nullptr, 0, a_ts_recv, a_ts_recv);

  LOGA_INFO(CDER, 1,
            "TD_H1WS_Connector::LogOnCompleted: Connector is now ACTIVE")

  // DEBUG:
  // read data from disk to inject
  // {
  //   std::ifstream ifs("/home/jlippuner/bl_spartan/maquette/tda/l1.in");
  //   std::string line;
  //   while (std::getline(ifs, line)) {
  //     if (line.size() > 0)
  //       m_inject_data.push_back(line);
  //   }

  //   // TimerHandler:
  //   IO::FDInfo::TimerHandler timerH([this](int DEBUG_ONLY(a_fd)) -> void {
  //     assert(a_fd == m_inject_fd);
  //     if (m_inject_idx < m_inject_data.size()) {
  //       const auto &dat = m_inject_data[m_inject_idx++];
  //       this->ProcessWSMsg(dat.c_str(), int(dat.size()), false, utxx::now_utc(),
  //                          utxx::now_utc());
  //     }
  //   });

  //   // ErrorHandler:
  //   IO::FDInfo::ErrHandler errH([this](int a_fd, int a_err_code, uint32_t a_events,
  //                                  char const *a_msg) -> void {
  //     this->AccessTokenTimerErrHandler(a_fd, a_err_code, a_events, a_msg);
  //   });

  //   // Create the m_access_token_timer_FD and add it to the Reactor:
  //   m_inject_fd = DER::m_reactor->AddTimer("Injector",
  //                                          10, // Initial offset
  //                                          10, // Period
  //                                          timerH, errH);
  // }
}

//-------------------------------------------------------------------------//
// "InitGracefulStop":                                                     //
//-------------------------------------------------------------------------//
template <typename Derived>
inline void TD_H1WS_Connector<Derived>::InitGracefulStop() {
  // Cancel all active orders (to be safe and polite; we may also have auto-
  // matic Cancelon-Disconnect):
  static_cast<Derived *>(this)->CancelAllOrders(0, nullptr,
                                                FIX::SideT::UNDEFINED, nullptr);

  // TODO should we send UNSUBS?
  SendWSReq("ADMIN", "LOGOUT", "");
}

//-------------------------------------------------------------------------//
// "GracefulStopInProgress":                                               //
//-------------------------------------------------------------------------//
template <typename Derived>
inline void TD_H1WS_Connector<Derived>::GracefulStopInProgress() const {
  // Send a "CLOSE" msg to the Server:
  (void)TCPWS::SendImpl(ProWS::CloseMsg, sizeof(ProWS::CloseMsg) - 1);
}

//-------------------------------------------------------------------------//
// "InitSession":                                                          //
//-------------------------------------------------------------------------//
template <typename Derived>
inline void TD_H1WS_Connector<Derived>::InitSession() {
  LOGA_INFO(CDER, 2, "TD_H1WS_Connector::InitSession")
  ProWS::InitWSHandShake("/ws");
}

//-------------------------------------------------------------------------//
// "InitLogOn":                                                            //
//-------------------------------------------------------------------------//
template <typename Derived>
inline void TD_H1WS_Connector<Derived>::InitLogOn() {
  LOGA_INFO(CDER, 2, "TD_H1WS_Connector::InitLogOn")

  std::string login_params;

  if (!GET("/v1/userprincipals?fields=streamerSubscriptionKeys"
           "%2CstreamerConnectionInfo")) {
    throw utxx::runtime_error("TD_H1WS_Connector::InitLogOn: Failed, Msg=",
                              std::string(GetResponse()));
  } else {
    // not performance critical, so use proper JSON parser
    try {
      rapidjson::Document json;
      json.Parse(GetResponse());

      const auto &sinfo = json["streamerInfo"];

      const rapidjson::Document::ValueType *accnt_ptr = nullptr;
      for (const auto &a : json["accounts"].GetArray()) {
        if (a["accountId"].GetString() == CDER::m_account_id) {
          accnt_ptr = &a;
          break;
        }
      }

      if (accnt_ptr == nullptr)
        throw utxx::runtime_error("Couldn't find account id in");
      const auto &accnt = *accnt_ptr;

      // convert timestamp to unix timestamp in ms
      tm time;
      // 2020-10-07T22:50:09+0000
      std::string time_str(sinfo["tokenTimestamp"].GetString());
      if ((time_str.size() != 24) || (time_str.substr(19, 5) != "+0000"))
        throw utxx::runtime_error("Invalid timestamp '", time_str, "'");

      auto beg = time_str.c_str();
      auto end = strptime(beg, "%Y-%m-%dT%H:%M:%S+0000", &time);
      if (end != (beg + 24))
        throw utxx::runtime_error("Failed to parse timestamp '", time_str, "'");

      std::stringstream creds;
      creds << "userid=" << CDER::m_account_id // must be account id, not
                                               // json["userId"].GetString()
            << "&token=" << sinfo["token"].GetString()
            << "&company=" << accnt["company"].GetString()
            << "&segment=" << accnt["segment"].GetString()
            << "&cddomain=" << accnt["accountCdDomainId"].GetString()
            << "&usergroup=" << sinfo["userGroup"].GetString()
            << "&accesslevel=" << sinfo["accessLevel"].GetString()
            << "&authorized=Y"
            << "&acl=" << sinfo["acl"].GetString()
            << "&timestamp=" << utxx::mktime_utc(&time) * 1000L
            << "&appid=" << sinfo["appId"].GetString();

      auto escaped_creds = curl_easy_escape(m_curl, creds.str().c_str(), 0);

      login_params = "\"token\":\"" + std::string(sinfo["token"].GetString()) +
                     "\",\"version\":\"1.0\",\"credential\":\"" +
                     std::string(escaped_creds) + "\"";

      curl_free(escaped_creds);

      auto key = json["streamerSubscriptionKeys"]
                     .GetObject()["keys"]
                     .GetArray()[0]["key"]
                     .GetString();

      m_subscription_key = std::string(key);

      LOGA_INFO(CDER, 2,
                "TD_H1WS_Connector::InitLogOn: "
                "Successful")
    } catch (std::exception const &ex) {
      throw utxx::runtime_error(
          "TD_H1WS_Connector::InitLogOn: Failed, Exception=",
          std::string(ex.what()), ", Msg=", std::string(GetResponse()));
    }
  }

  login_params += ",\"qoslevel\"=\"" + std::to_string(m_qos_level) + "\"";

  SendWSReq("ADMIN", "LOGIN", login_params.c_str());
}

//=======================================================================//
// HTTP/1 I/O:                                                           //
//=======================================================================//
template <typename Derived>
inline size_t
TD_H1WS_Connector<Derived>::RecvBufferCallback(void *contents, size_t size,
                                               size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  RecvBuffer *buf = reinterpret_cast<RecvBuffer*>(userp);

  char *ptr = reinterpret_cast<char*>
    (realloc(buf->ptr, buf->size + realsize + 1));
  if (ptr == nullptr)
    throw std::runtime_error("Out of memory");

  buf->ptr = ptr;
  memcpy(&(buf->ptr[buf->size]), contents, realsize);
  buf->size += realsize;
  buf->ptr[buf->size] = 0;

  return realsize;
}

template <typename Derived>
inline size_t
TD_H1WS_Connector<Derived>::RecvHeaderCallback(char *buffer, size_t size,
                                               size_t nitems, void *userdata) {
  size_t realsize = size * nitems;

  if (strncmp(buffer, "Location:", std::min(realsize, size_t(9))) == 0) {
    // got the Location header, store it
    auto loc = reinterpret_cast<char*>(userdata);
    strncpy(loc, buffer, realsize);
    auto end = loc + realsize;
    *end = 0;

    // remove trailing \n
    while (isspace(*(--end)))
      *end = 0;
  }

  return realsize;
}

template <typename Derived>
inline int TD_H1WS_Connector<Derived>::CURLDebug(CURL * /*handle*/,
                                                 curl_infotype type, char *data,
                                                 size_t size, void *userp) {
  const char *text;

  switch (type) {
  case CURLINFO_TEXT:
    text = "== Info: ";
    break;
  default: /* in case a new one is introduced to shock us */
    return 0;

  case CURLINFO_HEADER_OUT:
    text = "=> Send header:\n";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data:\n";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data:\n";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header:\n";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data:\n";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data:\n";
    break;
  }

  // data is non-null terminated
  std::string_view dat_str(data, size);
  spdlog::logger *log = reinterpret_cast<spdlog::logger*>(userp);
  log->info("{}{}", text, dat_str);

  return 0;
}

template <typename Derived>
inline bool TD_H1WS_Connector<Derived>::SendReq(const char *a_path) {
  m_recv_buffer.Reset();
  m_curl_err_buf[0] = 0;
  m_location[0] = 0;

  auto url = m_root_url + std::string(a_path);
  curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());

  auto ret = curl_easy_perform(m_curl);
  if (ret != CURLE_OK) {
    auto len = strlen(m_curl_err_buf);
    LOGA_ERROR(CDER, 1, "TD_H1WS_Connector::SendReq: CURL Error: {}",
               len > 0 ? m_curl_err_buf : curl_easy_strerror(ret))
    m_recv_buffer.Reset();
    return false;
  }

  // check HTTP status code
  curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &m_status_code);
  if ((m_status_code != 200) && (m_status_code != 201)) {
    LOGA_WARN(CDER, 2, "TD_H1WS_Connector::SendReq: Got HTTP code {}",
              m_status_code)
    return false;
  }

  return true;
}

template <typename Derived>
inline bool TD_H1WS_Connector<Derived>::POST(const char *a_path, bool a_use_put,
                                             bool a_not_json) {
  curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
  curl_easy_setopt(m_curl, CURLOPT_NOBODY, 0L);
  curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, m_H1_out_buf);
  curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER,
                   a_not_json ? m_out_headers : m_out_headers_json);

  if (a_use_put)
    curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "PUT");

  auto success = SendReq(a_path);

  curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, nullptr);
  curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, nullptr);
  m_H1_out_buf[0] = 0;

  return success;
}

template <typename Derived>
inline bool TD_H1WS_Connector<Derived>::GET(const char *a_path) {
  curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(m_curl, CURLOPT_NOBODY, 0L);
  curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_out_headers);
  return SendReq(a_path);
}

template <typename Derived>
inline bool TD_H1WS_Connector<Derived>::DELETE(const char *a_path) {
  curl_easy_setopt(m_curl, CURLOPT_NOBODY, 1L);
  curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_out_headers);

  auto success = SendReq(a_path);

  curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, nullptr);

  return success;
}

//=======================================================================//
// Authentication:                                                       //
//=======================================================================//

template <typename Derived>
inline void TD_H1WS_Connector<Derived>::SetAccessTokenTimer() {
  // Just in case, first remove the Timer if it exists:
  RemoveAccessTokenTimer();
  assert(m_access_token_timer_FD < 0);

  // TimerHandler:
  IO::FDInfo::TimerHandler timerH([this](int DEBUG_ONLY(a_fd)) -> void
  {
    assert(a_fd == m_access_token_timer_FD);
    this->UpdateAccessToken();
  });

  // ErrorHandler:
  IO::FDInfo::ErrHandler errH([this]
    (int a_fd, int a_err_code, uint32_t a_events, char const *a_msg) -> void
  {
    this->AccessTokenTimerErrHandler(a_fd, a_err_code, a_events, a_msg);
  });

  // Period: 25 min = 1500 sec = 1500000 msec:
  constexpr uint32_t USTimerPeriodMSec = 1'500'000;

  // Create the m_access_token_timer_FD and add it to the Reactor:
  m_access_token_timer_FD =
      DER::m_reactor->AddTimer("UpdateAccessToken",
                               0,                 // Initial offset
                               USTimerPeriodMSec, // Period
                               timerH, errH);
}

template <typename Derived>
inline void TD_H1WS_Connector<Derived>::RemoveAccessTokenTimer() {
  if (m_access_token_timer_FD >= 0) {
    if (utxx::likely(CDER::m_reactor != nullptr))
      DER::m_reactor->Remove(m_access_token_timer_FD);

    // Anyway, close the socket:
    (void)close(m_access_token_timer_FD);
    m_access_token_timer_FD = -1;
  }
}

template <typename Derived>
inline void TD_H1WS_Connector<Derived>::AccessTokenTimerErrHandler(
    int a_fd, int a_err_code, uint32_t a_events, char const *a_msg) {
  // There should be NO errors associated with this Timer at all. If one occ-
  // urs, we better shut down completely:
  assert(a_fd == m_access_token_timer_FD);
  LOGA_ERROR(CDER, 1,
             "TD_H1WS_Connector::AccessTokenTimerErrHandler: "
             "TimerFD={}, ErrCode={}, Events={}, Msg={}: STOPPING",
             a_fd, a_err_code, CDER::m_reactor->EPollEventsToStr(a_events),
             (a_msg != nullptr) ? a_msg : "")
  DER::Stop();
}

template <typename Derived>
inline void TD_H1WS_Connector<Derived>::UpdateAccessToken() {
  // clear any old headers
  curl_slist_free_all(m_out_headers);
  curl_slist_free_all(m_out_headers_json);

  m_out_headers = nullptr;
  m_out_headers_json = nullptr;

  curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, nullptr);

  {
    auto curr = m_H1_out_buf;
    curr = stpcpy(curr, "grant_type=refresh_token&refresh_token=");
    char *url_enc_refresh_token =
        curl_easy_escape(m_curl, m_refresh_token.c_str(), 0);
    curr = stpcpy(curr, url_enc_refresh_token);
    curl_free(url_enc_refresh_token);
    curr = stpcpy(curr, "&access_type=&code=&client_id=");
    curr = stpcpy(curr, m_client_id.c_str());
    curr = stpcpy(curr, "%40AMER.OAUTHAP&redirect_uri=");
  }

  if (!POST("/v1/oauth2/token", false, true)) {
    LOGA_ERROR(CDER, 1,
               "TD_H1WS_Connector::UpdateAccessToken: Failed to get access "
               "token, Msg=[{}]",
               GetResponse())
    DER::Stop();
    return;
  }

  auto a_msg_body = GetResponse();
  auto a_msg_len = int(GetResponseLen());
  assert(a_msg_body != nullptr && a_msg_len > 0 &&
         a_msg_body[a_msg_len] == '\0');

  char const *curr = a_msg_body;
  CHECK_ONLY(char *end = const_cast<char *>(a_msg_body + a_msg_len);)
  CHECK_ONLY(assert(curr < end);)

  // extract access_token
  try {
    SKP_STR("{\n  \"access_token\" : \"")
    GET_STR(token)
    m_access_token = std::string(token);

    CHECK_ONLY({
      curr += 4;
      SKP_STR("\"scope\" : \"")
      GET_STR(scope)
      curr += 4;
      SKP_STR("\"expires_in\" : ")
      int expire = -1;
      curr = (utxx::fast_atoi<int, false>)(curr, end, expire);
      curr += 4;
      SKP_STR("\"token_type\" : \"")
      GET_STR(type)

      if (expire != 1800)
        LOGA_WARN(CDER, 2,
                  "TD_H1WS_Connector::UpdateAccessToken: Unexpected "
                  "expire time {}",
                  expire)

      if (strcmp(type, "Bearer") != 0)
        LOGA_WARN(CDER, 2,
                  "TD_H1WS_Connector::UpdateAccessToken: Unexpected "
                  "token type {}",
                  type)

      LOGA_INFO(CDER, 3,
                "TD_H1WS_Connector::UpdateAccessToken: Got access "
                "token with scope={}, expire={}, type={}",
                scope, expire, type)
    })
  } catch (std::exception const &ex) {
    LOGA_ERROR(CDER, 1, "TD_H1WS_Connector::UpdateAccessToken: {} in Msg=[{}]",
               ex.what(), a_msg_body)
    DER::Stop();
    return;
  }

  std::string header = "Authorization: Bearer " + m_access_token;
  m_out_headers = curl_slist_append(m_out_headers, header.c_str());
  m_out_headers_json = curl_slist_append(m_out_headers_json, header.c_str());
  m_out_headers_json =
      curl_slist_append(m_out_headers_json, "Content-Type: application/json");

  LOGA_INFO(CDER, 2, "TD_H1WS_Connector::UpdateAccessToken: Successful")
}

//=======================================================================//
// WS I/O:                                                               //
//=======================================================================//
//-----------------------------------------------------------------------//
// "SendWSReq":                                                          //
//-----------------------------------------------------------------------//
// Send a request over the WebSocket
// Only Login/Logout and data subscription requests are supported by TD.
// Trades are done over the HTTP interface.
//
template <typename Derived>
inline void TD_H1WS_Connector<Derived>::SendWSReq(const char *a_service,
                                                  const char *a_command,
                                                  const char *a_params) {
  // Start the msg, appending it to any previously-output ones:
  auto data = DER::m_outBuff + DER::OutBuffDataOff;
  auto chunk = data + DER::m_outBuffDataLen;
  auto curr = chunk;

  curr = stpcpy(curr, "{\"requests\":[{");
  curr = stpcpy(curr, "\"service\":\"");
  curr = stpcpy(curr, a_service);
  curr = stpcpy(curr, "\",\"requestid\":\"");
  utxx::itoa(m_ws_req_id++, curr);
  curr = stpcpy(curr, "\",\"command\":\"");
  curr = stpcpy(curr, a_command);
  curr = stpcpy(curr, "\",\"account\":\"");
  curr = stpcpy(curr, m_account_id.c_str());
  curr = stpcpy(curr, "\",\"source\":\"");
  curr = stpcpy(curr, m_source_id.c_str());
  curr = stpcpy(curr, "\",\"parameters\":{");
  curr = stpcpy(curr, a_params);
  curr = stpcpy(curr, "}}]}");

  DER::WrapTxtData(int(curr - data));
  CHECK_ONLY(
      DER::template LogMsg<true>(chunk, "  [WS] OUT REQ");) // IsSend=true

  // "m_txSN" increment is for compatibility with "EConnector_OrdMgmt" only;
  // it is not really used here:
  (*m_txSN)++;
  DER::SendTxtFrame();
}

//=========================================================================//
// "ProcessWSMsg":                                                         //
//=========================================================================//
// Processing of  msgs received via WebSocket occurs here. The method returns
// "true" to indicate continuation, "false" to stop:
//
template <typename Derived>
inline bool TD_H1WS_Connector<Derived>::ProcessWSMsg(
    char const *a_msg_body, int a_msg_len, bool UNUSED_PARAM(a_last_msg),
    utxx::time_val a_ts_recv, utxx::time_val a_ts_handl) {
  //-----------------------------------------------------------------------//
  // Get the Msg:                                                          //
  //-----------------------------------------------------------------------//
  // NB: "a_msg_body" should be 0-terminated by the Caller:
  assert(a_msg_body != nullptr && a_msg_len > 0 &&
         a_msg_body[a_msg_len] == '\0');

  char const *curr = a_msg_body;
  char *end = const_cast<char *>(a_msg_body + a_msg_len);

  CHECK_ONLY({
    LOGA_INFO(CDER, 4, "TD_H1WS_Connector::ProcessWSMsg: Received msg={}",
              a_msg_body)
    DER::template LogMsg<false>(a_msg_body, "  [WS]  IN DAT");
  })

  try {
    SKP_STR("{")
    bool is_data;
    if (utxx::likely(strncmp(curr, "\"notify\":[{\"heartbeat\"", 22) == 0)) {
      // this is a heartbeat, nothing to do
      return true;
    } else if (strncmp(curr, "\"data\":[", 8) == 0) {
      is_data = true;
      curr += 8;
    } else if (strncmp(curr, "\"response\":[", 12) == 0) {
      is_data = false;
      curr += 12;
    } else {
      ++curr;
      GET_STR(type)
      LOGA_WARN(CDER, 2,
                "TD_H1WS_Connector::ProcessWSMsg: Skipping "
                "unknown message type {}",
                type)
      return true;
    }

    // process each message
    curr = strchr(curr, '{');
    while (curr != nullptr) {
      SKP_STR("{\"service\":\"")
      auto next = strchr(curr, '"');
      if (next == nullptr || int(next - a_msg_body) >= a_msg_len) {
        printf("Oops\n");
      }
      GET_STR(service)

      size_t timestamp = 0;
      if (is_data) {
        SKP_STR(
            ", \"timestamp\":") // Yes, there is a space, only for "data"...
        curr = utxx::fast_atoi<size_t, false>(curr, end, timestamp);
        utxx::time_val ts_exch = utxx::msecs(timestamp);
        SKP_STR(",\"command\":\"")
        GET_STR(command)
        SKP_STR(",\"content\":[")

        // find matching ]
        int n_open = 1; // num of open [
        auto end_content = curr;
        while (n_open > 0) {
          if (*end_content == '[')
            ++n_open;
          else if (*end_content == ']')
            --n_open;
          ++end_content;

          if (end_content == end) {
            end_content = nullptr;
            break;
          }
        }
        assert(end_content != nullptr);
        --end_content; // point to final ']'
        auto len = size_t(end_content - curr);
        *const_cast<char *>(end_content) = '\0';
        if (!DER::ProcessWSData(service, command, curr, ts_exch, a_ts_recv,
                                a_ts_handl))
          return false;

        curr += len + 1;
      } else {
        SKP_STR(",\"requestid\":\"")
        curr = SkipNFlds<1>(curr);
        SKP_STR("\"command\":\"")
        GET_STR(command)
        SKP_STR(",\"timestamp\":")
        curr = utxx::fast_atoi<size_t, false>(curr, end, timestamp);
        SKP_STR(",\"content\":{")

        int code = -1;
        SKP_STR("\"code\":")
        curr = utxx::fast_atoi<int, false>(curr, end, code);
        SKP_STR(",\"msg\":\"")
        GET_STR(message)

        if (code != 0) {
          // command failed
          LOGA_ERROR(CDER, 1,
                     "TD_H1WS_Connector WS request FAILED: "
                     "{}/{} => {}: {}",
                     service, command, code, message)
        } else {
          // process success messages
          LOGA_INFO(CDER, 2, "TD_H1WS_Connector WS request {}/{} SUCCEEDED",
                    service, command)

          if ((strcmp(service, "ADMIN") == 0) &&
              (strcmp(command, "LOGIN") == 0)) {
            LOGA_INFO(CDER, 2, "TD_H1WS_Connector: Login SUCCEEDED: {}",
                      message)
            LogOnCompleted(a_ts_recv);
          }
        }
      }

      // go to next message (if any)
      curr = strchr(curr, '{');
    }
  } catch (EPollReactor::ExitRun const &) {
    // This exception is propagated: It is not an error:
    throw;
  } catch (std::exception const &exn) {
    // Log the exception and stop:
    LOGA_ERROR(CDER, 1, "TD_H1WS_Connector::ProcessWSMsg: {}", exn.what())
    return false;
  }
  return true;
}

} // namespace MAQUETTE
