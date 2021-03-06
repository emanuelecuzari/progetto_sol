/**
 * interfaccia per la definizione del thread cassiere
*/

#if !defined(CASSIERE_H)
#define CASSIERE_H

#include <codaCassa.h>
#include <pthread.h>
#include <queue.h>

#define MIN 20
#define MAX 80
#define NOCLIENT (void*)-1

/**
 * struttura per definizione argomenti principali del thread cassiere
*/
typedef struct Cassiere{
    int id;                     /* id della cassa */
    int chiusure;               /* numero di chiusure */
    int clienti_serviti;        /* numero clienti serviti da cassa */
    int tot_acquisti;           /* totale prodotti elebaorati */
    double opening_time;        /* tempo di apertura della cassa */
    size_t tempo_fisso;         /* tempo fisso di gestione; compreso tra MIN e MAX */
    size_t gestione_p;          /* tempo di gestione di 1 prodotto */
    size_t t_notifica;          /* ogni quanto invio notifica */
    Queue_t* coda;              /* coda della cassa */
    LQueue_t* tservice_list;    /* lista dei tempi di servizio di una cassa */

    /* mutex e var di cond. */
    pthread_mutex_t* mtx;       /* mutex principale del sistema */
    pthread_mutex_t* sent;      /* mutex per invio/ricezione notifica */
    pthread_cond_t* sent_cond;  /* var condizionamento per invio/ricezione notifica*/
    
    /* var condivise */
    int* update;                /* flag che indica se la notifica è stata inviata o no */
    int* set_close;             /* flag che indica chiusura della cassa */
    int* notifica;              /* lunghezza coda da inviare a direttore */
}argsCassiere_t;


/**
 * funzione che inizializza cassiere
 * @param arg argomento del thread; può essere una struct
*/
void* cassiere(void* arg);

#endif /* CASSIERE_H */