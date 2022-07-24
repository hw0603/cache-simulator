// file: cachesim.c
// author : 2018115385_류형욱 (2021-2 COMP411007 Computer Architecture)
// datetime : 2022-07-25 01:50
// description : Program to simulate set-associative cache
// usage: ./cachesim -s=<cache size> -a=<set size> -b=<block size> -f=<trace file name>

// TODO: optimize cache and memory structure

#define TRUE 1
#define FALSE 0
#define READ 3 // Read instruction
#define WRITE 4 // Write instruction
#define BIT_MAX 32
#define WORDSIZE 4
#define CYCLE_HIT 1
#define CYCLE_MEM_ACC 200
#define verbose FALSE // trigger verbose output
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// define structure
typedef struct SET {
    struct BLOCK* block;
} SET;

typedef struct BLOCK {
    int lastused;
    int tag;
    int valid;
    int dirty;
    int* data;
} BLOCK;

typedef struct ADDRESS {
    int tag;
    int index;
    int block;
    int byte;
} ADDRESS;

typedef struct MEMORY {
    struct MEMDATA* head;
    struct MEMDATA* tail;
} MEMORY;

typedef struct MEMDATA {
    int address;
    int data;
    struct MEMDATA* prev;
    struct MEMDATA* next;
} MEMDATA;


// define global variables
int timecnt = 1;
int cache_size = 0, block_size = 0, set_size = 0;
int total_cycle = 0, hit_count = 0, miss_count = 0;
int index_total = 0, index_bit = 0, word_count = 0;
int byte_offset = 0, tag_bit = 0;
int insType = 0;
SET* cache = NULL;
BLOCK* block = NULL;
MEMORY* MEMptr = NULL;


// define functions
int log_2(int);
void parseargv(int, char**, int*, int*, int*, char**);
MEMDATA* getMemdata(MEMORY*, int);
void setMemdata(MEMORY*, int, int);
void printMemdata(MEMORY*, int, int);
void initcache();
void set_address();
unsigned int getmask(int start, int cnt);
int isHit(ADDRESS, int*);
int fetchblock(ADDRESS, int);
void write_to_cache(ADDRESS, int);
int read_from_cache(ADDRESS);
void printresult(int);
void deallocate();


// perform log_2 operation
int log_2(int num) {
    int result = 0;

    for (result = 0; num != 1; num >>= 1, result++);
    return result;
}

// check if argument is correctly passed to program
void parseargv(int argc, char* argv[], int* cache_size, int* block_size, int* set_size, char** file_name) {
    char* ch = NULL;

    // check argument length
    if (argc != 5) {
        puts("Invaild argument");
        exit(1);
    }
    // parse passed argument
    for (int i = 1; i < argc; i++) {
        ch = strtok(argv[i], "=-");
        if (!strcmp(ch, "s"))
            *cache_size = atoi(strtok(NULL, "\0"));
        if (!strcmp(ch, "b"))
            *block_size = atoi(strtok(NULL, "\0"));
        if (!strcmp(ch, "a"))
            *set_size = atoi(strtok(NULL, "\0"));
        if (!strcmp(ch, "f"))
            *file_name = strtok(NULL, "\0");
    }
    // check cache size is big enough
    if ((*block_size * (*set_size)) > *cache_size) {
        puts("Cache size too small");
        exit(1);
    }
}

// fetch WORD data from Memory
MEMDATA* getMemdata(MEMORY* MEMptr, int addr) {
    for (MEMDATA* cur = MEMptr->head; cur; cur = cur->next) {
        if (cur->address == addr) {
            return cur;
        }
    }
    return NULL;
}

