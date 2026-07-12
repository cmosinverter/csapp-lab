#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

typedef struct {
    int hit;
    int miss;
    int evict;
    int counter;
} global_t;

typedef struct {
    bool valid;
    int tag;
    int counter;
} block_t;

typedef struct {
    block_t *blocks;
} set_t;

typedef struct {
    int s;
    int e;
    int b;
    set_t *sets;
} cache_t;

typedef struct {
    char op;
    uint64_t addr;
} valgrind_t;

int int_power(int base, int exp) {
    int result = 1;
    for (int i = 0; i < exp; ++i) {
        result *= base;
    }
    return result;
}

cache_t *cache_init(int s, int e, int b) {
    cache_t *cache = (cache_t *)malloc(sizeof(cache_t));
    if (cache == NULL) {
        return NULL;
    }
    cache->s = int_power(2, s);
    cache->e = e;
    cache->b = int_power(2, b);
    cache->sets = (set_t *)malloc(cache->s * sizeof(set_t));
    if (cache->sets == NULL) {
        return NULL;
    }
    for (int i = 0; i < cache->s; i++) {
        cache->sets[i].blocks = (block_t *)malloc(cache->e * sizeof(block_t));
        if (cache->sets[i].blocks == NULL) {
            return NULL;
        }
        for (int j = 0; j < cache->e; j++) {
            cache->sets[i].blocks[j].valid = false;
            cache->sets[i].blocks[j].tag = 0;
            cache->sets[i].blocks[j].counter = 0; 
        }
    }
    return cache;
}

void get_index(int s, int b, uint64_t addr, uint64_t *tag, uint64_t *set, uint64_t *block) {
    *tag = addr >> (s + b);
    *set = (addr >> b) & ((1U << s) - 1);
    *block = addr & ((1U << b) - 1);
    printf("tag %llx set %llx ", (unsigned long long int)*tag, (unsigned long long int)*set);
}

int get_addr_op(valgrind_t *v, char *s) {

    char *ptr = s;
    if (*ptr++ != ' ') {
        return -1;
    }

    v->op = *ptr;
    v->addr = 0;
    ptr += 2;

    while (*ptr != ',') {
        uint32_t t = *ptr > '9' ? 10 + *ptr - 'a' : *ptr - '0';
        v->addr = (v->addr << 4) | (uint64_t)t;
        ptr++;
    }
    return 0;
}

void cache_access(cache_t *cache, global_t *glb, uint64_t addr, int s, int b) {
    uint64_t tag, set, block;
    get_index(s, b, addr, &tag, &set, &block);
    block_t *bptr = cache->sets[set].blocks;
    int mtrace = INT_MAX;
    int midx = 0;
    int invalid = -1;
    for (int i = 0; i < cache->e; i++) {
        if (bptr->valid && bptr->tag == tag) {
            printf("hit\n");
            glb->hit++;
            bptr->counter = ++glb->counter;
            return;
        }
        
        if (invalid == -1 && bptr->valid == false) {
           invalid = i;
        }

        if (bptr->counter < mtrace) {
            midx = i;
            mtrace = bptr->counter;
        }
        bptr++;
    }
    
    printf("miss ");
    if (invalid == -1) {
        printf("eviction");
        glb->evict++;
    }
    printf("\n");
    int idx = invalid >= 0 ? invalid : midx;
    bptr = cache->sets[set].blocks;
    bptr[idx].valid = 1; 
    bptr[idx].tag = tag;
    bptr[idx].counter = ++glb->counter;
    glb->miss++;
}

int main(int argc, char *argv[])
{
    char opt;
    char *t = NULL;
    int s, e, b;
    FILE *f;
    char buffer[32];
    
    while ((opt = getopt(argc, argv, ":s:E:b:t:")) != -1) {
        switch (opt) {
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                e = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                t = optarg;
                break;
            case ':':
                printf("Missing argument for -%c\n", optopt);
                return 1;
            case '?':
                printf("Invalid option -%c\n", optopt);
                return 1;
        }
    }

    cache_t *cache = cache_init(s, e, b);
    
    // printf("s = %d, E = %d, b = %d, file = %s\n", s, e, b, t);

    f = fopen(t, "r");
    valgrind_t *v = malloc(sizeof(valgrind_t));
    global_t *glb = malloc(sizeof(global_t));
    glb->hit = 0;
    glb->miss = 0;
    glb->evict = 0;
    glb->counter = 0;
    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        if (get_addr_op(v, buffer) < 0) {
            continue;
        }
        printf("%c %llx ", v->op, (unsigned long long int)v->addr);
        cache_access(cache, glb, v->addr, s, b);
        if (v->op == 'M') {
            cache_access(cache, glb, v->addr, s, b);
        }
    }

    printSummary(glb->hit, glb->miss, glb->evict);
    
    // free
    free(glb);
    free(v);
    for (int i = 0; i < cache->e; i++) {
        free(cache->sets[i].blocks);
    }
    free(cache);
    
    return 0;
}
