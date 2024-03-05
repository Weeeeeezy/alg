#pragma once

#include "spdlog/details/null_mutex.h"
#include "spdlog/sinks/base_sink.h"

#include <mutex>
#include <string>

#include "IOSendEmail.hpp"

namespace spdlog {
namespace sinks {
/*
 * Send log messages to email address
 */
template <typename Mutex> class email_sink final : public base_sink<Mutex> {
public:
  explicit email_sink(
      const std::shared_ptr<MAQUETTE::IO::IOSendEmail> &a_sender)
      : m_sender(a_sender) {}

protected:
  void sink_it_(const details::log_msg &msg) override {
    memory_buf_t formatted;
    base_sink<Mutex>::formatter_->format(msg, formatted);
    m_buffer += std::string(formatted.data());
  }

  void flush_() override {
    if (m_buffer != "") {
      m_sender->Send("[MAQUETTE] Error", m_buffer);
      m_buffer = "";
    }
  }

private:
  std::string m_buffer;
  std::shared_ptr<MAQUETTE::IO::IOSendEmail> m_sender;
};

using email_sink_mt = email_sink<std::mutex>;
using email_sink_st = email_sink<details::null_mutex>;

} // namespace sinks
} // namespace spdlog
