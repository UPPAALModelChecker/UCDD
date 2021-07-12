// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include "base/platform.h"

int main(int argc, char* argv[])
{
    size_t i, j;
    maclist_t* res = base_getMAC();
    if (!res) {
        fprintf(stderr, "Could not get MAC addresses.\n");
        return 1;
    }

    for (i = 0; i < res->size; ++i) {
        printf("MAC address:");
        for (j = 0; j < 6; ++j) {
            printf("%c%2.2x", j > 0 ? ':' : ' ', res->mac[i][j]);
        }
        printf("\n");
    }
    free(res);

    return 0;
}
