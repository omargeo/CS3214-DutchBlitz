#include "fairlock.h"
#include <signal.h>
#include <pthread.h> 

// create a new fair lock
struct fair_lock * fair_lock_new() {
    struct fair_lock* fairLock = malloc(sizeof(struct fair_lock));
    //init
    fairLock->isLocked = false;
    pthread_mutex_init(&fairLock->lock, NULL);
    list_init(&fairLock->listofThreads);
    return fairLock;
}

// lock this fair lock
void fair_lock(struct fair_lock *lock) {
    pthread_mutex_lock(&lock->lock);
    if (!lock->isLocked) {
        lock->isLocked = true;
    }
    else {
        // add to the list 
        struct fairwaiter* waiter = malloc(sizeof(struct fairwaiter));
        pthread_cond_init(&waiter->condVar, NULL);
        list_push_back(&lock->listofThreads, &waiter->elem);
        
        //move the thread to the BLOCKED state
        pthread_cond_wait(&waiter->condVar, &lock->lock);
        free(waiter);
    }
    pthread_mutex_unlock(&lock->lock);
}

// unlock this fair lock
void fair_unlock(struct fair_lock *lock) {
    pthread_mutex_lock(&lock->lock);
    if (!list_empty(&lock->listofThreads)) {
        struct list_elem* eleml = list_pop_front(&lock->listofThreads);
        struct fairwaiter* waiter = list_entry(eleml, struct fairwaiter, elem);
        pthread_cond_signal(&waiter->condVar);
    }
    else {
        lock->isLocked = false;
    }
    pthread_mutex_unlock(&lock->lock);
}

// wait on this fair condition variable
void fair_cond_wait(struct fair_cond *cond) {

    pthread_mutex_lock(&cond->lock);

    //adds itself to a queue of waiters
    struct fairwaiter* waiter = malloc(sizeof(struct fairwaiter));
    pthread_cond_init(&waiter->condVar, NULL);

    list_push_back(&cond->listofThreads, &waiter->elem);

    //unlocks the fair lock
    fair_unlock(cond->fairlock);

    //moves into the BLOCKED state
    pthread_cond_wait(&waiter->condVar, &cond->lock);

    pthread_mutex_unlock(&cond->lock);

    free(waiter);

    //locks the fair lock again
    fair_lock(cond->fairlock);
}

// wake up all threads waiting on that fair condition variable
void fair_cond_broadcast(struct fair_cond *cond) {
    pthread_mutex_lock(&cond->lock);

    if (!list_empty(&cond->listofThreads)) {
        while (!list_empty(&cond->listofThreads))
        {
            struct list_elem* eleml = list_pop_front(&cond->listofThreads);
            struct fairwaiter* waiter = list_entry(eleml, struct fairwaiter, elem);
            pthread_cond_signal(&waiter->condVar);
        }
    }
    pthread_mutex_unlock(&cond->lock);
}

// create a fair condition variable tied to the given fair lock
struct fair_cond *fair_cond_new(struct fair_lock *lock) {
    struct fair_cond* fairCond = malloc(sizeof(struct fair_lock));
    //init
    list_init(&fairCond->listofThreads);
    pthread_mutex_init(&fairCond->lock, NULL);
    fairCond->fairlock = lock;
    return fairCond;
}