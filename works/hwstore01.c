/*
 * Copyright 2023 Oleg Borodin  <borodin@unix7.org>
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int msleep(int tms) {
  return usleep(tms * 1000);
}

#define BYTERATE (8 + 2)

typedef struct {
    char*   data;
    int     size;
} hwmemory_t;

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

#define STOREHEAD_SIZE  ((int)sizeof(hwstore_t))
#define CELLHEAD_SIZE   ((int)sizeof(hwcell_t))
#define HWNULL          0
#define STORE_MAGIC     0xABBAABBA

void hwcell_init(hwcell_t* hwcell, int keysize, int valsize) {
    hwcell->keysize = keysize;
    hwcell->valsize = valsize;
    hwcell->capa = keysize + valsize;
    hwcell->next = HWNULL;
}

void hwcell_destroy(hwcell_t* hwcell) {
    // nop
}

void hwcell_print(hwcell_t* hwcell) {
    printf("cell size = %d, capa = %d, next = %d\n", hwcell->keysize + hwcell->valsize, hwcell->capa, hwcell->next);
}


void hwstore_init(hwstore_t* hwstore, hwmemory_t* hwmemory) {
    hwstore->hwmemory = hwmemory;
    hwstore->size = hwmemory_size(hwmemory);
    hwstore->head = HWNULL;
    hwstore->tail = HWNULL;
    hwstore->freehead = HWNULL;
}

void hwstore_read_chead(hwstore_t* hwstore, int pos, hwcell_t *cell) {
    hwmemory_read(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);

}

void hwstore_read_cell(hwstore_t* hwstore, int pos, hwcell_t *cell, char** key, char** val) {
    hwmemory_read(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
    pos += CELLHEAD_SIZE;
    *key = malloc(cell->keysize);
    *val = malloc(cell->valsize);
    hwmemory_read(hwstore->hwmemory, pos, *key, cell->keysize);
    pos += cell->keysize;
    hwmemory_read(hwstore->hwmemory, pos, *val, cell->valsize);
}

void hwstore_read_ckey(hwstore_t* hwstore, int pos, hwcell_t *cell, char** key) {
    //hwmemory_read(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
    pos += CELLHEAD_SIZE;
    *key = malloc(cell->keysize);
    hwmemory_read(hwstore->hwmemory, pos, *key, cell->keysize);
}

void hwstore_read_cval(hwstore_t* hwstore, int pos, hwcell_t *cell, char** val) {
    //hwmemory_read(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
    pos += CELLHEAD_SIZE;
    pos += cell->keysize;
    *val = malloc(cell->valsize);
    hwmemory_read(hwstore->hwmemory, pos, *val, cell->valsize);
}


void hwstore_write_cell(hwstore_t* hwstore, int pos, hwcell_t *cell, char* key, char* val) {
    hwmemory_write(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
    pos += CELLHEAD_SIZE;
    hwmemory_write(hwstore->hwmemory, pos, key, cell->keysize);
    pos += cell->keysize;
    hwmemory_write(hwstore->hwmemory, pos, val, cell->valsize);
}

void hwstore_write_chead(hwstore_t* hwstore, int pos, hwcell_t *cell) {
    hwmemory_write(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
}


void hwstore_write_shead(hwstore_t* hwstore) {
    hwstore_t chwstore = *hwstore;
    chwstore.magic = STORE_MAGIC;
    hwmemory_write(hwstore->hwmemory, 0, &chwstore, STOREHEAD_SIZE);
}

int hwstore_trywrite_tofree(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize) {
    /* Check free chain */
    if (hwstore->freehead == HWNULL) return -1;

    int datasize = keysize + valsize;

    hwcell_t freecell;
    int freepos = hwstore->freehead;
    hwstore_read_chead(hwstore, freepos, &freecell);

    if (freecell.capa >= datasize) {
        /* Delete cell from free chain */
        hwstore->freehead = freecell.next;
        /* Insert cell to chain */
        freecell.next = hwstore->head;
        hwstore->head = freepos;

        hwstore_write_cell(hwstore, freepos, &freecell, key, val);
        hwstore_write_shead(hwstore);

        hwcell_destroy(&freecell);
        return freepos;
    }

    while (freecell.next != HWNULL) {
        /* Read next cell */
        hwcell_t nextcell;
        int nextpos = freecell.next;
        hwstore_read_chead(hwstore, nextpos, &nextcell);

        if (nextcell.capa >= datasize) {
            /* Delete free cell from chain */
            freecell.next = nextcell.next;
            hwstore_write_chead(hwstore, freepos, &freecell);

            /* Insert next cell to used chain */
            nextcell.next = hwstore->head;
            hwstore_write_cell(hwstore, nextpos, &nextcell, key, val);

            hwstore->head = nextpos;
            hwstore_write_shead(hwstore);

            hwcell_destroy(&freecell);
            return freepos;
        }
        hwcell_destroy(&nextcell);

        hwstore_read_chead(hwstore, freepos, &freecell);
        freepos = freecell.next;
    }
    hwcell_destroy(&freecell);
    return -1;

}

