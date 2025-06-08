#pragma once
#include "util.h"
#include <atomic>
#include <mutex>
#include <vector>
using namespace std;



/*
Latest thoughts about the error:
- Maybe the error when increasing the size of the table (>10000) comes from touching an invalid memory address.

*/


class AlgorithmA {
public:
    static constexpr int TOMBSTONE = -1;
    static constexpr int EMPTY = -2;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];

    std::vector<int> table;
    char padding3[PADDING_BYTES];
    std::vector<mutex> mutexes;

    AlgorithmA(const int _numThreads, const int _capacity);
    ~AlgorithmA();
    bool insertIfAbsent(const int tid, const int & key);
    bool erase(const int tid, const int & key);
    long getSumOfKeys();
    void printDebuggingDetails(); 
};

/**
 * constructor: initialize the hash table's internals
 * 
 * @param _numThreads maximum number of threads that will ever use the hash table (i.e., at least tid+1, where tid is the largest thread ID passed to any function of this class)
 * @param _capacity is the INITIAL size of the hash table (maximum number of elements it can contain WITHOUT expansion)
 */
AlgorithmA::AlgorithmA(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity), table(_capacity), mutexes(_capacity) {
    for (int i = 0; i < capacity; i++){
        table[i] = EMPTY;
    }
}

// destructor: clean up any allocated memory, etc.
AlgorithmA::~AlgorithmA() {

}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmA::insertIfAbsent(const int tid, const int & key) {
    uint32_t h = murmur3(key);

    for (int i = 0; i < capacity; i++){
        int index = (h+i) % capacity;
        // printf("Lock %d acquired by thread %d\n", index, tid);
        mutexes[index].lock();
        int found = table[index];
        if (found == key){
            mutexes[index].unlock();
            // printf("Lock %d released by thread %d\n", index, tid);
            return false;            
        }
        else if (found == EMPTY){
            table[index] = key;
            mutexes[index].unlock();
            // printf("Lock %d released by thread %d\n", index, tid);
            return true;
        }
        // The cell is full, proceed to its next cell
        mutexes[index].unlock();
        // printf("Lock %d released by thread %d\n", index, tid);
    }

    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmA::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key);

    for (int i = 0; i < capacity; i++){
        int index = (h+i) % capacity;
        mutexes[index].lock();
        // printf("Lock %d acquired by thread %d\n", index, tid);
        int found = table[index];
        if (found == EMPTY){
            mutexes[index].unlock();
            // printf("Lock %d released by thread %d\n", index, tid);
            return false;
        }
        else if (found == key){
            table[index] = TOMBSTONE;
            mutexes[index].unlock();
            // printf("Lock %d released by thread %d\n", index, tid);
            return true;
        }
        mutexes[index].unlock();
        }
        
    return false;
}


// semantics: return the sum of all KEYS in the set
int64_t AlgorithmA::getSumOfKeys() {
    int64_t sum = 0;

    // Lock-based reading on the table to safely access shared data
    for (int i = 0; i < capacity; i++) {
        // std::lock_guard<std::mutex> lock(mutexes[i]);  // Lock the mutex before accessing the table
        mutexes[i].lock();
        if (table[i] != EMPTY && table[i] != TOMBSTONE) {
            sum += table[i];  // Add the key to the sum if it's not empty or tombstone
        }
        mutexes[i].unlock();
    }    

    return sum;
}


// print any debugging details you want at the end of a trial in this function
void AlgorithmA::printDebuggingDetails() {
    
}
