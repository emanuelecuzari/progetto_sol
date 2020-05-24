#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include "util.h"

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
        res = nanosleep(&t, &t);
    } while (retval && errno == EINTR);

    return retval;
}

int Lock_Acquire(pthread_mutex_t* mtx){
    int error=0;                    
    if(pthread_mutex_lock(&mtx)!= 0){
        errno=error;        //non conoscendo il tipo di errore eventualmente generato, setto errno a un valore indicativo
        return -1;
    }
    return 0;
}

int Lock_Release(pthread_mutex_t* mtx){
    int error=0; 
    if(pthread_mutex_unlock(&mtx)!= 0){
        errno=error;
        return -1;
    }
    return 0;
}

int cond_wait(pthread_cond_t* cond, pthread_mutex_t* mtx){
    int error=0; 
    if(pthread_cond_wait(&cond, &mtx)!= 0){
        errno=error;
        return -1;
    }
    return 0;
}

int cond_signal(pthread_cond_t* cond){
    int error=0; 
    if(pthread_cond_signal(&cond)!= 0){
        errno=error;
        return -1;
    }
    return 0;
}

int cond_broadcast(pthread_cond_t* cond){
    int error=0; 
    if(pthread_cond_broadcast(&cond)!= 0){
        errno=error;
        return -1;
    }
    return 0;
}