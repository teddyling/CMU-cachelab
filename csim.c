/**
 * This C file is a cache simulator that takes user arguments and simulates the operation of a cache. 
 * In order to use this simulator, four inputs are needed from the user, and they are "s," "E," "b," and "t."
 * s should be a nonnegative integer that represents the set bit of the cache. In other words, the cache will have 2**s sets. 
 * E should be a positive integer which represents how many cache lines are in a single set. 
 * b should be a nonnegative integer that represents the block offset. 
 * t should be a string that represents the directory of the input trace file. 
 * An example command line input is: ./csim -s 4 -E 10 -b 0 -t mytrace.trace.
 * Note that -s, -E, -b, -t can be in any order.
 * There are also two optional command, -h and -v. The first one will provide a usage insturction, and the second one will enable the verbose mode.  
 * 
 * This cache simulator was implemented based on an array of doubly-linked lists. 
 * Each node of the list has four fields, its next node, its previous node, its tag, and its dirty bits. 
 * It is able to tell that each node can represent a cache line, and the whole doubly linked list can represent a cache set. 
 * The doubly linked list has two fields, the head node, and the tail node. 
 * In order to avoid the edge case, the head and the tail node are just placeholders with no meaningful data inside. 
 * The node between the head and the tail nodes are meaningful nodes with tags and dirty bits. 
 * 
 * This cache simulator uses the eviction strategy of LRU, 
 * which means when a cache set is full, and eviction is required, the Least Recently Used cache line will be evicted. 
 * In order to achieve this implementation, two Doubly Linked list methods are used, addLast and deleteNode.
 * The most recently used cache line will be located right before the tail node, and the least recently used cache line will be located right after the head node. 
 * After reading a line from the trace file, the program will check if this line is a valid input. 
 * If so, the program will find the correct index of the Doubly-LinkedList array and check the size of this list. 
 * Here, there are three options, a cold miss, and a capacity miss and a cache hit.
 * 
 * Cold miss:
 * If there is currently no other node between the head and the tail, a cold miss happens, 
 * and a new node will be added in front of the tail node to indicate that this cache is the most recently used.
 * The tag of the cache line will be calculated, and the dirty bit will be set according to the operation type (Load or Store)
 * 
 * Capacity miss:
 * If there are nodes between the head and the tail, 
 * then the program will loop through all these nodes and check if the tag of one of these nodes matches the current tag. 
 * If there is no match, a capacity miss happens.
 * There are two decisions here:
 *   1.  If the current number of the node is smaller than the input E, no eviction happens. 
 *       A new node will be created, and its tag and the dirty bit will be adjusted 
 *       and finally inserted at the end of the Doubly Linked List right before the tail node indicates that it is the most recently used. 
 * 
 *   2.  If the current number of the node is equal to the input E, then eviction should take place.
 *       The Least Recently Used cache line will be located at the front of the Doubly LinkedList right after the head node.  
 *       This node will be deleted, and its memory will be freed, indicates a eviction happened. 
 *       Then a new node will be created, and its tag and the dirty bit will be adjusted 
 *       and finally inserted at the end of the Doubly Linked List right before the tail node indicates that it is the most recently used.
 * 
 * Cache Hit:
 * If there are nodes between the head and the tail,
 * then the program will loop through all these nodes and check if the tag of one of these nodes matches the current tag.
 * If there is a match, a cache hit happens.
 * Then this node will be deleted first and then reinserted at the end of the Doubly LinkedList right before the tail node,
 * indicating that it is the most recently used. 
 * 
 * The reason why a doubly linked list is used is that it can efficiently retrieve the Least Recently Used cache line and update the most recently used cache line. 
 * From the end to the front of the list, the usage time of the cache lines decreases, and the Least Recently Used cache line will always be right after the head node. 
 * The decision to use the head and tail node as placeholder can perfectly avoid some edge cases such as Nullptr and an empty list. 
*/

#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define FILENAMELENGTH 100
#define LINELENGTH 64
/**
 * This is the node structure. Each node has four fields, the reference of its next node, the reference of its previous node, the tag number, and the dirty bit. 
 * The node structure will form the Doubly Linked List, and the position inside the list indicates the usage time. 
*/
typedef struct DLLNode {
    struct DLLNode* next;
    struct DLLNode* prev;
    unsigned long tag;
    unsigned int dirty;
} Node;
/**
 * This is the DoublyLinkedList structure. It has two fields, the head reference, and the tail reference. 
 * The head and tail nodes are just placeholders that have no meaningful data inside.
 * An array of this structure will be used to simulate the cache operation. 
*/
typedef struct DoublyLinkedList {
    Node* head;
    Node* tail;
} DLL;

