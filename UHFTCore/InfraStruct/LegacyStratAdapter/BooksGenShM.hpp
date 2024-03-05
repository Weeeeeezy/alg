// vim:ts=2:et:sw=2
//===========================================================================//
//                            "BooksGenShM.hpp":                             //
//    Shared Memory-Allocated Books Collection (eg Trade, Order Books etc)   //
//===========================================================================//
#ifndef MAQUETTE_STRATEGYADAPTOR_BOOKSGENSHM_HPP
#define MAQUETTE_STRATEGYADAPTOR_BOOKSGENSHM_HPP

#include "Common/DateTime.h"
#include "Common/Threading.h"
#include "Connectors/ClientInfo.h"
#include "StrategyAdaptor/EventTypes.hpp"
#include "StrategyAdaptor/CircBuffSimple.hpp"
#include <utxx/compiler_hints.hpp>
#include <vector>
#include <string>
#include <utility>
#include <atomic>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <cassert>
#include <Infrastructure/Logger.h>
#include <functional>
#include <utility>

namespace MAQUETTE
{
  //=========================================================================//
  // Naming Conventions:                                                     //
  //=========================================================================//
  //-------------------------------------------------------------------------//
  // "GetBooksName":                                                         //
  //-------------------------------------------------------------------------//
  // Each "BooksGenShM" object is a collection of Books. Each Book is implemen-
  // ted as a "CircBuffSimple" and placed in a certain sub-segment in ShM. The
  // name of that sub-segment is "BooksName.SecID",  where  "BooksName" is de-
  // termined by the type of the contained Books,  ie  the template param "E":
  // Implementation of this function is by template specialisations:
  //
  template<typename E>
  char const* GetBooksName();

  //-------------------------------------------------------------------------//
  // "MkBookName":                                                           //
  //-------------------------------------------------------------------------//
  // Name of an individual Book: "BooksName.SecID" (not ".Symbol"!)
  //
  template<typename E>
  inline std::string MkBookName(Events::SecID sec_id)
    { return GetBooksName<E>() + std::string(".") + std::to_string(sec_id); }

  //=========================================================================//
  // "BooksGenShM" Class:                                                    //
  //=========================================================================//
  // NB: This class is designed for Shared Memory allocations,  and provides a
  // MsgQueue (which is itself Shared Memory-based) for sending Events between
  // processes;
  // "E" is the Book Entry Type:
  //
  template<typename E>
  class BooksGenShM
  {
  private:
    //=======================================================================//
    // Internal Types and Flds:                                              //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "Book":                                                               //
    //-----------------------------------------------------------------------//
    // (*) The Book may be subscribed for by multiple Clients. Consequently, it
    //     must contain a list of ptrs to MsgQs used to notify those Clients on
    //     Book updates. The MsgQs themselves are Shared Memory-based and noti-
    //     onally belong to the Clients.
    // (*) NB: it is not possible to use just 1 MsgQ on which multiple  Clients
    //     would wait, because there is an M<->M relationship between Clients &
    //     Books. So in that case,  each Client would have  to wait on multiple
    //     MsgQs -- and MsqQ "poll" is not easy to do.
    // (*) Furthermore, when a Client has received an update notification, it
    //     must be able to have unrestricted read-only access to the Order Book
    //     - shared with other Clients AND with the Connector which updates the
    //     Book. Thus, any kind of locking is unacceptable here.
    // (*) Instead, we must use a LOCK-FREE data structure with atomic update
    //     which simply advances the back of the Entries array, but does  NOT
    //     invalidate the views which the Clients have already received. So we
    //     cannot use the <vector> and such; use "CircBuffSimple" instead.
    // (*) Only the arrays of Entries for each Book are stored in Shared Memory
    //     (because Handles to Entries can be sent to other processes); and Msg
    //     Qs used for inter-process communications.  All other data structures
    //     are allocated in the normal address space of the calling process:
    //
    class Book
    {
    private:
      friend class BooksGenShM;

