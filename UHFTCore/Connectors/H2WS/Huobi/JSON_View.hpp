// vim:ts=2:et
//===========================================================================//
//            "Connectors/H2WS/Huobi/json_view.hpp"                          //
//===========================================================================//
#pragma once

#include "Connectors/H2WS/Huobi/EConnector_Huobi_Common.hpp"

namespace MAQUETTE
{
  /**
    Json-view is a fast accessor to json fields in flat dense json
    @a size is a maximum parsed elements in json
  */
  template<uint32_t size>
  class json_view
  {
      struct element
      {
        std::string_view name;
        char const* from;
        char const* to;
        bool operator <(element const& el)
        {
          return name < el.name;
        }
      };

    public:
      // Construct view from string slice
      json_view(char const* begin, char const* end)
      {
        uint32_t count{0};
        for(auto it = begin; it != end; ++count)
        {
          if(utxx::unlikely(count > size) || *it != '"')
          {
            throw utxx::runtime_error(
                  "json_view<", size, "> parse error:",
                  std::string(begin, end));
          }
          ++it;

          char const* ne = std::find(it, end, '"');
          auto& e = m_elements[count];
          e.name = std::string_view(it, uint32_t(ne - it));
          it = ne + 1;
          if(*it != ':')
          {
            throw utxx::runtime_error(
                  "json_view<", size, "> parse error:",
                  std::string(begin, end));
          }
          ++it;

          e.from = it;
          it = std::find(it, end, ',');
          e.to = it;
          if(it != end)
          {
            ++it;
          }
        }
        std::sort(m_elements.begin(), m_elements.end());
      }

    std::string_view get(std::string_view n, bool remove_quotes = false)
    {
      element tmp{n, nullptr, nullptr};
      auto e  = std::lower_bound(m_elements.begin(), m_elements.end(), tmp);
      if (utxx::unlikely(e == m_elements.end() || e->name != n))
      {
        throw utxx::runtime_error(
              "json_view<",
              size,
              "> element not found:",
              n);
      }
      if (remove_quotes)
      {
        if (utxx::unlikely(*(e->from) != '\"' || *(e->to - 1) != '\"'))
          throw utxx::runtime_error(
              "json_view<",
              size,
              "> quotes not found for:", n);
        else
        {
          return std::string_view(e->from + 1, size_t(e->to - e->from - 2));
        }
      }
      else
      {
        return std::string_view(e->from, size_t(e->to - e->from));
      }
    }

    template<typename func>
    auto get(std::string_view n, func f, bool remove_quotes = false)
    {
      auto v = get(n, remove_quotes);
      return f(v.data(), v.data() + v.size());
    }

    private:
      std::array<element, size> m_elements;
  };
}
