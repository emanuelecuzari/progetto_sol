/**
 * intrefaccia per definizione thread direttore
*/

#if !defined(DIRETTORE_H)
#define DIRETTORE_H

#include <pthread.h>
#include <cassiere.h>
#include <cliente.h>

#define MIN_OPEN 1

typedef struct Direttore{
    int casse_tot;                  /* numero totale casse */
    int boundClose;                 /* soglia chiusura cassa */
    int boundOpen;                  /* soglia apertura cassa */
    int tot_clienti;                /* numero totale clienti in supermercato */
    int* notifica;                  /* array per mantenimento numero clienti in coda per cassa */

    /* mutex e var di cond. */
    pthread_mutex_t* mtx;           /* mutex principale del sistema */
    pthread_mutex_t* ask_auth;      /* mutex per autorizzazione uscita cliente */
    pthread_mutex_t* sent;          /* mutex per gestione notifica dai cassieri */
    pthread_mutex_t* out;           /* mutex per comunicare uscita */
    pthread_cond_t* sent_cond;      /* var condizionamento per invio/ricezione notifica*/
    pthread_cond_t* ask_auth_cond;  /* var di cond. per richiesta autorizzazione */
    pthread_cond_t* out_cond;       /* var di cond. per comunicare uscita */

    /* var condivise */
    int* casse_aperte;              /* num casse aperte */
    int* casse_chiuse;              /* num casse chiuse */
    int* update;                    /* array per ricezione notifica */
    int* autorizzazione;            /* flag per autorizzazione a uscire */
    argsCassiere_t* cassieri;       /* array di cassieri */
    pthread_t* thid_casse;          /* array di thid casse */
    
}argsDirettore_t;

/**
 * funzione che inizializza cassiere
 * @param arg argomento del thread; può essere una struct
*/
void* direttore(void* arg);

#endif /* DIRETTORE_H */