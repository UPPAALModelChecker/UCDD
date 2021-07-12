// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: t; -*-
////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 2018 Aalborg University.
// All right reserved.
//
// Author: Marius Mikucionis marius@cs.aau.dk
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_RANGESET_H
#define INCLUDE_BASE_RANGESET_H

#include "utap/range.h"
#include "base/meta.hpp"

#include <algorithm>
#include <vector>
#include <cmath>

namespace base
{
    /**
     * Encodes sets as ranges (bitset implementation for sparse and compact sets).
     */
    template <typename T, typename Tag = void>
    class rangeset_t
    {
        // needed to implement as_tag(); allows for inspection of private
        // variables when both tag and type differ.
        template <typename U, typename K>
        friend class rangeset_t;

    public:
        using range_type = UTAP::range_t<T>;
        using store_type = std::vector<range_type>;

    private:
        static bool _lb_cmp(const range_type& a, const range_type& b)
        {
            return a.first() < b.first();
        }

        static bool _ub_cmp(const range_type& a, const range_type& b)
        {
            return a.last() < b.last();
        }

        static constexpr T prev_value(T value)
        {
            if constexpr (std::is_floating_point_v<T>) {
                return std::nexttoward(value, std::numeric_limits<T>::lowest());
            } else if constexpr (std::is_integral_v<T>) {
                return value - 1;
            } else {
                meta::TD<T> unsupported;  // not supported
            }
        }

        static constexpr T next_value(T value)
        {
            if constexpr (std::is_floating_point_v<T>) {
                return std::nexttoward(value, std::numeric_limits<T>::max());
            } else if constexpr (std::is_integral_v<T>) {
                return value + 1;
            } else {
                meta::TD<T> unsupported;  // not supported
            }
        }

        static bool _mergable(const range_type& a, const range_type& b)
        {
            return a.first() <= next_value(b.last()) && b.first() <= next_value(a.last());
        }
        /**
         * Merges elements in [s,e) into el, erasing [s,f) if erase is set
         * consecutive element of el.
         * @param el    the target
         * @param s     the first element to try to merge
         * @param e     the element after the last to merge
         * @param erase whether to erase merged elements or not
         * @return an iterator to the next element not merged in.
         */
        typename store_type::const_iterator merge(range_type& el,
                                                  typename store_type::const_iterator s,
                                                  typename store_type::const_iterator e, bool erase)
        {
            auto org = s;
            for (; s != e; ++s) {
                if (next_value(el.last()) >= s->first())
                    el |= (*s);
                else
                    break;
            }
            // remove elements covered
            if (erase)
                return _ranges.erase(org, s);
            else
                return s;
        }

        bool _dense() const
        {
            for (size_t i = 1; i < _ranges.size(); ++i) {
                if (_ranges[i - 1].empty())
                    return false;
                if (_ranges[i].empty())
                    return false;
                if (next_value(_ranges[i - 1].last()) == _ranges[i].first())
                    return false;
            }
            return true;
        }

        static store_type _intersect(const rangeset_t& a, const rangeset_t& b)
        {
            assert(a._dense());
            assert(b._dense());
            if (a.empty() || b.empty())
                return {};

            store_type intersected;
            auto lb_a = std::begin(a._ranges);
            auto lb_b = std::begin(b._ranges);
            while (true) {
                // progress lower bounds, find upper bound of first() and subtract one
                if (_lb_cmp(*lb_a, *lb_b)) {
                    lb_a = std::upper_bound(lb_a, std::end(a._ranges), *lb_b, _lb_cmp);
                    --lb_a;
                }

                if (_lb_cmp(*lb_b, *lb_a)) {
                    lb_b = std::upper_bound(lb_b, std::end(b._ranges), *lb_a, _lb_cmp);
                    --lb_b;
                }

                auto res = *lb_a & *lb_b;
                if (!res.empty())
                    intersected.emplace_back(res);

                // move the one with the lowest upper bound
                if (lb_a->last() < lb_b->last())
                    ++lb_a;
                else
                    ++lb_b;

                if (lb_a == std::end(a._ranges) || lb_b == std::end(b._ranges))
                    break;
            }
            return intersected;
        }

        rangeset_t(store_type&& ranges): _ranges(ranges){};

    public:
        rangeset_t() = default;

        rangeset_t(const T& e) { *this |= e; }

        rangeset_t(const T& first, const T& last) { *this |= range_type(first, last); }

