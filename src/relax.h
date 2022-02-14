#ifndef _RELAX_H
#define _RELAX_H

#include "cdd/kernel.h"

typedef struct
{
    ddNode* res;
    ddNode* node;
    raw_t lower;
    raw_t upper;
    int clock1;
    int clock2;
    int op;
} CddRelaxCacheData;

typedef struct
{
    CddRelaxCacheData* table;
    int tablesize;
} CddRelaxCache;

int CddRelaxCache_init(CddRelaxCache*, int);
void CddRelaxCache_reset(CddRelaxCache*);
void CddRelaxCache_done(CddRelaxCache*);

#endif
