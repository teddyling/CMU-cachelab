#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#define FILENAMELENGTH 100
#define LINELENGTH 64

typedef struct DLLNode {
    struct DLLNode* next;
    struct DLLNode* prev;
    unsigned long tag;
} Node;

typedef struct DoublyLinkedList {
    Node* head;
    Node* tail;
    int* capacity;
} DLL;

void getArguments(int argc, char ** argv);
void printMessage(void);
void processTraceFile(char *afile);
void initializeCache(int setNum);
void addLast(Node* n);
void deleteNode(Node* n);

int quit = 0;
int verbose;
int setBit = -1;
int blockBit = -1;
int linesPerSet = -1;
char fileName[FILENAMELENGTH] = "";
DLL* cache;

void initializeCache(int setNum) {
    for (int i = 0; i < setNum; i++) {
        cache[i].head = malloc(sizeof(Node));
        cache[i].tail = malloc(sizeof(Node));
        cache[i].capacity = calloc(1, sizeof(int));
        cache[i].head->next = cache[i].tail;
        cache[i].tail->prev = cache[i].head;
        
    }
}

void addLast(Node* n) {
    cache->tail->prev->next = n;
    n->prev = cache->tail->prev;
    n->next = cache->tail;
    cache->tail->prev = n;
    *(cache->capacity) = *(cache->capacity) + 1;
}

void deleteNode(Node* n) {
    n->prev->next = n->next;
    n->next->prev = n->prev;
    *(cache->capacity) = *(cache->capacity) - 1;
}

int main(int argc, char **argv) {
    getArguments(argc, argv);
    if (quit == 1 || setBit < 0 || blockBit < 0 || linesPerSet <= 0 || fileName[0] == 0 || setBit + blockBit >= 63) {
        printf("Invalid Argument!\n");
        printMessage();
        return 1;
    }
    printf("Set Index Bits: %d\n", setBit);
    printf("Block bits: %d\n", blockBit);
    printf("lines per set: %d\n", linesPerSet);
    printf("trace file name: %s\n", fileName);
    processTraceFile(fileName);

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

void processTraceFile(char *afile) {
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
    }

}
