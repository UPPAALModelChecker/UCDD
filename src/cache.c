// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: cache.c,v 1.4 2004/06/14 07:36:56 adavid Exp $
//
///////////////////////////////////////////////////////////////////////////////

#include "cache.h"

#include <stdio.h>
#include <stdlib.h>

int CddCache_init(CddCache* cache, size_t size)
{
    size_t n;

    if ((cache->table = (CddCacheData*)malloc(sizeof(CddCacheData) * size)) == NULL) {
        return cdd_error(CDD_MEMORY);
    }

    for (n = 0; n < size; n++) {
        cache->table[n].a = NULL;
        cache->table[n].b = NULL;
    }
    cache->tablesize = size;

    return 0;
}

void CddCache_done(CddCache* cache)
{
    free(cache->table);
    cache->table = NULL;
    cache->tablesize = 0;
}

void CddCache_reset(CddCache* cache) { memset(cache->table, 0, cache->tablesize * sizeof(CddCacheData)); }

void CddCache_flush(CddCache* cache)
{
    register CddCacheData* n;
    for (n = cache->table + cache->tablesize - 1; n >= cache->table; n--) {
        if (n->a && (!cdd_rglr(n->a)->ref || !cdd_rglr(n->res)->ref || (n->b && !cdd_rglr(n->b)->ref))) {
            n->a = n->b = NULL;
        }
    }
}
