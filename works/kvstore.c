/*
 * Copyright 2022 Oleg Borodin  <borodin@unix7.org>
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <unistd.h>
#include <time.h>

typedef struct cell cell_t;
struct cell {
    cell_t* next;
    char*   key;
    char*   val;
    int     hwnext;
    int     hwfree;
};

char* new_string(char* orig) {
    if (orig == NULL) return NULL;
    int strsize = strlen(orig) + 1;
    char* copy = malloc(strsize);
    memset(copy, '\0', strsize);
    strcpy(copy, orig);
    return copy;
}


cell_t* new_cell(char* key, char* val) {
    cell_t* cell = malloc(sizeof(cell_t));
    cell->key = new_string(key);
    cell->val = new_string(val);
    cell->next = NULL;
    return cell;
}

void cell_free(cell_t* cell) {
    if (cell == NULL) return;
    free(cell->key);
    free(cell->val);
    free(cell);
}

typedef struct store store_t;
struct store {
    int     count;
    cell_t* head;
    cell_t* tail;
};

typedef struct {
    int hwsize;
    int hwfree;
    int hwhead;
    int hwtail;
} hwstore_t;

typedef struct {
    int keysize;
    int valsize;
    int hwnext;
} hwcell_t;

void store_init(store_t* store) {
    store->head = NULL;
    store->tail = NULL;
    store->count = 0;
    return;
}

int store_hwsize(store_t* store, char* key, char* val) {
    int hwsize = sizeof(hwcell_t);
    hwsize += strlen(key);
    hwsize += strlen(val);
    return hwsize;
}

int store_set(store_t* store, char* key, char* val) {

    cell_t* currcell = store->head;
    while (currcell != NULL) {
        if (strcmp(currcell->key, key) == 0) {
            free(currcell->val);
            currcell->val = new_string(val);
            return 1;
        }
        currcell = currcell->next;
    }

    cell_t* newcell = new_cell(key, val);
    if (store->head == NULL) {
        store->head = newcell;
        store->tail = newcell;
        store->count++;
        return 1;
    }

    store->tail->next = newcell;
    store->tail = newcell;
    store->count++;
    return 1;
}

char* store_get(store_t* store, char* key) {
    cell_t* currcell = store->head;
    while (currcell != NULL) {
        if (strcmp(currcell->key, key) == 0) {
            return currcell->val;
        }
        currcell = currcell->next;
    }
    return NULL;
}


void store_del(store_t* store, char* key) {
    if (store->head == NULL) return;

    if (strcmp(store->head->key, key) == 0) {
        cell_t* delcell = store->head;
        store->head = store->head->next;
        store->count--;
        cell_free(delcell);
        return;
    }

    cell_t* currcell = store->head->next;
    while (currcell->next != NULL) {
        if (strcmp(currcell->next->key, key) == 0) {
            cell_t* delcell = currcell->next;
            currcell->next = currcell->next->next;
            store->count--;
            cell_free(delcell);
            return;
        }
        currcell = currcell->next;
    }
}

void store_iprint(store_t* store) {
    cell_t* currcell = store->head;
    while (currcell != NULL) {
        printf("key = %s, val = %s\n", currcell->key, currcell->val);
        currcell = currcell->next;
    }
}

long getnanotime() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long time = ts.tv_nsec;
    time += ts.tv_sec * 1000 * 1000 * 1000;
    return time;
}


int main(int argc, char **argv) {

    store_t store;
    store_init(&store);

    int count = 1000;
    long start = getnanotime();
    for (int i = 0; i < count; i++) {
        char *key = malloc(16);
        char *val = malloc(16);
        sprintf(key, "k%03d", i);
        sprintf(val, "v%03d", i);
        store_set(&store, key, val);
        free(key);
        free(val);
    }
    long stop = getnanotime();
    printf("time per set %ld\n", (stop - start) / count);

    //store_iprint(&store);

    return 0;
}
