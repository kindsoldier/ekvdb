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
} hwpage_t;

void hwpage_init(hwpage_t* hwpage, int size) {
    hwpage->data = malloc(size);
    memset(hwpage->data, 0, size);
    hwpage->size = size;
}

int hwpage_write(hwpage_t* hwpage, int pos, void* data, int size) {
    if ((pos + size) > hwpage->size) return -1;
    memcpy(&(hwpage->data[pos]), data, size);
    usleep(BYTERATE * size);
    return size;
}

int hwpage_read(hwpage_t* hwpage, int pos, void* data, int size) {
    if ((pos + size) > hwpage->size) {
        size = hwpage->size - pos;
    }
    memcpy(&(*data), &(hwpage->data[pos]), size);
    usleep(BYTERATE * size);
    return size;
}

int hwpage_size(hwpage_t* hwpage) {
    return hwpage->size;
}

void hwpage_destroy(hwpage_t* hwpage) {
    free(hwpage->data);
}

typedef struct __attribute__((packed)) {
    int     size;
    int     capa;
    int     next;
} hwcell_t;

typedef struct __attribute__((packed)) {
    union {
        hwpage_t*   hwpage;
        int         magic;
    };
    int     size;
    int     head;
    int     tail;
    int     freehead;
    //int     freetail;
} hwstore_t;

#define STOREHEAD_SIZE  ((int)sizeof(hwstore_t))
#define CELLHEAD_SIZE   ((int)sizeof(hwcell_t))
#define HWNULL          0
#define STORE_MAGIC     0xABBAABBA

void hwcell_init(hwcell_t* hwcell, int size) {
    hwcell->size = size;
    hwcell->capa = size;
    hwcell->next = HWNULL;
}

void hwcell_destroy(hwcell_t* hwcell) {
    // nop
}

void hwcell_print(hwcell_t* hwcell) {
    printf("cell size = %d, capa = %d, next = %d\n", hwcell->size, hwcell->capa, hwcell->next);
}


void hwstore_init(hwstore_t* hwstore, hwpage_t* hwpage) {
    hwstore->hwpage = hwpage;
    hwstore->size = hwpage_size(hwpage);
    hwstore->head = HWNULL;
    hwstore->tail = HWNULL;
    hwstore->freehead = HWNULL;
}

void hwstore_readcell(hwstore_t* hwstore, int pos, hwcell_t *cell) {
    hwpage_read(hwstore->hwpage, pos, cell, CELLHEAD_SIZE);
}

void hwstore_writecell(hwstore_t* hwstore, int pos, hwcell_t *cell) {
    hwpage_write(hwstore->hwpage, pos, cell, CELLHEAD_SIZE);
}

void hwstore_writehead(hwstore_t* hwstore) {
    hwstore_t chwstore = *hwstore;
    chwstore.magic = STORE_MAGIC;
    hwpage_write(hwstore->hwpage, 0, &chwstore, STOREHEAD_SIZE);
}

int hwstore_allocfree(hwstore_t* hwstore, int size) {
    /* Check free chain */
    if (hwstore->freehead == HWNULL) return -1;

    hwcell_t freecell;
    int freepos = hwstore->freehead;
    hwstore_readcell(hwstore, freepos, &freecell);

    if (freecell.capa >= size) {
        /* Delete cell from free chain */
        hwstore->freehead = freecell.next;
        /* Insert cell to chain */
        freecell.next = hwstore->head;
        hwstore->head = freepos;

        hwstore_writecell(hwstore, freepos, &freecell);
        hwstore_writehead(hwstore);

        hwcell_destroy(&freecell);
        return freepos + CELLHEAD_SIZE;
    }

    while (freecell.next != HWNULL) {
        /* Read next cell */
        hwcell_t nextcell;
        int nextpos = freecell.next;
        hwstore_readcell(hwstore, nextpos, &nextcell);

        if (nextcell.capa >= size) {
            /* Delete free cell from chain */
            freecell.next = nextcell.next;
            hwstore_writecell(hwstore, freepos, &freecell);

            /* Insert next cell to used chain */
            nextcell.next = hwstore->head;
            hwstore_writecell(hwstore, nextpos, &nextcell);

            hwstore->head = nextpos;
            hwstore_writehead(hwstore);

            hwcell_destroy(&freecell);
            return freepos + CELLHEAD_SIZE;
        }
        hwcell_destroy(&nextcell);

        hwstore_readcell(hwstore, freepos, &freecell);
        freepos = freecell.next;
    }
    hwcell_destroy(&freecell);
    return -1;

}

int hwstore_allochead(hwstore_t* hwstore, int size) {

    /* Check null store head */
    if (hwstore->head == HWNULL) {
        hwcell_t headcell;
        hwcell_init(&headcell, size);

        /* Write new cell */
        int headpos = 0 + STOREHEAD_SIZE;
        hwstore_writecell(hwstore, headpos, &headcell);

        /* Update store descriptor */
        hwstore->head = headpos;
        hwstore->tail = headpos;
        hwstore_writehead(hwstore);

        hwcell_destroy(&headcell);
        return headpos + CELLHEAD_SIZE;
    }
    return -1;
}

