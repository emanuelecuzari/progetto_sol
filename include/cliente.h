/**
 * interfaccia per definizione del thread cliente
*/

#if !defined(CLIENTE_H)
#define CLIENTE_H

#include <pthread.h>
#include <codaCassa.h>
#include <cassiere.h>

#define MIN_T_ACQUISTI 10

typedef struct Cliente{
    int id;                         /* id cleinte */
    int e;                          /* numero clienti che può uscire */
    int cambi;                      /* quante volte ha cambiato coda */
    int casse_tot;                  /* numero casse totali */
    int num_prodotti;               /* prodotti che acquista */
    unsigned int seed;              /* seme per scelta randomica cassa */
    struct timeval ts_incoda;       /* inzio attesa in coda*/
    struct timeval tend_incoda;     /* fine attesa in coda */
    size_t t_acquisti;              /* tempo per acquisti */
    size_t t_supermercato;          /* tempo trascorso nel supermercato */

    /* mutex e var di cond. */
    pthread_mutex_t* mtx;           /* mutex principale del sistema */
    pthread_mutex_t* personal;      /* muetx esclusiva del cliente */
    pthread_mutex_t* ask_auth;      /* mutex per richiesta autorizzazione */
    pthread_mutex_t* out;           /* mutex per comunicare uscita */
    pthread_cond_t* ask_auth_cond;  /* var di cond. per richiesta autorizzazione */
    pthread_cond_t* out_cond;       /* var di cond. per comunicare uscita */
    pthread_cond_t* wait;           /* var di cond. per attesa turno */

    /* var condivise */
    int is_out;                     /* flag per uscita cliente */
    int* autorizzazione;            /* autorizzazione per uscita */
    int* set_change;                /* flag per indicare se deve cambiare cassa */
    int* set_wait;                  /* flag per attesa in coda */
    int* num_uscite;                /* numero di clienti che escono */
    argsCassiere_t* cassieri;       /* array di cassieri/casse */
    Queue_t** code;                 /* array di code */
}argsClienti_t*;

/**
 * funzione che inizializza cassiere
 * @param arg argomento del thread; può essere una struct
*/
void* cliente(void* arg);

#endif /* CLIENTE_H */