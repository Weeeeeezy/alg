#pragma once

#if WITH_VMIME
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <boost/algorithm/string.hpp>
#include <vmime/vmime.hpp>

namespace {

class DummyVerifier : public vmime::security::cert::certificateVerifier {
public:
  // this function seems to exist with a const ref to the shard_ptr on one
  // machine but without a ref on another, so just provide both versions
  void verify(const std::shared_ptr<vmime::security::cert::certificateChain>
                  & /*chain*/,
              const std::string & /*hostname*/) override
  {
    return;
  }

  /*
  void verify(const std::shared_ptr<vmime::security::cert::certificateChain>
              chain,
              const std::string & hostname) {
    return;
  }
  */
};

} // namespace

namespace MAQUETTE {
namespace IO {

class IOSendEmail {
public:
  IOSendEmail(const std::string &a_recipients) : m_recipients(a_recipients) {
    if (m_recipients == "")
      return;

    auto session = vmime::net::session::create();
    m_tr = session->getTransport(vmime::utility::url(m_server));
    m_tr->setProperty("connection.tls", true);
    m_tr->setProperty("auth.username", m_sender);
    m_tr->setProperty("auth.password", m_password);
    m_tr->setProperty("options.need-authentication", true);
    m_tr->setCertificateVerifier(std::make_shared<DummyVerifier>());
  }

  void Send(const std::string &a_subject, const std::string &a_text) {
    if (m_recipients == "")
      return;

    try {
      vmime::messageBuilder mb;
      mb.setExpeditor(vmime::mailbox(m_sender));

      std::vector<std::string> recipients;
      boost::split(recipients, m_recipients, boost::is_any_of(","));

      vmime::addressList to;
      for (auto &r : recipients) {
        boost::algorithm::trim(r);
        to.appendAddress(std::make_shared<vmime::mailbox>(r));
      }
      mb.setRecipients(to);

      mb.setSubject(vmime::text(a_subject));
      mb.getTextPart()->setText(
          vmime::make_shared<vmime::stringContentHandler>(a_text));

      m_tr->connect();
      m_tr->send(mb.construct());
      m_tr->disconnect();
    } catch (std::exception &ex) {
      fprintf(stderr, "ERROR: Failed to send email: %s\n", ex.what());
    }
  }

private:
  constexpr static auto m_sender = "no-reply@lippuner.ca";
  constexpr static auto m_server = "smtp://smtp.googlemail.com:587";
  constexpr static auto m_password =
      "8&vJXbhsWPjexCW<@Ey@obw-4pp8LEBaWRNjiw=ipM?#>r4*oqYa<rYUB<m66Bp2";

  std::string m_recipients;
  std::shared_ptr<vmime::net::transport> m_tr;
};

} // namespace IO
} // namespace MAQUETTE
#endif // WITH_VMIME
