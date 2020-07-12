#include <stdio.h>
#include <stdlib.h>
#include <util.h>
#include <pthread.h>
#include <queue.h>

LQueue_t* init_LQueue(){
    LQueue_t* q = (LQueue_t*)malloc(sizeof(LQueue_t));
    if(!q){
        perror("malloc q\n");
        return NULL;
    }
    q->len = 0;
    q->head = malloc(sizeof(Node_t));
    if(!q->head){
        perror("malloc head\n");
        return NULL;
    }
    q->head->info = NULL;
    q->head->next = NULL;
    q->tail = q->head;
    return q;
}

void deleteLQueue(LQueue_t* q){
    if(!q){
        perror("coda inesistente\n");
        return;
    }

    Node_t* p = q->head;
    Node_t* t;
    while(p != NULL){
        if(p->info) free(p->info);
        t = p;
        p = p->next;
        free(t);
    }
    free(q);
}

int insertLQueue(LQueue_t* q, void* arg){
    if(q == NULL||arg == NULL){
        perror("errore in push\n");
        return -1;
    }

    Node_t* n = malloc(sizeof(Node_t));
    if(n == NULL) return -1;
    n->info = arg;
    n->next = NULL;

    if(q->len == 0){
        q->head = n;
        q->tail = n;
    }
    else{
        q->tail->next = n;
        q->tail = n;   
    }
    q->len += 1;
    return 0;
}

void* popLQueue(LQueue_t* q){
    if(q == NULL){
        perror("errore in push\n");
        return NULL;
    }
    if(q->len == 0) return NULL;

    void* ret = q->head->info;
    Node_t* p = malloc(sizeof(Node_t));
    p = q->head;
    q->head = q->head->next;
    q->len -= 1;
    free(p);
    return ret;
}