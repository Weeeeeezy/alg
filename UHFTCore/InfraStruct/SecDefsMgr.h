// vim:ts=2:et
//===========================================================================//
//                         "InfraStruct/SecDefsMgr.h":                       //
//           ShM-Based Singleton Class Containing ALL "SecDefD" Objs         //
//===========================================================================//
#pragma once

#include "Basis/SecDefs.h"
#include "Connectors/EConnector.h"
#include "InfraStruct/PersistMgr.h"
#include "InfraStruct/StaticLimits.h"
#include <boost/core/noncopyable.hpp>
#include <boost/container/static_vector.hpp>

namespace MAQUETTE
{
  //=========================================================================//
  // "SecDefsMgr" Class:                                                     //
  //=========================================================================//
  // Its single instance lives in ShM, and is accessed via the static "s_pm":
  //
  class SecDefsMgr: public boost::noncopyable
  {
    //-----------------------------------------------------------------------//
    // Types and Data Flds:                                                  //
    //-----------------------------------------------------------------------//
  private:
    // STATIC "PersistMgr" which can be used to store a persistent "SecDefsMgr"
    // object:
    static PersistMgr<>  s_pm;
    bool   const         m_isProd;

  public:
    // The actual SecDefs vector (should be long enough, eg for Crypto-Assets):
    using  SecDefsVec  =
           boost::container::static_vector<SecDefD, Limits::MaxInstrs>;
  private:
    SecDefsVec           m_secDefs;

    // The following is required in order to construct "SecDefsMgr" objs in ShM,
    // because the above Ctor is private (to prevent on-stack and on-heap  ctor
    // invocations):
    template<class T, bool IsIterator, class ...Args>
    friend struct  BIPC::ipcdetail::CtorArgN;

  public:
    //-----------------------------------------------------------------------//
    // Names of Persistent Objs:                                             //
    //-----------------------------------------------------------------------//
    constexpr static char const* SecDefsMgrON() { return "SecDefsMgr"; }

  private:
    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // This Ctor is private: it can only be invoked from within the ShM segment
    // mgr, or from "EConnector" which is a friend class:
    friend class EConnector;

    SecDefsMgr(bool a_is_prod)
    : m_isProd(a_is_prod)
    {}

    // Default Ctor is deleted altogether:
    SecDefsMgr() = delete;

  public:
    //-----------------------------------------------------------------------//
    // Static Factory and Properties:                                        //
    //-----------------------------------------------------------------------//
    static SecDefsMgr* GetPersistInstance
    (
      bool                  a_is_prod,
      std::string    const& a_prefix   // May be empty
    );

    bool IsProdEnv() const  { return m_isProd; }

    template <class ConnectorT>
    static std::vector<SecDefS> const* GetSecDefs()
    { return ConnectorT::GetSecDefs(); }

    //-----------------------------------------------------------------------//
    // "Add"ing a new SecDef:                                                //
    //-----------------------------------------------------------------------//
    // (Or modifies an existing SecDef if it matches the args). Returns a const
    // ref to the SecDefD installed or updated:
    //
    SecDefD const& Add
    (
      SecDefS const& a_sds,           // Static basis
      bool           a_use_tenors,    // Use Tenors in SecID computation?
      int            a_settl_date,    // May be 0 (eg for Crypto)
      int            a_settl_date2,   // 0 except for Swaps
      double         a_pass_fee_rate, // Passive Trading Fee Rate, NaN OK
      double         a_aggr_fee_rate  // Aggrssv Trading Fee Rate, NaN OK
    );

    //-----------------------------------------------------------------------//
    // Retrieving SecDefs:                                                   //
    //-----------------------------------------------------------------------//
    // By SecID:
    // Returns NULL if the SecID in question was not found. Throws an exception
    // if the SecID is empty (0) or ambiguous:
    //
    SecDefD    const* FindSecDefOpt(SecID a_sec_id) const;

    // By InstrName = Symbol[:Segment[:Tenor[:Tenor2]]]:
    // Again, returns NULL if the SecDef was not found, and throws an exception
    // if the SecDef cannot be uniquely identified:
    //
    SecDefD    const* FindSecDefOpt(char const* a_instr_name) const;

    // As above, but also throws an expection if the SecDef was not found:
    //
    SecDefD    const& FindSecDef   (char const* a_instr_name) const;

    // Getting the vector of All SecDefs:
    //
    SecDefsVec const& GetAllSecDefs()    const  { return m_secDefs; }

  private:
    // Internal version: Searching by Symbol, Segment, Tenor and Tenor2 (all
    // must be non-NULL, but apart from Symbol, may be empty if applicable):
    //
    SecDefD const* FindSecDefOpt
    (
      char const*  a_symbol,    // Non-NULL, non-empty
      char const*  a_exchange,  // ditto
      char const*  a_segm,      // Non-NULL, but may be empty
      char const*  a_tenor,     // ditto
      char const*  a_tenor2     // ditto
    )
    const;
  };
} // End namespace MAQUETTE
