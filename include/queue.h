/**
 * interfaccia per la definizione di una coda unbounded
*/

#if !defined(QUEUE_H)
#define QUEUE_H

/* struttura per definizione dei nodi */
typedef struct Node{    
    void* info;
    struct Node* next;
}Node_t;

/* struttura per definizione della coda */
typedef struct LQueue{     
    Node_t* head;
    Node_t* tail;
    int len;
}LQueue_t;

LQueue_t* init_LQueue();
/**
 * funzione per inizializzaione coda
 * @return q success, NULL failure
*/

void deleteLQueue(LQueue_t* q);
/**
 * funzione per cancellazione coda
 * @param q è coda da cancellare
*/

int insertLQueue(LQueue_t* q, void* arg);
/**
 * funzione per inserimento nuovo nodo
 * @param q coda in cui inserire
 * @param arg dato da inserire
 * @return 0 success, -1 failure
*/

void* popLQueue(LQueue_t* q);
/**
 * funzione per rimozione nodo da coda
 * @param q coda da cui estrarre dato
 * @return dato estratto success, NULL failure
*/

#endif /* QUEUE_H */