void getArguments(int argc, char ** argv);
void printMessage(void);
int mainProcess(char *afile);
int initializeCache(unsigned long setNum);
void addLast(unsigned long index, Node* n);
void deleteNode(Node* n);
int cacheOperation(char op, unsigned long address, unsigned long block);

/**
 * Indicates if the user gives invalid command line argument. 1 if argument is invalid. 
*/
int quit = 0;
/**
 * 
*/
int verbose;
/**
 * Indicates the user input set bit and used for cache simulation. 
*/
int setBit = -1;
/**
 * Indicates the user input block bit and used for cache simulation. 
*/
int blockBit = -1;
/**
 * Indicates the user input cache line per set and used for cache simulation. 
*/
int linesPerSet = -1;
/**
 * Indicates the user input trace file name and used for getting operation inputs.
*/
char fileName[FILENAMELENGTH] = "";
/**
 * The array of Doubly Linked List. Each index contains a Doubly Linked List which simulates a cache set. 
*/
DLL* cache;
/**
 * The structure for keeping track of the number of the cache hit, 
 * cache miss, cache eviction, dirty bytes evicted, and dirty bytes remained. Used for function printSummary.
 * The source codes of this struct are in given "cachelab.h" file.
*/
csim_stats_t myStats = {
    .hits = 0,
    .misses = 0,
    .evictions = 0,
    .dirty_bytes = 0,
    .dirty_evictions = 0
};
/**
 * This function takes the set number as a parameter and allocates memory for the array of Doubly LinkedList. 
 * It then allocates memory for the head node and tail node for each Doubly Linked List.
 * If any malloc fails, it returns 1, indicating that an error occurred. Otherwise, return 0.  
*/
int initializeCache(unsigned long setNum) {
    cache = malloc(setNum * sizeof(*cache));
    for (unsigned long i = 0; i < setNum; i++) {
        cache[i].head = malloc(sizeof(Node));
        cache[i].tail = malloc(sizeof(Node));
        if (cache[i].head == NULL || cache[i].tail == NULL) {
            return 1;
        }
        cache[i].head->next = cache[i].tail;
        cache[i].tail->prev = cache[i].head;
    }
     return 0;    
}
/**
 * This function takes the set number as a parameter and clears the memory allocated to each node of each list inside the array. 
*/
void cleanUp(unsigned long setNum) {
    for (unsigned long i = 0; i < setNum; i++) {
        Node* curr = cache[i].head;
        while (curr != NULL) {
            Node* temp = curr->next;
            free(curr);
            curr = temp;
        }
    }
    free(cache);
}
/**
 * This function takes two parameters, an integer index, and a node n. This function will retrieve the correct Doubly LinkedList inside the array 
 * and insert a node at the end of the list right before the tail node, indicating that the inserted node is the most recently used.  
 * This function is called for every cache operation because the most recently used cache will change everytime. 
*/
void addLast(unsigned long index, Node* n) {
    cache[index].tail->prev->next = n;
    n->prev = cache[index].tail->prev;
    n->next = cache[index].tail;
    cache[index].tail->prev = n;
}
/**
 * This function takes a node reference as the parameter. It deletes this node from the linked list by resetting its previous and next node's pointers. 
 * This function is called whenever an eviction happens. 
*/
void deleteNode(Node* n) {
    n->prev->next = n->next;
    n->next->prev = n->prev;
}
/**
 * This function is the function that simulates the cache operation.
 * It takes three parameters, a char indicating the operation type, an unsigned long indicating the address, 
 * and an unsigned long indicating the number of bytes visited.
 * 
 * These parameters are all parsed from a line from the input trace file.
 * It first calculates the tag from the address. 
 * The tag is part of the address except for the set index and the block offset. 
 * So the tag is calculated by right shifting the address by (s + b) bit.
 * Then, the set index is calculated. 
 * Because the set index is located in the middle of the address between the tag and the block offset, 
 * so a mask is first generated by shifting 1 to the left by the number of set bits and then minus 1. 
 * This mask will have its least significant s bits set to 1. 
 * Then the address is right-shifted by the number of block bits so that the least s significant bits are the set index. 
 * Finally, a binary "and" operation is performed to acquire the set index. 
 * 
 * At this point, all the required information is collected, and the cache simulation starts. 
 * It first traverses the Doubly LinkedList located at the set index of the array to acquire how many nodes are there now. 
 *  If there is no node between the head and the tail node, this cache set is cold, and a cold miss happens.
 * (Please see the start of this file for the definition of cold miss and how the simulator will work in this circumstance).
 *  The number of misses will be incremented, and a node will be created and inserted into this list.
 *  Finally, this function will return to its caller. 
 * 
 * If the cache set is not cold, then the program will traverse all the existing nodes between the head and the tail node to see if there is a tag match. 
 * If so, a cache hit happens. 
 * (Please see the start of this file for the definition of cache hit and how the simulator will work in this circumstance).
 * Ths number of hits will be incremented, and the node representing this cache line will be relocated inside the list. 
 * 
 * If there is no cache hit, then a cache miss happens. 
 * (Please see the start of this file for the definition of cache hit and how the simulator will work in this circumstance.)
 * The number of misses will be incremented, and then the number of nodes between the head and the tail node will be compared to user input E to 
 * determine if evictions take place and adjust the number of eviction accordingly. 
 * 
 * Since this function will also possibly call malloc to create new nodes, malloc may fail.
 * So this function will return 1 if any malloc calls fail and return 0 if no error occurs.  
*/
int cacheOperation(char op, unsigned long address, unsigned long block) {
    /* Calculate required information: tag, set index, and bytes visited.*/
    unsigned long thisTag = address >> (setBit + blockBit);
    unsigned long thisSetNum = (address >> blockBit) & ((1L << setBit) - 1L);
    unsigned long blockByteNum = 1L << blockBit;
    int size = 0;
    /* Check how many nodes are between the head and the tail now*/
    Node* sizeCheck = cache[thisSetNum].head->next;
    while (sizeCheck != cache[thisSetNum].tail) {
        size++;
        sizeCheck = sizeCheck->next;
    }
    /* No nodes between the head and tail, a cold cache line, a cold miss must take place*/
    if (size == 0) {
        myStats.misses = myStats.misses + 1;
        Node* aNode = malloc(sizeof(Node));
        if (aNode == NULL) {
            return 1;
        }
        aNode->dirty = 0;
        if (op == 'S') {
            aNode->dirty = 1;
            myStats.dirty_bytes = myStats.dirty_bytes + blockByteNum;
        }
        aNode->tag = thisTag;
        addLast(thisSetNum, aNode);
        if (verbose == 1) {
            printf("A Cold Miss\n");
        }
        return 0;
    }
    /* Traverse all the nodes to see if there is a tag match*/
    bool hit = false;
    Node* curr = cache[thisSetNum].head->next;
    while (curr != cache[thisSetNum].tail) {
        if (curr->tag == thisTag) {
            hit = true;
            break;
        }
        curr = curr->next;
    }
    /* If there is a tag match, a cache hit must take place.*/
    if (hit) {
        myStats.hits = myStats.hits + 1;
        /* If the operation is "Store", and the hit cache line is not dirty, set the dirty bit of this cache line and increment dirty bytes existing*/
        if (op == 'S' && curr->dirty == 0) {
            curr->dirty = 1;
            myStats.dirty_bytes = myStats.dirty_bytes + blockByteNum;
        }
        /* Relocate this node because it is used recently*/
        deleteNode(curr);
        addLast(thisSetNum, curr);
        if (verbose == 1) {
            printf("Hit!\n");
        }

    /* No tag match, a cache miss must take place.*/
    } else {
        myStats.misses = myStats.misses + 1;
        Node* aNode = malloc(sizeof(Node));
        if (aNode == NULL) {
            return 1;
        }
        aNode->dirty = 0;
        aNode->tag = thisTag;
        /* If the operation is "Store", set the dirty bit and increment dirty bytes existing*/
        if (op == 'S') {
            aNode->dirty = 1;
            myStats.dirty_bytes = myStats.dirty_bytes + blockByteNum;
        }
        /* Check the number of node inside the list between the head and the tail, if they are equal right now, need eviction*/
        if (size == linesPerSet) {
            myStats.evictions = myStats.evictions + 1;
            Node* toDelete = cache[thisSetNum].head->next;
            /* Check if the evicted cache line is dirty, if so, increment dirty byte evicted and decrement dirty byte existing*/
            if (toDelete->dirty == 1) {
                myStats.dirty_evictions = myStats.dirty_evictions + blockByteNum;
                myStats.dirty_bytes = myStats.dirty_bytes - blockByteNum;
            }
            /* Add this node to the end of the list indicating it's the most recently used*/
            deleteNode(toDelete);
            free(toDelete);
            addLast(thisSetNum, aNode);
            if (verbose == 1) {
                printf("A Cache Miss and Eviction\n");
            }

        /* If the number of existing node is smaller than E, no eviction take place. */    
        } else {
            addLast(thisSetNum, aNode);
            if (verbose == 1) {
                printf("A Cache Miss\n");
            }
        }
    }
    return 0;
}
/**
 * The main function. 
 * It first calls "getArguments" to acquire the set bit, lines per set, and block bits. 
 * If any of these are invalid, it tells the user that the input is invalid, and return 1 indicates that an error occurred. 
 * 
 * It then calls "initializeCache" to allocate memory to the Doubly Linked List array, 
 * and if any memory allocation fails, it returns 1, indicating that an error occurred. 
 * 
 * It then calls the function "main process" to parse the trace file and simulate the cache. 
 * If any line of the trace file is invalid or any memory allocation failed during the cache simulation, it returns 1 indicating that an error occurred. 
 * 
 * After the simulation is done, it clear the memory allocated for the Doubly Linked List array and all the nodes inside the list by calling the function "cleanUp."
 * 
 * Finally, it calls the function printSummary to print out the number of the cache hit, cache miss, cache eviction, dirty bytes existing, and dirty bytes evicted. 
 * The source code of "printSummary" is inside provided "cachelab.h" file. 
*/
int main(int argc, char **argv) {
    getArguments(argc, argv);
    if (quit == 1 || setBit < 0 || blockBit < 0 || linesPerSet <= 0 || fileName[0] == 0 || setBit + blockBit >= 64) {
        printf("Invalid Argument!\n");
        printMessage();
        return 1;
    }
    unsigned long setNum = 1 << setBit;
    if (initializeCache(setNum) == 1) {
        return 1;
    }
    if (mainProcess(fileName) == 1) {
        return 1;
    };
    cleanUp(setNum);
    printSummary(&myStats);
    return 0;
}
/**
 * This function processes the user input command line arguments. It checks if verbose mode is enabled, 
 * if a helper usage message needs to be printed, and the value of the set bit, lines per set, and block bits. 
 * If the input is invalid, it sets the global variable "quit" to 1, and this global variable will determine if the program should be aborted in the main function. 
 * The main structure of the function is acquired from the recitation slides of Spring 2022. The site is https://www.cs.cmu.edu/afs/cs/academic/class/15213-s22/www/recitations/rec06_slides.pdf
*/
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
/**
 * This function prints out the usage instruction of this simulator if the user inputs invalid command or typed -h in the command. 
*/
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
/**
 * This function takes the input of a string which should be the user input trace file name. 
 * It then parses each line of the trace file to acquire required information such as operation type, address, and visited byte number.
 * For each line of the trace file, this function will check the validity of this line and store the operation type, address, and byte number inside three local variables. 
 * Then these three variables will be used as parameters to call the function "cacheOperation."
 * Since the lines of the trace files may be invalid or the input file may not exist or failed to open, 
 * this function will return 1, indicating an error occurred. Otherwise, it will process all lines of the trace file and finally return 0.
*/
int mainProcess(char *afile) {
    FILE *inputTrace = fopen(afile, "r");
    if (!inputTrace) {
        printf("Failed open trace file!\n");
        return 1;
    }
    char lineBuffer[LINELENGTH];
    char type;
    char* left;
    char* end;
    unsigned long address;
    unsigned long block;
    while (fgets(lineBuffer, LINELENGTH, inputTrace)) {
        type = lineBuffer[0];
        if (type != 'S' && type != 'L') {
            return 1;
        }
        errno = 0;
        address = strtoul(lineBuffer + 2, &left, 16);
        if ((address == 0 && errno == 1) || *left != ',') {
            return 1;
        }
        errno = 0;
        block = strtoul(left + 1, &end, 10);
        if (block == 0 && errno == 1) {
            return 1;
        }
        if (verbose == 1) {
            printf("%c %lx,%ld ", type, address, block);
        }
        if (cacheOperation(type, address, block) == 1) {
            return 1;
        }
    }
    fclose(inputTrace);
    return 0;
}