int hwstore_trywrite_tohead(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize) {

    /* Check null store head */
    if (hwstore->head == HWNULL) {
        hwcell_t headcell;
        hwcell_init(&headcell, keysize, valsize);

        /* Write new cell */
        int headpos = 0 + STOREHEAD_SIZE;
        hwstore_write_cell(hwstore, headpos, &headcell, key, val);

        /* Update store descriptor */
        hwstore->head = headpos;
        hwstore->tail = headpos;
        hwstore->freehead = HWNULL;
        hwstore_write_shead(hwstore);

        hwcell_destroy(&headcell);
        return headpos;
    }
    return -1;
}

int hwstore_trywrite_totail(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize) {
    /* Check tail space */
    int datasize = keysize + valsize;

    /* Read tail call from device */
    hwcell_t tailcell;
    int tailpos = hwstore->tail;
    hwstore_read_chead(hwstore, tailpos, &tailcell);

    /* Calculate exists and future bound of cells */
    int tailend = hwstore->tail + CELLHEAD_SIZE + tailcell.capa;
    int nextend = tailend + CELLHEAD_SIZE + datasize;

    /* Compare future bound and size of device */
    if (nextend < hwstore->size) {
        hwcell_t nextcell;
        hwcell_init(&nextcell, keysize, valsize);

        /* Write new tail cell */
        int nextpos = tailend + 1;
        hwstore_write_cell(hwstore, nextpos, &nextcell, key, val);

        /* Update old tail cell */
        tailcell.next = nextpos;
        hwstore_write_chead(hwstore, tailpos, &tailcell);

        /* Update store descriptor */
        hwstore->tail = nextpos;
        hwstore_write_shead(hwstore);

        hwcell_destroy(&nextcell);
        hwcell_destroy(&tailcell);
        return nextpos;
    }

    hwcell_destroy(&tailcell);
    return -1;
}

int hwstore_alloc(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize) {

    int addr = -1;

    if ((addr = hwstore_trywrite_tohead(hwstore, key, keysize, val, valsize)) > 0) {
        return addr;
    }

    if ((addr = hwstore_trywrite_tofree(hwstore, key, keysize, val, valsize)) > 0) {
        return addr;
    }

    if ((addr = hwstore_trywrite_totail(hwstore, key, keysize, val, valsize)) > 0) {
        return addr;
    }

    return addr;
}

