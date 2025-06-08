#pragma once
#include "util.h"
#include <atomic>
using namespace std;

class AlgorithmC {
public:
    static constexpr int TOMBSTONE = -1;
    int EMPTY = -2;

    char padding0[PADDING_BYTES];
    const int numThreads;
    int capacity;
    char padding2[PADDING_BYTES];

    std::vector<std::atomic<int>> table;
    char padding3[PADDING_BYTES];

    AlgorithmC(const int _numThreads, const int _capacity);
    ~AlgorithmC();
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
AlgorithmC::AlgorithmC(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity), table(_capacity) {
    for (int i = 0; i < capacity; i++){
        table[i].store(EMPTY, std::memory_order_relaxed);
    }
}

// destructor: clean up any allocated memory, etc.
AlgorithmC::~AlgorithmC() {
    
}


// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmC::insertIfAbsent(const int tid, const int & key) {
    uint32_t h = murmur3(key);

    for (int i = 0; i < capacity; i++){
        int index = (h+i) % capacity;
        int found = table[index];

        if (found == key){
            return false;
        }
        else if (found == EMPTY){
            int emptyTemp = EMPTY; // compare and exchange needs the arguments to be int, not const expr
            if(table[index].compare_exchange_strong(emptyTemp, key)){
                return true;
            }
            else if (table[index] == key)
                return false;
        }
    }
    return false;
}

// // semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmC::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key);

    for (int i = 0; i < capacity; i++){
        int index = (h+i) % capacity;
        int found = table[index];

        if(found == EMPTY){
            return false;
        }
        else if (found == key){
            int tempKey = key;
            return table[index].compare_exchange_strong(tempKey, TOMBSTONE);
        }
    }
    return false;
}


// // semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
// bool AlgorithmC::insertIfAbsent(const int tid, const int & key) {
//     uint32_t h = murmur3(key);

//     for (int i = 0; i < capacity; i++){
//         int index = (h+i) % capacity;
//         int found = table[index];

//         // CAS to insert data
//         int emptyTemp = EMPTY; // compare and exchange needs the arguments to be int, not const expr
//         if (table[index].compare_exchange_strong(emptyTemp, key)){
//             return true;
//         }

//         // If key is available, return false
//         else if(found == key)
//             return false;
//     }
//     return false;
// }


// // semantics: try to erase key. return true if successful, and false otherwise
// bool AlgorithmC::erase(const int tid, const int & key) {
//     uint32_t h = murmur3(key);

//     for (int i = 0; i < capacity; i++){
//         int index = (h+i) % capacity;
//         int found = table[index];

//         // CAS to erase
//         int tempKey = key;
//         if (table[index].compare_exchange_strong(tempKey, TOMBSTONE))
//             return true;

//         // Break if the cell is empty
//         else if(found == EMPTY){
//             return false;
//         }   
//     }
//     return false;
// }


// Get sum of all keys (not lock-free, but reads safely)
int64_t AlgorithmC::getSumOfKeys() {
    int64_t sum = 0;
    for (int i = 0; i < capacity; i++) {
        int val = table[i].load(std::memory_order_relaxed);
        if (val != EMPTY && val != TOMBSTONE) {
            sum += val;  
        }
    }    
    return sum;
}


// print any debugging details you want at the end of a trial in this function
void AlgorithmC::printDebuggingDetails() {
    
}
