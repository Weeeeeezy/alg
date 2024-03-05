// vim:ts=2:et
//===========================================================================//
//                         "InfraStruct/PersistMgr.h":                       //
//          Support for Persistent Objects (in ShM or MMaped Files)          //
//===========================================================================//
#pragma once

#include "Basis/BaseTypes.hpp"
#include <utxx/error.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/detail/named_proxy.hpp>
#include <boost/property_tree/ptree.hpp>
#include <spdlog/spdlog.h>
#include <cstring>
#include <cassert>

namespace MAQUETTE
{
  namespace BIPC = ::boost::interprocess;

  using FixedShM = BIPC::fixed_managed_shared_memory;
  using FixedMMF = BIPC::basic_managed_mapped_file
                   <char,
                    BIPC::rbtree_best_fit<BIPC::mutex_family, void*>,
                    BIPC::iset_index
                   >;
  //=========================================================================//
  // "PersistMgr" Class:                                                     //
  //=========================================================================//
  template<typename ST = FixedShM>
  class PersistMgr: public boost::noncopyable
  {
  public:
    //=======================================================================//
    // Constants:                                                            //
    //=======================================================================//
    // Re-export the Storage Type:
    using                          SegmT = ST;

    // Maximum size of a ShM/MMF segment which can be mapped:
    constexpr static unsigned long MaxSegmSz   =
      0x000400000000UL;            // 16GB = 2^34

    // Base address used for mapping ShM/MMF segments (the value below is known
    // to be  "safest" on Linux -- minimises the chance of clashes):
    constexpr static unsigned long BaseMapAddr =
      0x500000000000UL;            // 5 * 2^44

    constexpr static unsigned long MaxMapAddr  = 1UL << 47;  // 2^47

    // Max number of mapped segms:
    // The largest mapping addr must still be < 2^47 = 8 * 2^44 (incl the addi-
    // tive Base!)
    // So must have: SegmIdx < 2^11 - 1, then the highest end-map addr will be
    // < Base + (SegmIdx + 1) * MaxSegmSz < 5 * 2^44 + 2^(11+34)
    //                                    = 5 * 2^44 + 2^45 = 7 * 2^44: OK!
    // And in addition, if the SegmIdx is taken modulo the largest prime < 2^11
    // which will introduce additional hash randomisation! So:
    //
    constexpr static unsigned long SegmIdxMod     = 2039;

  protected:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    ST*             m_segm;        // ShM/MMF Segment for Persistence
    void*           m_mapAddr;
    size_t          m_mapSize;
    int             m_debugLevel;  // For logging
    spdlog::logger* m_logger;      // Ptr NOT owned

  public:
    //=======================================================================//
    // Ctors, Dtor:                                                          //
    //=======================================================================//
    // Default Ctor DOES exist -- creates an empty object:
    PersistMgr    ()
    : m_segm      (nullptr),
      m_mapAddr   (nullptr),
      m_mapSize   (0),
      m_debugLevel(0),
      m_logger    (nullptr)
    {}

    // Non-Default Ctor: OpenOrCreate a Segment -- RW:
    PersistMgr
    (
      std::string const&                  a_name,
      boost::property_tree::ptree const*  a_params,  // Optional (may be NULL)
      size_t                              a_map_sz,
      int                                 a_debug_level,
      spdlog::logger*                     a_logger
    )
    : m_segm      (nullptr),
      m_mapAddr   (nullptr),
      m_mapSize   (0),
      m_debugLevel(a_debug_level),
      m_logger    (a_logger)
    { Init(a_name, a_params, a_map_sz); }

    // Non-Default Ctor: Open an Existing Segment -- RO:
    PersistMgr
    (
      std::string const&                  a_full_segm_name,
      unsigned long                       a_base_addr,
      int                                 a_debug_level,
      spdlog::logger*                     a_logger
    )
    : m_segm      (nullptr),
      m_mapAddr   (nullptr),
      m_mapSize   (0),
      m_debugLevel(a_debug_level),
      m_logger    (a_logger)
    { Init(a_full_segm_name, a_base_addr); }

    // Dtor:
    ~PersistMgr() noexcept
    {
      delete       m_segm;
      m_segm       = nullptr;
      m_mapAddr    = nullptr;
      m_mapSize    = 0;
      m_debugLevel = 0;
      m_logger     = nullptr;
    }

    //=======================================================================//
    // The actual "Init" methods:                                            //
    //=======================================================================//
    // For OpenOrCreate, with RW Access:
    void Init
    (
      std::string const&                  a_name,
      boost::property_tree::ptree const*  a_params,  // Optional (may be NULL)
      size_t                              a_map_sz
    );

    // For OpenExisting, with RO Access:
    void Init
    (
      std::string const&                  a_full_segm_name,
      unsigned long                       a_base_addr = 0
    );

    //=======================================================================//
    // Accessors:                                                            //
    //=======================================================================//
    ST*   GetSegm()    const { return m_segm;    }
    void* GetMapAddr() const { return m_mapAddr; }

    bool  IsEmpty()    const
    {
      bool   res =  (m_segm    == nullptr);
      // NB: MapSize may be 0 (if RO) even for an non-empty map, but the MapAddr
      // must not be:
      assert(res == (m_mapAddr == nullptr));
      return res;
    }

    //=======================================================================//
    // "GetMapAddr" (static methods):                                        //
    //=======================================================================//
    // (1) With a PropertyTree (which can, in particular, specify an explicit
    //     MapAddr or a BaseAddr):
    static void* GetMapAddr
    (
      std::string                 const& a_name,
      boost::property_tree::ptree const* a_params               // NULL is OK
    );

    // (2) With a BaseAddr (if 0, the default "BaseMapAddr" is used):
    static void* GetMapAddr
    (
      std::string                 const& a_name,
      unsigned long                      a_base_addr           // 0 is OK
    );

    //=======================================================================//
    // "FindOrConstruct": ShM Segment Mgmt:                                  //
    //=======================================================================//
    // This is a more sophisticated replacement of Boost's "find_or_construct",
    // with verification of the Persistent Arrays' Sizes:
    //
    template<typename T>
    inline T* FindOrConstruct(char const* a_name, unsigned* a_count)
    {
      // Check the Args:
      assert(a_name != nullptr  && a_count != nullptr);

      // Try to find an existing object first:
      if (utxx::unlikely(IsEmpty()))
        throw utxx::badarg_error("PersistMgr::FindOrConstruct: Empty");
      assert(m_segm != nullptr);

      auto res = m_segm->template find<T>(a_name);
      if  (res.first != nullptr)
      {
        // Yes, found; but what is the curr size of it (res.second)?
        assert(res.second > 0);
        if (utxx::unlikely(res.second < size_t(*a_count)))
          // The array exists but is too small; we cannot enlarge it without de-
          // leting and re-creating it, which is currently not supported (as in
          // that case we may need to re-create the whole ShM segment):
          throw utxx::badarg_error
                ("PersistMgr::FindOrConstruct: ",  a_name,
                 ": RequestedCount=", *a_count, ", ExistingCount=", res.second,
                 ": Cannot enlarge");
        else
          // Set *a_count to the actual (likely larger than requested) size:
          *a_count = unsigned(res.second);

        // OK: Return ptr to an existing object:
        return res.first;
      }
      else
      {
        // No existing object -- create a new one, using default ctor for array
        // elements:
        T*     ptr  = m_segm->template construct<T>(a_name)[*a_count]();
        assert(ptr != nullptr);
        return ptr;
      }
      // All Done!
      __builtin_unreachable();
    }
  };
} // End namespace MAQUETTE
