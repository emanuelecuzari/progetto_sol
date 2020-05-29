/* interfaccia per la definizione del thread cliente */

#if !defined(CLIENTE_H)
#define CLIENTE_H

#define T_MIN 10        /* tempo minimo acquisti(in millisecondi) */

/* struttura per gli argomenti utili al thread */
typedef struct clientArgs{
    int id;
    int prodotti;
    int num_casse;
    int casse_aperte;                   /* numero casse aperte */
    int out;                            /* flag per dire se cliente è uscito */
    int* tot_uscite;
    int change;
    unsigned int seed;
    long t_inmarket;              
    size_t t_acquisti;                  /* tempo per gli acquisti */
    bool autorizzazione;                /* autorizzaione a uscire da supermercato */
    struct timeval ts_incoda;           /* tempo entrata in coda cassa */
    struct timeval tend_incoda;         /* tempo uscita da coda cassa */
    pthread_mutex_t* mtx;
    pthread_mutex_t* exit;              /* mutex per controllare le uscite */
    pthread_mutex_t* personal;          /* mutex del cliente */
    pthread_cond_t* cond;        
    pthread_cond_t* authorized;         /* var di condizionamento per attendere autorizzazione */
    Queue_t** cashbox_queue;            /* array code casse */
    client_state_opt* state;
    stato_cassa_opt* cashbox_state;     /* array stati casse */
}clientArgs_t;

/* stati possibili assumbili dal cliente */
typedef enum client_state{
    IN_MARKET,          /* cliente entra nel supermercato*/
    IN_QUEUE,           /* cliente in coda */
    CHANGE,             /* cassa chiusa, cambia coda*/
    PAYING,             /* cliente sta pagando */  
    PAID,               /* cliente ha pagato */
    SIGNAL_OUT          /* interruzione: esci */
}client_state_opt;

void* cliente(void* arg);
/**
 * funzione che inizializza thread
 * @param arg argomento/i passati al thread(può essere una struct)
*/

#endif /* CLIENTE_H */