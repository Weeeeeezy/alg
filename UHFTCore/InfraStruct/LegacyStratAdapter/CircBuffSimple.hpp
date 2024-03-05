// vim:ts=2:et:sw=2
//===========================================================================//
//                             "CircBuffSimple.hpp":                         //
//  Wait-Free Circular Buffer (Static or in Shared Memory). INSERTIONS ONLY: //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_CIRCBUFFSIMPLE_HPP
#define MAQUETTE_STRATEGYADAPTOR_CIRCBUFFSIMPLE_HPP

#include <cstddef>
#include <utxx/meta.hpp>
#include <utxx/math.hpp>
#include <utxx/compiler_hints.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <atomic>
#include <algorithm>
#include <utility>
#include <Infrastructure/Logger.h>
#include <cassert>

namespace BIPC = boost::interprocess;

namespace MAQUETTE
{
  //=========================================================================//
  // "CircBuffSimple" Class:                                                 //
  //=========================================================================//
  // NB:
  // (*) template Parameter "E" is the Entry Type;
  // (*) this structure is WAIT-FREE; it is assumed that it has only 1 Writer
  //     thread and any number of PASSIVE reader threads (which only
  //     access items stored on the Circular Buffer but do not remove them):
  //
  template<typename E, unsigned int StaticCapacity=0, bool WithSync=true>
  class CircBuffSimple
  {
  private:
    //-----------------------------------------------------------------------//
    // Internals:                                                            //
    //-----------------------------------------------------------------------//
    // Total number of Entries inserted (including those which over-wrote the
    // previous ones). It is "atomic" if WithSync param is set:
    //
    typename
      std::conditional<WithSync, std::atomic<unsigned int>, unsigned int>::type
      m_size;

    // Array holding the data, of "m_capacity" total length. XXX: If the Stat-
    // icCapacity is 0, we use a a C-style variable-size class / struct here
    // (ie the header and body are aloocated at once, for efficiency reasons).
    // Otherwise, the Capacity is rounded up to the nearest power of 2;
    // (NB: the result is 0 if Capacity is 0, ie the corresp exponent of 2 is
    // -oo):
    unsigned int m_capacity;
    unsigned int m_mask;
    E            m_entries[utxx::upper_power<StaticCapacity, 2>::value];

    // NB: Copy Ctor, Assignemnt and Equality Testing are not deleted but gen-
    // erally discouraged...

    //-----------------------------------------------------------------------//
    // Non-Default Ctor:                                                     //
    //-----------------------------------------------------------------------//
    // NB:
    // (*) it does not allocate the space for "m_entries" -- the Caller is res-
    //     ponsible for allocating the whole "CircBuffSimple" object of the cor-
    //     resp size;
    // (*) it is applicable only if the StaticCapacity is 0;
    // (*) the actualy capacity is rounded DOWN to a power of 2 from the value
    //     provided (based on the memory available);
    // The Ctor is private (but not deleted!) because it is supposed to  be in-
    // voked from within "CreateInShM" only:
    //
    CircBuffSimple(unsigned int capacity)
    : m_size      (0),
      m_capacity  (0),
      m_mask      (0)
    {
      if (StaticCapacity != 0 || capacity == 0)
        throw std::invalid_argument
              ("CircBuffSimple: InValid / InConsistent Capacity");

      unsigned int cap2 = utxx::math::upper_power(capacity, 2);
      assert(capacity >= 1 && cap2 >= 1);

      if (cap2 > capacity)
        cap2 /= 2;
      assert(1 <= cap2 && cap2 <= capacity);

      m_capacity = cap2;
      m_mask     = cap2 - 1;
      assert((m_capacity & m_mask) == 0);   // Power of 2 indeed!
    }

  public:
    //-----------------------------------------------------------------------//
    // Default Ctor:                                                         //
    //-----------------------------------------------------------------------//
    // (Does not make much sense unless StaticCapacity > 0):
    //
    CircBuffSimple()
    : m_size      (0),
      m_capacity  (sizeof(m_entries) / sizeof(E)),     // NB: ACTUAL capacity!
      m_mask      (m_capacity - 1)
    {
      // "m_capacity" is a power of 2:
      assert((m_capacity & m_mask) == 0);   // Power of 2 indeed!
    }

