/* interfaccia per definizione thread direttore */

#if !defined(DIRETTORE_H)
#define DIRETTORE_H

#define MIN_APERTURA 1

typedef enum state{
    ATTIVO,
    INT_SIGHUP,
    INT_SIGQUIT
}director_state_opt;

typedef struct direttore{
    int tot_casse;
    int boundOpen;                  /* soglia per apertura cassa */
    int boundClose;                 /* soglia per chiusura cassa */
    int* casse_chiuse;              /* num casse chiuse */
    int* casse_aperte;              /* num casse aperte */
    int* tot_clienti;               /* num clienti in supermercato */
    bool* update;                   /* true quando notifica arriva */
    Cassa_t* checkbox;              /* array di casse */
    stato_cassa_opt* stato_casse;   /* array stato casse */
    icl_hash_t* hashtable;          /* hash per associazione code-clienti*/
    clientArgs_t* customer;         /* array di clienti */
    director_state_opt* stato;
    pthread_t* thid_casse;
    pthread_t* thid_clienti;
    pthread_mutex_t* mtx;
    pthread_mutex_t* check;
    pthread_cond_t* update_cond;
}directorArgs_t;

void* Direttore(void* arg);
/**
 * funzione che inizializza thread
 * @param arg argomento/i passati al thread(può essere una struct)
*/
#endif /* DIRETTORE_H */