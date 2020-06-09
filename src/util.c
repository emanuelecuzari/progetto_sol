#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <util.h>

//implementazione dell'interfaccia util.h

int msleep(long msec){
    struct timespec t;
    int retval;
    if(msec<0){
        errno=EINVAL;
        return -1;
    }
    t.tv_sec=msec/1000;
    t.tv_nsec=(msec%1000)*1000000;
    do {
        retval = nanosleep(&t, &t);
    } while (retval && errno == EINTR);
    return retval;
}

int Lock_Acquire(pthread_mutex_t* mutex){                 
    if(pthread_mutex_lock(mutex)!= 0){
        perror("Lock not acquired\n");
        return -1;
    }
    return 0;
}

int Lock_Release(pthread_mutex_t* mutex){ 
    if(pthread_mutex_unlock(mutex)!= 0){
        perror("Lock not released\n");
        return -1;
    }
    return 0;
}

int cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex){
    if(pthread_cond_wait(cond, mutex)!= 0){
        perror("Thread not suspended\n");
        return -1;
    }
    return 0;
}

int cond_signal(pthread_cond_t* cond){
    if(pthread_cond_signal(cond)!= 0){
        perror("Lock not woken up\n");
        return -1;
    }
    return 0;
}