#ifndef _CACHE_H
#define _CACHE_H

#include "cdd/kernel.h"

/**
 * @file cache.h
 *
 * Private header file for the operation cache.
 */

/**
 * An entry in a \c CddCache cache structure. It contains the
 * arguments and the result of a binary operation.
 */
typedef struct
{
    ddNode* res;   /**< The result of the operation */
    ddNode *a, *b; /**< The arguments of the operation */
    int c;         /**< The operation */
} CddCacheData;

/**
 * A cache structure. Used as an operation cache by the library. The
 * cache is a hash table without collision lists (it will overwrite
 * entries which hash to the same bucket).
 */
typedef struct
{
    CddCacheData* table; /**< The hash table */
    size_t tablesize;    /**< The size of the hash table */
} CddCache;

/**
 * Initialise a cache structure. A hash table with \a size elements
 * will be allocated.
 * @param cache An uninitialized cache structure
 * @param size The size of the hash table to allocate
 * @return An error code
 */
extern int CddCache_init(CddCache* cache, size_t size);

/**
 * Clears all entries in the cache.
 * @param cache A cache structure
 */
extern void CddCache_reset(CddCache* cache);

/**
 * Deletes a cache structure. This function releases all resources
 * allocated by \c CddCache_init().
 * @param cache A cache structure
 */
extern void CddCache_done(CddCache* cache);

/**
 * Removes all entries with a dead decision tree. This function removes
 * all those entries from the cache where one or more of the argument or
 * the result decision trees has a reference counter with value zero.
 * This is used after a garbage collection run to avoid references to
 * garbage collected nodes.
 * @param cache A cache structure
 */
extern void CddCache_flush(CddCache* cache);

/**
 * Returns the entry in the cache for the given hash value. A
 * reference to the entry is returned, so you can assign to it.
 * @param cache A cache structure
 * @param hash A 32-bit hash value
 * @return The entry for this hash value
 */
#define CddCache_lookup(cache, hash) (&(cache)->table[(hash) % (cache)->tablesize])

/**
 * Returns the size of the hash table of a cache.
 * @param cache A cache structure
 * @return The size
 */
#define CddCache_size(cache) ((cache)->tablesize)

#endif /* _CACHE_H */
