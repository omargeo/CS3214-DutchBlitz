#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "list.h"

struct fair_lock {
    // pthread_cond_t fairCond;
    bool isLocked;
    pthread_mutex_t lock;
    struct list listofThreads;
};

struct fair_cond{
    //list that holds the threads
    struct list listofThreads;
    pthread_mutex_t lock; // to protect the list
    struct fair_lock* fairlock;

};

struct fairwaiter {
    struct list_elem elem;
    pthread_cond_t condVar;
};

// create a new fair lock
struct fair_lock * fair_lock_new();

// lock this fair lock
void fair_lock(struct fair_lock *lock);

// unlock this fair lock
void fair_unlock(struct fair_lock *lock);

// wait on this fair condition variable
void fair_cond_wait(struct fair_cond *cond);

// wake up all threads waiting on that fair condition variable
void fair_cond_broadcast(struct fair_cond *cond);

// create a fair condition variable tied to the given fair lock
struct fair_cond *fair_cond_new(struct fair_lock *lock);
