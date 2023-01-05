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


#define STOREHEAD_SIZE  ((int)sizeof(hwstore_t))
#define CELLHEAD_SIZE   ((int)sizeof(hwcell_t))


static void hwcell_init(hwcell_t* hwcell, int keysize, int valsize);

static void hwstore_read_chead(hwstore_t* hwstore, int pos, hwcell_t *cell);
static void hwstore_read_cell(hwstore_t* hwstore, int pos, hwcell_t *cell, char** key, char** val);
static void hwstore_read_ckey(hwstore_t* hwstore, int pos, hwcell_t *cell, char** key);
static void hwstore_read_cval(hwstore_t* hwstore, int pos, hwcell_t *cell, char** val);

static void hwstore_write_cell(hwstore_t* hwstore, int pos, hwcell_t *cell, char* key, char* val);
static void hwstore_write_chead(hwstore_t* hwstore, int pos, hwcell_t *cell);
static void hwstore_write_shead(hwstore_t* hwstore);


static int hwstore_trywrite_tofree(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize);
static int hwstore_trywrite_tohead(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize);
static int hwstore_trywrite_totail(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize);
static int hwstore_alloc(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize);
static void hwstore_free(hwstore_t* hwstore, int addr);

static int hwstore_find(hwstore_t* hwstore, char* key, int keysize, hwcell_t* currcell);

static void hwcell_init(hwcell_t* hwcell, int keysize, int valsize) {
    hwcell->keysize = keysize;
    hwcell->valsize = valsize;
    hwcell->capa = keysize + valsize;
    hwcell->next = HWNULL;
}

void hwstore_init(hwstore_t* hwstore, hwmemory_t* hwmemory) {
    hwstore->hwmemory = hwmemory;
    hwstore->size = hwmemory_size(hwmemory);
    hwstore->head = HWNULL;
    hwstore->tail = HWNULL;
    hwstore->freehead = HWNULL;
}

static void hwstore_read_chead(hwstore_t* hwstore, int pos, hwcell_t *cell) {
    hwmemory_read(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
}

static void hwstore_read_cell(hwstore_t* hwstore, int pos, hwcell_t *cell, char** key, char** val) {
    hwmemory_read(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
    pos += CELLHEAD_SIZE;
    *key = malloc(cell->keysize);
    *val = malloc(cell->valsize);
    hwmemory_read(hwstore->hwmemory, pos, *key, cell->keysize);
    pos += cell->keysize;
    hwmemory_read(hwstore->hwmemory, pos, *val, cell->valsize);
}

static void hwstore_read_ckey(hwstore_t* hwstore, int pos, hwcell_t *cell, char** key) {
    //hwmemory_read(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
    pos += CELLHEAD_SIZE;
    *key = malloc(cell->keysize);
    hwmemory_read(hwstore->hwmemory, pos, *key, cell->keysize);
}

static void hwstore_read_cval(hwstore_t* hwstore, int pos, hwcell_t *cell, char** val) {
    //hwmemory_read(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
    pos += CELLHEAD_SIZE;
    pos += cell->keysize;
    *val = malloc(cell->valsize);
    hwmemory_read(hwstore->hwmemory, pos, *val, cell->valsize);
}


static void hwstore_write_cell(hwstore_t* hwstore, int pos, hwcell_t *cell, char* key, char* val) {
    hwmemory_write(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
    pos += CELLHEAD_SIZE;
    hwmemory_write(hwstore->hwmemory, pos, key, cell->keysize);
    pos += cell->keysize;
    hwmemory_write(hwstore->hwmemory, pos, val, cell->valsize);
}

static void hwstore_write_chead(hwstore_t* hwstore, int pos, hwcell_t *cell) {
    hwmemory_write(hwstore->hwmemory, pos, cell, CELLHEAD_SIZE);
}

static void hwstore_write_shead(hwstore_t* hwstore) {
    hwstore_t chwstore = *hwstore;
    chwstore.magic = STORE_MAGIC;
    hwmemory_write(hwstore->hwmemory, 0, &chwstore, STOREHEAD_SIZE);
}

static int hwstore_trywrite_tofree(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize) {
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

            return freepos;
        }

        hwstore_read_chead(hwstore, freepos, &freecell);
        freepos = freecell.next;
    }
    return -1;

}

static int hwstore_trywrite_tohead(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize) {

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
        return headpos;
    }
    return -1;
}

static int hwstore_trywrite_totail(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize) {
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
        return nextpos;
    }

    return -1;
}

static int hwstore_alloc(hwstore_t* hwstore, char* key, int keysize, char* val, int valsize) {
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

static void hwstore_free(hwstore_t* hwstore, int addr) {

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

static int hwstore_find(hwstore_t* hwstore, char* key, int keysize, hwcell_t* currcell) {
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