        rangeset_t(const range_type& range) { *this |= range; }

        /** computes a (precise) union with an element */
        rangeset_t& operator|=(const T& e) { return *this |= range_type(e); }

        /** computes a (precise) union with a range */
        rangeset_t& operator|=(const range_type& range)
        {
            // find an insertion point:
            if (range.empty())
                return *this;
            auto it = std::upper_bound(std::begin(_ranges), std::end(_ranges), range, _lb_cmp);
            if (it != std::begin(_ranges))
                --it;
            if (_ranges.empty() || it->first() > next_value(range.last())) {
                // either empty list or new first element
                _ranges.emplace(it, range);
                return *this;
            }

            // we are either intersecting or touching this or the next element
            // otherwise we are in between this and next (or next is end)
            if (_mergable(*it, range))
                *it |= range;
            else {
                ++it;
                if (it == std::end(_ranges)) {
                    _ranges.emplace_back(range);
                    return *this;
                }

                if (_mergable(*it, range)) {
                    *it |= range;
                } else {
                    it = _ranges.insert(it, range);
                    return *this;
                }
            }
            // this only happens on inline merge
            // remove redundant elements.
            merge(*it, it + 1, std::end(_ranges), true);
            return *this;
        }

        /** computes a (precise) union with another rangeset
         *  this should take O(n). No more.
         */
        rangeset_t& operator|=(const rangeset_t& other)
        {
            assert(other._dense());
            assert(_dense());
            if (empty()) {
                _ranges = other._ranges;
                return *this;
            }
            if (other.empty())
                return *this;

            // this could be moved out like _intersect.
            auto lb_a = std::begin(_ranges);
            auto lb_b = std::begin(other._ranges);
            // this could be done inline, by use a new vector for now
            store_type merged;
            if (lb_a->first() < lb_b->first())
                merged.emplace_back(*lb_a);
            else
                merged.emplace_back(*lb_b);
            while (true) {
                bool changed;
                // this could be done quicker with some lb/ub magic
                do {
                    changed = false;
                    while (lb_a != std::end(_ranges) && _mergable(*lb_a, merged.back())) {
                        merged.back() |= *lb_a;
                        ++lb_a;
                        changed = true;
                    }
                    while (lb_b != std::end(other._ranges) && _mergable(*lb_b, merged.back())) {
                        merged.back() |= *lb_b;
                        ++lb_b;
                        changed = true;
                    }
                } while (changed);
                // nothing should be mergeable here, move one element
                // check trivial cases where we have reached end of one
                if (lb_a == std::end(_ranges)) {
                    merged.insert(merged.end(), lb_b, std::end(other._ranges));
                    break;
                }
                if (lb_b == std::end(other._ranges)) {
                    merged.insert(merged.end(), lb_a, std::end(_ranges));
                    break;
                }
                // otherwise, move over the lowest element
                if (lb_b->first() < lb_a->first()) {
                    merged.emplace_back(*lb_b);
                    ++lb_b;
                } else {
                    merged.emplace_back(*lb_a);
                    ++lb_a;
                }
            }
            _ranges = merged;
            return *this;
        }

        /** computes an intersection with an element */
        rangeset_t& operator&=(const T& e) { return *this &= range_type(e); }

        /** computes an intersection with a range */
        rangeset_t& operator&=(const range_type& range)
        {
            // this can be moved like _intersect
            assert(_dense());
            if (range.empty() || empty()) {
                clear();
                return *this;
            }

            // remove all above
            auto eit = std::upper_bound(std::begin(_ranges), std::end(_ranges), range, _ub_cmp);
            if (eit != std::end(_ranges) && (*eit && range))
                ++eit;
            _ranges.erase(eit, std::end(_ranges));

            // remove all below
            auto it = std::lower_bound(std::begin(_ranges), std::end(_ranges), range, _lb_cmp);
            if (it != std::begin(_ranges))
                --it;
            it = _ranges.erase(std::begin(_ranges), it);

            // intersect with first and last element if any (rest are in dense domain)
            if (_ranges.size() > 0) {
                _ranges.back() &= range;
                if (_ranges.back().empty())
                    _ranges.pop_back();
            }

            // check size again, could have removed something above
            if (_ranges.size() > 0) {
                _ranges.front() &= range;
                if (_ranges.front().empty())
                    _ranges.erase(_ranges.begin());
            }

            return *this;
        }

