// vim:ts=2:et
//===========================================================================//
//                        "InfraStruct/PersistMgr.cpp":                      //
//          Support for Persistent Objects (in ShM or MMaped Files)          //
//===========================================================================//
#include "Basis/TimeValUtils.hpp"
#include "Basis/XXHash.hpp"
#include "InfraStruct/PersistMgr.h"
#include <utxx/error.hpp>
#include <unistd.h>

using namespace std;

namespace MAQUETTE
{
  //=========================================================================//
  // "Init" (RW):                                                            //
  //=========================================================================//
  // XXX: Currently the only implemented persistence mechanism is ShM:
  //
  template<typename ST>
  void   PersistMgr<ST>::Init
  (
    string const&                       a_name,
    boost::property_tree::ptree const*  a_params,       // May be NULL
    size_t                              a_map_sz
  )
  {
    // Check the args. NB: a_params==NULL is OK:
    if (utxx::unlikely(a_name.empty() || a_map_sz == 0))
      throw utxx::badarg_error("PersistMgr::Init(RW): Invalid arg(s)");

    if (utxx::unlikely(a_map_sz > MaxSegmSz))
      throw utxx::badarg_error("PersistMgr::Init(RW): MapSz Too Large");

    // Otherwise: Generic Case:
    // First of all, cannot re-Initialise an already-Initialised object:
    if (utxx::unlikely(!IsEmpty()))
      throw utxx::badarg_error("PersistMgr::Init(RW): Already Inited");

    // If OK: Create the Full Segm Name (w/ or w/o the CurrDate prefix; by def-
    // ault, do NOT use it):
    bool datedSegm =
      (a_params != nullptr)
      ? a_params->get<bool>("UseDateInShMSegmName", false)
      : false;

    string fullName =
      datedSegm
      ? (string(GetCurrDateStr()) + "-" + a_name)
      : a_name;

    // Prepend username to fullName to support multiple users runing MAQUETTE
    // on the same system:
    fullName = string(cuserid(nullptr)) + "-" + fullName;

    // NB: Even if the CurrDate is used in forming the segment name, it is still
    // NOT used in the MapAddr computation -- for invariance;  a_params==NULL is
    // OK here:
    m_mapAddr = GetMapAddr(a_name, a_params);
    m_mapSize = a_map_sz;
    try
    {
      m_segm  = new ST
      (
        BIPC::open_or_create,
        fullName.data(),
        m_mapSize,
        m_mapAddr,
        BIPC::permissions(0660)
      );
    }
    catch(std::exception const& exn)
    {
      // An exception here most likely means that the was an address clash in
      // segment mapping:
      throw utxx::runtime_error
            ("PersistMgr::Init(RW): Cannot map: ", fullName, " @ ", m_mapAddr,
             ", Size=", m_mapSize, " : Probably an address clash?");
    }
    // If OK:
    LOG_INFO(2,
      "PersistMgr::Init(RW): Mapped: {}, Addr={}, Size={}",
      fullName, m_mapAddr, m_mapSize)
  }

  //=========================================================================//
  // "Init" (RO): Opening an Existing Segment:                               //
  //=========================================================================//
  template<typename ST>
  void   PersistMgr<ST>::Init
  (
    string const&   a_full_segm_name,
    unsigned long   a_base_addr
  )
  {
    // XXX: Cannot re-Initialise an already-Initialised object:
    if (utxx::unlikely(m_segm != nullptr))
      throw utxx::badarg_error("PersistMgr::Init(RO): Already Inited");

    if (utxx::unlikely(a_full_segm_name.empty()))
      throw utxx::badarg_error("PersistMgr::Init(RO): Empty SegmName");

    // XXX: If "a_full_segm_name" contains a date prefix,  still  use that full
    // name in opening the segment, but drop that prefix from hash computations
    // for MapAddr:
    char const* fullName = a_full_segm_name.data();
    char const* rootName =
       (a_full_segm_name.size() > 9 && strncmp(fullName, "20", 2) == 0 &&
        fullName[8] == '-')
      ? fullName + 9
      : fullName;

    // Again, prepend username to "fullName" to support multiple users runing
    // MAQUETTE on the same system:
    string finalName = string(cuserid(nullptr)) + "-" + string(fullName);

    // Again, catch errors -- they are most likely due to address clashes.
    // NB: In this case, "m_mapSize" remains 0:
    m_mapAddr = GetMapAddr(rootName, a_base_addr);
    try
    {
      m_segm  = new ST
      (
        BIPC::open_read_only,
        finalName.data(),
        m_mapAddr
      );
    }
    catch (...)
    {
      throw utxx::runtime_error
            ("PersistMgr::Ctor: Cannot map (RO): ", finalName.data(),
            " @ ", m_mapAddr, " : Probably an address clash!");
    }
    // If OK:
    LOG_INFO(2,
      "PersistMgr::Init: Mapped(RO): {}, Addr={}, Size={}",
      finalName, m_mapAddr, m_mapSize)
  }

  //=========================================================================//
  // "GetMapAddr(PropertyTree)":                                             //
  //=========================================================================//
  template<typename ST>
  void*  PersistMgr<ST>::GetMapAddr
  (
    string                      const& a_name,
    boost::property_tree::ptree const* a_params               // NULL is OK
  )
  {
    // First, see if the explicit mapping addr is provided (normally it is NOT):
    unsigned long res =
      (a_params != nullptr)
      ? strtoul(a_params->get<string>("MapAddr", "0").data(), nullptr, 0)
      : 0;

    if (utxx::unlikely(res != 0))
    {
      // Check the result validity:
      if (utxx::unlikely(res >= MaxMapAddr))
        throw utxx::runtime_error("GetMapAddr: Result Too Large");
      // OK, done:
      return reinterpret_cast<void*>(res);
    }

    // If we did not get it, use the BaseAddr-based version:
    unsigned long baseAddr =
      (a_params != nullptr)
      ? strtoul(a_params->get<string>("BaseMapAddr", "0").data(), nullptr, 0)
      : 0;
    return GetMapAddr(a_name, baseAddr);
  }

  //=========================================================================//
  // "GetMapAddr(BaseAddr)":                                                 //
  //=========================================================================//
  // As above, but BaseAdrr is given explicitly:
  //
  template<typename ST>
  void*  PersistMgr<ST>::GetMapAddr
  (
    string const& a_name,
    unsigned long a_base_addr
  )
  {
    // If "a_base_addr" is unavailable, use the compiled-in default:
    if (a_base_addr == 0)
      a_base_addr = BaseMapAddr;

    // Then (FIXME: Collisions are still possible here, because only about 11
    // bits of the hash code are actually used!):
    // NB: "MkHashCode64" can still produce "correlated" bit sub-seqs for so-
    // mewhat "similar" string args (as has been observed), so to shuffle the
    // hash code more, take a remainder modulo a prime:
    //
    unsigned long idx = MkHashCode64(a_name)    % SegmIdxMod;
    unsigned long res = a_base_addr + MaxSegmSz * idx;

    // Check the address validity:
    assert(res != 0);
    if (utxx::unlikely(res >= MaxMapAddr))
      throw utxx::runtime_error("GetMapAddr: Result Too Large");

    return reinterpret_cast<void*>(res);
  }

  //=========================================================================//
  // Both Instances:                                                         //
  //=========================================================================//
  template class PersistMgr<FixedShM>;
  template class PersistMgr<FixedMMF>;

} // End namespace MAQUETTE
