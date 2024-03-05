// vim:ts=2:et
//===========================================================================//
//                        "Connectors/SeqNumBuffer.hpp":                     //
//            Buffer for storing Data Items equipped with "SeqNum"s          //
//===========================================================================//
// Provides proper sequencing of Items coming from 1 or more  Data Channles,
// possibly out-of-order (as may happen eg for UDP data feeds). Valid range
// of "SeqNum"s is assumed to be 1..+oo:
//
#pragma once

#include "Basis/BaseTypes.hpp"
#include <utxx/compiler_hints.hpp>
#include <utxx/error.hpp>
#include <utxx/time_val.hpp>
#include <spdlog/spdlog.h>
#include <boost/core/noncopyable.hpp>
#include <utility>
#include <cassert>

namespace MAQUETTE
{
  //=========================================================================//
  // "SeqNumBuffer" Class:                                                   //
  //=========================================================================//
  template
  <
    typename T,         // Type of msgs being buffered
    typename Emplacer,  // (T* place)->void
    typename Processor  // (SeqNum sn, T const& item, bool init_mode)->void
  >
  class SeqNumBuffer: public boost::noncopyable
  {
  private:
    //=======================================================================//
    // "BuffEntry" Type:                                                     //
    //=======================================================================//
    // Each Buffer Entry is actually an object of type "T" along with its Seq-
    // Num and TimeStamps:
    struct BuffEntry
    {
      SeqNum          m_sn;
      T               m_item;
      utxx::time_val  m_recvTS;
      utxx::time_val  m_handlTS;
    };

    //=======================================================================//
    // Data Flds:                                                            //
    //=======================================================================//
    BuffEntry*        m_data;       // The actual storage
    int               m_capacity;   // Total size of "m_data" array above
    int const         m_maxGap;
    Processor*        m_processor;  // Ptr NOT owned
    spdlog::logger*   m_logger;     // Ptr NOT owned
    int               m_debugLevel;

    mutable int       m_bi;         // BeginIndex of curr window (0 if empty)
    mutable int       m_ei;         // EndIndex   of curr window (NOT incl)
    mutable SeqNum    m_xsn;        // Expected next SeqNum
    mutable bool      m_initMode;   // Init Mode

    // Default Ctor is deleted:
    SeqNumBuffer() =  delete;

