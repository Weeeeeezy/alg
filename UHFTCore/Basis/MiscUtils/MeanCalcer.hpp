// vim:ts=2:et
//===========================================================================//
//                    "Basis/MiscUtils/MeanCalcer.hpp":                      //
//===========================================================================//
#pragma  once
#include "Basis/MiscUtils/MeanCalcer.hpp"

namespace MAQUETTE
{
  template <class T, std::size_t Cap>
  class MeanCalcer: public SumCalcer<T, Cap>
  {
  public:
    using Base = SumCalcer<T, Cap>;

    MeanCalcer(std::size_t a_size) : Base(a_size)
    {}

    T PushBack(T a_item);

    T GetMean() const { return m_mean; }

  protected:
    T m_mean = NaN<T>;
  };

  template <class T, std::size_t Cap>
  inline T MeanCalcer<T, Cap>::PushBack(T a_item)
  {
    auto replaced = Base::PushBack(a_item);

    m_mean = Base::GetSum() / double(Base::Size());

    return replaced;
  }
}
// End namespace MAQUETTE
