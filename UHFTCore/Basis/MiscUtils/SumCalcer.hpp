// vim:ts=2:et
//===========================================================================//
//                       "Basis/MiscUtils/SumCalcer.hpp":                    //
//===========================================================================//
#pragma once
#include "Basis/MiscUtils/RingBuffer.hpp"

namespace MAQUETTE
{
  template <class T, std::size_t Cap>
  class SumCalcer: public RingBuffer<T, Cap>
  {
  public:
    using Buffer = RingBuffer<T, Cap>;
    SumCalcer(std::size_t a_size) : Buffer(a_size)
    {}

    T PushBack(T a_item);

    T GetSum() const { return m_sum; }

    void Refresh();

  protected:
    T m_sum = T();
  };

  template <class T, std::size_t Cap>
  inline void SumCalcer<T, Cap>::Refresh()
  {
    m_sum = T();
    for (std::size_t i = 0; i < this->Size(); ++i)
      m_sum += this->m_buffer[i];
  }

  template <class T, std::size_t Cap>
  inline T SumCalcer<T, Cap>::PushBack(T a_item)
  {
    auto replaced = Buffer::PushBack(a_item);

    m_sum += a_item - replaced;

    return replaced;
  }
}
// End namespace MAQUETTE
