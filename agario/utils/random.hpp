#include <random>
#include <type_traits>

#include "agario/core/num_wrapper.hpp"

template<class T>
using uniform_distribution =
  typename std::conditional<
    // if the type T is a floating point derivative
    // then use the uniform_real_distribution
    std::is_floating_point<typename T::value_type>::value,
    std::uniform_real_distribution<typename T::value_type>,
    // otherwise, if T is an int derivative
    // use the uniform_int_distribution
    typename std::conditional<
      std::is_integral<typename T::value_type>::value,
      std::uniform_int_distribution<typename T::value_type>,
      void
    >::type
  >::type;
