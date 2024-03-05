// vim:ts=2:et
//===========================================================================//
//                        "Basis/MiscUtils/FastPImpl.hpp":                   //
//                              FastPImpl Pattern                            //
//===========================================================================//
#pragma  once
#include <type_traits>
#include <memory>

namespace MAQUETTE
{
  //=========================================================================//
  // "FastPImpl" Class:                                                      //
  //=========================================================================//
  // Provides a way to use PImpl pattern without heap memory allocation.
  // Usage:
  // 1. Provide forward declaration for class/struct YourT.
  // 2. Declare member FastPImpl<YourT, S, A> as member of your class with
  //    arbitrary S, A.
  // 3. Build.
  // 4. Get hints about S, A params from compile time error.
  // 5. Change S, A.
  //
  // Note: for debug purposes you can change == to >= in check() method
  //
  template <typename T, std::size_t ImplSize, std::size_t ImplAlign>
  class FastPImpl
  {
  private:
    template <std::size_t v>
    struct Value
    {
      static constexpr std::size_t value = v;
    };

    template <std::size_t RealSize, std::size_t RealAlign>
    void check()
    {
      static_assert(Value<ImplSize>::value == Value<RealSize>::value,
                    "size and sizeof(T) mismatch");
      static_assert(Value<ImplAlign>::value == Value<RealAlign>::value,
                    "alignment and alignof(T) mismatch");
    }

  public:
    T* Ptr() { return reinterpret_cast<T*>(&m_data); }

    template <typename... Args>
    explicit FastPImpl(Args&&... args)
    {
      new (Ptr()) T(std::forward<Args>(args)...);
    }

    ~FastPImpl()
    {
      check<sizeof(T), alignof(T)>();
      Ptr()->~T();
    }

    FastPImpl& operator=(FastPImpl&& rhs)
    {
      *Ptr() = std::move(rhs);
      return *this;
    }

    T* operator->() noexcept { return Ptr(); }
    T* operator->() const noexcept { return Ptr(); }
    T& operator*() noexcept { return *Ptr(); }
    T& operator*() const noexcept { return *Ptr(); }

  private:
    std::aligned_storage_t<ImplSize, ImplAlign> m_data;
  };
} // End namespace MAQUETTE
