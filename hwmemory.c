/*
 * Copyright 2023 Oleg Borodin  <borodin@unix7.org>
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <hwmemory.h>

int msleep(int tms) {
  return usleep(tms * 1000);
}

#define BYTERATE (8 + 2)


void hwmemory_init(hwmemory_t* hwmemory, int size) {
    hwmemory->data = malloc(size);
    memset(hwmemory->data, 0, size);
    hwmemory->size = size;
}

int hwmemory_write(hwmemory_t* hwmemory, int pos, void* data, int size) {
    if ((pos + size) > hwmemory->size) return -1;
    memcpy(&(hwmemory->data[pos]), data, size);
    usleep(BYTERATE * size);
    return size;
}

int hwmemory_read(hwmemory_t* hwmemory, int pos, void* data, int size) {
    if ((pos + size) > hwmemory->size) {
        size = hwmemory->size - pos;
    }
    memcpy(&(*data), &(hwmemory->data[pos]), size);
    usleep(BYTERATE * size);
    return size;
}

int hwmemory_size(hwmemory_t* hwmemory) {
    return hwmemory->size;
}

void hwmemory_destroy(hwmemory_t* hwmemory) {
    free(hwmemory->data);
}
