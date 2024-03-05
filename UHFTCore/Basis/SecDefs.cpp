// vim:ts=2:et
//===========================================================================//
//                              "Basis/SecDefs.cpp":                         //
//                  Generic Security (Instrument) Definitions:               //
//===========================================================================//
#include "Basis/SecDefs.h"
#include "Basis/Maths.hpp"
#include "Basis/XXHash.hpp"
#include <cstring>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "SecDefS": Non-Default Ctor:                                            //
  //=========================================================================//
  // For static initialisation, we provide a full Non-Default Ctor:
  // NB: "ExpirePrec" is omitted from the Ctor args list, because it is a
  // function of (ExpireDate, ExpireTime):
  SecDefS::SecDefS
  (
    SecID                 a_sec_id,
    char const*           a_symbol,
    char const*           a_alt_symbol,
    char const*           a_sec_desc,
    char const*           a_cfi_code,
    char const*           a_exchange,
    char const*           a_segment,
    char const*           a_tenor,
    char const*           a_tenor2,
    char const*           a_asset_a,
    char const*           a_quote_ccy,     // AKA AssetB
    char                  a_qty_type,      // 'A'|'B'
    double                a_ab_qty,
    double                a_lot_size,
    long                  a_min_size_lots,
    double                a_px_step,       // In Points (same units as RawPx)
    char                  a_px_type,       // 'A'|'C'
    double                a_point_px,
    int                   a_expire_date,   // As YYYYMMDD
    int                   a_expire_time,   // Intra-day secs in UTC
    double                a_strike,
    SecID                 a_underlying_sec_id,
    char const*           a_underlying_symbol
  )
  : // Time-Constant Data Flds:
    m_SecID               (a_sec_id),
    m_Symbol              (),
    m_AltSymbol           (),
    m_SecurityDesc        (),
    m_CFICode             (),
    m_Exchange            (),
    m_SessOrSegmID        (),
    m_Tenor               (),
    m_Tenor2              (),
    m_AssetA              (),
    m_QuoteCcy            (),
    m_ABQtys              { a_qty_type == 'A' ? a_ab_qty : NaN<double>,
                            a_qty_type == 'B' ? a_ab_qty : NaN<double> },
    m_LotSize             (a_lot_size),
    m_MinSizeLots         (a_min_size_lots),
    m_PxStep              (a_px_step),
    // NB:
    // (*) ContractPx can be quoted per unit of 'A' OR per Contract ('C') as de-
    //     termined by "a_px_type";
    // (*) ContractPx  may be expressed in some "Points" rather than in units of
    //     'B' itself;
    // (*) "a_point_px" arg is #B in 1 Point;
    // (*) the stored "m_PxFactAB" is a coeff which translates the Quoted Contr
    //     Px into the normalised Px(A/B) (that is, #B equivalent to 1A);
    // (*) however, if ContractQty is specified in units of 'B', not 'A',  (see
    //     "a_qty_type"), that is, m_ABQtys[0]==0, then PxFactAB is  UNDEFINED:
    //     a check below throws an exception:
    m_PxFactAB            ((a_px_type == 'A')
                           ? a_point_px : (a_point_px / m_ABQtys[0])),
    m_ExpireDate          (a_expire_date),
    m_ExpireTime          (a_expire_time),
    m_ExpirePrec          (),                // Computed below
    m_Strike              (a_strike),
    m_UnderlyingSecID     (a_underlying_sec_id),
    m_UnderlyingSymbol    ()
  {
    // Verify the data:
    // NB: 0s below are OK for non-tradable securities, eg Baskets or Indices.
    // EXACTLY ONE of "m_ABQtys[0,1] must be valid (ie positive):
    if (utxx::unlikely
       (a_symbol    == nullptr || *a_symbol   == '\0' ||
        a_exchange  == nullptr || *a_exchange == '\0' ||
        (a_qty_type != 'A' &&  a_qty_type != 'B')     ||
        (a_px_type  != 'A' &&  a_px_type  != 'C')     ||
        m_PxStep   <  0.0      || m_LotSize < 0       || m_MinSizeLots < 0 ||
       !((m_ABQtys[0] > 0.0) ^ (m_ABQtys[1] > 0.0))   ))
      throw utxx::badarg_error
            ("SecDefS::Ctor: Symbol=", a_symbol, ": Invalid param(s)");

    // NB: "m_PxFactAB" would be undefined in just one case: PxType=='C' (not
    // 'A') and ABQtys[0] is NaN (ie QtyType=='B') (provided, of course, that
    // "a_point_px" is valid).  An invalid "m_PxFactAB" is prohibited;  it is
    // always required to be finite and positive:
    assert(a_px_type == 'A' || a_qty_type == 'A' || !IsFinite(m_PxFactAB));

    if (utxx::unlikely(!(IsFinite(m_PxFactAB) && m_PxFactAB > 0.0)))
      throw utxx::badarg_error
            ("SecDefS::Ctor: Symbol=", a_symbol, ": UnDefined PxFactAB");

    // Now copy the strings over explicitly:
    InitFixedStr  (&m_Symbol,           a_symbol);
    InitFixedStr  (&m_AltSymbol,        a_alt_symbol);
    StrNCpy<true> (m_SecurityDesc,      a_sec_desc);
    StrNCpy<true> (m_CFICode,           a_cfi_code);
    InitFixedStr  (&m_Exchange,         a_exchange);
    InitFixedStr  (&m_SessOrSegmID,     a_segment);
    InitFixedStr  (&m_Tenor,            a_tenor);
    InitFixedStr  (&m_Tenor2,           a_tenor2);
    InitFixedStr  (&m_AssetA,           a_asset_a);
    InitFixedStr  (&m_QuoteCcy,         a_quote_ccy);
    InitFixedStr  (&m_UnderlyingSymbol, a_underlying_symbol);

    // NB: ExpireTime make sense even if ExpireDate is ot set,  because the
    // latter can be adjusted dynamically from "SettlDate" (see "SecDefD").
    // The "ExpireTime" is UTC intra-day seconds, but applied to local mkts.
    // So theoretically, it could be negative (for Eastern mkts)  or beyond
    // 1d (for Western mkts).
    // Eg, Chicago is CST/CDT with UTC offset: -6h/-5h,  so any time after
    // 18:00 CST would be next day UTC. Negative offsets are very unlikely,
    // however (eg the Eastern-most is New Zeland; NZST/NZDT is UTC +12h /
    // +13h, local closing time is 17:00, which is 04:00 UTC as the earliest):
    // TODO: Full proper TimeZone support:
    if (utxx::unlikely(m_ExpireTime < 0))
      throw utxx::badarg_error
            ("SecDefS::Ctor: Invalid ExpireTime=", m_ExpireTime);

    // ExpireDate (Maturity or Expiration Date):
    if (m_ExpireDate != 0)
    {
      if (utxx::unlikely(m_ExpireDate < EarliestDate))
        throw utxx::badarg_error
              ("SecDefS::Ctor: Invalid ExpireDate=", m_ExpireDate);

      // If "ExpireDate" is available, also set "ExpirePrec". If "ExpireTime"
      // is not available (0), set it at the END of the trading day (in UTC)
      // rather that at the beginning. TODO: Proper time zones mgmt:
      //
      if (m_ExpireTime == 0)
          m_ExpireTime = 86359;   // 23:59:59

      m_ExpirePrec =
        DateToTimeValFAST<true>(m_ExpireDate) + utxx::secs(m_ExpireTime);
    }

    // XXX: If the Asset is empty (as is typically the case for Non-FX
    // Instrs), then we use the Instrument (merely Symbol) itself as Asset.
    // Such Instrs are considered to be Non-FX ones; all others (ie those
    // with pre-configured non-trivial Assets) are candidates into FX ones:
    if (IsEmpty(m_AssetA))
    {
      m_AssetA = m_Symbol;
      m_IsFX   = false;
    }
    else
      m_IsFX   = true;

    // However, in order to qualify for the FX Instr, we also require the corr
    // CFICode and a 3-letter Ccy code for the Asset. TODO:  Provide a list of
    // ISO 4217 Ccy Codes  -- but we  then need to add some non-standard codes
    // as well (eg CNH), Crypto-Ccy and Commodity Codes: so not done yet:
    m_IsFX &=
      ((strncmp(m_CFICode, "MRC", 3) == 0  ||
        strncmp(m_CFICode, "RCS", 3) == 0) &&
       (strlen(m_AssetA.data()) < CcySz));

    // If would be extremely strange if the AssetA and QuoteCcy are given by
    // the same string (though they have different max lengths):
    if (utxx::unlikely(strcmp(m_AssetA.data(), m_QuoteCcy.data()) == 0))
      throw utxx::badarg_error
            ("SecDefS::Ctor: Symbol=",           m_Symbol.data(),
             ": Identical Asset and QuoteCcy: ", m_AssetA.data());

    // And Swaps are those Instrs with non-trivial Tenor2:
    m_IsSwap  = !IsEmpty(m_Tenor2);

    // All Done!
  }

  //=========================================================================//
  // "MkSecID":                                                              //
  //=========================================================================//
  // Hash Function for SecID creation (if not given explicitly by the Exchange),
  // Uses both "Symbol" and "Segment", and possibly "Tenor"s (but only if their
  // use is EXPLICITLY enabled and they are indeed available; explicit enabling
  // is to ensure compatible SecID generation in "SecDefD" Ctor and in receiving
  // MktData streams). On th eother hand, using Tenors is sometimes is essential
  // to distinguish otherwise-similar Instruments (eg in FX):
  // NB: The "buff" construction here is simplified compared to stringification
  // in "SecDefD::ToString" below -- the resulting strings are NOT the same!
  //
  template<bool UseTenors>
  inline SecID  MkSecID
  (
    char const* a_symbol,    // Always non-empty
    char const* a_exchange,  // Always non-empty
    char const* a_segment,   // May be NULL or "" (but not recommended)
    char const* a_tenor,     // Ignored in any case if !UseTenors
    char const* a_tenor2     // ditto
  )
  {
    // Symbol must always be non-NULL and non-empty; Segment may be NULL or
    // empty:
    CHECK_ONLY
    (
      if (utxx::unlikely
         (a_symbol   == nullptr || *a_symbol   == '\0' ||
          a_exchange == nullptr || *a_exchange == '\0' ))
        throw utxx::badarg_error("MkSecID: No Symbol or Exchange");
    )
    char buff[128];         // Should always be sufficient

    // NB: Unlike "FullName" of "SecDefD", here in SecID computation, we do
    // NOT use '-' separators between the flds, just direct concatenation:
    // Install "Symbol" and "Exchange" (both are mandatory):
    char* curr   = stpcpy(buff, a_symbol);
    curr         = stpcpy(curr, a_exchange);

    // "Segment" or "SessionID" is optional, but is normally present:
    if (utxx::likely(a_segment != nullptr && *a_segment != '\0'))
      curr = stpcpy(curr, a_segment);

    // Tenors may be de-configured from SecID calculation completely (though
    // they remain non-empty in "SecDefS"), or just be skipped if empty:
    if (UseTenors)
    {
      bool hasTenor  = (a_tenor != nullptr  && *a_tenor  != '\0');
      if  (hasTenor)
        curr = stpcpy(curr, a_tenor);

      if  (a_tenor2 != nullptr && *a_tenor2 != '\0')
      {
        // In this case, Tenor itself must also be set:
        assert(hasTenor);
        curr = stpcpy(curr, a_tenor2);
      }
    }
    // Check the buffer:
    assert(*curr == '\0' && size_t(curr - buff) < sizeof(buff));

    // Now generate the HashCode:
    SecID  res = MkHashCode64(buff);
    return res;
  }

  //-------------------------------------------------------------------------//
  // "MkSecID" Instances:                                                    //
  //-------------------------------------------------------------------------//
  template SecID MkSecID<false>
  (
    char const* a_symbol, char const* a_exchange, char const* a_segment,
    char const* a_tenor,  char const* a_tenor2
  );

  template SecID MkSecID<true>
  (
    char const* a_symbol, char const* a_exchange, char const* a_segment,
    char const* a_tenor,  char const* a_tenor2
  );

  //=========================================================================//
  // "SecDefD": Non-Default Ctor:                                            //
  //=========================================================================//
  // Copies an existing "SecDefS" into "SecDefD", other flds are set to their
  // default vals. May modify the SecID and Segment:
  //
  SecDefD::SecDefD
  (
    SecDefS const&    a_sds,
    bool              a_use_tenors,     // For SecID computation
    int               a_settl_date,
    int               a_settl_date2
  )
  : SecDefS           (a_sds),
    m_SettlDate       (a_settl_date),
    m_SettlDateStr    (),               // Set below
    m_SettlDate2      (a_settl_date2),
    m_SettlDate2Str   (),               // Set below
    m_FullName        (),               // Set below
    m_TradingStatus   (SecTrStatusT::UNDEFINED),
    m_LowLimitPx      (NaN<double>),
    m_HighLimitPx     (NaN<double>),
    m_Volatility      (NaN<double>),
    m_TheorPx         (NaN<double>),
    m_InitMarginOnBuy (NaN<double>),
    m_InitMarginOnSell(NaN<double>),
    m_InitMarginHedged(NaN<double>)
  {
    //-----------------------------------------------------------------------//
    // "SettlDate" Mgmt:                                                     //
    //-----------------------------------------------------------------------//
    assert(m_SettlDate >= 0 && m_SettlDate2 >= 0 && m_ExpireDate >= 0);

    if (m_SettlDate > 0)
    {
      // Then the specified SettlDate must be valid. XXX: Still, we do not
      // require here that it is >= GetCurrDateInt():
      if (utxx::unlikely(m_SettlDate != 0 && m_SettlDate < EarliestDate))
        throw utxx::badarg_error
              ("SecDefD::Ctor: Symbol=", m_Symbol.data(),
               ": Invalid SettlDate=",   m_SettlDate);

      // Also, in that case, SettlDate2 may be set, and must be > SettlDate:
      if (utxx::unlikely(m_SettlDate2 != 0 && m_SettlDate2 <= m_SettlDate))
        throw utxx::badarg_error
              ("SecDefD::Ctor: Symbol=", m_Symbol.data(),
               ": Invalid SettlDate2=",  m_SettlDate2);

      // If there is no ExpireDate in the "SecDefS" part,   propagate SettlDate
      // (or SettlDate2, if available) to the ExpireDate. XXX: However, in this
      // case, we don't know how to set the ExpireTime precisely, unless it was
      // already set independently:
      //
      if (m_ExpireDate == 0)
      {
        m_ExpireDate =
          utxx::unlikely(m_SettlDate2 != 0)   ? m_SettlDate2 : m_SettlDate;
        m_ExpirePrec =
          DateToTimeValFAST<true>(m_ExpireDate) + utxx::secs(m_ExpireTime);
      }
      else
        // Tenor exists -- verify the SettlDate against it. XXX: Although it is
        // theoretically possible that eg SettlDate = Tenor + N, we   currently
        // disallow that, to be on a safe side:
        if (utxx::unlikely(m_SettlDate > m_ExpireDate))
          throw utxx::badarg_error
                ("SecDefD::Ctor: Inconsistency: TenorDate=", m_ExpireDate,
                 ", SettlDate=", m_SettlDate);
    }
    else
    {
      // SettlDate is 0: This may be acceptable (eg for Crypto-Assets), and it
      // means that "settlement is conceptually immediate", ie there is no spe-
      // cific Clearing Time.
      // In that case, SettlDate2 must not be set -- and we don't really know
      // how to set it (in the generic case):
      //
      if (utxx::unlikely(m_SettlDate2 > 0))
        throw utxx::badarg_error
              ("SecDefD::Ctor: Symbol=", m_Symbol.data(),
               ": SettlDate2 is not allowed since SettlDate was not set");

      // Try to propagate the TenorDate if available. XXX: is it always OK? We
      // do NOT take TenorTime and TimeZone into account here. However, Settl-
      // Date is a Segment-type attribute only;  it is currently not used for
      // any exact time calculations (unlike TenorPrec which  may be used for
      // BSM etc), so this should be acceptable for now:
      //
      if (m_ExpireDate >= EarliestDate)
        m_SettlDate = m_ExpireDate;
    }

    //-----------------------------------------------------------------------//
    // So: have we got a valid SettlDate at the end?                         //
    //-----------------------------------------------------------------------//
    if (utxx::unlikely(m_SettlDate == 0))
      // Still no SettlDate -- Zero-out the "SettlDateStr". But once again,
      // this is OK:
      memset(m_SettlDateStr, '\0', sizeof(m_SettlDateStr));
    else
    {
      // Got a concrete SettlDate -- stringify it. For safety, also to zero-out
      // the remaining bytes:
      assert(m_SettlDate >= EarliestDate);
      (void) utxx::itoa_left<int, 8>(m_SettlDateStr,  m_SettlDate);
      memset(m_SettlDateStr + 8,  '\0', sizeof(m_SettlDateStr)  - 8);
    }

    // Similar for SettlDate2 -- but the latter is normally and likely 0:
    if (utxx::likely(m_SettlDate2 == 0))
      memset(m_SettlDate2Str, '\0', sizeof(m_SettlDate2Str));
    else
    {
      assert(m_SettlDate2 >= EarliestDate);
      (void) utxx::itoa_left<int, 8>(m_SettlDate2Str, m_SettlDate);
      memset(m_SettlDate2Str + 8, '\0', sizeof(m_SettlDate2Str) - 8);
    }

    //-----------------------------------------------------------------------//
    // Now compute the SecID (if it is not available statically):            //
    //-----------------------------------------------------------------------//
    if (m_SecID == 0)
      m_SecID =
        a_use_tenors
        ? MkSecID<true>
          (m_Symbol.data(), m_Exchange.data(), m_SessOrSegmID.data(),
           m_Tenor.data(),  m_Tenor2.data())
        : MkSecID<false>
          (m_Symbol.data(), m_Exchange.data(), m_SessOrSegmID.data());

    //-----------------------------------------------------------------------//
    // Generate the Full Instrument Name:                                    //
    //-----------------------------------------------------------------------//
    // Format: Symbol[-SessOrSegmID][-Tenor][-Tenor2]-SettlDate[-SettlDate2]:
    //
    // Symbol   -- always present:
    assert(!IsEmpty(m_Symbol));
    char* curr = stpcpy(m_FullName.data(), m_Symbol.data());

    // Exchange -- always present:
    assert(!IsEmpty(m_Exchange));
    *curr = '-';
    curr  = stpcpy(curr + 1,   m_Exchange.data());

    // SessOrSegmID -- normally present:
    if (utxx::likely(!IsEmpty(m_SessOrSegmID)))
    {
      *curr = '-';
      curr  = stpcpy(curr + 1, m_SessOrSegmID.data());
    }

    // Tenor -- usually NOT required:
    if (utxx::unlikely(!IsEmpty(m_Tenor)))
    {
      *curr = '-';
      curr  = stpcpy(curr + 1, m_Tenor.data());
    }

    // Tenor2 -- very seldom used (for Swaps only):
    if (utxx::unlikely(!IsEmpty(m_Tenor2)))
    {
      *curr = '-';
      curr  = stpcpy(curr + 1, m_Tenor2.data());
    }

    // SettlDate: If 0, SettlDateStr is empty, so skip it then:
    if (m_SettlDate != 0)
    {
      *curr   = '-';
      curr    = stpcpy(curr + 1, m_SettlDateStr);
    }

    // SettlDate2 -- very seldom used (for Swaps only):
    if (utxx::unlikely(m_SettlDate2 != 0))
    {
      assert(m_SettlDate2 > 0);
      *curr = '-';
      curr  = stpcpy(curr + 1, m_SettlDate2Str);
    }
    // 0-Terminator:
    *curr   = '\0';

    // Check for overflow:
    if (utxx::unlikely
       (int(curr - m_FullName.data()) >= int(sizeof(m_FullName))))
      throw utxx::badarg_error
            ("SecDefD::Ctor: Symbol=", m_Symbol.data(),
             ": FullName Too Long");

    // All Done!
  }
} // End namespace MAQUETTE