int hwstore_alloctail(hwstore_t* hwstore, int size) {

    /* Check tail space */
    /* Read tail call from device */
    hwcell_t tailcell;
    int tailpos = hwstore->tail;
    hwstore_readcell(hwstore, tailpos, &tailcell);

    /* Calculate exists and future bound of cells */
    int tailend = hwstore->tail + CELLHEAD_SIZE + tailcell.capa;
    int nextend = tailend + CELLHEAD_SIZE + size;

    /* Compare future bound and size of device */
    if (nextend < hwstore->size) {
        hwcell_t nextcell;
        hwcell_init(&nextcell, size);

        /* Write new tail cell */
        int nextpos = tailend + 1;
        hwstore_writecell(hwstore, nextpos, &nextcell);

        /* Update old tail cell */
        tailcell.next = nextpos;
        hwstore_writecell(hwstore, tailpos, &tailcell);

        /* Update store descriptor */
        hwstore->tail = nextpos;
        hwstore_writehead(hwstore);

        hwcell_destroy(&nextcell);
        hwcell_destroy(&tailcell);
        return nextpos + CELLHEAD_SIZE;
    }

    hwcell_destroy(&tailcell);
    return -1;
}

int hwstore_alloc(hwstore_t* hwstore, int size) {

    int addr = -1;
    if ((addr = hwstore_allocfree(hwstore, size)) > 0) {
        return addr;
    }

    if ((addr = hwstore_allochead(hwstore, size)) > 0) {
        return addr;
    }

    if ((addr = hwstore_alloctail(hwstore, size)) > 0) {
        return addr;
    }

    return -1;
}

void hwstore_free(hwstore_t* hwstore, int addr) {

    if (hwstore->head == HWNULL) return;

    /* Check head for address */
    int headpos = hwstore->head;
    if ((headpos + CELLHEAD_SIZE) == addr) {
        hwcell_t headcell;
        hwstore_readcell(hwstore, headpos, &headcell);

        /* Delete cell from used head */
        hwstore->head = headcell.next;

        /* Insert cell to free head */
        headcell.next = hwstore->freehead;
        hwstore->freehead = headpos;

        hwstore_writecell(hwstore, headpos, &headcell);
        hwstore_writehead(hwstore);
        return;
    }

    /* Check cell chain after head cell */
    int currpos = hwstore->head;
    hwcell_t currcell;
    hwstore_readcell(hwstore, currpos, &currcell);

    while (currcell.next != HWNULL) {
        if ((currcell.next + CELLHEAD_SIZE) == addr) {

            printf("del pos = %d, addr = %d\n", currcell.next, currcell.next + CELLHEAD_SIZE);

            /* Read next cell */
            hwcell_t nextcell;
            int nextpos = currcell.next;
            hwstore_readcell(hwstore, nextpos, &nextcell);

            /* Delete next used cell from chain */
            currcell.next = nextcell.next;
            hwstore_writecell(hwstore, currpos, &currcell);

            /* Insert next cell to free head */
            nextcell.next = hwstore->freehead;

            hwstore->freehead = nextpos;
            hwstore_writecell(hwstore, nextpos, &nextcell);
            hwstore_writehead(hwstore);
            return;
        }
        currpos = currcell.next;
        hwstore_readcell(hwstore, currpos, &currcell);
    }
    return;
}

void hwstore_print(hwstore_t* hwstore) {
    int currpos = hwstore->head;
    while (currpos != HWNULL) {
        hwcell_t currcell;
        hwstore_readcell(hwstore, currpos, &currcell);
        printf("## used cell pos = %3d, addr = %3d\n", currpos, currpos + CELLHEAD_SIZE);
        currpos = currcell.next;
    }

    currpos = hwstore->freehead;
    while (currpos != HWNULL) {
        hwcell_t currcell;
        hwstore_readcell(hwstore, currpos, &currcell);
        printf("#  free cell pos = %3d, addr = %3d\n", currpos, currpos + CELLHEAD_SIZE);
        currpos = currcell.next;
    }
    return;
}


int main(int argc, char **argv) {

    hwpage_t hwpage;
    hwpage_init(&hwpage, 256);

    hwstore_t hwstore;
    hwstore_init(&hwstore, &hwpage);

    int size = 5;

    int count = 7;
    for (int i = 0; i < count; i++) {
        int address = hwstore_alloc(&hwstore, size);
        printf("i - %d, cell = %3d, addr = %3d\n", i, address - CELLHEAD_SIZE, address);
    }

    for (int i = 0; i < 100; i++) {
        hwstore_free(&hwstore, i);
    }

    hwstore_free(&hwstore, 90);

    //int address = hwstore_alloc(&hwstore, size);
    //printf("new addr = %d\n", address);
    //printf("freehead = %d, data addr = %d\n", hwstore.freehead, hwstore.freehead + CELLHEAD_SIZE);


    //hwstore_t* hhwstore = ((hwstore_t*)(hwpage.data));
    //printf("hhw magic = %X\n", hhwstore->magic);
    //printf("freehead = %d, data addr = %d\n", hhwstore->freehead, hwstore.freehead + CELLHEAD_SIZE);

    hwstore_print(&hwstore);


    return 0;
}
