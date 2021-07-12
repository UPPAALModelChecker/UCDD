// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// Provides a lazy filter wrapper around a sequence using a specified predicate.
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 2019, Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////
//
// Author: Nikolaj Jensen Ulrik <nulrik16@student.aau.dk>

#ifndef FILTER_SEQUENCE_H
#define FILTER_SEQUENCE_H

#include "base/meta.hpp"
#include <iterator>
#include <type_traits>

/**
 * Wrapper type for filtering elements of a container using a predicate on the
 * container elements. Interfaces with standard containers.
 * Sample usage:
 *
 * auto even_ints = FilterSequence{vector<int>{1, 2, 3, 4, 5}, [](int i){return i % 2 == 0;}};
 * for (auto& val : even_ints) { cout << val << endl; }
 *
 * which prints 2 and 4.
 *
 * Defaults to using the const_iterator of the container. Non-const iterator is
 * not tested.
 *
 * TContainer has to be a standard-compatible container, i.e. it has to have
 * begin(), end(). If TIterator is not custom specified, it also needs a
 * const_iterator with standard functionality (++, *, ==). Predicate has to be a
 * callable from TElement to bool-compatible.
 */
template <typename TContainer, typename Predicate,
          typename TElement = meta::element_type<TContainer>,
          typename TIterator = typename TContainer::const_iterator>
class sequence_filter
{
public:
    static_assert(std::is_invocable_r<bool, Predicate, TElement>::value);
    static_assert(meta::is_container_v<TContainer>, "must be a container");

    sequence_filter(const TContainer& container, Predicate&& predicate):
        container(container), predicate(predicate)
    {}

    struct FilterIterator
        : std::iterator<typename TIterator::iterator_category, TElement,
                        typename TIterator::difference_type, typename TIterator::pointer,
                        typename TIterator::reference>
    {
        FilterIterator(TIterator iter, const sequence_filter<TContainer, Predicate>& parent):
            iter(iter), parent(parent)
        {}
        TIterator iter;
        sequence_filter<TContainer, Predicate> parent;
        FilterIterator& operator++()
        {
            _skip_to_next();
            return *this;
        }
        FilterIterator operator++(int) const
        {
            auto newIter = FilterIterator(this->iter, this->parent);
            ++newIter;
            return newIter;
        }
        TElement operator*() const { return *iter; }
        bool operator==(const FilterIterator& o) { return o.iter == iter; }
        bool operator!=(const FilterIterator& o) { return !(*this == o); }

    private:
        void _skip_to_next()
        {
            do {
                iter++;
            } while (iter != parent.end().iter && !(parent.predicate(*iter)));
        }
    };

    FilterIterator begin() const
    {
        /* Need to spool to first satisfying element, since operator* would
         * otherwise always give that element (and it should be const) */
        auto iter = FilterIterator(std::begin(container), *this);
        if (iter != end() && !predicate(*iter)) {
            ++iter;
        }
        return iter;
    }
    FilterIterator end() const { return FilterIterator(std::end(container), *this); }

private:
    const TContainer& container;
    Predicate predicate;
};

template <typename TContainer, typename Predicate>
sequence_filter(const TContainer, Predicate&&)
    -> sequence_filter<typename std::remove_reference<TContainer>::type, Predicate,
                       typename meta::element_type<TContainer>,
                       typename TContainer::const_iterator>;

#endif