    // NB: Dtor is auto-generated, and trivial

    //-----------------------------------------------------------------------//
    // "IsEmpty":                                                            //
    //-----------------------------------------------------------------------//
    bool IsEmpty() const
      { return (m_size.load() == 0); }

    //-----------------------------------------------------------------------//
    // "Add": Inserts a new Entry into the Buffer:                           //
    //-----------------------------------------------------------------------//
    // Returns a direct access ptr to the Entry stored:
    //
    template<class ...Args>
    E const* Add(Args&&... ctor_args)
    {
      // Check if the "SeqNo" associated with the new entry is already there:
      // The curr front of the Buffer (where next insertion would occur):
      // NB: Since capacity is a power of 2, modular arithmetic can be done as
      // a logical AND:
      //
      unsigned int front = m_size & m_mask;
      assert(front < m_capacity);

      // Store the value at "front" and atomically increment "m_size", so the
      // Reader Clients will always have a consistent view;  "m_size"  itself
      // is monotonically-increasing (NOT modular).
      // The new entry is constructed in-place (a partial case is applying the
      // Copy Ctor on "E"):
      //
      E*   at  = m_entries + front;
      new (at) E(ctor_args...);
      m_size++;
      return at;
    }

    //-----------------------------------------------------------------------//
    // "GetCurrPtr": Ptr to the most recent entry:                           //
    //-----------------------------------------------------------------------//
    E const* GetCurrPtr() const
    {
      unsigned int sz =  m_size;
      if (sz == 0)
        // CANNOT produce a valid ptr because there are no, as yet, Entries to
        // point to:
        throw std::runtime_error("CircBuffSimple:GetCurrPtr: No Entries");

      // GENERIC CASE: The most-recently-inserted pos. Again, using the logical
      // AND for power-of-2 modular arithmetic:
      //
      unsigned int last = (sz - 1) & m_mask;
      assert(last < m_capacity);
      return m_entries + last;
    }

    //-----------------------------------------------------------------------//
    // "GetCurrIdx": Index of the most recent entry:                         //
    //-----------------------------------------------------------------------//
    unsigned int GetCurrIdx() const
    {
      unsigned int sz =  m_size;
      if (sz == 0)
        // CANNOT produce a valid ptr because there are no, as yet, Entries to
        // point to:
        throw std::runtime_error("CircBuffSimple:GetCurrIdx: No Entries");

      unsigned int last = (sz - 1) & m_mask;
      assert(last < m_capacity);
      return last;
    }

    //-----------------------------------------------------------------------//
    // "GetCumSize": Total Number of Entries Saved So Far:                   //
    //-----------------------------------------------------------------------//
    // (including over-written ones):
    //
    unsigned int GetCumSize() const { return m_size; }

    //-----------------------------------------------------------------------//
    // Accessing the Circular Buffer by Index:                               //
    //-----------------------------------------------------------------------//
    // "Const" version:
    //
    E const& operator[](unsigned int idx) const
    {
#     ifndef NDEBUG
      // If the buffer is full, then any "idx" (< capacity) is OK; otherwise,
      // it must be < size:
      unsigned int limit = std::min<unsigned int>(m_size.load(), m_capacity);
      if (utxx::unlikely(idx >= limit))
        throw std::invalid_argument
              ("CircBuffSimple: Invalid Idx=" + std::to_string(idx)      +
               ": Capacity=" + std::to_string(m_capacity) + ", TotSize=" +
               std::to_string(m_size));
      assert(idx < m_capacity);
#     endif
      return m_entries[idx];
    }

    // "Non-Const" version: The logic is similar to the above:
    //
    E& operator[](unsigned int idx)
    {
#     ifndef NDEBUG
      unsigned int limit = std::min<unsigned int>(m_size, m_capacity);
      if (utxx::unlikely(idx >= limit))
        throw std::invalid_argument
              ("CircBuffSimple: Invalid Idx=" + std::to_string(idx)      +
               ": Capacity=" + std::to_string(m_capacity) + ", TotSize=" +
               std::to_string(m_size));
      assert(idx < m_capacity);
#     endif
      return m_entries[idx];
    }

