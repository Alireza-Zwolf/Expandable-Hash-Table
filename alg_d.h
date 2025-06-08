#pragma once
#include "util.h"
#include <atomic>
#include <cmath>
#include <cassert>
using namespace std;

#define EXPANSION_RATE 7
#define TABLE_PARTITION_SIZE 4096
const double EXPANSION_CAPACITY_TRIGGER = 0.85;

// #define EXPANSION_RATE 7
// #define TABLE_PARTITION_SIZE 4096
// const double EXPANSION_CAPACITY_TRIGGER = 0.9;



/*
./benchmark.out -a D -sT 10000 -m 15000 -sR 1000000 -t 16

LD_PRELOAD=./libjemalloc.so perf record -e LLC-load-misses ./benchmark.out -a D -sT 10000 -m 6000 -sR 1000000 -t 8

LD_PRELOAD=./libjemalloc.so perf record -e cycles ./benchmark.out -a D -sT 10000 -m 6000 -sR 1000000 -t 8


gdb ./benchmark.out -ex="set args -a D -sT 10000 -m 15000 -sR 1000000 -t 8"

./benchmark.out -a D -sT 10000 -m 10000 -sR 1000000 -t 16
*/


/*
Scenarios:
*/

struct PaddedInt64Atomic {
    // Note that this is not 64 bytes int!
    // Padding makes the program so much slower!
    std::atomic<int> v;
};


class AlgorithmD {
private:
    enum {
        MARKED_MASK = (int) 0x80000000,     // most significant bit of a 32-bit key
        TOMBSTONE = (int) 0x7FFFFFFF,       // largest value that doesn't use bit MARKED_MASK
        EMPTY = (int) 0
    }; // with these definitions, the largest "real" key we allow in the table is 0x7FFFFFFE, and the smallest is 1 !!

    char padding2[PADDING_BYTES];
    int numThreads;
    int initCapacity; 
    int tablePartitionSize = 4096;
    // more fields (pad as appropriate)
    char padding3[PADDING_BYTES];

    struct table {
        // data types
        // std::atomic<int> * data;
        // std::atomic<int> * old;
        // int capacity;
        // int oldCapacity;
        // counter * approxSize;
        // std::atomic<int> chunksClaimed;
        // std::atomic<int> chunksDone;

        
        alignas(PADDING_BYTES) PaddedInt64Atomic *data;         // Pointer to data array

        alignas(PADDING_BYTES) PaddedInt64Atomic *old;          // Pointer to old table (during expansion)

        alignas(PADDING_BYTES) int capacity;                   // Current table capacity
        char padding2[PADDING_BYTES - sizeof(int)];

        alignas(PADDING_BYTES) int oldCapacity;                // Old table capacity (before expansion)
        char padding3[PADDING_BYTES - sizeof(int)];

        alignas(PADDING_BYTES) counter *approxSize;            // Approximate size counter
        char padding4[PADDING_BYTES - sizeof(counter*)];

        alignas(PADDING_BYTES) counter *tombStoneSize;            // Approximate size counter
        char padding7[PADDING_BYTES - sizeof(counter*)];

        alignas(PADDING_BYTES) std::atomic<int> chunksClaimed; // Number of chunks claimed in migration
        char padding5[PADDING_BYTES - sizeof(std::atomic<int>)];

        alignas(PADDING_BYTES) std::atomic<int> chunksDone;    // Number of completed migrations
        char padding6[PADDING_BYTES - sizeof(std::atomic<int>)];

        // Constructor
        table(int init_capacity, PaddedInt64Atomic* oldTableData)
        : data(new PaddedInt64Atomic[init_capacity]),
          old(oldTableData),
          capacity(init_capacity), 
          oldCapacity(0),
          chunksClaimed(0), 
          chunksDone(0) 
        {
            // Initialize all data elements to EMPTY
            for (int i = 0; i < capacity; i++) {
                data[i].v.store(EMPTY, std::memory_order_relaxed);  // Initialize data to EMPTY
            }

            // remember to intiate counter here too!
        }

        // Copy constructor
        table(const table& oldTable)
        {
            // This copy constructor is for making new tables during expansion
            // capacity = (oldTable.approxSize) * EXPANSION_RATE;
            oldCapacity = oldTable.capacity;
            capacity = std::max((int)(oldTable.approxSize->get() - oldTable.tombStoneSize->get()) * EXPANSION_RATE , oldCapacity);
            // capacity = (oldTable.approxSize->get() - oldTable.tombStoneSize->get() * EXPANSION_RATE);
            data = new PaddedInt64Atomic[capacity];
            old = oldTable.data;
            chunksClaimed = 0;
            chunksDone = 0;
            // approxSize = new counter(numThreads);

            for (int i = 0; i < capacity; i++) {
                data[i].v.store(EMPTY, std::memory_order_relaxed);
            }
        }