  public:
    //=======================================================================//
    // Non-Default Ctor:                                                     //
    //=======================================================================//
    // NB:
    // (*) If "a_init_mode" is set, we initially allow unlimited-size gaps (be-
    //     cause incoming Items are accumulated and not immediately processed).
    //     The Init Mode is reset when "CloseInitMode" is invoked (this normal-
    //     ly happens after all initialising SnapShots and concurrent IncrUpda-
    //     tes are received and processed).
    // (*) Otherwise, there is no Init Mode -- all gaps are managed in the stan-
    //     dard way (ie gaps > "a_max_gap" will always mean data loss).
    // (*) This could also be achieved via a bool template param, but the advan-
    //     tages of that solution appear to be very minimal:
    //
    SeqNumBuffer
    (
      int             a_capacity,
      int             a_max_gap,
      Processor*      a_processor,
      spdlog::logger* a_logger,
      int             a_debug_level,
      bool            a_init_mode
    )
    : m_data      (new BuffEntry[size_t(a_capacity)]),
      m_capacity  (a_capacity),
      m_maxGap    (a_max_gap),
      m_processor (a_processor),
      m_logger    (a_logger),
      m_debugLevel(a_debug_level),
      m_bi        (0),
      m_ei        (0),
      m_xsn       (-1),    // NB: Don't use 0 which is valid in Init Mode
      m_initMode  (a_init_mode)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely
            (m_capacity <= 0 || m_processor == nullptr) || m_logger == nullptr)
          throw utxx::badarg_error("SeqNumBuffer::Ctor");
      )
      // NB: For extra safety, explicitly initialise all "SeqNum"s in "m_data"
      // to (-1), not even to 0:
      for (int i = 0; i < m_capacity; ++i)
        m_data[i].m_sn = -1;
    }

    //=======================================================================//
    // Dtor:                                                                 //
    //=======================================================================//
    ~SeqNumBuffer() noexcept
    {
      if (m_data != nullptr)
      {
        delete[] m_data;
        m_data = nullptr;
      }
      m_capacity  = 0;
      // m_maxGap is a "const", cannot reset it
      m_processor = nullptr;
      m_bi        = 0;
      m_ei        = 0;
      m_xsn       = -1;
      m_initMode  = false;    // Does not really matter
    }

    //=======================================================================//
    // "CloseInitMode":                                                      //
    //=======================================================================//
    void CloseInitMode(SeqNum a_sn)
    {
      CHECK_ONLY
      (
        if (utxx::unlikely(!m_initMode))
          // Simply do nothing -- repreated invocation is OK:
          return;

        // "a_sn" is the "lowest SnapShot SeqNum", so it must be valid;  there
        // is no way of closing the Init Mode without providing a valid SeqNum
        // arg (though it may or may not have a direct effect):
        //
        if (utxx::unlikely(a_sn <= 0))
          throw utxx::badarg_error
                ("SeqNumBuffer::CloseInitMode: Invalid LowestSnapShotSN=",
                 a_sn);
      )
      // If the Buffer is currently non-empty (contains a non-empty window):
      if (utxx::likely(m_ei > 0))
      {
        // (*) Purge any Items with with SeqNum <= a_sn (ie up to and including
        //     the earliest SnapShot available) from the Buffer, since they will
        //     definitely not be required anymore.
        // (*) Also close the artificially-introduced initial gap created when
        //     the 1st Item was buffered in the Init Mode; m_xsn==0 may be the
        //     result of such a gap (in all other cases,   m_xsn >0):
        // (*) This artificial gap could not have been closed earlier via a nor-
        //     mal "Put" -- we have an explicit check preventing that. It could
        //     only be closed by this method:
        //
        assert(m_bi == 0   && m_ei >= 2 && m_data[0].m_sn == -1 &&
               m_data[m_ei-1].m_sn >  0 && m_xsn >= 0);

        // Thus, the new XSN will be:
        SeqNum newXSN = std::max<SeqNum>(m_xsn, a_sn) + 1;
        assert(newXSN > 0);

        // The number of obsolete items to be purged (will be at least 1, which
        // corresponds to an empty slot in front, which would never be filled in
        // a normal way):
        int     purgeCount = int(newXSN - m_xsn);
        assert (purgeCount > 0);

        // Purging outdated Items is incrementing "m_bi" AND re-setting the slot
        // "SeqNum"s:
        {
          BuffEntry*       curr = m_data;
          BuffEntry const* end  = curr  + purgeCount;
          while (curr < end)
          {
            curr->m_sn = -1;    // For [m_bi], it is (-1) already anyway: empty
            ++curr;
          }
        }
        m_bi = purgeCount;

        // But the Buffer could have become completely empty as a result; then
        // reset it properly:
        if (m_bi >= m_ei)
          m_bi = m_ei = 0;

        // Can now actually set the new Expected SeqNum:
        m_xsn  = newXSN;
        assert(m_xsn > 0);

        // Furthermore, if we got a congiguous window as a result of moving XSN
        // forward, it needs to be processed; this would increase "m_xsn" furt-
        // her:
        if (m_data[m_bi].m_sn != -1)
          // In particular, then the SeqNum @ [m_bi] must be equal to "m_xsn"
          // (verified by the the following method):
          SafeProcessContiguous();
      }
      else
      {
        // The Buffer was empty: No Items were received before the SnapShot
        // arrived:
        assert(m_bi == 0 && m_ei == 0 && m_xsn == -1);
        m_xsn = a_sn + 1;
      }

      // ONLY NOW we can reset the Init Mode (DON'T DO IT EARLIER,  because  we
      // don't want to pass init_mode=false to the Processor above: it would be
      // misleading:
      m_initMode = false;
    }

    //=======================================================================//
    // "GetExpectedSeqNum":                                                  //
    //=======================================================================//
    // This is for information only:
    SeqNum GetExpectedSeqNum() const
      { return m_xsn; }

    //=======================================================================//
    // Safe "Emplace" operation -- exceptions are caught:                    //
    //=======================================================================//
    void SafeEmplace
    (
      Emplacer*       a_emplacer,
      SeqNum          a_sn,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl,
      int             a_pos
    )
    {
      // The window can be empty, or non-empty (in which case it has length of
      // at least 2);
      // XXX: the pre-cond of this method is that in the worst case, "a_pos"
      // could be @ "m_ei";   if it happened to be beyond that, "m_ei" would
      // first be corrected by the Caller:
      assert((m_bi == 0 && m_ei == 0     && a_pos == 0)                     ||
             (0 <= m_bi && m_bi <= a_pos && a_pos <= m_ei && m_bi <= m_ei-2 &&
              m_ei <= m_capacity         && a_pos <  m_capacity));

      // NB: The "a_pos" must always be clear (SeqNum==-1); "m_xsn" may be 0 in
      // a rare case in the Init Mode:
      BuffEntry& targ = m_data[a_pos];   // XXX: Ref!

      // The target slot must be empty at this point:
      assert(a_emplacer != nullptr && a_sn  >  0 && a_sn >= m_xsn &&
             targ.m_sn  == -1      && m_xsn >= 0);

      // Mark this slot as occupied (even if the Emplacer below fails):
      targ.m_sn      = a_sn;
      targ.m_recvTS  = a_ts_recv;
      targ.m_handlTS = a_ts_handl;
      try
      {
        // Actually invoke the Emplacer: It installs the actual Data Item:
        (*a_emplacer)(&(targ.m_item));
      }
      catch (std::exception const& exc)
      {
        // XXX: Generally speaking, if Item Emplacement has failed, the corresp
        // Item would better NOT be processed later to avoid the risk of double-
        // exception; but this situation, over-all, is extremely unlikely, so we
        // do not handle it specifically:
        LOG_ERROR(2,
          "SeqNumBuffer::SafeEmplace: Emplacement of SN={} (XSN={}) @ Pos={} "
          "failed: {}", a_sn, m_xsn, a_pos, exc.what())
      }
      // IMPORTANT: The window is extended if we emplaced the Item @ "m_ei":
      if (a_pos == m_ei)
      {
        ++m_ei;
        assert(m_ei <= m_capacity);
      }
    }

    //=======================================================================//
    // Safe "Process" operation -- exceptions are caught:                    //
    //=======================================================================//
    void SafeProcessContiguous()
    {
      // NB:
      // (*) At the point when this method is invoked, the window invariant is
      //     "broken": the window starts from a non-empty slot @ "m_bi",  and
      //     has a minimum length of 1 (not 2);
      // (*) "m_ei" is still BEYOND the last item of the window, so the window
      //     length is (m_si - m_bi) as  usual;
      // (*) While "m_xsn" may (very unlikely) be 0 in "SafeEmplace", it is al-
      //     ways > 0 here:
      assert(m_xsn > 0 && m_bi <= m_ei-1 &&
             m_data[m_bi].m_sn == m_xsn  && m_data[m_ei-1].m_sn > 0);

      // We process all contiguous pending Items and increase "m_xsn" while do-
      // ing so:
      while (m_bi < m_ei)
      {
        // The "curr" Item, if non-empty, must correspond to "m_xsn". The 1st
        // Item to be processed must always be non-empty  (hence "do-while"),
        // since "Process" is invoked after an "Emplace" @ "m_xsn":
        BuffEntry* curr    = m_data + m_bi;
        SeqNum     curr_sn = curr->m_sn;

        if (curr_sn == -1)
          // The contiguous chunk has ended:
          break;

        // Otherwise, go ahead. At the beginning of the window, there should be
        // an item with a valid SeqNum, same as the expected value (m_xsn > 0);
        // in particular, "curr_sn" cannot be 0:
        assert(curr_sn == m_xsn);

        // Actually invoke the Processor. Invocation of all in-depth call-backs
        // occurs HERE:
        try
        {
          (*m_processor)
            (curr->m_sn,     curr->m_item,  m_initMode,
             curr->m_recvTS, curr->m_handlTS);
        }
        catch (std::exception const& exc)
        {
          LOG_ERROR(2,
            "SeqNumBuffer::SafeProcessContiguous: Processing of SN={} failed: "
            "{}", m_xsn, exc.what())
        }
        // Successfully processed or otherwise, the Item is deemed to be done;
        // NOW we increment the next Expected SeqNum and clean-up the slot:
        curr->m_sn = -1;
        ++m_bi;
        ++m_xsn;
      }

      // IMPORTANT: If the window becomes empty at the end (ie we have completed
      // the above loop to the end), reset in properly:
      if (utxx::likely(m_bi >= m_ei))
        m_bi = m_ei = 0;

      // NB: The above logic works correctly, in particular, in the most typi-
      // cal case when the Buffer was originally empty (no window),  then we
      // emplaced an expected Item @ m_bi==0  (and did NOT increment "m_ei"),
      // and then called "SafeProcessContguous" -- processing will be done and
      // the Buffer will remain empty.
    }

    //=======================================================================//
    // "Put":                                                                //
    //=======================================================================//
    // To be invoked when a new item of type "T" is arriving; its SeqNum  is al-
    // ready known, but the item itself may not be constructed yet (this allows
    // for in-place construction and processing, see the implementation):
    // Optionally, we can immediately create a Gap Window in the buffer by spe-
    // cifying the expected expected SeqNum ("a_xsn") which is LESS than "a_sn",
    // but this can only be done if the Buffer is currently empty, and NOT in
    // the InitMode.
    // Returns "true" iff the msg with the given "a_sn" has actually been put
    // into the Buffer.
    //
    bool Put
    (
      Emplacer*       a_emplacer,
      SeqNum          a_sn,
      utxx::time_val  a_ts_recv,
      utxx::time_val  a_ts_handl,
      SeqNum          a_xsn = -1  // XXX: Use with care!
    )
    {
      //---------------------------------------------------------------------//
      // Verification:                                                       //
      //---------------------------------------------------------------------//
      // Check the Args: "a_sn" must be a valid SeqNum:
      assert(a_emplacer != nullptr);
      CHECK_ONLY
      (
        if (utxx::unlikely(a_sn <= 0))
          throw utxx::badarg_error("SeqNumBuffer::Put: Invalid SeqNum=", a_sn);
      )
      // Check the following invariants:
      // (*) either the Buffer is empty (m_bi==m_ei==0),
      // (*) or it has a valid window open within it, starting  from an empty
      //     slot and ending with a non-empty one (@ (m_ei-1)), so the minimum
      //     window size is 2:
      // (*) In the Init Mode, "m_xsn" may (very unlikely) be 0 here with a
      //     non-empty buffer:
      //
      CHECK_ONLY(bool isEmpty = (m_bi == 0 && m_ei == 0);)

      assert(isEmpty                                                      ||
             (0 <= m_bi && m_bi <= m_ei - 2    &&  m_ei <= m_capacity     &&
             m_xsn >= 0 && m_data[m_ei-1].m_sn == (m_xsn + m_ei - m_bi -1)));

      // So in any case, the slot @ "m_bi" is empty (corresponds to "m_xsn"
      // which is expected but has not arrived yet):
      assert(m_data[m_bi].m_sn == -1);

      //---------------------------------------------------------------------//
      // Special Case (New Explicit XSN to create a Gap Window)?             //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(a_xsn > 0))
      {
        // To avoid any inconsistencies, setting XSN  is only allowed for an
        // empty Buffer, and NOT in the InitMode. We also check that the new
        // XSN is non-decreasing:
        //
        CHECK_ONLY
        (
          if (utxx::unlikely(!isEmpty || a_xsn < m_xsn || m_initMode))
            throw utxx::badarg_error
                  ("SeqNumBuffer::Put: Cannot set NewXSN=", a_xsn, ": BI=",
                   m_bi,  ", EI=",  m_ei,  ", IsEmpty=",    isEmpty,
                   ", CurrXSN=",    m_xsn, ", InitMode=",   m_initMode);
        )
        // Then simply set the new XSN and proceed normally:
        m_xsn = a_xsn;
      }

      //---------------------------------------------------------------------//
      // Very first invocation?                                              //
      //---------------------------------------------------------------------//
      if (utxx::unlikely(m_xsn == -1))
      {
        // This can only happen if the Buffer was empty and has never received
        // any Items yet -- otherwise, we would know the next SeqNum we are wa-
        // iting for (incl 0, which is an artificial wait target which will ne-
        // ver arrive, of course).
        // NB: The converse is of course NOT true -- the Buffer can be empty but
        // XSN may be non-trivial (awaiting a next Item):
        assert(isEmpty);

        // Then initialise "m_xsn":
        // (*) If the Init Mode is set, then we must  NOT start processing  the
        //     Items until the SnapShots are received, that is, until "CloseIn-
        //     itMode" is called explicitly; so we must open the Gap Window; to
        //     do that in a generic way, we create an artficial gap at the beg-
        //     inning (m_xsn = a_sn-1);  it is HERE "x_sn"  may (theoretically,
        //     but very unlikely) become 0;
        // (*) Otherwise, there is no gap -- "m_xsn" will start from "a_sn":
        //
        m_xsn  = m_initMode ? (a_sn - 1) : a_sn;
        assert(m_xsn >= 0);

        // NB: m_xsn==0 is possible if the very 1st Item we have received was
        // with a_sn==1, and it was in the Init Mode. THIS IS THE PRIMARY REA-
        // SON why we had to make "SeqNum" a Signed type, and use (-1), rather
        // than 0, for the empty value!
      }
      else
      //---------------------------------------------------------------------//
      // Out-Dated Item?                                                     //
      //---------------------------------------------------------------------//
      if (utxx::likely(a_sn < m_xsn || (m_initMode && a_sn == m_xsn)))
        // (*) If we get "a_sn" LESS than the expected value, this  is  NOT an
        //     error at all -- it is completely normal when multiple feeds are
        //     used. Nothing to do in this case;
        // (*) If we are in the Init Mode, DO NOT ALLOW the artificially-created
        //     gap to be filled by a "Put" with (a_sn == m_xsn); only   "Close-
        //     InitMode" (presumably invoked upon receiving full SnapShots) can
        //     do that. So reject this Item; this will NOT result in any data
        //     loss, because the slot @ "m_xsn" was created artificially, below
        //     the 1st real SeqNum, so that Item would eventually be  discarded
        //     anyway:
        return false;

      // At this point, "m_xsn" is always >= 0 (and >= 1 unless it's a very
      // rare case in the Init Mode):
      assert((m_initMode && m_xsn == 0)     || m_xsn > 0);

      // And the invariant regarding "a_sn" and "m_xsn": At this point, a_sn >=
      // m_xsn,  and the equality is only possible if we are NOT in the Init
      // Mode:
      assert((!m_initMode && m_xsn == a_sn) || m_xsn < a_sn);

      //---------------------------------------------------------------------//
      // In most cases, "a_sn" we got is the expected one:                   //
      //---------------------------------------------------------------------//
      if (utxx::likely(a_sn == m_xsn))
      {
        assert(!m_initMode);

        // No matter whether the Buffer is empty or not, the Item with an  exp-
        // ected SeqNum is placed at beginning of the curr window; for an empty
        // Buffer, it would be m_data[0].m_item.
        // NB:
        // (*) For the sake of uniformity, the next expected SeqNum ("m_xsn") is
        //     incremented NOT now (when the curr expected Item was emplaced),
        //     but when an Item is processed (below); "m_xsn" may be incremented
        //     more than once if there are buffered Items.
        // (*) "m_bi" and "m_ei" are not affected by Item emplacement IFF it was
        //     at the Expected SeqNum:
        //
        SafeEmplace(a_emplacer, a_sn, a_ts_recv, a_ts_handl, m_bi);

        // Now process the new Item (and subsequent contiguous ones):
        // When we added a new Item at the beginning of the window (at "m_bi"),
        // a contiguous chunk of non-0 SeqNums could have been formed, so  we
        // need to process all of them. While doing so, "m_xsn" (which is the
        // next SeqNum we will be expecting) is incremented each time:
        //
        assert(m_xsn > 0);   // Because it's same as "a_sn", so can "Process":
        SafeProcessContiguous();

        // All done:
        return true;
      }

      //---------------------------------------------------------------------//
      // Otherwise: Not the expected SeqNum:                                 //
      //---------------------------------------------------------------------//
      // It can only be that the received SeqNum is larger than the expected
      // one, eg we experienced a gap in "SeqNums":
      int    gap = int(a_sn - m_xsn);
      assert(gap >= 1);

      // Index at which the incoming Item will be placed (if otherwise OK):
      int pos = m_bi + gap;
      assert(m_bi < pos);

      // It may either fall within an existing window, or beyond it; check if
      // it can be emplaced at all; BUT "m_maxGap" is ineffective in the Init
      // Mode:
      //---------------------------------------------------------------------//
      // No valid "pos" to emplace the Item? Do "Recover" and try again:     //
      //---------------------------------------------------------------------//
      // XXX: In the InitMode, we allow for an unlimited artificial gap -- but
      // what happens if a real gap is encountered then???
      //
      if (utxx::unlikely((!m_initMode && gap > m_maxGap) || pos >= m_capacity))
      {
        // NB: In particular, in this case, "a_sn" cannot be a repeated SeqNum
        // -- simply because it did not fit in the current Buffer (so its pre-
        // vious incarnation could not be there either):
        //
        if (utxx::likely(Recover(pos)))
        {
          // The Buffer, empty or otherwise, is now available from the begin-
          // ning:
          assert(m_bi == 0);

          // AND TRY AGAIN RECURSIVELY!
          // XXX: How to control recursive depth here? Is infinite recursion
          // possible?
          // Also note that now a_xsn=-1: We do NOT modify XSN anymore:
          //
          return Put(a_emplacer, a_sn, a_ts_recv, a_ts_handl, -1);
        }
        else
        {
          // Could not find the space for the new Item -- probably the gap was
          // too large. Drop it with an error msg:
          LOG_ERROR(2,
            "SeqNumBuffer::Put: No room for ArgSN={}: XSN={}, Gap={}",
            a_sn, m_xsn, gap)
          return false;
        }
        // This point is NOT REACHABLE:
        __builtin_unreachable();
      }

      //---------------------------------------------------------------------//
      // Not the expected SeqNum, but Window is OK (empty or otherwise):     //
      //---------------------------------------------------------------------//
      if (pos < m_ei)
      {
        //-------------------------------------------------------------------//
        // Emplacement "pos" is within an already-existing window:           //
        //-------------------------------------------------------------------//
        // It will be placed @ pos = m_bi + gap, but only if there is no previ-
        // ous incarnation of "a_sn" there:
        //
        assert(pos < m_capacity);

        if (m_data[pos].m_sn == -1)
          SafeEmplace(a_emplacer, a_sn, a_ts_recv, a_ts_handl, pos);
      }
      else
      {
        //-------------------------------------------------------------------//
        // Create a new window (if Buffer is empty) or extend existing one:  //
        //-------------------------------------------------------------------//
        assert(0 <= m_bi && m_bi <= m_ei && m_ei <= pos && pos < m_capacity);
        // But gap == pos - m_bi > 0  strictly; ie gap >= 1
        // In particular,   m_bi == m_ei is possible if the curr window (and
        // Buffer as a whole) are empty

        // We will emplace the new Item @ "pos", so "m_ei" will be moved bey-
        // ond that. NB: all cells involved must be empty:
        DEBUG_ONLY(
        for (int i = m_ei; i <= pos; ++i)
          assert(m_data[i].m_sn == -1);
        )
        // We need to move "m_ei" first, as a pre-condition of "SafeEmplace":
        m_ei = pos + 1;

        // So the window size is indeed >= 2:
        // size = m_ei - m_bi = (pos + 1) - m_bi = (pos - m_bi) + 1 = gap + 1
        //     >= 2:
        assert(m_ei - m_bi >= 2);

        // XXX: Because the window is extended, the destination slot MUST NOT
        // be marked with a non-0 SeqNum ("SafeEmplace" will check that),  so
        // there is no previous incarnation of "a_sn" in this case:
        SafeEmplace(a_emplacer, a_sn, a_ts_recv, a_ts_handl, pos);
      }

      // But cannot do any processing yet -- the window begins with an empty
      // slot:
      assert(m_data[m_bi].m_sn == -1);

      // Still, "Put" had succeded:
      return true;
    }

    //=======================================================================//
    // "Recover":                                                            //
    //=======================================================================//
    // This method is invoked when we encountered a too large gap (perhaps, so-
    // me SeqNum was lost forever -- XXX: is there any other scenario with non-
    // closing windows?)
    // Returns "true" iff the recovery has succeeded, and the Buffer is usable
    // again:
    //
    bool Recover(int a_pos)
    {
      assert(m_xsn >= 0);  // From the Caller condition

      //---------------------------------------------------------------------//
      // How many SeqNums will be declared lost?                             //
      //---------------------------------------------------------------------//
      // First of all, all Items which are still missing at the beginning of
      // the window, will be declared permanently lost. There would be at least
      // 1 such Item, otherwise we would not get into Recover at all:
      int lost_count = 0;
      for (; m_bi < m_ei && m_data[m_bi].m_sn == -1; ++m_bi, ++lost_count);

      if (utxx::unlikely(lost_count == 0))
      {
        // How could this be possible? If we had (m_bi < m_ei) at least once
        // above, then, for that initial slot, SeqNum must have been -1,  so
        // "lost_count" would be incremented at least to 1. Thus, the Buffer
        // was actually 0, but we got into "Recover" because the gap was too
        // large. In this case,  there is nothing we can do --  the incoming
        // Item wil have to be dropped:
        assert(m_bi == m_ei && m_ei == 0);
        return false;
      }

      //---------------------------------------------------------------------//
      // Expell the "lost slots" from the window front:                      //
      //---------------------------------------------------------------------//
      // Otherwise: The following items will be declared "lost forever", so
      // the Buffer could be moved forward:
      SeqNum lost_from  = m_xsn;
      SeqNum lost_upto  = m_xsn + lost_count - 1;
      assert(lost_from <= lost_upto);

      LOG_WARN(2,
        "SeqNumBuffer::Recover: SeqNum(s)={}..{} are LOST FOREVER ({} items "
        "altogether): MaxGap={}, Pos={}, Capacity={}",
        lost_from, lost_upto, lost_count, m_maxGap, a_pos, m_capacity)

      // Move the expected SeqNum ("m_xsn") forward:
      m_xsn += lost_count;

      // Because we had lost_count >= 1, then m_xsn >= 1 as well (even if it
      // was 0 originally):
      assert(m_xsn > 0);

      // NB: It is possible (though unlikely) that the Buffer is now empty
      // (after "m_bi" was incremented).   No need to explicitly clear the
      // SeqNums here -- other way round, we got "lost_count" on the basis
      // that those cells were clear!
      if (utxx::unlikely(m_bi >= m_ei))
      {
        m_bi = m_ei = 0;
        return true;     // It's OK to use the Buffer again!
      }

      //---------------------------------------------------------------------//
      // Process the remaining Items (while contiguous):                     //
      //---------------------------------------------------------------------//
      // Otherwise, run the Processor on remaining Items -- this can increase
      // "m_xsn" further, and again, can make the Buffer empty:
      // NB: The window is inconsistent at the moment -- starting from a non-
      //     empty cell; this is what "Process" expects;
      assert(m_xsn > 0 && m_data[m_bi].m_sn == m_xsn);
      SafeProcessContiguous();

      //---------------------------------------------------------------------//
      // If the Buffer is still non-empty:                                   //
      //---------------------------------------------------------------------//
      // XXX: Move the remainig window to the beginning of the Buffer (it was
      // definitely NOT at the beginning at this moment, since some positions
      // have been freed up in front side). This move may be VERY EXPENSIVE:
      //
      if (m_ei != 0)
      {
        // There is still a valid remaining Window in the Buffer, that's why
        // "Process" has stopped:
        assert(0 < m_bi && m_bi + 2 <= m_ei && m_ei <= m_capacity      &&
              m_data[m_bi].m_sn == -1       && m_data[m_ei-1].m_sn != -1);

        // The length to be moved (remember that "m_ei" is BEYOND the Window);
        // because a Window consists of at least 1 empty Item (in [m_bi]) and
        // 1 non-empty Item (in [m_ei-1]), its length is at least 2:
        int    wlen = m_ei - m_bi;
        assert(wlen >= 2);

        // Now do the actual move: m_bi --> 0:
        for (int i = 0; i < wlen; ++i)
        {
          // NB: "src" is not "const" because it will be cleared:
          BuffEntry& src = m_data[m_bi + i];
          BuffEntry& dst = m_data[i];

          // If all logic is correct, "dst" must be clear at the moment:
          assert(dst.m_sn == -1);

          if (src.m_sn == -1)
            // Nothing to move (can happen for some intermediate empty slots);
            // in particular, this happens for i==0:
            continue;

          // Otherwise: Do move (copy, and clear the "src"):
          dst = src;
          src.m_sn = -1;

          // [i] (previously [m_bi+i]) must now have the SeqNum = m_xsn+i >= 1:
          assert(dst.m_sn == m_xsn + i);
        }

        // Move "m_bi" and "m_ei" to the beginning:
        m_bi = 0;
        m_ei = wlen;
      }
      return true;
    }
  };
} // End namespace MAQUETTE
