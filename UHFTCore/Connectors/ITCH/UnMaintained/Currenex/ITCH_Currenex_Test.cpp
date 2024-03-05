// vim:ts=2:expandtab
//===========================================================================//
//                        "CurrenexItchTest.cpp":                            //
//===========================================================================//
#include <string>
#include <boost/test/unit_test.hpp>
#include <Connectors/ITCH_MsgBuilder_Currenex.hpp>

#include <utxx/meta.hpp>
#include <utxx/string.hpp>

using namespace ITCH;
using namespace Arbalete;
using namespace std;

namespace
{
  struct TestCase {
    char const*                name;
    const std::vector<uint8_t> data;
  };

  const TestCase s_Tests[] = {
    {"Logon",   {
      0x01,0x00,0x00,0x00,0x01,0x00,0x55,0x70,0xD8,0x41,0x69,0x69,
      0x69,0x69,0x69,0x69,0x74,0x63,0x68,0x20,0x20,0x20,0x20,0x20,
      0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
      0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
      0x20,0x20,0x00,0x00,0x75,0x31,0x03
    }},
    {"Logout", {
      0x01,0x00,0x00,0x00,0x02,0x04,0xfb,0xa7,0xe7,0x42,0x69,0x69,
      0x69,0x69,0x69,0x69,0x74,0x63,0x68,0x20,0x20,0x20,0x20,0x20,
      0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x75,0x31,0x20,0x20,
      0x20,0x03,
      0x01,0x00,0x00,0x00,0x02,0x04,0xfb,0xa7,0xe7,0x42,0x69,0x69,
      0x69,0x69,0x69,0x69,0x74,0x63,0x68,0x20,0x20,0x20,0x20,0x20,
      0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x75,0x31,0x41,0x35,
      0x20,0x03
    }},
    {"Heartbeat", {
      0x01,0x00,0x00,0x00,0x03,0x00,0x55,0x70,0xFE,0x43,0x00,0x00,
      0x00,0x02,0x03
    }},
    {"InstrumentInfo", {
      0x01,0x00,0x00,0x00,0x04,0x00,0x55,0x70,0xDA,0x44,0x00,0x00,
      0x75,0x31,0x00,0x01,0x31,0x41,0x55,0x44,0x2F,0x43,0x41,0x44,
      0x2D,0x53,0x50,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
      0x20,0x00,0x00,0x01,0x46,0xDD,0x32,0x4E,0x00,0x03
    }},
    {"SubscrReply", {
      0x01,0x00,0x00,0x00,0x05,0x00,0x55,0x70,0xFD,0x47,0x00,0x00,
      0x75,0x31,0x00,0x01,0x31,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
      0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
      0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
      0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
      0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x03
    }},
    {"PriceMsg", {
      0x01,0x00,0x00,0x00,0x06,0x00,0x55,0x70,0xFE,0x48,0x00,0x01,
      0x00,0x00,0x00,0x02,0x31,0x00,0x00,0x00,0x00,0x05,0xF5,0xE1,
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x88,
      0xE2,0x32,0x20,0x20,0x20,0x20,0x03
    }},
    {"PriceCancel", {
      0x01,0x00,0x00,0x00,0x07,0x00,0x55,0x70,0xFE,0x49,0x00,0x01,
      0x00,0x00,0x00,0x02,0x03
    }}
  };

  struct CurrenexItchProcessor
  {
    typedef MsgBuilder<ExchangeT::Currenex, CurrenexItchProcessor> Parser;

    virtual ~CurrenexItchProcessor() {}

    char const* testName;

    template <class T>
    SecDef& UpdateSecDefMaps(std::string const&,T) { static SecDef s; return s; }
    std::map<int, SecDef> m_SecIDsMap;

    CurrenexItchProcessor& Connector()              { return *this; }
    bool                   IsEnvironment(EnvType e) { return e == EnvType::TEST;}
    bool                   NoShM()          const   { return true;  }
    int                    DebugLevel()  const   { return 0;     }

    void onData
    (
      logon  const&         m,
      Parser const*         p,
      utxx::time_val        recv_time,
      utxx::time_val        ts
    )
    {
      BOOST_REQUIRE_EQUAL("Logon",      testName);
      BOOST_REQUIRE_EQUAL("iiiiiitch",  m.user_id().to_string(' '));
      BOOST_REQUIRE_EQUAL("",           m.password().to_string(' '));
      BOOST_REQUIRE_EQUAL(30001,        m.session_id());
      char const* q = reinterpret_cast<char const*>(&m)+sizeof(m);
      BOOST_REQUIRE_EQUAL(1,            p->iseqno());
      BOOST_REQUIRE_EQUAL('\3', *q);
      BOOST_REQUIRE(p->loggedIn());
    }

    void onData
    (
      logout_req const&     m,
      Parser const*         p,
      utxx::time_val        recv_time,
      utxx::time_val        ts
    )
    {
      static int s_instance;

      BOOST_REQUIRE_EQUAL("Logout",                 testName);
      BOOST_REQUIRE_EQUAL("iiiiiitch",              m.user_id().to_string(' '));
      BOOST_REQUIRE_EQUAL(30001,                    m.session_id());
      if (s_instance++)
        BOOST_REQUIRE_EQUAL(utxx::to_underlying
                              (logout_req::logout_reason_t::AUTH_FAILURE),
                            utxx::to_underlying(m.logout_reason()));
      else
        BOOST_REQUIRE_EQUAL(utxx::to_underlying
                              (logout_req::logout_reason_t::UNDEFINED),
                            utxx::to_underlying(m.logout_reason()));

      BOOST_REQUIRE      (!p->loggedIn());
      BOOST_REQUIRE_EQUAL(2,                        p->iseqno());
      char const* q = reinterpret_cast<char const*>(&m)+sizeof(m);
      BOOST_REQUIRE_EQUAL('\3', *q);
    }

