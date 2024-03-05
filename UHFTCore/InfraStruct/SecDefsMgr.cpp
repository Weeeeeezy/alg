// vim:ts=2:et
//===========================================================================//
//                        "InfraStruct/SecDefsMgr.cpp":                      //
//           Mgr for a ShM Segment containing all "SecDefD" objs             //
//===========================================================================//
#include "InfraStruct/SecDefsMgr.h"
#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "PersistMgr" Obj for the "SecDefsMgr":                                  //
  //=========================================================================//
  PersistMgr<> SecDefsMgr::s_pm;  // Initially empty

  //=========================================================================//
  // "GetPersistInstance":                                                   //
  //=========================================================================//
  SecDefsMgr* SecDefsMgr::GetPersistInstance
  (
    bool          a_is_prod,
    string const& a_prefix    // May be empty
  )
  {
    // ShM/MMF ObjName -- depends on the Prod/Test mode:
    string objName =
      (a_prefix.empty() ? "SecDefsMgr-" : (a_prefix + "-SecDefsMgr-")) +
      (a_is_prod        ? "Prod"        : "Test");

    // Open or Create the ShM/MMF Segment. For that, create a "PersistMgr"  obj
    // if it was not created yet. The CurrDate is NOT used in segment name; the
    // BaseMapAddr is the default one:
    if (utxx::likely(s_pm.IsEmpty()))
      s_pm.Init
      (
        objName.data(),
        nullptr,
        size_t(1.1 * double(sizeof(SecDefsVec)))
      );
    assert(!s_pm.IsEmpty());

    // Find or construct the "SecDefsMgr" inside the ShM Segment:
    SecDefsMgr* res =
      s_pm.GetSegm()->find_or_construct<SecDefsMgr>(SecDefsMgrON())(a_is_prod);
    assert(res != nullptr);
    return res;
  }

  //=========================================================================//
  // "FindSecDefOpt" (SecID):                                                //
  //=========================================================================//
  // (*) Returns NULL if the SecID is not found -- the corrective action is
  //     up to the Caller;
  // (*) For internal use only (protected) -- returns a non-const ptr:
  //
  SecDefD const* SecDefsMgr::FindSecDefOpt(SecID a_sec_id) const
  {
    // SecID must not be empty:
    if (utxx::unlikely(a_sec_id == 0))
      throw utxx::badarg_error("SecDefsMgr::FindSecDefOpt: Empty SecID");

    // If OK: Get the 1st occurrence of this SecID:
    auto selector =
        [a_sec_id](SecDefD const&  a_curr)-> bool
        { return a_curr.m_SecID == a_sec_id; };

    auto cit =
      boost::container::find_if(m_secDefs.cbegin(), m_secDefs.cend(), selector);

    // If not found at all, return NULL (this is not an error):
    if (cit == m_secDefs.cend())
      return nullptr;

    // If found: check UNIQUENESS. This is not strictly necessary, but provided
    // for extra safety (this method is NOT time-critical):
    auto twit =
      boost::container::find_if(next(cit), m_secDefs.cend(), selector);

    if (utxx::unlikely(twit != m_secDefs.cend()))
      throw utxx::runtime_error
            ("SecDefsMgr::FindSecDefOpt: SecID=", a_sec_id, ": Not Unique");

    // If OK: return a raw ptr to the SecDef found:
    return &(*cit);
  }

  //=========================================================================//
  // "FindSecDefOpt" (InstrName):                                            //
  //=========================================================================//
  // Throws an exception if more than 1 matching SecDef is  found (ambiguity);
  // but if none is found, returns NULL. This method is for external use -- so
  // it returns a "const" ptr.
  // InstrName format: Symbol-Exchange[-Segment[-Tenor[-TenorName]]]
  //                   (cal also use '|' separators):
  //
  SecDefD const* SecDefsMgr::FindSecDefOpt(char const* a_instr_name) const
  {
    if (utxx::unlikely(a_instr_name == nullptr || *a_instr_name == '\0'))
      return nullptr;

    // Generic Case: Parse "a_instr_name" (NB: this method is NOT performance-
    // critical, so using "string"s and "boost::split" is OK):
    vector<string> nameParts;
    string         name(a_instr_name);
    boost::split  (nameParts, name, boost::is_any_of("-|"));
    // NB: Do not use '_' or ':', as these chars may be part of the Symbol!

    int    n = int(nameParts.size());
    assert(n >= 1);
    if (utxx::unlikely(n < 2 || n > 5))
      return nullptr;   // This is not a valid InstrName format

    // "Symbol" and "Exchange" are compulsory:
    char const* symbol   = nameParts[0].data();
    char const* exchange = nameParts[1].data();
    if (utxx::unlikely(*symbol == '\0' || *exchange == '\0'))
      return nullptr;   // Again, this is not a valid InstrName format

    // Segment/SessionID, Tenor and Tenor2 are optional:
    char const* segm   = (n >= 3) ? nameParts[2].data() : "";
    char const* tenor  = (n >= 4) ? nameParts[3].data() : "";
    char const* tenor2 = (n == 5) ? nameParts[4].data() : "";

    // Perform full search by Symbol, Segment, Tenor and Tenor2:
    return FindSecDefOpt (symbol, exchange, segm, tenor, tenor2);
  }

  //=========================================================================//
  // "FindSecDefOpt" (Symbol, Exchange, Segment, Tenor, Tenor2):             //
  //=========================================================================//
  // NB: All args must be non-NULL (but may be empty if appropriate):
  //
  SecDefD const* SecDefsMgr::FindSecDefOpt
  (
    char const* a_symbol,    // Non-NULL, non-empty
    char const* a_exchange,  // ditto
    char const* a_segm,      // Non-NULL, but may be empty
    char const* a_tenor,     // ditto
    char const* a_tenor2     // ditto
  )
  const
  {
    if (utxx::unlikely
       (a_symbol    == nullptr || *a_symbol   == '\0'  ||
        a_exchange  == nullptr || *a_exchange == '\0'  ||
        a_segm      == nullptr ||  a_tenor  == nullptr || a_tenor2 == nullptr))
      throw utxx::badarg_error("SecDefsMgr::FindSecDefOpt(5): Invalid arg(s)");

    auto selector =
        [a_symbol, a_exchange, a_segm, a_tenor, a_tenor2]
        (SecDefD const& a_curr) -> bool
        {
          return
            (strcmp(a_symbol,   a_curr.m_Symbol      .data()) == 0 &&
             strcmp(a_exchange, a_curr.m_Exchange    .data()) == 0 &&
             strcmp(a_segm,     a_curr.m_SessOrSegmID.data()) == 0 &&
             strcmp(a_tenor,    a_curr.m_Tenor       .data()) == 0 &&
             strcmp(a_tenor2,   a_curr.m_Tenor2      .data()) == 0);
        };
    // Try to find the 1st occurrence:
    auto cit =
      boost::container::find_if(m_secDefs.cbegin(), m_secDefs.cend(), selector);

    // Not found?
    if (utxx::unlikely(cit == m_secDefs.cend()))
      return nullptr;

    // Now check UNIQUENESS: Search from the next entry on, to make sure the
    // Symbol does not occur twice:
    auto twit =
      boost::container::find_if(next(cit), m_secDefs.cend(), selector);

    if (utxx::unlikely(twit != m_secDefs.cend()))
      throw utxx::logic_error
            ("SecDefsMgr::FindSecDefOpt(5): Symbol=",  a_symbol, ", Segment=",
             a_segm, ", Tenor=", a_tenor, ", Tenor2=", a_tenor2,
             ": Not Unique");
    // If OK:
    return &(*cit);
  }

  //=========================================================================//
  // "FindSecDef" (InstrName):                                               //
  //=========================================================================//
  // As "FindSecDefOpt" above, but also throws an exception of  the SecDef in
  // question was not found. Otherwise, returns a const ref (rather than ptr):
  //
  SecDefD const& SecDefsMgr::FindSecDef(char const* a_instr_name) const
  {
    SecDefD const* instr = FindSecDefOpt(a_instr_name);

    if (utxx::unlikely(instr == nullptr))
      throw utxx::badarg_error
            ("SecDefsMgr::FindSecDef: Not Found: InstrName=",
            (a_instr_name != nullptr) ? a_instr_name : "NULL",
            ": Check its SettlDate?");
    // If found:
    return *instr;
  }

  //=========================================================================//
  // "Add":                                                                  //
  //=========================================================================//
  // Creates a new "SecDefD" from a "SecDefS" and a "SettlDate", and adds it to
  // the global vector (if not there yet), or over-writes an existing entry. Re-
  // turns a const ref to the entry created or modified:
  //
  SecDefD const& SecDefsMgr::Add
  (
    SecDefS const& a_sds,
    bool           a_use_tenors,
    int            a_settl_date,
    int            a_settl_date2,
    double         a_pass_fee_rate,
    double         a_aggr_fee_rate
  )
  {
    // Construct a tmp "instr" obj. FIXME: No "SettlDate2" as yet, as we do not
    // have Calendar functions required for that:
    SecDefD instr(a_sds,  a_use_tenors, a_settl_date, a_settl_date2);
    assert (instr.m_SecID != 0);

    // Fees can be set directly:
    instr.m_passFeeRate = a_pass_fee_rate;
    instr.m_aggrFeeRate = a_aggr_fee_rate;

    // Check if a "SecDefD" with the given SecID *or* (Symbol, Segment) com-
    // bination already exists; if both exist, it must be same "SecDefD":
    SecDefD* exist0 = const_cast<SecDefD*>(FindSecDefOpt(instr.m_SecID));
    SecDefD* exist1 = const_cast<SecDefD*>(FindSecDefOpt
      (instr.m_Symbol.data(),       instr.m_Exchange.data(),
       instr.m_SessOrSegmID.data(), instr.m_Tenor   .data(),
       instr.m_Tenor2.data()));

    if (utxx::unlikely(exist0 != exist1))
      throw utxx::logic_error
            ("SecDefsMgr::Add: SecDef Inconsistency for ",
             instr.m_FullName.data(),  ", SecID=",  instr.m_SecID);

    // If consistent:
    if (utxx::likely(exist0 == nullptr))
    {
      assert(exist1 == nullptr);
      // Append a newly-constructed "instr". XXX: Incurres copying overhead:
      m_secDefs.push_back(instr);
      exist0 = exist1 = &(m_secDefs.back());
    }
    else
    {
      // Over-write an existing "SecDefD". Some flds may be updated, eg the
      // SettlDate, but obviously, SecID, Symbol and Segment  must stay un-
      // changed:
      assert(exist1 != nullptr      && exist1 == exist0  &&
             exist0->m_SecID        == instr.m_SecID      &&
             exist0->m_Symbol       == instr.m_Symbol     &&
             exist0->m_SessOrSegmID == instr.m_SessOrSegmID);
      *exist0 = instr;
    }
    assert (exist0 != nullptr && exist0 == exist1);
    return *exist0;
  }
} // End namespace MAQUETTE
