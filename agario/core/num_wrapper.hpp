#pragma once

#include <limits>

/*
 * Adapted from
 * https://stackoverflow.com/questions/17793298/c-class-wrapper-around-fundamental-types
 */


template<class T, int type_distinguisher>
class numWrapper {
  T value;
public:
  typedef T value_type;

  numWrapper() : value() {}

  numWrapper(T v) : value(v) {}

  operator T() const { return value; }

  static numWrapper<T, type_distinguisher> max() {
    return numWrapper(std::numeric_limits<T>::max());
  }

  //modifiers
  numWrapper &operator=(T v) {
    value = v;
    return *this;
  }

  numWrapper &operator+=(T v) {
    value += v;
    return *this;
  }

  numWrapper &operator-=(T v) {
    value -= v;
    return *this;
  }

  numWrapper &operator*=(T v) {
    value *= v;
    return *this;
  }

  numWrapper &operator/=(T v) {
    value /= v;
    return *this;
  }

  numWrapper &operator%=(T v) {
    value %= v;
    return *this;
  }

  numWrapper &operator++() {
    ++value;
    return *this;
  }

  numWrapper &operator--() {
    --value;
    return *this;
  }

  numWrapper operator++(int) { return numWrapper(value++); }

  numWrapper operator--(int) { return numWrapper(value--); }

  numWrapper &operator&=(T v) {
    value &= v;
    return *this;
  }

  numWrapper &operator|=(T v) {
    value |= v;
    return *this;
  }

  numWrapper &operator^=(T v) {
    value ^= v;
    return *this;
  }

  numWrapper &operator<<=(T v) {
    value <<= v;
    return *this;
  }

  numWrapper &operator>>=(T v) {
    value >>= v;
    return *this;
  }

  //accessors
  numWrapper operator+() const { return numWrapper(+value); }

  numWrapper operator-() const { return numWrapper(-value); }

  numWrapper operator!() const { return numWrapper(!value); }

  numWrapper operator~() const { return numWrapper(~value); }

  //friends
  friend numWrapper operator+(numWrapper iw, numWrapper v) { return iw += v; }

  template<typename U>
  friend numWrapper operator+(numWrapper iw, U v) { return iw += v; }

  template<typename U>
  friend numWrapper operator+(U v, numWrapper iw) { return numWrapper(v) += iw; }

  friend numWrapper operator-(numWrapper iw, numWrapper v) { return iw -= v; }

  template<typename U>
  friend numWrapper operator-(numWrapper iw, U v) { return iw -= v; }

  template<typename U>
  friend numWrapper operator-(U v, numWrapper iw) { return numWrapper(v) -= iw; }

  friend numWrapper operator*(numWrapper iw, numWrapper v) { return iw *= v; }

  template<typename U>
  friend numWrapper operator*(numWrapper iw, U v) { return iw *= v; }

  template<typename U>
  friend numWrapper operator*(U v, numWrapper iw) { return numWrapper(v) *= iw; }

  friend numWrapper operator/(numWrapper iw, numWrapper v) { return iw /= v; }

  template<typename U>
  friend numWrapper operator/(numWrapper iw, U v) { return iw /= v; }

  template<typename U>
  friend numWrapper operator/(U v, numWrapper iw) { return numWrapper(v) /= iw; }

  // overload the other operators (%, &, |, ^, <<, >>) in the same way if needed
};