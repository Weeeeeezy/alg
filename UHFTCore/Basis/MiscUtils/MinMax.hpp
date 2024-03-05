// vim:ts=2:et
//===========================================================================//
//                     "Basis/MiscUtils/RingBuffer.hpp":                     //
//===========================================================================//
#pragma once
#include "Basis/MiscUtils/RingBuffer.hpp"

namespace MAQUETTE
{
  template <class T, std::size_t Cap>
  class MinMaxCalcer: public RingBuffer<T, Cap>
  {
  public:
    using Buffer = RingBuffer<T, Cap>;
    MinMaxCalcer(std::size_t a_size) : Buffer(a_size)
    {}

    template <bool IsMin>
    void Update();

    T PushBack(T a_item);

    void Refresh();

    T GetMin() const { return m_min; }
    T GetMax() const { return m_max; }

  protected:
    T m_min = NaN<T>;
    T m_max = NaN<T>;
  };

  template <class T, std::size_t Cap>
  inline void MinMaxCalcer<T, Cap>::Refresh()
  {
    Update<true>();
    Update<false>();
  }

  template <class T, std::size_t Cap>
  template <bool IsMin>
  inline void MinMaxCalcer<T, Cap>::Update()
  {
    auto& buff = this->m_buffer;
    auto  size = this->Size();

    if constexpr (IsMin)
    {
      m_min = buff[0];
      for (std::size_t i = 1; i < size; ++i)
        if (buff[i] < m_min)
          m_min = buff[i];
    }
    else
    {
      m_max = buff[0];
      for (std::size_t i = 1; i < size; ++i)
        if (buff[i] > m_max)
          m_max = buff[i];
    }
  }

  template <class T, std::size_t Cap>
  inline T MinMaxCalcer<T, Cap>::PushBack(T a_item)
  {
    auto replaced = Buffer::PushBack(a_item);

    // Min/Max not set yet
    if (utxx::unlikely(!IsFinite(m_min) || !IsFinite(m_max)))
    {
      m_min = a_item;
      m_max = a_item;
      return replaced;
    }

    // New min
    if (utxx::likely(m_min > a_item))
      m_min = a_item;
    else
    // Or old min removed, recalc
    if (utxx::unlikely(replaced == m_min))
      Update<true>();

    // New max
    if (utxx::likely(m_max < a_item))
      m_max = a_item;
    else
    // Or old max removed, recalc
    if (utxx::unlikely(replaced == m_max))
      Update<false>();

    return replaced;
  }
}
// End namespace MAQUETTE