        /** computes an intersection with another rangeset */
        rangeset_t& operator&=(const rangeset_t& set)
        {
            _ranges = _intersect(this, set);
            return *this;
        }

        /** computes a (precise) union of a rangeset and an element */
        rangeset_t operator|(const T& e) const { return rangeset_t(*this) |= range_type(e); }

        /** computes a (precise) union of rangeset and a range */
        rangeset_t operator|(const range_type& range) const { return rangeset_t(*this) |= range; }

        /** computes a (precise) union of rangeset and a range */
        rangeset_t operator|(const rangeset_t& other) const { return rangeset_t(*this) |= other; }

        /** computes an intersection with an element */
        rangeset_t operator&(const T& e) const
        {
            if (*this && e)
                return rangeset_t(e, e);
            else
                return rangeset_t();
        }

        /** computes an intersection with a range */
        rangeset_t operator&(const range_type& range) const { return rangeset_t(*this) &= range; }

        /** computes an interesection with another rangeset */
        rangeset_t operator&(const rangeset_t& other) const
        {
            return rangeset_t(_intersect(*this, other));
        }

        /** checks if the rangeset contains an element */
        bool operator&&(const T& e) const { return *this && range_type(e); }

        /** checks if the rangeset intersects with the range */
        bool operator&&(const range_type& range) const
        {
            assert(_dense());
            auto it = std::upper_bound(std::begin(_ranges), std::end(_ranges), range, _lb_cmp);
            if (it != std::begin(_ranges))
                --it;
            // either it or next element intersects
            if (it != std::end(_ranges)) {
                if (*it && range)
                    return true;
                ++it;
                if (it != std::end(_ranges))
                    if (*it && range)
                        return true;
            }
            return false;
        }

        /** checks if the rangeset intersects with another rangeset */
        bool operator&&(const rangeset_t& set) const
        {
            assert(_dense());
            assert(set._dense());
            if (empty() || set.empty())
                return false;
            auto lb_a = std::begin(_ranges);
            auto lb_b = std::begin(set._ranges);
            while (true) {
                // progress lower bounds
                if (_lb_cmp(*lb_a, *lb_b)) {
                    lb_a = std::upper_bound(lb_a, std::end(_ranges), *lb_b, _lb_cmp);
                    --lb_a;
                }

                if (_lb_cmp(*lb_b, *lb_a)) {
                    lb_b = std::upper_bound(lb_b, std::end(set._ranges), *lb_a, _lb_cmp);
                    --lb_b;
                }

                if (*lb_a && *lb_b)
                    return true;

                // move the one with the lowest upper bound
                if (lb_a->last() < lb_b->last())
                    ++lb_a;
                else
                    ++lb_b;

                if (lb_a == std::end(_ranges) || lb_b == std::end(set._ranges))
                    return false;
            }
            return false;  // unreachable
        }

        /** checks if a union has any elements */
        bool operator||(const T& e) const { return true; }
        /** checks if a union has any elements */
        bool operator||(const range_type& range) const { return !range.empty() || !empty(); }

        /** checks if a union has any elements */
        bool operator||(const rangeset_t& other) const
        {
            assert(_dense());
            assert(other._dense());
            return !other.empty() || !empty();
        }

        /** subtracts a rangeset with complexity O(N+M) */
        rangeset_t& operator-=(const rangeset_t& other)
        {
            // TODO, probably better to make a fresh vector to build result into
            // and swap at the end - to avoid constant copying.
            auto i = std::begin(_ranges);
            auto oi = std::begin(other._ranges);
            while (i != std::end(_ranges) && oi != std::end(other._ranges)) {
                // skip our ranges until one may overlap:
                // our:   skip]   stop] ...
                // their:      [x [x [x
                i = std::lower_bound(i, std::end(_ranges), *oi);  // while (*i++ < *oi)
                if (i == std::end(_ranges))
                    break;
                // skip their ranges until they definetely overlap:
                // our:         [x ...
                // their: skip]   stop] ...
                oi = std::lower_bound(oi, std::end(other._ranges), *i);  // while (*oi++ < *i)
                if (oi == std::end(other._ranges))
                    break;
                // now we have: (oi->first <= i->last) && (i->first <= oi->last)
                if (i->first() < oi->first()) {     // && oi->first <= i->last
                    if (i->last() <= oi->last()) {  // cut the tail
                        i->leq(prev_value(oi->first()));
                        if (i->empty())
                            i = _ranges.erase(i);
                        else
                            ++i;  // lost the tail, so move on
                    } else {      // (i->first < oi->first) && (oi->last < i->last), middle cut
                        auto tail = range_type{next_value(oi->last()), i->last()};
                        i->leq(prev_value(oi->first()));  // cut the tail
                        i = _ranges.insert(++i, tail);    // insert the new tail
                    }
                    // oi may overlap again with next i, thus no increment here
                } else {  // oi->first <= i->first    && i->first <= oi->last
                    i->geq(next_value(oi->last()));  // cut the head
                    if (i->empty())
                        i = _ranges.erase(i);
                    ++oi;  // oi will not overlap anymore
                }
            }
            return *this;
        }