        // Destructor
        ~table() {
            delete[] data;         // Clean up the data array
            // delete[] old;
            delete approxSize;     // Clean up the approxSize counter
            delete tombStoneSize;  // Clean up the tombStoneSize counter
        }
    };

    char padding0[64];
    atomic<table *> currentTable;
    char padding1[64];
    
    int migrationCount = 0; 
    
    bool expandAsNeeded(const int tid, table * t, int i);
    void helpExpansion(const int tid, table * t);
    void startExpansion(const int tid, table * t);
    void migrate(const int tid, table * t, int myChunk);
    
    
    
public:
    AlgorithmD(const int _numThreads, const int _capacity);
    ~AlgorithmD();
    bool insertIfAbsent(const int tid, const int & key, bool ExpansionMode);
    bool erase(const int tid, const int & key);
    AlgorithmD::table* createNewTableStruct(const int tid);
    long getSumOfKeys();
    void printDebuggingDetails(); 
    void printTable(PaddedInt64Atomic* data, int capacity);

};

/**
 * constructor: initialize the hash table's internals
 * 
 * @param _numThreads maximum number of threads that will ever use the hash table (i.e., at least tid+1, where tid is the largest thread ID passed to any function of this class)
 * @param _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
 */
AlgorithmD::AlgorithmD(const int _numThreads, const int _capacity)
: numThreads(_numThreads), initCapacity(_capacity) {
    // Check how to initialize the data[indexes] of a table to EMPTY here in the constructor
    table* initialTable = new table(initCapacity, nullptr);
    initialTable->approxSize = new counter(numThreads);      // Initialize the counter for approximate size of inserts
    initialTable->tombStoneSize = new counter(numThreads);   // Initialize the counter for approximate size of tombstones

    // Setting partion size the same as the number of threads
    int tablePartitionSize = _capacity / (_numThreads);

    // Set currentTable to the newly created table
    currentTable.store(initialTable, std::memory_order_acquire);
}

// destructor: clean up any allocated memory, etc.
AlgorithmD::~AlgorithmD() {
}

bool AlgorithmD::expandAsNeeded(const int tid, table * t, int i) {
    
    helpExpansion(tid, t);

    // printf("Approx Size: %ld, Tombstone Size: %ld, Capacity: %d\n", t->approxSize->get(), t->tombStoneSize->get(), t->capacity);


    if (t->approxSize->get() + t->tombStoneSize->get() >= t->capacity * EXPANSION_CAPACITY_TRIGGER
        // || (i > 10 && t->approxSize->getAccurate() >= triggerPoint)
    ){
    // printf("Approx Size: %ld, Tombstone Size: %ld, Capacity: %d\n", t->approxSize->get(), t->tombStoneSize->get(), t->capacity);
        // printf("A - %ld \n", t->approxSize->get());
        startExpansion(tid, t);
        return true;
    }
    // else if (i > 10 && t->approxSize->getAccurate() + t->tombStoneSize->getAccurate() >= t->capacity * EXPANSION_CAPACITY_TRIGGER){
    //     startExpansion(tid, t);
    //     return true;
    // }

    

    
    return false;
}

void AlgorithmD::helpExpansion(const int tid, table * t) {

    int totalOldChunks = ceil((float)t->oldCapacity / (tablePartitionSize));
    // printf("Total Old Chunks: %d\n", totalOldChunks);
    // printf("Old Capacity: %d\n", t->oldCapacity);

    // printf(" Migration TID=%d\n",tid);
    bool flag = false;
    
    while (t->chunksClaimed < totalOldChunks) {
        flag = true;
        // printf("helpExpansion's inside touched\n");
        int myChunk = t->chunksClaimed.fetch_add(1) + 1;
        // printf("Migration TID=%d, myChunk=%d\n",tid ,myChunk);

        // printf("ChunksClaimed Org: %d\n", t->chunksClaimed.load());
        // printf("MyChunk: %d", myChunk);
        if (myChunk <= totalOldChunks) {
            // printf("Migrate touched - chunk #%d\n", myChunk);
            migrate(tid, t, myChunk);
            t->chunksDone.fetch_add(1);
        }
        // printf("Claimed:%d, ChunksDone:%d", t->chunksClaimed.load(), t->chunksDone.load());
    }
    while (t->chunksDone < totalOldChunks){
        // printf("TID: %d \n", tid);
    }
    // if (flag)
    //     printf("Migration Done, New Table Size: %d, Old Table Size: %d\n", t->capacity, t->oldCapacity);
    // printf("Old table: TID: %d \n", tid);
    // printTable(t->old, t->oldCapacity);
}

void AlgorithmD::startExpansion(const int tid, table *t) {
    // printf("Touched\n");
    if (currentTable == t){
        // printf("Touched 2\n");
        table* t_new = new table(*t); 
        t_new->approxSize = new counter(numThreads);
        t_new->tombStoneSize = new counter(numThreads);


        if (currentTable.compare_exchange_strong(t, t_new)){
            // delete t_new;
            tablePartitionSize = t_new->capacity / (numThreads);
        }
        else{
            t_new->~table();
        }
    }
    helpExpansion(tid, currentTable);
}

