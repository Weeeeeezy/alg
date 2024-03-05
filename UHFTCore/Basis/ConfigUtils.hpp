// vim:ts=2:et
//===========================================================================//
//                           "Basis/ConfigUtils.hpp":                        //
//                     Config Files and Property Trees Mgmt                  //
//===========================================================================//
#pragma  once

#include <utxx/error.hpp>
#include <utxx/compiler_hints.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <filesystem>

namespace MAQUETTE
{
  //=========================================================================//
  // "GetParamsSubTree": PropertyTree Mgmt:                                  //
  //=========================================================================//
  inline boost::property_tree::ptree const& GetParamsSubTree
  (
    boost::property_tree::ptree const& a_root,
    std::string                 const& a_section_name
  )
  {
    auto it = a_root.find(a_section_name);

    if (utxx::unlikely(it == a_root.not_found()))
      throw utxx::badarg_error
            ("GetParamsSubTree: Section Not Found: [", a_section_name, ']');
    return  it->second;
  }

  //=========================================================================//
  // "GetParamsOptTree":                                                     //
  //=========================================================================//
  // Like "GetParamsSubTree" but returns NULL (instead of throwing an exception)
  // if the named sub-tree was not found:
  //
  inline boost::property_tree::ptree const* GetParamsOptTree
  (
    boost::property_tree::ptree const& a_root,
    std::string                 const& a_section_name
  )
  {
    auto it = a_root.find(a_section_name);
    return
      (it != a_root.not_found())
      ? &(it->second)
      : nullptr;
  }

  //=========================================================================//
  // "TraverseParamsSubTree":                                                //
  //=========================================================================//
  // Action: (std::string const& a_key, std::string const& a_val)->bool
  //
  template<typename Action>
  inline void TraverseParamsSubTree
  (
    boost::property_tree::ptree const& a_params,
    Action const&                      a_action
  )
  {
    for (auto it  = a_params.ordered_begin();
              it != a_params.not_found();  ++it)
    {
      // XXX: Here we implicitly assume a 2D .INI format; more general Property
      // Trees need to be implemented:
      std::string const&  key  = it->first;
      std::string const&  val  = it->second.data();

      bool cont = a_action(key, val);
      if (utxx::unlikely(!cont))
        break;
    }
  }

  //=========================================================================//
  // "ProcessAccounts":                                                      //
  //=========================================================================//
  // Adds (several) AcctPfxs to an OMC's "AccountPfx" based on [Intrument#]
  //
  inline void ProcessAccounts
  (
    boost::property_tree::ptree const* a_params,
    std::string a_instr_section_pfx = "Instrument"
  )
  {
    assert(a_params != nullptr);
    for (auto& section: *a_params)
      if (strncmp(section.first.data(), a_instr_section_pfx.data(), 10) == 0)
      {
        // accPfx of an Instrument#
        auto accPfx = section.second.template get<std::string>("AccountPfx");
        // It's OMC
        auto omc = section.second.template get<std::string>("OMC");
        auto& omcParams = const_cast<boost::property_tree::ptree&>
          (GetParamsSubTree(*a_params, omc));
        // Append a new one
        omcParams.add("AccountPfx", accPfx);
      }
  }

  //=========================================================================//
  // "ProcessSecDefs":                                                       //
  //=========================================================================//
  // Adds (several) "Ticker" fields to MDCs and OMCs (per account)
  // used to specify explicit SecDefs
  //
  inline void ProcessSecDefs
  (
    boost::property_tree::ptree const* a_params,
    std::string a_instr_section_pfx = "Instrument"
  )
  {
    assert(a_params != nullptr);
    for (auto& section: *a_params)
      if (strncmp(section.first.data(), a_instr_section_pfx.data(), 10) == 0)
      {
        // Add ticker to the corresponding MDC
        auto mdc = section.second.template get<std::string>("MDC");
        auto& mdcParams = const_cast<boost::property_tree::ptree&>
          (GetParamsSubTree(*a_params, mdc));
        auto instr = section.second.template get<std::string>("Instr");
        mdcParams.add("Ticker", instr);
        // Also process secondary symbols (e.g. for market data)
        auto refInstrs =
          section.second.template get<std::string>("RefInstrs", "");
        if (refInstrs != "")
        {
          std::vector<std::string> ri;
          boost::split(ri, refInstrs, boost::is_any_of(","));
          for (auto tckr: ri)
          {
            boost::trim(tckr);
            mdcParams.add("Ticker", tckr);
          }
        }

        // Add ticker to the corresponding OMC WITH AN ACCOUNT PFX
        auto accPfx = section.second.template get<std::string>("AccountPfx");
        auto omc = section.second.template get<std::string>("OMC");
        auto& omcParams = const_cast<boost::property_tree::ptree&>
          (GetParamsSubTree(*a_params, omc));
        omcParams.add("Ticker_" + accPfx, instr);
      }
  }

  //=========================================================================//
  // "SetLogPaths":                                                          //
  //=========================================================================//
  // For convenience: only specify Log folder, Strat name and log file names.
  // Example: LogFolderPfx = X.
  // - Creates "Logs-X/" folder.
  // - Loops through the config looking for "*LogFile" fields, changes those to
  //   "Logs-X/<*LogFile>"
  //
  inline void SetLogPaths(boost::property_tree::ptree* a_params)
  {
    auto logPfx = a_params->template get<std::string>("Exec.LogFolderPfx");
    auto strNme = a_params->template get<std::string>("Main.Title");
    auto logPth = logPfx + "-" + strNme + "/";
    std::filesystem::create_directory(logPth);

    for (auto& section: *a_params)
    {
      auto& sc = section.second;
      for (auto& kv: sc)
      {
        if (boost::algorithm::ends_with(kv.first, "LogFile"))
        {
          if (std::string(kv.second.data()) != "")
            sc.put(kv.first, logPth + std::string(kv.second.data()));
        }
      }

      if (strncmp(section.first.data(), "Exec", 4) == 0)
        section.second.put("LogFile", logPth + strNme);
    }
  }

  //=========================================================================//
  // "SetAccountKey":                                                        //
  //=========================================================================//
  // Currently used to amend PropTree sections for initialising OMCs :
  // (*) "AccountKey" is normally NOT provided directly in "a_params";
  //     rather, "AccountPfx" is provided, from which AccountKey is constructed
  //     at run time and stored in the PropTree ("a_params");
  // (*) XXX: The arg is a "const" ptr because it is normally returned
  //     by "GetParamsSubTree" above -- so it requires a "const_cast":
  //
  inline void SetAccountKey
  (
    boost::property_tree::ptree const* a_params,
    std::string const&                 a_val
  )
  {
    assert(a_params != nullptr);
    const_cast<boost::property_tree::ptree*>(a_params)->
      put("AccountKey",  a_val);
  }

  // Smilar to above, but for the specified Section where the AccountKey is in-
  // stalled:
  //
  inline boost::property_tree::ptree const& SetAccountKey
  (
    boost::property_tree::ptree* a_params,
    std::string const&           a_section,
    std::string const&           a_val
  )
  {
    assert(a_params  != nullptr);
    std::string  path = a_section + ".AccountKey";

    a_params->put(path, a_val);
    return GetParamsSubTree(*a_params, a_section);
  }
} // End namespace MAQUETTE