    void onData
    (
      heartbeat const&      m,
      Parser const*         p,
      utxx::time_val        recv_time,
      utxx::time_val        ts
    )
    {
      BOOST_REQUIRE_EQUAL("Heartbeat",  testName);
      BOOST_REQUIRE_EQUAL(2,            m.session_id());
      BOOST_REQUIRE_EQUAL(3,            p->iseqno());
      char const* q = reinterpret_cast<char const*>(&m)+sizeof(m);
      BOOST_REQUIRE_EQUAL('\3', *q);
    }

    void onData
    (
      instr_info const&     m,
      Parser const*         p,
      utxx::time_val        recv_time,
      utxx::time_val        ts
    )
    {
      BOOST_REQUIRE_EQUAL("InstrumentInfo", testName);
      BOOST_REQUIRE_EQUAL(30001,            m.session_id());
      BOOST_REQUIRE_EQUAL(1,                m.instr_index());
      BOOST_REQUIRE_EQUAL(utxx::to_underlying
                            (instr_info::instr_type_t::FOREX),
                          utxx::to_underlying(m.instr_type()));
      BOOST_REQUIRE_EQUAL(4,                p->iseqno());
      char const* q = reinterpret_cast<char const*>(&m)+sizeof(m);
      BOOST_REQUIRE_EQUAL('\3', *q);
    }

    void onData
    (
      subscr_reply const&   m,
      Parser const*         p,
      utxx::time_val        recv_time,
      utxx::time_val        ts
    )
    {
      BOOST_REQUIRE_EQUAL("SubscrReply",                  testName);
      BOOST_REQUIRE_EQUAL(30001,                          m.session_id());
      BOOST_REQUIRE_EQUAL(1,                              m.instr_index());
      BOOST_REQUIRE_EQUAL(utxx::to_underlying
                            (subscr_reply::sub_rep_type_t::SUBSCR_ACCEPTED),
                          utxx::to_underlying(m.sub_rep_type()));
      BOOST_REQUIRE_EQUAL("", m.sub_rep_reason().to_string(' '));
      BOOST_REQUIRE_EQUAL(5,                              p->iseqno());
      char const* q = reinterpret_cast<char const*>(&m)+sizeof(m);
      BOOST_REQUIRE_EQUAL('\3', *q);
    }

    void onData
    (
      price_msg const&      m,
      Parser const*         p,
      utxx::time_val        recv_time,
      utxx::time_val        ts
    )
    {
      BOOST_REQUIRE_EQUAL("PriceMsg",               testName);
      BOOST_REQUIRE_EQUAL(6,                        p->iseqno());
      BOOST_REQUIRE_EQUAL(1,                        m.instr_index());
      BOOST_REQUIRE_EQUAL(2,                        m.price_id());
      BOOST_REQUIRE_EQUAL(utxx::to_underlying
                            (price_msg::side_t::BID),
                          utxx::to_underlying(m.side()));
      BOOST_REQUIRE_EQUAL(1000000.0,                m.max_amount());
      BOOST_REQUIRE_EQUAL(0.0,                      m.min_amount());
      BOOST_REQUIRE_EQUAL(10.0578,                  m.price());
      BOOST_REQUIRE_EQUAL(utxx::to_underlying
                            (price_msg::attributed_t::NOT_ATTRIBUTED),
                          utxx::to_underlying(m.attributed()));
      BOOST_REQUIRE_EQUAL("    ",m.price_provider().to_string());
      char const* q = reinterpret_cast<char const*>(&m)+sizeof(m);
      BOOST_REQUIRE_EQUAL('\3', *q);
    }

    void onData
    (
      price_cancel const&   m,
      Parser const*         p,
      utxx::time_val        recv_time,
      utxx::time_val        ts
    )
    {
      BOOST_REQUIRE_EQUAL("PriceCancel", testName);
      BOOST_REQUIRE_EQUAL(7,             p->iseqno());
      BOOST_REQUIRE_EQUAL(1,             m.instr_index());
      BOOST_REQUIRE_EQUAL(2,             m.price_id());
      char const* q = reinterpret_cast<char const*>(&m)+sizeof(m);
      BOOST_REQUIRE_EQUAL('\3', *q);
    }

    // All other types: Catch-All template:
    template <class Msg>
    void onData
    (
      Msg const&    m,
      Parser const* p,
      utxx::time_val        recv_time,
      utxx::time_val        ts
    )
    {}
  };

  BOOST_AUTO_TEST_CASE( test_currenex_itch )
  {
    CurrenexItchProcessor         proc;
    CurrenexItchProcessor::Parser parser(&proc, 1);  // Debug mode

    for
    (
      TestCase const* p = s_Tests, *e = s_Tests + utxx::length(s_Tests);
      p != e;
      ++p
    ) {
      proc.testName = p->name;
      BOOST_MESSAGE("Test case: " << p->name);
      parser.copy((const char*)&p->data[0], p->data.size());

      try
      {
        parser.processEvents(parser.fd());
      }
      catch (...)
      {
        throw;
      }
    }

  }
} // namespace