void AlgorithmD::migrate(const int tid, table * t, int myChunk) {
    int start = ((myChunk - 1) * tablePartitionSize);
    int end = min(start + tablePartitionSize, t->oldCapacity); 

    bool migrated = false;  
    // printf("Migrating Chunk: %d, TID: %d\n", myChunk, tid);
    for (int i = start; i < end; i++) {
        migrated = true;
        // printf("TID:%d, Migrating index number: %d", tid, i);
        int key = t->old[i].v.load();

        if (key == TOMBSTONE)
            continue;

        if (!(t->old[i].v.compare_exchange_strong(key, key | MARKED_MASK))){
            i--;
            // printf("Noooooo!\n");
            continue;
        }
        
        if (key != EMPTY && key != TOMBSTONE) {  // Only migrate valid keys
            // Insert into the new table (disable expansion)
            bool MigrateDone = insertIfAbsent(tid, key, true);
            assert(MigrateDone);
        }
    }

    // migrationCount += 1;

    // if (migrated && migrationCount == 1){
    //     printf("Old table: TID: %d \n", tid);
    //     printTable(t->old, t->oldCapacity);
    // //     // printf("\n\nNew Table:");
    // //     // printf(t->data, t->capacity);
    // }
}

bool AlgorithmD::insertIfAbsent(const int tid, const int& key, bool ExpansionMode=false) {
    table* t = currentTable;
    uint32_t h = murmur3(key);

    for (int i = 0; i < t->capacity; i++) {
        if (!ExpansionMode && expandAsNeeded(tid, t, i)) 
            return insertIfAbsent(tid, key);

        int index = (h + i) % t->capacity;
        int found = t->data[index].v.load(); 

        if (!ExpansionMode && (found & MARKED_MASK)) {
            // printf("Marked Cell cathed in Insert\n");
            return insertIfAbsent(tid, key);
        } 
        else if (found == key) {
            return false;
        } 
        else if (found == EMPTY) {
            int expected = EMPTY;
            if (t->data[index].v.compare_exchange_strong(expected, key)) {
                t->approxSize->inc(tid);
                // printf("inc\n");
                return true;
            } else {
                // printf("Changed?!\n");
                found = t->data[index].v;
                if (!ExpansionMode && (found & MARKED_MASK)) {
                    // printf("Edge case catched.\n");
                    return insertIfAbsent(tid, key);
                }
                else if (found == key)
                    return false;
                // printf("Oops!");
            }
        }
    }
    return false;
}

bool AlgorithmD::erase(const int tid, const int& key) {
    table* t = currentTable.load();
    uint32_t h = murmur3(key);

    for (int i = 0; i < t->capacity; i++) {
        if (expandAsNeeded(tid, t, i)) 
            return erase(tid, key);

        int index = (h + i) % t->capacity;
        // int found = t->data[index].v.load(std::memory_order_relaxed);
        int found = t->data[index].v;


        // printf("Found: %d, MASK:%d\n", found, MARKED_MASK);
        
        if (found & MARKED_MASK){
            // printf("Marked Cell cathed in Insert");
            return erase(tid, key);
        }

        if (found == EMPTY){
            return false;
        }

        // printf("A");
        if (found == key) {
            // printf("B");
            int expected = key;
            if (t->data[index].v.compare_exchange_strong(expected, TOMBSTONE)) {
                t->tombStoneSize->inc(tid);
                // printf("C!!\n");
                return true;
            } 
            else {
                found = t->data[index].v.load();
                if (found & MARKED_MASK)
                    return erase(tid, key);
                else if (found == TOMBSTONE || found == EMPTY)
                    return false;
            }
        }

    }
    return false;
}

// semantics: return the sum of all KEYS in the set
int64_t AlgorithmD::getSumOfKeys() {

    table* t = currentTable.load();  // Get the current table
    int64_t sum = 0;

    for (int i = 0; i < t->capacity; i++) {
        int key = t->data[i].v.load(std::memory_order_relaxed);  // Read value
        if (key != EMPTY && key != TOMBSTONE)   // Only sum up valid keys
            sum += key;
    }
    
    return sum;
}

// print any debugging details you want at the end of a trial in this function
void AlgorithmD::printDebuggingDetails() {

}


void AlgorithmD::printTable(PaddedInt64Atomic* data, int capacity){
    for (int i = 0; i < capacity; i++){
        int dataPoint = data[i].v.load();
        if(dataPoint & MARKED_MASK)
            printf("# | ");
        else if (dataPoint == TOMBSTONE)
            printf("T | ");
        else if (dataPoint == EMPTY)
            printf("E | ");
        else
            printf("%d | ", dataPoint);
        // printf("%d-", data[i].v.load());
        if(i % 20 == 0)
            printf("\n");
    }
    printf("\n\n");
    
}


