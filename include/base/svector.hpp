//
// Created by marius on 01/12/2020.
//

#ifndef UPPAAL_SVECTOR_HPP
#define UPPAAL_SVECTOR_HPP

#include <vector>
#include <cassert>

/** Stable vector does not allow reallocations, insists on one-time allocation */
template <typename T>
class svector : private std::vector<T>
{
public:
    using std::vector<T>::vector;
    using std::vector<T>::begin;
    using std::vector<T>::end;
    using std::vector<T>::operator[];

    void resize(size_t newsize)
    {
        assert(std::vector<T>::empty());
        std::vector<T>::resize(newsize);
    }
};

#endif  // UPPAAL_SVECTOR_HPP