      //---------------------------------------------------------------------//
      // Internal Flds and Methods:                                          //
      //---------------------------------------------------------------------//
      // NB: "m_clients" is a vector pf pairs (ClientInfo*, ReqID).
      // The 2nd pair component is only needed for "FindSubscrByReqID":
      //
      std::string                                                 m_bookName;
      std::shared_ptr<BIPC::fixed_managed_shared_memory>          m_segment;
      std::vector<std::pair<CliInfo const*, OrderID>>             m_clients;
      Events::SymKey                                              m_symbol;

      // NB: The CircBuff is allocated entirely  in ShMem (not only the Entries
      // array, but admin data as well):
      CircBuffSimple<E>*                                          m_buff;

      // Should this Book persist even if it has no subscriptions?
      bool                                                        m_persistent;

      // The Default Ctor, Copy Ctor and Const Assignment are deleted, but
      // there are Move alternatives for them:
      Book()                         = delete;
      Book(Book const&)              = delete;
      Book& operator=(Book const&)   = delete;

    public:
      //---------------------------------------------------------------------//
      // Ctors:                                                              //
      //---------------------------------------------------------------------//
      //---------------------------------------------------------------------//
      // (1) Non-Default Ctor:                                               //
      //---------------------------------------------------------------------//
      Book
      (
        Events::SecID                                             sec_id,
        Events::SymKey const&                                     symbol,
        bool                                                      force_new,
        int                                                       capacity,
        std::shared_ptr<BIPC::fixed_managed_shared_memory> const& segment,
        bool                                                      persistent
      )
      : m_bookName  (MkBookName<E>(sec_id)), // NB: Construct a qualified name
        m_segment   (segment),
        m_clients   (),
        m_symbol    (symbol),
        m_buff      (CircBuffSimple<E>::CreateInShM
                    (capacity, segment.get(), m_bookName.c_str(), force_new)),
        m_persistent(persistent)
      {}

      //---------------------------------------------------------------------//
      // (2) Move Ctor:                                                      //
      //---------------------------------------------------------------------//
      Book(Book&&   right)
      : m_bookName  (std::move(right.m_bookName)),
        m_segment   (std::move(right.m_segment)),
        m_clients   (std::move(right.m_clients)),
        m_symbol    (std::move(right.m_symbol)),
        m_buff      (right.m_buff),
        m_persistent(right.m_persistent)
      {
        // The CircBuff ptr is copied into the LHS and removed from the RHS:
        right.m_buff       = NULL;
        right.m_persistent = false;
      }

      //---------------------------------------------------------------------//
      // Dtor:                                                               //
      //---------------------------------------------------------------------//
      // NB: Any remaining Client Subscriptions will be automatically removed,
      // ClientInfo Dtors run; if any corresp MsgQ End-Point becomes  unrefer-
      // enced, it will be de-allocated (but the MsgQ itself persists in ShMem
      // as it belongs to the Client):
      ~Book()
      {
        // Need to run the Dtor explicitly on the CircBuff, and then remove it
        // from the ShMem segment (the callee checks for NULL ptr):
        CircBuffSimple<E>::DestroyInShM(m_buff, m_segment.get());
        m_buff = NULL;
      }

      //---------------------------------------------------------------------//
      // "Clear":                                                            //
      //---------------------------------------------------------------------//
      // Data in the CircBuff is cleared, but all other dat astructures incl
      // the current subscriptions, are preserved, so the Book remains valid
      // but empty (and can be filled again).
      // XXX: USE WITH CARE -- this will invalidate all  previously-released
      // ptrs to Book Entries:
      //
      void Clear()
      {
        if (utxx::likely(m_buff != NULL))
          m_buff->Clear();
      }
    };
    // End of the "Book" class

    //=======================================================================//
    // Books Collection: map {SecID => Book}:                                //
    //=======================================================================//
    std::map<Events::SecID, Book>                      m_Books;
    int                                                m_capacity; // For Books
    std::shared_ptr<BIPC::fixed_managed_shared_memory> m_segment;
    mutable Arbalete::Mutex                            m_mutex;    // See "Add"
    bool                                               m_forceNew;
    bool                                               m_debug;