void hwstore_free(hwstore_t* hwstore, int addr) {

    if (hwstore->head == HWNULL) return;

    /* Check head for address */
    int headpos = hwstore->head;
    if (headpos == addr) {
        hwcell_t headcell;
        hwstore_read_chead(hwstore, headpos, &headcell);

        /* Delete cell from used head */
        hwstore->head = headcell.next;

        /* Insert cell to free head */
        headcell.next = hwstore->freehead;
        hwstore->freehead = headpos;

        hwstore_write_chead(hwstore, headpos, &headcell);
        hwstore_write_shead(hwstore);
        return;
    }

    /* Check cell chain after head cell */
    int currpos = hwstore->head;
    hwcell_t currcell;
    hwstore_read_chead(hwstore, currpos, &currcell);

    while (currcell.next != HWNULL) {
        if (currcell.next == addr) {

            printf("del addr = %d\n", currcell.next);

            /* Read next cell */
            hwcell_t nextcell;
            int nextpos = currcell.next;
            hwstore_read_chead(hwstore, nextpos, &nextcell);

            /* Delete next used cell from chain */
            currcell.next = nextcell.next;
            hwstore_write_chead(hwstore, currpos, &currcell);

            /* Insert next cell to free head */
            nextcell.next = hwstore->freehead;

            hwstore->freehead = nextpos;
            hwstore_write_chead(hwstore, nextpos, &nextcell);
            hwstore_write_shead(hwstore);
            return;
        }
        currpos = currcell.next;
        hwstore_read_chead(hwstore, currpos, &currcell);
    }
    return;
}

void hwstore_print(hwstore_t* hwstore) {
    int currpos = hwstore->head;
    while (currpos != HWNULL) {
        hwcell_t currcell;
        char* key = NULL;
        char* val = NULL;
        hwstore_read_cell(hwstore, currpos, &currcell, &key, &val);
        printf("## used cell addr = %3d, key = %s, val=%s\n", currpos, key, val);
        free(key);
        free(val);
        currpos = currcell.next;
    }

    currpos = hwstore->freehead;
    while (currpos != HWNULL) {
        hwcell_t currcell;
        hwstore_read_chead(hwstore, currpos, &currcell);
        printf("#  free cell addr = %3d\n", currpos);
        currpos = currcell.next;
    }
    return;
}

int hwstore_get(hwstore_t* hwstore, char* key, int keysize, char** val) {
    int currpos = hwstore->head;
    while (currpos != HWNULL) {
        hwcell_t currcell;
        hwstore_read_chead(hwstore, currpos, &currcell);
        if (currcell.keysize == keysize) {
            char* hwkey = NULL;
            hwstore_read_ckey(hwstore, currpos, &currcell, &hwkey);

            if (memcmp(key, hwkey, keysize) == 0) {
                hwstore_read_cval(hwstore, currpos, &currcell, val);

                free(hwkey);
                return currpos;
            }

            free(hwkey);
        }
        currpos = currcell.next;
    }
    return -1;
}

int hwstore_find(hwstore_t* hwstore, char* key, int keysize, hwcell_t* currcell) {
    int currpos = hwstore->head;
    while (currpos != HWNULL) {
        hwstore_read_chead(hwstore, currpos, currcell);
        if (currcell->keysize == keysize) {
            char* hwkey = NULL;
            hwstore_read_ckey(hwstore, currpos, currcell, &hwkey);

            if (memcmp(key, hwkey, keysize) == 0) {
                free(hwkey);
                return currpos;
            }
            free(hwkey);
        }
        currpos = currcell->next;
    }
    return -1;
}

int hwstore_del(hwstore_t* hwstore, char* key, int keysize) {
    int addr = -1;
    hwcell_t currcell;
    if ((addr = hwstore_find(hwstore, key, keysize, &currcell)) > 0) {
        hwstore_free(hwstore, addr);
    }
    return addr;
}

int hwstore_set(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize) {
    int addr = -1;
    hwcell_t currcell;

    if ((addr = hwstore_find(hwstore, key, keysize, &currcell)) > 0) {
        int datasize = keysize + valsize;
        if (datasize > currcell.capa) {
            hwstore_free(hwstore, addr);
            int newaddr = hwstore_alloc(hwstore, key, keysize, val, valsize);
            return newaddr;
        }

        currcell.keysize = keysize;
        currcell.valsize = valsize;
        hwstore_write_cell(hwstore, addr, &currcell, key, val);
        return addr;
    }
    addr = hwstore_alloc(hwstore, key, keysize, val, valsize);
    return addr;
}

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
