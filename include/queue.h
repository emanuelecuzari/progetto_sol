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
}LQueue_t;

LQueue_t* init_LQueue();

void deleteLQueue(LQueue_t* q);

int insertLQueue(LQueue_t* q, void* arg);

void* popLQueue(LQueue_t* q);

#endif /* QUEUE_H */