    // Iterator types short-cuts:
    typedef
      typename std::map<Events::SecID, Book>::iterator
      BooksIter;

    typedef
      typename std::map<Events::SecID, Book>::const_iterator
      ConstBooksIter;

    typedef
      typename std::vector<std::pair<CliInfo const*, OrderID>>
               ::iterator
      ClientsIter;

    typedef
      typename std::vector<std::pair<CliInfo const*, OrderID>>
               ::const_iterator
      ConstClientsIter;


    //-----------------------------------------------------------------------//
    // Default and Copy Ctor, Assignemnt and Equality are Hidden / Deleted:  //
    //-----------------------------------------------------------------------//
    BooksGenShM()                     = delete;
    BooksGenShM(BooksGenShM const&)   = delete;

    BooksGenShM& operator= (BooksGenShM const&);
    bool         operator==(BooksGenShM const&) const;
    bool         operator!=(BooksGenShM const&) const;

  public:
    //=======================================================================//
    // External API:                                                         //
    //=======================================================================//
    //=======================================================================//
    // Non-Default Ctor, Dtor:                                               //
    //=======================================================================//
    BooksGenShM
    (
      int                                                       capacity,
      std::shared_ptr<BIPC::fixed_managed_shared_memory> const& segment,
      bool                                                      force_new,
      bool                                                      debug
    )
    : m_Books    (),
      m_capacity (capacity),
      m_segment  (segment),
      m_forceNew (force_new),
      m_debug    (debug)
    {
      if (capacity <= 0 || segment.get() == NULL)
        throw std::invalid_argument
              ("BooksGenShM::Ctor: "+ std::string(GetBooksName<E>()) +
               ": Invalid Capacity and/or Shm Segment");
    }

    // Dtor: Nothing specific -- all components are destroyed automatically:
    ~BooksGenShM() {}

    // "size" is that of the contained map:
    size_t size() const  { return m_Books.size(); }

    //=======================================================================//
    // Subscriptions Management:                                             //
    //=======================================================================//
    //=======================================================================//
    // "CreateNewBook":                                                      //
    //=======================================================================//
    BooksIter CreateNewBook
    (
      Events::SecID         sec_id,
      Events::SymKey const& symbol,
      bool                  persistent
    )
    {
      // Install an empty book with the Capacity and Segment as specified by
      // this object:
      std::pair<BooksIter, bool> ins =
        m_Books.insert
        (std::make_pair
          (sec_id,
           Book(sec_id, symbol, m_forceNew, m_capacity, m_segment, persistent)
        ));
      assert(ins.second);

      if (m_debug)
        SLOG(INFO) << "BooksGenShM::CreateNewBook: "     << GetBooksName<E>()
                  << ": Installed new SecID=" << sec_id << ", Symbol="
                  << Events::ToCStr(symbol)   << std::endl;
      return ins.first;
    }