// write WORD data to Memory
void setMemdata(MEMORY* MEMptr, int addr, int data) {
    MEMDATA* mem = NULL;

    // no need to add MEMDATA if address already exist -> only overwrite data
    mem = getMemdata(MEMptr, addr);
    if (mem) {
        mem->data = data;
        return;
    }
    else {
        mem = (MEMDATA*)malloc(sizeof(MEMDATA));

        mem->data = data;
        mem->address = addr;
        mem->prev = NULL;
        mem->next = NULL;

        // when data is added for the first time (no MEMDATA in MEMORY)
        if (MEMptr->head == NULL && MEMptr->tail == NULL)
            MEMptr->head = MEMptr->tail = mem;
        else {
            MEMDATA* cur = MEMptr->head;
            while (cur->next) {
                if (addr > cur->address && addr < cur->next->address) {
                    // when data goes to tail
                    if (cur->next == NULL)
                        MEMptr->tail = mem;
                    cur->next->prev = mem;
                    mem->next = cur->next;
                    cur->next = mem;
                    mem->prev = cur;
                    return;
                }
                cur = cur->next;
            }
            // when data goes to head
            if (addr < cur->address) {
                MEMptr->head->prev = mem;
                mem->next = MEMptr->head;
                MEMptr->head = mem;
            }
            // when data goes to tail
            else {
                MEMptr->tail->next = mem;
                mem->prev = MEMptr->tail;
                MEMptr->tail = mem;

            }
        }
    }
}

// print all Memory
void printMemory(MEMORY* MEMptr) {
    int cnt = 1;
    for (MEMDATA* p = MEMptr->head; p; p = p->next, cnt++) {
        printf("Address: %.8X --> DATA: %d\n", p->address, p->data);
        if (cnt % word_count == 0)
            putchar('\n');
    }
}

// initalize and assign the cache structure
void initcache() {
    // calculate the value needed
    index_total = cache_size / block_size / set_size;
    index_bit = log_2(index_total);

    word_count = block_size / WORDSIZE; // block 안에 있는 WORD의 개수
    byte_offset = log_2(word_count) + log_2(WORDSIZE); // 1바이트 단위로 뛰기 위한 오프셋

    tag_bit = BIT_MAX - (index_bit + byte_offset);

    // assign the list of set, which will be entire cache
    cache = (SET*)malloc(sizeof(SET) * index_total);
    for (int i = 0; i < index_total; i++) { // for each set in cache
        cache[i].block = (BLOCK*)malloc(sizeof(BLOCK) * set_size);

        for (int j = 0; j < set_size; j++) { // for each block in set
            cache[i].block[j].tag = 0;
            cache[i].block[j].valid = 0;
            cache[i].block[j].dirty = 0;
            cache[i].block[j].data = (int*)malloc(sizeof(int) * word_count);

            for (int k = 0; k < word_count; k++) // for each WORD in block
                cache[i].block[j].data[k] = 0;
        }
    }

    // Initalize MEMORY (Linked List)
    MEMptr = (MEMORY*)malloc(sizeof(MEMORY));
    MEMptr->head = NULL;
    MEMptr->tail = NULL;
}

// construct proper address structure
void set_address(ADDRESS* addr, int address_int) {
    unsigned int mask = 0;

    // set byte offset
    mask = getmask(0, byte_offset);
    addr->byte = address_int & mask;

    // set index bit
    mask = getmask(byte_offset, index_bit);
    addr->index = (address_int & mask) >> (byte_offset);

    // set tag bit
    mask = getmask(byte_offset + index_bit, tag_bit);
    addr->tag = (address_int & mask) >> (byte_offset + index_bit);

    // set block offset
    addr->block = addr->byte / WORDSIZE;
}

// return mask generated from start bit index and bit count from that bit
unsigned int getmask(int start, int cnt) {
    int upper_bit = (start + cnt >= BIT_MAX ? -1 : (1 << (start + cnt)) - 1);
    int lower_bit = ((1 << start) - 1);

    return upper_bit - lower_bit;
}

// check if cache already contains address --> HIT!
int isHit(ADDRESS addr, int* resultidx) {
    BLOCK current_block;

    total_cycle += CYCLE_HIT; // increment total memory access cycle
    for (int i = 0; i < set_size; i++) {
        current_block = cache[addr.index].block[i];
        if (current_block.valid && (current_block.tag == addr.tag)) {
            if (verbose)
                printf("Hit! - ");
            hit_count++;
            *resultidx = i;
            return TRUE;
        }
    }
    if (verbose)
        printf("Miss - ");
    miss_count++;
    return FALSE;
}

