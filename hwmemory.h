/*
 * Copyright 2023 Oleg Borodin  <borodin@unix7.org>
 */

#ifndef HWMEMORY_H_QWERTY
#define HWMEMORY_H_QWERTY

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    char*   data;
    int     size;
} hwmemory_t;

void hwmemory_init(hwmemory_t* hwmemory, int size);
int hwmemory_write(hwmemory_t* hwmemory, int pos, void* data, int size);
int hwmemory_read(hwmemory_t* hwmemory, int pos, void* data, int size);
int hwmemory_size(hwmemory_t* hwmemory);
void hwmemory_destroy(hwmemory_t* hwmemory);

#endif
