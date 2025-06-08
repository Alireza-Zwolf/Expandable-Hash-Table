#pragma once
#include "util.h"
#include <atomic>
#include <mutex>
#include <vector>
using namespace std;

class AlgorithmB {
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

    AlgorithmB(const int _numThreads, const int _capacity);
    ~AlgorithmB();
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
AlgorithmB::AlgorithmB(const int _numThreads, const int _capacity)
: numThreads(_numThreads), capacity(_capacity), table(_capacity), mutexes(_capacity) {
    for (int i = 0; i < capacity; i++){
        table[i] = EMPTY;
    }
}

// destructor: clean up any allocated memory, etc.
AlgorithmB::~AlgorithmB() {

}

// semantics: try to insert key. return true if successful (if key doesn't already exist), and false otherwise
bool AlgorithmB::insertIfAbsent(const int tid, const int & key) {
    uint32_t h = murmur3(key);          
    // Such a big deal in performance! If we put the h type as uint64, the performance degrades ~ 15%
    volatile bool flag = false;
    for (int i = 0; i < capacity; i++){
        int index = (h+i) % capacity;
        int found = table[index];
        flag = false;
        
        if (found == EMPTY){
            mutexes[index].lock();
            if (found == table[index]){
                table[index] = key;
                flag = true;
            }
            mutexes[index].unlock();
            if (flag == true)
                return true;
        }
        else if (found == key){
            return false;            
        }
    }
    return false;
}

// semantics: try to erase key. return true if successful, and false otherwise
bool AlgorithmB::erase(const int tid, const int & key) {
    uint32_t h = murmur3(key);
    volatile bool flag = false;

    for (int i = 0; i < capacity; i++){
        int index = (h+i) % capacity;
        int found = table[index];
        flag = false;

        if (found == key){
            mutexes[index].lock();
            if (found == table[index]){
                table[index] = TOMBSTONE;
                flag = true;
            }
            mutexes[index].unlock();
            if (flag == true)
                return true;      
        }
        else if (found == EMPTY){
            return false;
        }
    }
    return false;
}


// semantics: return the sum of all KEYS in the set
int64_t AlgorithmB::getSumOfKeys() {
    int64_t sum = 0;
    for (int i = 0; i < capacity; i++) {
        // std::lock_guard<std::mutex> lock(mutexes[i]);  // Lock the mutex before accessing the table
        if (table[i] != EMPTY && table[i] != TOMBSTONE) {
            sum += table[i];  
        }
    }    
    return sum;
}


// print any debugging details you want at the end of a trial in this function
void AlgorithmB::printDebuggingDetails() {
    
}

