#if !defined(QUEUE_H)
#define QUEUE_H

typedef struct Node{    
    void* info;
    struct Node* next;
}Node_t;

typedef struct LQueue{     
    Node_t* head;
    Node_t* tail;
    int len;
    pthread_mutex_t mtx_queue;
    pthread_cond_t cond_queue;
}LQueue_t;

LQueue_t* init_LQueue();

void deleteLQueue(LQueue_t* q);

int insertLQueue(LQueue_t* q, void* arg);

void* popLQueue(LQueue_t* q);

#endif /* QUEUE_H */