// fetch block from memory and return index of block in SET
int fetchblock(ADDRESS addr, int blockidx) {
    int lruidx = 0; // index of the Least Recently Used block in SET
    MEMDATA* block_on_memory = NULL; // start address of block on memory includes addr
    ADDRESS blockaddr; // start address of block on memory includes addr
    int blockaddr_to_int = 0;
    int lrublockaddr_to_int = 0;
    int isemptyblock = FALSE;

    // When cache miss occur, there are two cases
    // 1. Empty block(valid: 0) exists in SET -> find index of that block and write data
    // 2. All blocks in SET are full -> find LRU block and write that block to memory. Then, write data to block(in cache)

    // iterate BLOCK in SET to get proper block address
    for (blockidx = 0, lruidx = 0; blockidx < set_size; blockidx++) {
        // block++ until it finds empty block
        if (cache[addr.index].block[blockidx].valid == 0) {
            isemptyblock = TRUE;
            break;
        }
        // check used-time(block->lastused) and update lruidx so that we can get the index of least-used block
        else if (cache[addr.index].block[blockidx].lastused < cache[addr.index].block[lruidx].lastused) {
            lruidx = blockidx;
        }
    }


    // Case #2. write LRU block to Memory and set blockidx to lruidx if SET is full
    if (isemptyblock == FALSE) {
        // calculate start address of LRU block
        lrublockaddr_to_int = (cache[addr.index].block[lruidx].tag) << (index_bit + byte_offset);
        lrublockaddr_to_int += (addr.index << byte_offset);

        // when LRU block is dirty, write data of block to memory
        if (cache[addr.index].block[lruidx].dirty) {
            for (int i = 0; i < word_count; i++) {
                setMemdata(MEMptr, lrublockaddr_to_int + (WORDSIZE * i), cache[addr.index].block[lruidx].data[i]);
            }
            total_cycle += CYCLE_MEM_ACC; // increment total memory access cycle
        }
        // set blockidx to lruidx 
        blockidx = lruidx;
    }
    

    // set address information(start address of block) to blockaddr
    blockaddr.tag = addr.tag;
    blockaddr.index = addr.index;
    blockaddr.block = blockaddr.byte = 0;

    // convert struct ADDRESS to int
    blockaddr_to_int += (blockaddr.tag << (index_bit + byte_offset));
    blockaddr_to_int += (blockaddr.index << byte_offset);

    // copy Memory block to cache
    if ((block_size != WORDSIZE) || (insType == READ)) {
        /*
        No need to fetch block from memory if (block_size == WORDSIZE) && (insType == WRITE),
        since that block will be overwritten immediately
        */
        for (int i = 0; i < word_count; i++) {
            block_on_memory = getMemdata(MEMptr, blockaddr_to_int + (WORDSIZE * i));
            cache[addr.index].block[blockidx].data[i] = block_on_memory ? block_on_memory->data : 0;
        }
        total_cycle += CYCLE_MEM_ACC; // increment total memory access cycle
    }

    // return blockidx to use later
    return blockidx;
}

// perform "W" operation
void write_to_cache(ADDRESS addr, int data) {
    int blockidx = -1; // index of the block that we write data

    // directly write to cache when HIT
    // fetch block from Memory when MISS
    if (!isHit(addr, &blockidx)) {
        blockidx = fetchblock(addr, blockidx);
        
        cache[addr.index].block[blockidx].tag = addr.tag;
    }
    
    // write new data(passed to argument) to cache
    cache[addr.index].block[blockidx].dirty = 1;
    cache[addr.index].block[blockidx].valid = 1;
    cache[addr.index].block[blockidx].lastused = timecnt++;
    cache[addr.index].block[blockidx].data[addr.block] = data;
}