    //-----------------------------------------------------------------------//
    // "CreateInShM":                                                        //
    //-----------------------------------------------------------------------//
    // A factory function which allocates a new "CircBuffSimple"  of a given
    // "capacity" on a given ShM "segment", and runs a Ctor to initialise it.
    // Because the object has a dynamically-determined size, it is allocated
    // as an array of "long double"s (this guarantees proper alignment), and
    // that array ptr is converted into void* in "placement new":
    //
    static CircBuffSimple<E, StaticCapacity, WithSync>* CreateInShM
    (
      int                                capacity,
      BIPC::fixed_managed_shared_memory* segment,
      char const*                        obj_name,
      bool                               force_new
    )
    {
      // Does it already exist?
      assert(obj_name != NULL && *obj_name != '\0');
      std::pair<long double*, size_t> fres =
        segment->find<long double>(obj_name);
      bool exists = (fres.first != NULL);

      if (exists && !force_new)
      {
        assert(fres.second != 0);
        return (CircBuffSimple<E>*)(fres.first);
      }

      // Otherwise: Create a new CircBuff:
      assert(segment != NULL);
      if (capacity <= 0)
        throw std::invalid_argument
              ("CircBuffSimple::CreateInShM: Invalid Capacity)");
      try
      {
        // Remove the existing one first:
        if (exists)
          (void) segment->destroy<long double>(obj_name);

        // Calculate the full object size:
        int buffSz  = sizeof(CircBuffSimple<E>) + capacity * sizeof(E);

        // Allocate it as a named array of bytes, zeroing them out. XXX: how-
        // ever, we need a right alignment here -- so do 16-byte alignment:
        // XXX:  or do we need an explicit alignment? Maybe it's 16 bytes by
        // default on 64-bit systems?
        //
        static_assert(sizeof(long double) == 16, "sizeof(long double)!=16 ?");
        int sz16 = (buffSz % 16 == 0) ? (buffSz / 16) : (buffSz / 16 + 1);

        long double* mem =
          segment->construct<long double>(obj_name)[sz16](0.0L);
        assert(mem != NULL);

        // Run the "placement new" and the CircBuff Ctor on the bytes allo-
        // cated:
        CircBuffSimple<E, StaticCapacity, WithSync>* res =
          new ((void*)(mem))
              CircBuffSimple<E, StaticCapacity, WithSync>(capacity);
        assert(res != NULL);
        return res;
      }
      catch (std::exception const& exc)
      {
        // Delete the CircBuff if it has already been constructed:
        (void) segment->destroy<long double>(obj_name);

        // Log and re-throw the error:
        SLOG(ERROR) << "CircBuffSimple::CreateInShM: " << obj_name
                   << ": Cannot Create: "     << exc.what() << std::endl;
        throw;
      }
    }

    //=======================================================================//
    // "DestroyInShM":                                                       //
    //=======================================================================//
    // De-allocates a named "CircBuffSimple" object from ShM:
    //
    static void DestroyInShM
    (
      CircBuffSimple<E, StaticCapacity, WithSync>* obj,
      BIPC::fixed_managed_shared_memory*           segment
    )
    {
      if (obj == NULL)
        return; // Nothing to do

      // Run a Dtor, although it is trivial in this case:
      obj->CircBuffSimple<E, StaticCapacity, WithSync>::
          ~CircBuffSimple<E, StaticCapacity, WithSync>();

      // De-allocate the CircBuff by ptr. NB: It was allocated as an array of
      // "long double"s with 16-byte alignment, so do same for de-allocation:
      //
      assert(segment != NULL);
      segment->destroy_ptr((long double*)(obj));
    }

    //=======================================================================//
    // "Clear":                                                              //
    //=======================================================================//
    // The data are cleared but the CircBuff itself is preserved. XXX: This ob-
    // viously invalidates all Entry ptrs, so USE WITH CARE:
    //
    void Clear() { m_size = 0; }
  };
}
#endif  // MAQUETTE_STRATEGYADAPTOR_CIRCBUFFSIMPLE_HPP
