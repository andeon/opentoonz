#pragma once

#ifndef TCG_ITERATOR_OPS_H
#define TCG_ITERATOR_OPS_H

// tcg includes
#include "traits.h"
#include "ptr.h"

// STD includes
#include <iterator>

namespace tcg {

//****************************************************************************
//    Traits
//****************************************************************************

template <typename It>
struct iterator_traits : public std::iterator_traits<It> {
  typedef It inheritable_iterator_type;
};

template <typename T>
struct iterator_traits<T *> : public std::iterator_traits<T *> {
  typedef ptr<T> inheritable_iterator_type;
};

//***********************************************************************
//    Step Iterator class
//***********************************************************************

/*!
  The Step Iterator class is a simple random access iterator wrapper which
  moves by a fixed number of items.

\warning The size of the container referenced by the wrapped iterator should
  always be a multiple of the specified step.
*/

template <typename RanIt>
class step_iterator {
protected:
  RanIt m_it;
  typename std::iterator_traits<RanIt>::difference_type m_step;

public:
  typedef std::random_access_iterator_tag iterator_category;
  typedef typename std::iterator_traits<RanIt>::value_type value_type;
  typedef typename std::iterator_traits<RanIt>::difference_type difference_type;
  typedef typename std::iterator_traits<RanIt>::pointer pointer;
  typedef typename std::iterator_traits<RanIt>::reference reference;

  step_iterator() {}
  step_iterator(const RanIt &it, difference_type step)
      : m_it(it), m_step(step) {}

  step_iterator &operator++() {
    m_it += m_step;
    return *this;
  }

  step_iterator &operator--() {
    m_it -= m_step;
    return *this;
  }

  step_iterator operator++(int) {
    step_iterator it(*this);
    operator++();
    return it;
  }

  step_iterator operator--(int) {
    step_iterator it(*this);
    operator--();
    return it;
  }

  step_iterator &operator+=(difference_type val) {
    m_it += val * m_step;
    return *this;
  }

  step_iterator &operator-=(difference_type val) {
    m_it -= val * m_step;
    return *this;
  }

  difference_type operator-(const step_iterator &it) const {
    return (m_it - it.m_it) / m_step;
  }

  step_iterator operator+(difference_type val) const {
    step_iterator it(*this);
    it += val;
    return it;
  }

  step_iterator operator-(difference_type val) const {
    step_iterator it(*this);
    it -= val;
    return it;
  }

  reference operator*() const { return *m_it; }
  pointer operator->() const { return m_it.operator->(); }

  const RanIt &it() const { return m_it; }
  int step() const { return m_step; }

  bool operator==(const step_iterator &it) const { return m_it == it.m_it; }
  bool operator!=(const step_iterator &it) const { return !operator==(it); }

  bool operator<(const step_iterator &it) const { return m_it < it.m_it; }
  bool operator>(const step_iterator &it) const { return m_it > it.m_it; }
  bool operator<=(const step_iterator &it) const { return m_it <= it.m_it; }
  bool operator>=(const step_iterator &it) const { return m_it >= it.m_it; }
};

}  // namespace tcg

#endif  // TCG_ITERATOR_OPS_H