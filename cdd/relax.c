// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: relax.c,v 1.3 2004/06/14 07:36:56 adavid Exp $
//
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include "relax.h"
#include "debug/malloc.h"

int CddRelaxCache_init(CddRelaxCache* cache, int size)
{
    cache->table = (CddRelaxCacheData*)malloc(sizeof(CddRelaxCacheData) * size);
    if (cache->table == NULL) {
        return cdd_error(CDD_MEMORY);
    }

    memset(cache->table, 0, sizeof(CddRelaxCacheData) * size);
    cache->tablesize = size;

    return 0;
}

void CddRelaxCache_done(CddRelaxCache* cache)
{
    free(cache->table);
    cache->table = NULL;
    cache->tablesize = 0;
}

void CddRelaxCache_reset(CddRelaxCache* cache)
{
    memset(cache->table, 0, cache->tablesize * sizeof(CddRelaxCacheData));
}