    //=======================================================================//
    // "Subscribe":                                                          //
    //=======================================================================//
    // Attach a new Client MsgQ (identified by a system-wide name) to a new or
    // existing Book.  If the Client is already subscribed,  this  method is a
    // no-op; no exceptions are propagated.
    // Returns a flag indicating whether a new Book (for the given "sec_id")
    // has been created:
    //
    bool Subscribe
    (
      CliInfo        const& info,
      Events::SecID         sec_id,
      Events::SymKey const& symbol,
      Events::OrderID       req_id       // Eg for FIX
    )
    {
      bool book_created = false;
      Arbalete::LockGuard lock(m_mutex);

      //---------------------------------------------------------------------//
      // Find the book to determine if it already exists:                    //
      //---------------------------------------------------------------------//
      BooksIter it   = m_Books.find(sec_id);

      if (it == m_Books.end())
      {
        // If the Book is created on first Subscribe, it is NOT deemed to be
        // persistent:
        it           = CreateNewBook(sec_id, symbol, false);
        book_created = true;
      }

      // The Book to be processed (old or new):
      assert(it->first == sec_id);
      Book& book = it->second;

      //---------------------------------------------------------------------//
      // Is THIS Client already subscribed?                                  //
      //---------------------------------------------------------------------//
      ConstClientsIter cit = find_if
      (
        book.m_clients.cbegin(), book.m_clients.cend(),
        [&info](std::pair<CliInfo const*, OrderID> const& curr)->bool
        {
          assert (curr.first != NULL);
          return (*curr.first == info);
        }
      );

      if (cit != book.m_clients.cend())
        // It is already there, it should have completely same CliInfo --
        // but we do not check that, simply do nothing:
        //
        SLOG(INFO) << "BooksGenShM::Subscribe: " << GetBooksName<E>()
                  << ": Symbol="  << Events::ToCStr(symbol) << ", Client="
                  << info.Name()  << "(queue="   << info.RespQName()
                  << ") is already subscribed"   << std::endl;
      else
      {
        // OTHERWISE: Install the Info to mark it as "subscribed":
        book.m_clients.push_back(std::make_pair(&info, req_id));

        SLOG(INFO) << "BooksGenShM::Subscribe: "  << GetBooksName<E>()
                  << ": Symbol=" << Events::ToCStr(symbol)
                  << ": Added subscription for "
                  << info.Name() << "(queue="    << info.RespQName()
                  << ", ref="    << info.Ref()   << ')' << std::endl;
      }

      //---------------------------------------------------------------------//
      // Send the Initial Data to the Client:                                //
      //---------------------------------------------------------------------//
      // Upon successful subscription (or even if the Client is already subscr-
      // ibed), IMMEDIATELY SEND the latest data item to the Client (if such a
      // data item exists):
      if (!book.m_buff->IsEmpty())
        try
        {
          E const* curr = book.m_buff->GetCurrPtr();
          assert(curr  != NULL);
          info.SendBookPtrToClient<E>(curr);
        }
        catch (std::exception const& e)
        {
          // Any communication errors would be logged already. If cannot com-
          // municate with the Client, Unsubscribe it -- but without locking,
          // or a deadlock would occur:
          UnsubscribeAll(info, NULL, false);

          SLOG(ERROR) << "BooksGenShM::Subscribe: " << GetBooksName<E>()
                      << ": Symbol="                << Events::ToCStr(symbol)
                      << ": Dropped all subscriptions for "     << info.Name()
                      << ": Communication Error: "  << e.what() << std::endl;
          // Propagate the exception to the Caller:
          throw;
        }
      else
        SLOG(WARNING) << "BooksGenShM::Subscribe: " << GetBooksName<E>()
                      << ": Cannot send the initial data to " << info.Name()
                      << ": No data available..."   << std::endl;
      return book_created;
    }

    //=======================================================================//
    // "Unsubscribe":                                                        //
    //=======================================================================//
    bool Unsubscribe
    (
      CliInfo const&        client,
      Events::SecID         sec_id,
      Events::SymKey const& symbol,
      bool                  with_lock = true
    )
    {
      // Is the whole Book for this "sec_id" removed as a result?
      bool book_removed = false;

      Arbalete::UniqLock lock(m_mutex, std::defer_lock);
      if (with_lock)
        lock.lock();

      // Get the Book by the SecID:
      BooksIter bit = m_Books.find(sec_id);

      if (bit == m_Books.end())
        // This SecID is not subscribed for, nothing to do:
        return book_removed;

      // Otherwise: Is this Client subscribed?
      assert(bit->first == sec_id);
      Book& book = bit->second;

      ClientsIter clit = find_if
      (
        book.m_clients.begin(), book.m_clients.end(),
        [&client](std::pair<CliInfo const*, Events::OrderID> const& curr)
        {
          assert (curr.first != NULL);
          // XXX: FIXME: Better compare hash codes?
          return (*curr.first == client);
        }
      );

      if (clit != book.m_clients.end())
      {
        // Yes, the Client is subscribed; remove it (the MsgQ end-point will
        // be managed automatically via its "shared_ptr"):
        book.m_clients.erase(clit);

        if (m_debug)
          SLOG(INFO) << "BooksGenShM::Unsubscribe: " << GetBooksName<E>()
                    << ": Symbol=" << Events::ToCStr(symbol)
                    << ", Client=" << client.Name()  << ": Unsubscribed"
                    << std::endl;

        // Is the whole Book to be removed (NOT if it is persistent):
        if (book.m_clients.empty() && !book.m_persistent)
        {
          // Remove the Book from the map:
          m_Books.erase(bit);

          if (m_debug)
            SLOG(INFO) << "BooksGenShM::Unsubscribe: "   << GetBooksName<E>()
                      << ": Symbol=" << Events::ToCStr(symbol) << " REMOVED"
                      << std::endl;
          book_removed = true;
        }
      }
      // Otherwise, the Client is not subscribed for this "sec_id" -- nothing
      // to remove. No msgs here to avoid excessive output when called from
      // "UnsubscribeAll"...

      // Return the flag indicating whether the Book has been removed:
      return book_removed;
    }

