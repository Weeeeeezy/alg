// vim:ts=2:et
//===========================================================================//
//                  "Connectors/H2WS/Binance/Sender_MDC.hpp":                //
//                  H2WS-Based MDC for Binance: Sending Orders               //
//===========================================================================//
#pragma  once
#include "Connectors/H2WS/Binance/EConnector_H2WS_Binance_MDC.h"
#include "Connectors/H2WS/EConnector_H2WS_MDC.hpp"
#include <utxx/convert.hpp>
#include <nghttp2/nghttp2.h>
#include <ctime>

namespace MAQUETTE
{
  //=========================================================================//
  // "SendReq":                                                              //
  //=========================================================================//
  // Construct the Headers, sign the Req and and send out the  Headers Frame.
  // NB: Binance MDC currently does not use the  Data Frame -- all Req Params
  // are encoded via the Path in the Headers Frame. Returns the StreamID:
  //
  template <Binance::AssetClassT::type AC>
  inline uint32_t EConnector_H2WS_Binance_MDC<AC>::SendReq
  (
    char const* a_method,     // "GET", "PUT", "POST" or "DELETE"
    char const* a_path        // If NULL, m_path is used (via outHdrs[3])
  )
  {
    assert(a_method != nullptr);

    if (utxx::unlikely(MDCH2WS::m_currH2Conn == nullptr))
      throw utxx::runtime_error
            ("EConnector_H2WS_Binance_MDC::SendReq: No Active H2Conns");

    //-----------------------------------------------------------------------//
    // Header Values:                                                        //
    //-----------------------------------------------------------------------//
    auto& outHdrsFr = MDCH2WS::m_currH2Conn->m_outHdrsFrame; // Ref!
    auto& outHdrs   = MDCH2WS::m_currH2Conn->m_outHdrs;      // ditto

    // Generic HTTP/2 Part:
    // [2]: Method:
    outHdrs[2].value    = const_cast<uint8_t*>
                            (reinterpret_cast<uint8_t const*>(a_method));
    outHdrs[2].valuelen = strlen(a_method);

    // [3]: Path: Must already be in, as outHdrs[3], unless a_path is given:
    uint8_t* &pathRef     = outHdrs[3].value;
    size_t   &pathLenRef  = outHdrs[3].valuelen;

    if (utxx::unlikely(a_path != nullptr && *a_path != '\0'))
    {
      // Point "pathRef" to "a_path":
      pathRef    = const_cast<uint8_t*>
                  (reinterpret_cast<uint8_t const*>(a_path));
      pathLenRef = strlen(a_path);
    }
    else
      assert(pathRef != nullptr && pathLenRef > 0);

    //-----------------------------------------------------------------------//
    // Deflate the Headers and put the resulting string into Hdrs Frame:     //
    //-----------------------------------------------------------------------//
    // NB: There are currently 5 Hdrs being sent in Binance. The compressed Hdrs
    // come at offset 0 in the Data(PayLoad) section of the HdrsFrame as we do
    // NOT use Paddings or Priorities:
    //
    constexpr size_t NHeaders = 4;
    long outLen =
      nghttp2_hd_deflate_hd
      (
        MDCH2WS::m_currH2Conn->m_deflater,
        reinterpret_cast<uint8_t*>(outHdrsFr.m_data),
        size_t(H2Conn::MaxOutHdrsDataLen),
        outHdrs,
        NHeaders
      );
    // Check for compression failure or overflow:
    CHECK_ONLY
    (
      if (utxx::unlikely(outLen <= 0 || outLen >= H2Conn::MaxOutHdrsDataLen))
        throw utxx::runtime_error
              ("EConnector_H2WS_MDC::SendReq: Invalid CompressedHdrsLen=",
               outLen, ": Valid Range=1..", H2Conn::MaxOutHdrsDataLen-1);
    )
    //-----------------------------------------------------------------------//
    // Fill in all flds of the Headers Frame, and send it out:               //
    //-----------------------------------------------------------------------//
    outHdrsFr.SetDataLen(int(outLen));
    outHdrsFr.m_type = H2Conn::H2FrameTypeT::Headers;

    // END_HEADERS (0x04) and END_STREAM (0x01) are always set, the latter be-
    // cause there is no Data Frame. No PADDED or PRIORITY flags are currently
    // used:
    outHdrsFr.m_flags = 0x05;

    //-----------------------------------------------------------------------//
    // Log it:                                                               //
    //-----------------------------------------------------------------------//
    CHECK_ONLY
    (
      MDCWS::template LogMsg<true>
      (
        const_cast<const char*>(reinterpret_cast<char*>(outHdrs[3].value)),
        a_method
      );
    )
    // Set the StreamID(computed automatically):
    uint32_t streamID = MDCH2WS::m_currH2Conn->StreamIDofReqID(0);
    outHdrsFr.SetStreamID(streamID);

    // Send the Headers Frame out (IsData = false); TimeStamp is ignored here:
    (void) MDCH2WS::m_currH2Conn->template SendFrame<false>();
    return streamID;
  }
}
// End namespace MAQUETTE