        /** subtracts a range */
        rangeset_t& operator-=(const range_type& el) { return *this -= rangeset_t{el}; }

        /** subtracts an element */
        rangeset_t& operator-=(const T& el) { return *this -= rangeset_t{el}; }

        /** subtracts a rangeset */
        rangeset_t operator-(const rangeset_t& other) { return rangeset_t{*this} -= other; }

        bool operator==(const rangeset_t& other) const { return _ranges == other._ranges; }

        bool operator!=(const rangeset_t& other) const { return !(*this == other); }

        void plus(T el)
        {
            for (auto& r : _ranges)
                r += el;
        }

        void plus(const range_type range)
        {
            store_type new_ranges;
            for (const auto& r : _ranges) {
                range_type next = r + range;
                if (new_ranges.size() > 0 && _mergable(new_ranges.back(), next))
                    new_ranges.back() |= next;
                else
                    new_ranges.emplace_back(std::move(next));
            }
            std::swap(new_ranges, _ranges);
        }

        void minus(range_type range)
        {
            range *= -1;
            plus(range);
        }

        void minus(T el) { plus(-el); }

        bool contains(const T& el) const { return !(*this && el); }

        bool empty() const
        {
            assert(_dense());
            if (_ranges.empty())
                return true;
            return false;
        }

        T first() const
        {
            assert(!empty());
            return _ranges.begin()->first();
        }

        T last() const
        {
            assert(!empty());
            return _ranges.rbegin()->last();
        }

        void clear() { _ranges.clear(); }

        size_t size() const
        {
            T sum = 0;
            for (auto& e : _ranges)
                sum += e.size();
            return sum;
        }

        friend std::ostream& operator<<(std::ostream& os, const rangeset_t& rangeset)
        {
            auto b = std::begin(rangeset._ranges), e = std::end(rangeset._ranges);
            if (b != e) {
                os << *b;
                while (++b != e)
                    os << "," << *b;
            }
            return os;
        }

        template <typename K>
        rangeset_t<T, K> as_tag() const
        {
            auto r = rangeset_t<T, K>();
            r._ranges = _ranges;
            return r;
        }

        auto begin() const { return iterator(*this, _ranges.begin()); }
        auto end() const { return iterator(*this, _ranges.end()); }

        class iterator : public std::iterator<std::forward_iterator_tag, T>
        {
        protected:
            friend class rangeset_t;
            typename store_type::const_iterator _range_element;
            T _range_pos = 0;
            const rangeset_t& _parent;
            iterator(const iterator&) = default;
            iterator(iterator&&) = default;

            iterator(const rangeset_t& parent, typename store_type::const_iterator range_element,
                     T range_pos = 0):
                _range_element{range_element},
                _range_pos{range_pos}, _parent{parent}
            {}

        public:
            iterator& operator++()
            {
                ++_range_pos;
                if (_range_pos + _range_element->first() == next_value(_range_element->last())) {
                    ++_range_element;
                    _range_pos = 0;
                }
                return *this;
            }

            T operator*() const { return _range_element->first() + _range_pos; }

            T operator->() const { return _range_element->first() + _range_pos; }

            bool operator==(const iterator& rhs) const
            {
                return &_parent == &rhs._parent && _range_element == rhs._range_element &&
                       _range_pos == rhs._range_pos;
            }

            bool operator!=(const iterator& rhs) const { return !((*this) == rhs); }
        }; /* iterator */

    private:
        store_type _ranges;
    }; /* rangeset_t */

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    rangeset_t(const T& element) -> rangeset_t<T>;

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    rangeset_t(const T& first, const T& last) -> rangeset_t<T>;

    template <typename T>
    rangeset_t(const UTAP::range_t<T>& range) -> rangeset_t<T>;

}  // namespace base

#endif /* INCLUDE_BASE_RANGESET_H */
