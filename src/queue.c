#include <stdio.h>
#include <stdlib.h>
#include <util.h>
#include <pthread.h>
#include <queue.h>

LQueue_t* init_LQueue(){
    LQueue_t* q=(LQueue_t*)malloc(sizeof(LQueue_t));
    if(!q){
        perror("malloc q\n");
        return NULL;
    }
    q->len=0;
    q->head=malloc(sizeof(Node_t));
    if(!q->head){
        perror("malloc head\n");
        return NULL;
    }
    q->head->info = NULL;
    q->head->next = NULL;
    q->tail = q->head;
    if(pthread_mutex_init(&(q->mtx_queue), NULL)!=0){
        perror("in init mtx_queue\n");
        return NULL;
    }
    if(pthread_cond_init(&(q->cond_queue), NULL)!=0){
        perror("in init empty\n");
        return NULL;
    }
    return q;
}

void deleteLQueue(LQueue_t* q){
    if(!q){
        perror("coda inesistente\n");
        return;
    }
    while(q->head!=q->tail){
        Node_t*p=q->head;
        q->head=q->head->next;
        free(p);
    }
    free(q->head);
    pthread_mutex_destroy(&q->mtx_queue);
    pthread_cond_destroy(&q->cond_queue);
    free(q);
}

int insertLQueue(LQueue_t* q, void* arg){
    if(q==NULL||arg==NULL){
        perror("errore in push\n");
        return -1;
    }
    Node_t* n=malloc(sizeof(Node_t));    
    n->info = arg;
    n->next = NULL;
    Lock_Acquire(&q->mtx_queue);
    q->tail->next = n;
    q->tail = n;      
    q->len += 1;
    pthread_cond_signal(&q->cond_queue);
    Lock_Release(&q->mtx_queue);
    return 0;
}

void* popLQueue(LQueue_t* q){
    if(q==NULL){
        perror("errore in push\n");
        return NULL;
    }
    Lock_Acquire(&q->mtx_queue);
    if(q->len==0) pthread_cond_wait(&q->cond_queue, &q->mtx_queue);
    Node_t* p=malloc(sizeof(Node_t));
    p=(Node_t*)q->head;
    void* ret=p->next->info;
    q->head=q->head->next;
    q->len-=1;
    Lock_Release(&q->mtx_queue);
    free(p);
    return ret;
}