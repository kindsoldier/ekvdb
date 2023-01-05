/*
 * Copyright 2023 Oleg Borodin  <borodin@unix7.org>
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <hwmemory.h>
#include <hwstore.h>

int main(int argc, char **argv) {

    hwmemory_t hwmemory;
    hwmemory_init(&hwmemory, 1024 * 16);

    hwstore_t hwstore;
    hwstore_init(&hwstore, &hwmemory);


    int count = 12;
    for (int i = 0; i < count; i++) {

        char* key = NULL;
        char* val = NULL;
        asprintf(&key, "key%04d", i);
        asprintf(&val, "val%04d", i);
        int keysize = strlen(key) + 1;
        int valsize = strlen(val) + 1;

        int address = hwstore_set(&hwstore, key, keysize, val, valsize);

        printf("i = %3d, addr = %3d\n", i, address);
        free(key);
        free(val);
    }

    for (int i = 0; i < count; i++) {

        char* key = NULL;
        char* val = NULL;
        asprintf(&key, "key%04d", i);
        asprintf(&val, "VAR%04d", i);
        int keysize = strlen(key) + 1;
        int valsize = strlen(val) + 1;

        int address = hwstore_set(&hwstore, key, keysize, val, valsize);

        printf("i = %3d, addr = %3d\n", i, address);
        free(key);
        free(val);
    }

    for (int i = 0; i < count; i++) {

        char* key = NULL;
        char* val = NULL;

        asprintf(&key, "key%04d", i);
        asprintf(&val, "val%04d", i);
        int keysize = strlen(key) + 1;

        char* rval = NULL;
        int addr = HWNULL;
        if ((addr = hwstore_get(&hwstore, key, keysize, &rval)) > 0) {
            printf("i = %3d, get addr = %3d, key = %s, val = %s\n", i, addr, key, rval);
        }

        free(rval);
        free(key);
        free(val);
    }

    hwstore_print(&hwstore);

    return 0;
}
