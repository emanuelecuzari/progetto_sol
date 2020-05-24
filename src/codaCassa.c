#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "codaCassa.h"
#include "util.h"

/* file di implementazione dell'interfaccia per la bounded queue della cassa */

/*-----------------------------------FUNZIONI DI UTILITA'-------------------------------------*/
static void handleError(){                 
    if(q->buf) free(q->buf);
    if(q->mtx_coda) pthread_mutex_destroy(q->mtx_coda);
    if(q->empty) pthread_cond_destroy(q->empty);
    if(q->full) pthread_cond_destroy(q->full);
    free(q);
}
/*--------------------------------------------------------------------------------------------*/

Queue_t* initQueue(size_t n){
    Queue_t* q=(Queue_t*)malloc(sizeof(Queue_t));
    if(!q){
        perror("in malloc coda\n");
        return NULL;
    }
    q->buf=calloc(n, sizeof(void*));
    if(!q->buf){
        perror("in calloc buf\n");
        handleError();
        return NULL;
    }
    if(pthread_mutex_init(q->mtx_coda,NULL)!=0){
        perror("in init mutex\n");
        handleError();
        return NULL;
    }
    if(pthread_cond_init(q->empty,NULL)!=0){
        perror("in init cond empty\n");
        handleError();
        return NULL;
    }
    if(pthread_cond_init(q->full,NULL)!=0){
        perror("in init cond full\n");
        handleError();
        return NULL;
    }
    q->clienti=0;
    q->dim=n;
    q->head=q->tail=0;
    return q;
}

void delQueue(Queue_t *q){
    if(!q){
        perror("coda inesistente\n");
        return;
    }
    free(q->buf);
    pthread_mutex_destroy(q->mtx_coda);
    pthread_cond_destroy(q->cond);
    free(q);
}

int insert(Queue_t *q, void *data){
    if(!q){
        perror("coda inesistente: non posso inserire\n");
        return -1;
    }
    if(!data){
        perror("nulla da inserire in coda\n");
        return -1;
    }
    Lock_Acquire(&q->mtx_coda);
    while(q->clienti==q->dim){
        if(cond_wait(&q->full, &q->mtx_coda)==-1){
            perror("CRITICAL ERROR\n");
            return -1;
        }
    }
    q->buf[q->tail]=data;
    q->clienti+=1;
    q->tail+=(q->tail+1 >= q->dim) ? (1-q->dim) : 1;     /* if semplificato: quando raggiungo dim risposiziono tail all'inizio del buffer */
    if(cond_signal(&q->empty)==-1){
        perror("CRITICAL ERROR\n");
        return -1;
    }
    Lock_Release(&q->mtx_coda);
    return 0;
}

void* pop(Queue_t *q){
    if(!q){
        perror("coda inesistente: non posso servire nessuno\n");
        return -1;
    }
    Lock_Acquire(&q->mtx_coda);
    void* ret;
    while(q->clienti==0{
        if(cond_wait(&q->empty, &q->mtx_coda)==-1){
            perror("CRITICAL ERROR\n");
            return -1;
        }
    }
    ret=q->buf[q->head];
    q->buf[q->head]=NULL;
    q->clienti-=1;
    q->head+=(q->head+1 >=q->dim) ? (1-q->dim) : 1;
    if(cond_signal(&q->full)==-1){
        perror("CRITICAL ERROR\n");
        return -1;
    }
    Lock_Release(&q->mtx_coda);
    return ret;
}

int isEmpty(Queue_t* q){
    int i=0;
    int flag=1;
    Lock_Acquire(&q->mtx_coda);
    while(i<q->dim){
        if(q->buf[i]==NULL) flag=0;
        else{
            flag=1;
            break;
        }
    }
    Lock_Release(&q->mtx_coda);
    if(flag==1) return -1
    return 0;
}