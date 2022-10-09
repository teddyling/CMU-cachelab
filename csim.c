#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#define FILENAMELENGTH 100
#define LINELENGTH 64

typedef struct DLLNode {
    struct DLLNode* next;
    struct DLLNode* prev;
    unsigned long tag;
    unsigned int dirty;
} Node;

typedef struct DoublyLinkedList {
    Node* head;
    Node* tail;
} DLL;

void getArguments(int argc, char ** argv);
void printMessage(void);
void mainProcess(char *afile);
void initializeCache(unsigned long setNum);
void addLast(unsigned long index, Node* n);
void deleteNode(Node* n);
void cacheOperation(char op, unsigned long address, unsigned long block);

int quit = 0;
int verbose;
int setBit = -1;
int blockBit = -1;
int linesPerSet = -1;
char fileName[FILENAMELENGTH] = "";


DLL* cache;
csim_stats_t myStats = {
    .hits = 0,
    .misses = 0,
    .evictions = 0,
    .dirty_bytes = 0,
    .dirty_evictions = 0
};

void initializeCache(unsigned long setNum) {
    cache = malloc(setNum * sizeof(*cache));
    for (unsigned long i = 0; i < setNum; i++) {
        cache[i].head = malloc(sizeof(Node));
        cache[i].tail = malloc(sizeof(Node));
        cache[i].head->next = cache[i].tail;
        cache[i].tail->prev = cache[i].head;
        
    }
}
void addLast(unsigned long index, Node* n) {
    cache[index].tail->prev->next = n;
    n->prev = cache[index].tail->prev;
    n->next = cache[index].tail;
    cache[index].tail->prev = n;
}

void deleteNode(Node* n) {
    n->prev->next = n->next;
    n->next->prev = n->prev;
}
void cacheOperation(char op, unsigned long address, unsigned long block) {
    unsigned long thisTag = address >> (setBit + blockBit);
    unsigned long thisSetNum = (address >> blockBit) & ((1L << setBit) - 1L);
    unsigned long blockByteNum = 1L << blockBit;
    int size = 0;
    Node* sizeCheck = cache[thisSetNum].head->next;
    while (sizeCheck != cache[thisSetNum].tail) {
        size++;
        sizeCheck = sizeCheck->next;
    }
    /* Cold Miss*/
    if (size == 0) {
        myStats.misses = myStats.misses + 1;
        Node* aNode = malloc(sizeof(Node));
        aNode->dirty = 0;
        if (op == 'S') {
            aNode->dirty = 1;
            myStats.dirty_bytes = myStats.dirty_bytes + blockByteNum;
        }
        aNode->tag = thisTag;
        addLast(thisSetNum, aNode);
        printf("A Cold Miss\n");
        return;
    }
    bool hit = false;
    Node* curr = cache[thisSetNum].head->next;
    while (curr != cache[thisSetNum].tail) {
        if (curr->tag == thisTag) {
            hit = true;
            break;
        }
        curr = curr->next;
    }
    /* Cache Hit (read or write)*/
    if (hit) {
        myStats.hits = myStats.hits + 1;
        /* Write Hit*/
        if (op == 'S') {
            curr->dirty = 1;
            myStats.dirty_bytes = myStats.dirty_bytes + blockByteNum;
        }
        deleteNode(curr);
        addLast(thisSetNum, curr);
        printf("Hit!\n");
    /* Cache Miss*/
    } else {
        myStats.misses = myStats.misses + 1;
        Node* aNode = malloc(sizeof(Node));
        aNode->dirty = 0;
        aNode->tag = thisTag;
        if (op == 'S') {
            aNode->dirty = 1;
            myStats.dirty_bytes = myStats.dirty_bytes + blockByteNum;
        }
        /* All cache lines are occupied, need eviction*/
        if (size == linesPerSet) {
            myStats.evictions = myStats.evictions + 1;
            Node* toDelete = cache[thisSetNum].head->next;
            if (toDelete->dirty == 1) {
                myStats.dirty_evictions = myStats.dirty_evictions + blockByteNum;
                myStats.dirty_bytes = myStats.dirty_bytes - blockByteNum;
            }
            deleteNode(toDelete);
            free(toDelete);
            addLast(thisSetNum, aNode);
            printf("A Cache Miss and Eviction\n");
        /* Some cache lines still available*/    
        } else {
            addLast(thisSetNum, aNode);
            printf("A Cache Miss\n");
        }
    }

}
int main(int argc, char **argv) {
    getArguments(argc, argv);
    if (quit == 1 || setBit < 0 || blockBit < 0 || linesPerSet <= 0 || fileName[0] == 0 || setBit + blockBit >= 64) {
        printf("Invalid Argument!\n");
        printMessage();
        return 1;
    }
    printf("Set Index Bits: %d\n", setBit);
    printf("Block bits: %d\n", blockBit);
    printf("lines per set: %d\n", linesPerSet);
    printf("trace file name: %s\n", fileName);
    unsigned long setNum = 1 << setBit;
    initializeCache(setNum);
    mainProcess(fileName);
    printSummary(&myStats);

    return 0;
}

void getArguments(int argc, char ** argv) {
    int opt;

    while ((opt = getopt(argc, argv, "vs:E:b:t:h")) != -1) {
        switch(opt) {
            case 'v':
                verbose = 1;
                break;
            case 's':
                setBit = atoi(optarg);
                break;
            case 'E':
                linesPerSet = atoi(optarg);
                break;
            case 'b':
                blockBit = atoi(optarg);
                break;
            case 't':
                strcpy(fileName, optarg);
                break;
            case 'h':
                printMessage();
                break;
            default:
                quit = 1;
                printMessage();
                break;

        }
    }
}
void printMessage(void) {
    printf("Usage :  ./csim -ref [-v] -s <s> -E <E> -b <b> -t <trace>\n");
    printf("./csim -h\n");
    printf("    -h    Print this help message and exit\n");
    printf("    -v    Verbose mode: report effects of each memory operation\n");
    printf("    -s <s>    Number of set index bits (there are 2**s sets\n");
    printf("    -b <b>    Number of block bits (there are 2**b blocks)\n");
    printf("    -E <E>    Number of lines per set (associativity)\n");
    printf("    -t <trace >    File name of the memory trace to process\n");
    printf("The -s, -b, -E, and -t options must be supplied for all simulations.\n");
}

void mainProcess(char *afile) {
    FILE *inputTrace = fopen(afile, "r");
    if (!inputTrace) {
        printf("Failed open trace file!\n");
        return;
    }
    char lineBuffer[LINELENGTH];
    char type;
    char* left;
    char* end;
    unsigned long address;
    unsigned long block;
    while (fgets(lineBuffer, LINELENGTH, inputTrace)) {
        type = lineBuffer[0];
        address = strtoul(lineBuffer + 2, &left, 16);
        block = strtoul(left + 1, &end, 10);
        printf("type is %c, address is %lx, block is %ld\n", type, address, block);
        if ((type != 'S' && type != 'L') || *left != ',') {
            printf("Wrong Input!\n");
        }
        cacheOperation(type, address, block);
    }
    fclose(inputTrace);
}
