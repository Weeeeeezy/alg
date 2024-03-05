// vim:ts=2:et:sw=2
//===========================================================================//
//                           "Configs_Currenex.h":                           //
//===========================================================================//
#ifndef MAQUETTE_CONNECTORS_CONFIGS_CURRENEX_H
#define MAQUETTE_CONNECTORS_CONFIGS_CURRENEX_H

#include <string>
#include <utxx/config_tree.hpp>
#include <Infrastructure/Marshal.h>
#include <Connectors/IOChannel.h>
#include <StrategyAdaptor/EventTypes.h>

namespace CNX
{
  //=========================================================================//
  // I/O Channel's auxillary data                                            //
  //=========================================================================//
  struct ChannelStateData : public MAQUETTE::ChannelStateData<>
  {
    using Base   = MAQUETTE::ChannelStateData<>;
    using StateT = typename Base::StateT;

    ChannelStateData() = default;
    using Base::ChannelStateData;
    ChannelStateData(ChannelStateData  const& a) = delete;
    ChannelStateData(utxx::config_tree const& a) : Base(a) {}
  };

  // Conversion of an Erlang term into a SymKey:
  //
  inline Events::SymKey ToSymKey(const eixx::string& a_str)
  {
    Events::SymKey key;
    Events::InitFixedStrGen(&key, a_str.c_str());
    return key;
  }
}
#endif  // MAQUETTE_CONNECTORS_CONFIGS_CURRENEX_H