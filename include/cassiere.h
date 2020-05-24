#if !defined(CASSIERE_H)
#define CASSIERE_H

/*interfaccia per la definizione del thread cassiere*/

#define MIN_REGULAR_TIME 20
#define MAX_REGULAR_TIME 80

/*struttura per argomenti del thread*/
typedef struct Cassa{ 
    int id;                 //indice che identifica cassa
    int* notifica;
    size_t t_trasmissione;  //tempo che trascorre tra un'invio dei dati al direttore e un altro;
    size_t gest_prod;       //tempo gestione singolo prodotto
    size_t t_fisso;         //tempo fisso della cassa; randomico tra 20-80 msec
    Queue_t* coda;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    stato_cassa_opt* state; //stato cassa         
}Cassa_t;

/*possibili stati della cassa*/
typedef enum stato_cassa{
    OPEN,               //cassa aperta, servo clienti
    CLOSING,            //cassa in chiusura: servo cliente attuale, sposto gli altri
    CLOSED,             //cassa chiusa
    CLOSING_MARKET,     //supermercato in chiusura: termino la coda e chiudo
    SHUT_DOWN           //chiusura immediata: termino cliente attuale e chiudo cassa
}stato_cassa_opt;

void* Cassiere(void* arg);
/**
 * funzione che inizializza thread
 * @param arg argomento/i passati al thread(può essere una struct)
*/

#endif /* CASSIERE_H */