    //=======================================================================//
    // "UnsubscribeAll":                                                     //
    //=======================================================================//
    // XXX: The outer loop is performed without any locking in place -- is it
    // guaranteed to be OK?   Probably yes -- it is read-only and so does not
    // affect "Add"s, and subscription management itself is always done by  1
    // thread only.
    // If the last arg is non-NULL, it returns a list of SecIDs   whose Books
    // have actually been removed -- these Symbols may require unsubscription
    // at the Network level (eg in FIX):
    //
    void UnsubscribeAll
    (
      CliInfo const&              client,
      std::vector<Events::SecID>* books_removed = NULL,
      bool                        with_lock     = true
    )
    {
      // Iterate over all Books, remove subscriptions for this "q_name":
      //
      for (BooksIter bit = m_Books.begin(); bit != m_Books.end(); )
      {
        // Try to remove all subscriptions from the curr Book for that
        // "client". This MAY result in the Book itself being removed,
        // and that would invalidate "bit", so advance it first:
        //
        Events::SecID sec_id = bit->first;
        Book const&   book   = bit->second;
        bit++;

        // XXX: "book.m_symbol" is used for logging only:
        bool book_removed  =
          Unsubscribe(client, sec_id, book.m_symbol, with_lock);

        if (books_removed != NULL && book_removed)
          books_removed->push_back(sec_id);
      }
    }

    //=======================================================================//
    // "ClearAllBooks":                                                      //
    //=======================================================================//
    // Clear data from all Books; however, all Subscriptions etc remain valid.
    // This invalidates all previously-released ptrs, so USE WITH CARE:
    //
    void ClearAllBooks()
    {
      for (typename std::map<Events::SecID, Book>::iterator it =
           m_Books.begin();  it != m_Books.end();  ++it)
        it->second.Clear();
    }