// perform "R" operation
int read_from_cache(ADDRESS addr) {
    int blockidx = -1; // index of the block that we write data

    // directly return data from cache when HIT
    // fetch block from Memory when MISS
    if (!isHit(addr, &blockidx)) {
        // fetch block from Memory when MISS
        blockidx = fetchblock(addr, blockidx);

        cache[addr.index].block[blockidx].dirty = 0; // dirty bit = 0 since only fetched block from memory
        cache[addr.index].block[blockidx].valid = 1;
        cache[addr.index].block[blockidx].tag = addr.tag;
    }

    cache[addr.index].block[blockidx].lastused = timecnt++; // time++ since "R" operation is also an access

    return cache[addr.index].block[blockidx].data[addr.block];
}

// prints simulation result
void printresult(int printvalue) {
    double miss_rate = 0, average_cycle = 0;
    int dirty_count = 0;

    for (int i = 0; i < index_total; i++) {
        printf("%d: ", i);
        for (int j = 0; j < set_size; j++) {
            if (j != 0)
                printf("   ");

            for (int k = 0; k < word_count; k++) {
                printf("%.8X ", cache[i].block[j].data[k]);
                if (verbose)
                    printf("(%5d)\t", cache[i].block[j].data[k]);
            }
            if (cache[i].block[j].dirty == 1)
                dirty_count++;

            printf("v:%d d:%d\n", cache[i].block[j].valid, cache[i].block[j].dirty);
        }
    }

    miss_rate = 100.0 * miss_count / (hit_count + miss_count);
    average_cycle = (double)total_cycle / (hit_count + miss_count);

    if (printvalue) {
        printf("\ntotal number of hits: %d\n", hit_count);
        printf("total number of misses: %d\n", miss_count);
        printf("miss rate: %.1f%%\n", miss_rate);
        printf("total number of dirty blocks: %d\n", dirty_count);
        printf("total memory access cycle: %d\n", total_cycle);
        printf("average memory access cycle: %.1f\n", average_cycle);
    }
}

// free dynamically allocated memory
void deallocate() {
    MEMDATA* cur = NULL;

    // free Cache structure
    for (int i = 0; i < index_total; i++) {
        for (int j = 0; j < set_size; j++) {
            free(cache[i].block[j].data);
        }
        free(cache[i].block);
    }
    free(cache);

    // free Memory structure
    if (MEMptr->head) {
        for (cur = MEMptr->head->next; cur; cur = cur->next) {
            free(cur->prev);
        }
        free(cur);
    }
}


int main(int argc, char* argv[]) {
    FILE* fp = NULL;
    ADDRESS addr;
    char address[9], accesstype;
    char* file_name;
    int address_int = 0;
    int data;

    // check and parse argument passed to program
    parseargv(argc, argv, &cache_size, &block_size, &set_size, &file_name);

    // initalize the cache structure
    initcache();

    // read memory access log from trace file and simulate the operation
    fp = fopen(file_name, "r");
    while (EOF != fscanf(fp, "%s %c", address, &accesstype)) {
        address_int = strtol(address, NULL, 16);
        set_address(&addr, address_int);

        if (accesstype == 'R') {
            insType = READ;
            data = read_from_cache(addr);
        }
        else if (accesstype == 'W') {
            insType = WRITE;
            fscanf(fp, "%d", &data);
            write_to_cache(addr, data);
        }

        if (verbose) {
            if (accesstype == 'R')
                printf("[%d] Read from %s --> %d Found\n", timecnt - 1, address, data);
            else if (accesstype == 'W')
                printf("[%d] Write %d to %s\n", timecnt - 1, data, address);

            puts("--------------------------------------------------------");
            printresult(TRUE);
            puts("--------------------------------------------------------");
            printMemory(MEMptr);
            puts("--------------------------------------------------------");
        }
    }

    // close input file
    fclose(fp);

    // prints out simulation result
    printresult(TRUE);
    if (verbose)
        printMemory(MEMptr);

    // free allocated memory
    deallocate();

    return 0;
}
