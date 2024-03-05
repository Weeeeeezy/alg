// vim:ts=2:et
//===========================================================================//
//          "Connectors/H2WS/Huobi/EConnector_H2WS_Huobi_Common.hpp"         //
//===========================================================================//
#pragma once

#define ZLIB_CONST
#include "zlib.h"

#include <fstream>
#include <array>

namespace MAQUETTE
{
namespace Huobi
{
  static voidpf zlib_alloc(voidpf opaque, uInt items, uInt size);
  static void zlib_free(voidpf, voidpf) {}

  struct zlib
  {
    std::array<char, 1024 * 1024> buf;
    std::array<char, 4 * 1024 * 1024> abuf;
    uint32_t abuf_used;

    z_stream stream;

    zlib()
    {
      stream.zalloc = &zlib_alloc;
      stream.zfree = &zlib_free;
      stream.opaque = this;
    }

    std::pair<char const*, char const*> decompress(
        char const* msg,
        uint32_t    size)
    {
      stream.next_in = reinterpret_cast<Bytef const*>(msg);
      stream.avail_in = size;
      abuf_used = 0;

      stream.next_out = reinterpret_cast<Bytef*>(&buf[0]);
      stream.avail_out = uInt(buf.size());

      if(inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
        throw std::runtime_error("zlib::inflateInit() error");

      int ret = inflate(&stream, Z_FINISH);
      if(ret == Z_STREAM_END)
      {
        buf[stream.total_out] = '\0';
        return {&buf[0], &buf[0] + stream.total_out};
      }
      throw std::runtime_error("zlib::inflate() error");
    }

    void* alloc(uInt items, uInt size)
    {
      uint32_t req = items * size;
      if(abuf.size() < req + abuf_used)
          throw std::runtime_error("zlib::alloc error");

      uint32_t ret = abuf_used;
      abuf_used += req;
      return &abuf[ret];
    }
  };

  static voidpf zlib_alloc(voidpf opaque, uInt items, uInt size)
  {
    zlib* z = reinterpret_cast<zlib*>(opaque);
    return z->alloc(items, size);
  }

  template <typename str>
  inline void skip_fixed(char const*& it, str const& v)
  {
    static_assert(std::is_array<str>::value);
    bool eq = std::equal(it, it + sizeof(v) - 1, v);
    if (utxx::unlikely(!eq))
    {
      throw utxx::runtime_error(
          "skip_fixed error, expect: |",
          v,
          "| in |",
          std::string(it, it + 100));
    }
    it += sizeof(v) - 1;
  }

  // Move pointer after next occurence of character
  inline void skip_to_next(char const*& begin, char const* end, char c)
  {
    begin = std::find(begin, end, c);
    if (utxx::unlikely(begin == end))
    {
      throw utxx::runtime_error("EConnector_Huobi::skip to next failed");
    }
    ++begin;
  }

  // Move pointer after next occurrence of string
  inline void skip_to_next(
      char const*&            begin,
      char const*             end,
      std::string_view const& v)
  {
    begin = std::search(begin, end, std::begin(v), std::end(v));
    if (utxx::unlikely(begin == end))
    {
      throw utxx::runtime_error("EConnector_Huobi::skip to next failed");
    }
    begin += v.size();
  }

  template <typename str>
  inline bool skip_if_fixed(char const*& it, str const& v)
  {
    static_assert(std::is_array<str>::value);
    bool eq = std::equal(it, it + sizeof(v) - 1, v);
    if (eq)
      it += sizeof(v) - 1;
    return eq;
  }

  inline PriceT read_price(char const* from, char const* to)
  {
    char*  after = nullptr;
    double px    = strtod(from, &after);
    if (utxx::unlikely(after != to))
    {
      throw utxx::runtime_error(
          "EConnector_Huobi::read_price error: ",
          std::string(from, to));
    }
    return PriceT(px);
  }

  inline Qty<QtyTypeT::QtyA, double> read_count(
      char const* from,
      char const* to)
  {
    char*  after = nullptr;
    double qt    = strtod(from, &after);
    if (utxx::unlikely(after != to))
    {
      throw utxx::runtime_error(
          "EConnector_Huobi::read_count error: ",
          std::string(from, to));
    }
    return Qty<QtyTypeT::QtyA, double>(qt);
  }

  template <typename itype>
  inline itype read_int(char const* from, char const* to)
  {
    itype       v  = 0;
    char const* it = utxx::fast_atoi<itype, false>(from, to, v);
    if (utxx::unlikely(it != to))
    {
      throw utxx::runtime_error(
          "EConnector_Huobi::read_int error: ",
          std::string(from, to));
    }
    return v;
  }

} // namespace Huobi
} // namespace MAQUETTE
