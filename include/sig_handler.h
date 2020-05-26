/* interfaccia per definizione thread signal handler */

#if !defined(SIG_HANDLER_H)
#define SIG_HANDLER_H

#include <direttore.h>

typedef struct handler{
    director_state_opt* stato;
    pthread_mutex_t* check;
}handler_t;

static bool signal_handler(int sig, handler_t* h);
/**
 * funzione per gestione di diversi tipi di segnale
 * @param sig segnale da gestire
 * @param h struttura dati per aggiornare stato direttore/supermercato
 * @return true se segnale è SIGHUP o SIGQUIT, false altrimenti
*/

void* main_handler(void* arg);
/**
 * funzione per inizializzaione thread signal handler
 * @param arg argomento del thread(può essere struct)
 * @return NULL
*/

#endif /* SIG_HANDLER_H */