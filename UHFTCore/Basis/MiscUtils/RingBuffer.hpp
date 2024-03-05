// vim:ts=2:et
//===========================================================================//
//                    "Basis/MiscUtils/RingBuffer.hpp":                      //
//===========================================================================//
#pragma once

#include "Basis/Maths.hpp"
#include "utxx/compiler_hints.hpp"
#include <iostream>
#include <boost/container/static_vector.hpp>

namespace MAQUETTE
{
  template <class T, std::size_t Cap>
  class RingBuffer
  {
    static_assert(std::is_arithmetic_v<T>, "Buffer Type has to be arithmetic");

  public:
    RingBuffer(std::size_t a_size, T a_val = T()): m_size(a_size)
    {
      m_buffer.resize(m_size, a_val);
    }

    void Fill(T a_item)
    {
      m_full = true;
      m_head = m_tail;
      for (std::size_t i = 0; i < m_size; ++i)
        m_buffer[i] = a_item;
    }

    void Reset()
    {
      m_head = m_tail;
      m_full = false;
    }

    bool Empty() const
    { return (!m_full && (m_head == m_tail)); }

    bool Full() const
    { return m_full; }

    std::size_t Capacity() const
    { return m_size; }

    std::size_t Size() const
    {
      return m_full ? m_size
                    : m_head > m_tail ? m_head - m_tail
                                      : m_size + m_head - m_tail;
    }

    // Data manipulation
    T PushBack(T a_item);

    void Print() const;

  protected:
    boost::container::static_vector<T, Cap> m_buffer;
    std::size_t m_size = 0;

    std::size_t m_head = 0;
    std::size_t m_tail = 0;
    bool m_full = false;
  };

  template <class T, std::size_t Cap>
  inline T RingBuffer<T, Cap>::PushBack(T a_item)
  {
    T prevValue = m_buffer[m_head];
    m_buffer[m_head] = a_item;

    if (utxx::likely(m_full))
    {
      ++m_tail;
      m_tail %= m_size;
    }
    ++m_head;
    m_head %= m_size;
    m_full  = m_head == m_tail;

    // Return the value that has been replaced
    return prevValue;
  }

  template <class T, std::size_t Cap>
  inline void RingBuffer<T, Cap>::Print() const
  {
    for (std::size_t i = 0; i < m_size; ++i)
      std::cout << double(m_buffer[i]) << "  ";
    std::cout << std::endl;
  }
}
// End namespace MAQUETTE
