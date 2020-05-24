#if !defined(UTIL_H)
#define UTIL_H

#include <pthread.h>

/*interfaccia per definzione di alcune funzioni di utlità*/

int msleep(long msec);
/**
 * funzione che manda processo in pausa
 * 
 * @param msec millisecondi per cui processo è in pausa
 * @return tempo trascorso success
 * @return -1 failure, sets errno
*/

int Lock_Acquire(pthread_mutex_t* mtx);
/**
 * funzione per acquisizione lock
 * @param mtx mutex su cui acquisisco il lock
 * @return 0 success
 * @return -1 failure, sets errno
*/

int Lock_Release(pthread_mutex_t* mtx);
/**
 * funzione per rilascio lock
 * @param mtx mutex di cui rilascio il lock
 * @return 0 success
 * @return -1 failure, sets errno
*/

int cond_signal(pthread_cond_t* cond);
/**
 * funzione che risveglia un thread sospeso sulla variabile di condizione cond
 * @param cond variabile di condizione
 * @return 0 success
 * @return -1 failure, sets errno
*/

int cond_wait(pthread_cond_t* cond, pthread_mutex_t* mtx);
/**
 * funzione che sospende un thread sulla variabile di condizione cond
 * @param cond variabile di condizione
 * @param mtx mutex su cui è sospeso il thread
 * @return 0 success
 * @return -1 failure, sets errno
*/

int cond_broadcast(pthread_cond_t* cond);
/**
 * funzione che risveglia tutti i thread sospesi sulla variabile di condizione cond
 * @param cond variabile di condizione
 * @return 0 success
 * @return -1 failure, sets errno
*/

#endif /* UTIL_H */

