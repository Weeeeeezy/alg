// vim:ts=2:et
//===========================================================================//
//                                "MDReader.hpp":                            //
//                Reader for Historical MktData: Generic Part                //
//===========================================================================//
// NB: This class inherits from "EConnector_MktData" but it is not intended for
// use an an "EConnector". It must be further extended  by  Exchange- and Data-
// Format-specific classes:
#pragma once

#include "Connectors/EConnector_MktData.hpp"
#include "Basis/TimeValUtils.hpp"
#include "MDSaver.hpp"
#include <utxx/error.hpp>
#include <string>
#include <vector>
#include <algorithm>

namespace MAQUETTE
{
namespace QuantSupport
{
  //=========================================================================//
  // "MDReader" Class:                                                       //
  //=========================================================================//
  template<typename Derived>
  class MDReader:   public EConnector_MktData
  {
  public:
    //=======================================================================//
    // Types:                                                                //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "FileTS":                                                             //
    //-----------------------------------------------------------------------//
    // When MktData come in multiple DataFiles, each file must be paired with
    // some kind of TimeStamp which allows us to sort files in the increasing
    // time order:
    //
    struct FileTS
    {
      // Data Flds:
      std::string     m_fileName;
      utxx::time_val  m_mdTS;     // Used for sorting: MktData "From" TS
      int             m_mdDate;   // Optional (YYYYMMDD)
      int             m_mdHour;   // Optional
      int             m_mdMin;    // Optional

      // Default Ctor:
      FileTS()
      : m_fileName(),
        m_mdTS    (),
        m_mdDate  (0),
        m_mdHour  (-1),
        m_mdMin   (-1)
      {}

      // Non-Default Ctor:
      FileTS(std::string const& a_file_name, utxx::time_val a_ts, int a_date,
             int a_hour, int    a_min)
      : m_fileName(a_file_name),
        m_mdTS    (a_ts),
        m_mdDate  (a_date),
        m_mdHour  (a_hour),
        m_mdMin   (a_min)
      {
        assert(!m_fileName.empty()      && !m_mdTS.empty() &&
               m_mdDate >= EarliestDate && 0 <= m_mdHour   &&
               m_mdHour <= 23           && 0 <= m_mdMin    &&
               m_mdMin  <= 59);
      }
    };

  protected:
    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    std::vector<std::string>      const  m_instrNames;
    std::vector<FileTS>           const  m_dataFiles;
    bool                          const  m_fixCollisions;

    // Default Ctor is deleted:
    MDReader() = delete;

    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    MDReader
    (
      std::vector<std::string>    const& a_instr_names,    // Our sub-set
      std::vector<FileTS>         const& a_data_files,
      boost::property_tree::ptree const& a_params
    )
    : //---------------------------------------------------------------------//
      // "EConnector_MktData" Ctor:                                          //
      //---------------------------------------------------------------------//
      EConnector_MktData
      (
        true,           // Yes, MDC is Enabled!
        nullptr,        // No Primary MDC -- this MDReader is Primary by itself
        a_params,
        Derived::IsFullAmt,
        Derived::IsSparse,
        false,          // SecNums  not used (no datagrams here: From File!)
        false,          // RptSeqs  not used (same reason)
        false,          // Irrelevant (whether RptSeqs are continuous)
        0,              // MktDepth=oo
        Derived::WithTrades,
        false,          // No OrdersLog-Implied Trades
        Derived::WithDynInit
      ),                // XXX: DynInitMode may still be a useful concept here!
      //---------------------------------------------------------------------//
      // "MDReader" Ctor:                                                    //
      //---------------------------------------------------------------------//
      m_instrNames    (a_instr_names),
      m_dataFiles     (a_data_files),
      m_fixCollisions (a_params.get<bool>("FixBidAskCollisions"))
    {
      // Sort the DataFiles in the increasing order of TimeStamps (XXX: casting
      // them into non-const!)
      std::vector<FileTS>& sortableRef =
        const_cast<std::vector<FileTS>&>(m_dataFiles);

      std::sort
      (
        sortableRef.begin(),     sortableRef.end(),

        [](FileTS const& a_left, FileTS const& a_right) -> bool
        {
          utxx::time_val leftTS  = a_left .m_mdTS;
          utxx::time_val rightTS = a_right.m_mdTS;

          assert(!leftTS.empty() && !rightTS.empty());
          return leftTS < rightTS;
        }
      );
    }

    //-----------------------------------------------------------------------//
    // Dtor must still be "virtual" because this class is Non-Final:         //
    //-----------------------------------------------------------------------//
    virtual ~MDReader() noexcept override {}

  public:
    //=======================================================================//
    // A Static Factory Method (Wrapper for the Non-Default Ctor):           //
    //=======================================================================//
    // NB:
    // (*) Arg1 is a list of Symbols, not full InstrNames!
    // (*) Method returns a "Derived" Ptr!
    //
    static Derived* New
    (
      std::vector<std::string> const& a_symbols,
      std::vector<FileTS>      const& a_data_files,
      long                            a_max_orders,
      int                             a_max_ob_levels,
      bool                            a_fix_collisions,
      int                             a_clustering_msec,
      std::string              const& a_log_file,
      int                             a_debug_level
    )
    {
      //---------------------------------------------------------------------//
      // Pack the params:                                                    //
      //---------------------------------------------------------------------//
      boost::property_tree::ptree params;
      params.put
        ("MaxOrders",
        ( a_max_orders    > 0)
        ? a_max_orders    : Derived::DefaultMaxOrders);
      params.put
        ("MaxOrderBookLevels",
        ( a_max_ob_levels > 0)
        ? a_max_ob_levels : Derived::DefaultMaxOBLevels);
      params.put
        ("FixBidAskCollisions",   a_fix_collisions);
      params.put
        ("MatchesClusteringMSec", a_clustering_msec);
      params.put
        ("LogFile",
        (!a_log_file.empty())  ?  a_log_file    : "stdout");
      params.put
        ("DebugLevel",
        ( a_debug_level  >= 0) ?  a_debug_level : 0);

      // XXX: For all Symbols, convert them into full InstrNames and  install
      // SettlDates. The actual date is quite irrelevant, but it should be in
      // the remote future in order to avoid any conflicts:
      //
      std::vector<std::string> instrNames;
      for (std::string const&  symbol: a_symbols)
      {
        // Conversion of Symbol to InstrName is done by the "Derived" class:
        // It knows all details such as MktSegments, Tenors etc:
        std::string instrName = Derived::MkFullInstrName(symbol);
        instrNames.push_back   (instrName);
        params.put (instrName,  20991231);
      }
      //---------------------------------------------------------------------//
      // Create a "Derived" MDReader:                                        //
      //---------------------------------------------------------------------//
      return new Derived(instrNames, a_data_files, params);
    }

    //=======================================================================//
    // Methods Implementing the Abstract Interfaces of Parent Classes:       //
    //=======================================================================//
    // "Start", "Stop" and "IsActive" do not do anything:
    void Start()          override {}
    void Stop()           override {}
    bool IsActive() const override { return false; }
  };
} // End namespace QuantSupport
} // End namespace MAQUETTE
