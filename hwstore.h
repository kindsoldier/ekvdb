/*
 * Copyright 2023 Oleg Borodin  <borodin@unix7.org>
 */

#ifndef HWSTORE_H_QWERTY
#define HWSTORE_H_QWERTY

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define HWNULL          0
#define STORE_MAGIC     0xABBAABBA

typedef struct __attribute__((packed)) {
    int     keysize;
    int     valsize;
    int     capa;
    int     next;
} hwcell_t;

typedef struct __attribute__((packed)) {
    union {
        hwmemory_t*   hwmemory;
        int         magic;
    };
    int     size;
    int     head;
    int     tail;
    int     freehead;
} hwstore_t;


void hwstore_init(hwstore_t* hwstore, hwmemory_t* hwmemory);

int hwstore_set(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize);
int hwstore_get(hwstore_t* hwstore, char* key, int keysize, char** val);
int hwstore_del(hwstore_t* hwstore, char* key, int keysize);

void hwstore_print(hwstore_t* hwstore);

#endif