    //=======================================================================//
    // "Add":                                                                //
    //=======================================================================//
    // NB: It is assumed that there calls to "Add" and "GetLatest" (together)
    // are already serialised by the Caller, eg there is only 1 Caller thread
    // which does these calls, or they are performed  from a critical section
    // (ie, if multiple Channels are used, they are first synchronised and se-
    // rialiased by the Caller).
    // On the contrary, calls to "Subscribe" / "Unsubscribe" are very infrequ-
    // ent but asynchronous to "Add" / "GetLatest", as the former are done by
    // a Management Thread:
    //
    void Add
    (
      Events::SecID         sec_id,
      Events::SymKey const& symbol,
      E const&              entry,
      bool                  create_book = false
    )
    {
      // NB: The cost of creating this lock should be 0 as the Mutex is almost
      // never locked:
      Arbalete::LockGuard lock(m_mutex);

      // Get the Book by the SecID:
      BooksIter it = m_Books.find(sec_id);

      if (it == m_Books.end())
      {
        // This simply means that this "sec_id" is not subscribed -- this is
        // NOT an error condition. If the "create_book" flag is set,  create
        // a new Book without any subscriptions yet (and mark it as persist-
        // ent), otherwise just return:
        //
        if (create_book)
          it = CreateNewBook(sec_id, symbol, true);
        else
          return;
      }
      //---------------------------------------------------------------------//
      // If found or created: Install the "entry" in the "book":             //
      //---------------------------------------------------------------------//
      assert(it->first == sec_id);
      Book& book = it->second;

      assert(book.m_buff != NULL);
      (void) book.m_buff->Add(entry);

      //--------------------------------------------------------------------//
      // Send a notification Event to all MsgQs subscribed to this Book:    //
      //--------------------------------------------------------------------//
      // Ptr to the installed Entry -- we don't need type info on it as it will
      // now be converted to a handle, so use "void":
      //
      E const* curr = book.m_buff->GetCurrPtr();
      assert(curr != NULL);

      // Go through all subscribed Clients:
      for (int i = 0; i < int(book.m_clients.size()); )
      {
        auto info = book.m_clients[i].first;
        assert(info != NULL);
        try
        {
          info->template SendBookPtrToClient<E>(curr);
          ++i;  // If OK, go to the next Client
        }
        catch (...)
        {
          // All communication errors are already logged by the resp functions.
          // If cannot communicate with a Client, Unsubscribe it without lock-
          // ing, to prevent a dead-lock:
          // NB: DON'T use a ref to "m_q_name" -- book.m_clients[i] is removed!
          //
          UnsubscribeAll(*info, NULL, false);   // No "i" increment

          SLOG(WARNING) << "BooksGenShM::Add: " << GetBooksName<E>()
                        << ": Symbol="          << Events::ToCStr(symbol)
                        << ": Dropped all subscriptions for " << info->Name()
                        << ": Communication Error"            << std::endl;
        }
      }
    }

    //=======================================================================//
    // Accessors:                                                            //
    //=======================================================================//
    //-----------------------------------------------------------------------//
    // "GetLatest": Implemented on top of "GetCurrPtr":                      //
    //-----------------------------------------------------------------------//
    E const* GetLatest(Events::SecID sec_id) const
    {
      Arbalete::LockGuard lock(m_mutex);

      // Get the Book by the SecID:
      ConstBooksIter cit = m_Books.find(sec_id);

      if (cit == m_Books.cend())
        // Throw an exception, don't return "NULL":
        throw std::runtime_error("BooksGenShM::GetLatest: SecID=" +
                                 std::to_string(sec_id) +" not found");

      // Get the Raw Ptr direct from the CurcBuff of that Book:
      E const* curr = cit->second.m_buff->GetCurrPtr();
      assert(curr  != NULL);
      return curr;
    }

    //-----------------------------------------------------------------------//
    // "IsEmpty":                                                            //
    //-----------------------------------------------------------------------//
    bool IsEmpty() const
    {
      // Lock for extra safety against concurrent "Subscribe" & Co:
      Arbalete::LockGuard lock(m_mutex);
      return m_Books.empty();
    }

    //-----------------------------------------------------------------------//
    // "IsSubscribed":                                                       //
    //-----------------------------------------------------------------------//
    bool IsSubscribed(Events::SecID sec_id) const
    {
      // Lock again:
      Arbalete::LockGuard lock(m_mutex);
      return (m_Books.find(sec_id) != m_Books.cend());
    }

    //=======================================================================//
    // "FindSubscrByReqID":                                                  //
    //=======================================================================//
    std::pair<Events::SecID, CliInfo const*>
      FindSubscrByReqID(Events::OrderID req_id) const
    {
      // Traverse all Books (it's OK -- this call is very rare):
      for (ConstBooksIter cbit = m_Books.begin(); cbit != m_Books.end(); ++cbit)
      {
        Events::SecID sec_id = cbit->first;
        Book   const& book   = cbit->second;

        // Traverse all Subscriptions:
        for (ConstClientsIter ccit = book.m_clients.begin();
             ccit != book.m_clients.end();  ++ccit)
          if (ccit->second == req_id)
            return std::make_pair(sec_id, ccit->first);
      }
      // If we got here: Nothing:
      return std::pair<Events::SecID, CliInfo const*>(0, NULL);
    }
  };
}

#endif  // MAQUETTE_STRATEGYADAPTOR_BOOKSGENSHM_HPP
