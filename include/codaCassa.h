#if !defined(CODACASSA_H)
#define CODACASSA_H

#include <pthread.h>

/** Struttura dati bounded queue(specifica per cassa) 
 * inizializzazione e cancellazione cassa possono essere
 * effettuate solo dal processo principale
*/
typedef struct Queue {
    void **buf;       //buffer circolare 
    int clienti;      //numero clienti in coda
    size_t head;
    size_t tail;
    size_t dim;
    pthread_mutex_t mtx_coda;
    pthread_cond_t empty;
    pthread_cond_t full;
} Queue_t;

Queue_t* initQueue(size_t n);
/** Alloca ed inizializza una coda
 * Ogni coda è esclusiva per ogni cassa.
 * @param n dimensione coda
 * @return puntatore alla coda creata se successo; NULL altrimenti
 */

void delQueue(Queue_t *q);
/** Cancella una coda allocata. 
 * La cancellazione di una coda si può fare solo a cassa chiusa, dunque sarà il direttore a farlo.
 * @param q punta a coda da cancellare.
 */

int insert(Queue_t *q, void *data);
/** Inserisce un dato nella coda.
 * @param data puntatore a dato che insersico
 * @return 0 se successo, -1 se ho errore
 */

void* pop(Queue_t *q);
/** Estrae un dato dalla coda.
 * @param q è puntatore alla coda da cui estraggo.
 * @return dato estratto se successo, -1 se errore
 */

int isEmpty(Queue_t* q);
/**
 * Controlla se coda è vuota
 * @param q coda
 * @return 0 success
 * @return -1 failure
*/

#endif /* CODACASSA_H */