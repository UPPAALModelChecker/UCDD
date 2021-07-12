// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2006, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_HASH_POINTERTABLE_H
#define INCLUDE_HASH_POINTERTABLE_H

#include <cinttypes>

namespace uhash
{
    /// Store non-NULL & non 0xffffffff pointers.
    class PointerTable
    {
    public:
        PointerTable();
        ~PointerTable();

        /// Clear the table.
        void clear();

        /// @return true if the pointer is in the table.
        bool has(const void*) const;

        /// @return true if the pointer is newly added in the table.
        bool add(const void*);

        /// @return true if the pointer was removed from the table.
        bool del(const void*);

        /// @return the number of pointer.
        std::size_t size() const { return nbPointers; }

        /// @return true if the tables contain the same set of pointers.
        bool operator==(const PointerTable&) const;

    private:
        void rehash();

        std::size_t getIndex(const void* ptr) const
        {
            return (((std::size_t)ptr) >> 3u) & (tableSize - 1);
        }

        const void** table;
        std::size_t tableSize, nbPointers;
    };
}  // namespace uhash

#endif  // INCLUDE_HASH_POINTERTABLE_H
