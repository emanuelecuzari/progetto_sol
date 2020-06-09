#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <cassiere.h>
#include <codaCassa.h>
#include <queue.h>
#include <cliente.h>
#include <util.h>

/* pid del processo */
extern pid_t pid;

/* flags per distinguere cosa fare quando arriva segnale */
extern volatile sig_atomic_t sig_hup;
extern volatile sig_atomic_t sig_quit;

/*-----------------------------------------MACRO---------------------------------------*/
#define CHECK_NULL(x)                       \
            if(x == NULL){                  \
                perror("CRITICAL ERROR\n"); \
                kill(pid, SIGKILL);         \
            }

#define T_SERVIZIO(fisso, n, t_singolo)     \
            return (fisso + n*t_singolo)

#define T_OPEN(start_sec, start_usec, end_sec, end_usec)                            \
            return ((end_sec * 1000 + end_usec) - (start_sec * 1000 + start_usec))
/*-------------------------------------------------------------------------------------*/

void* cassiere(void* arg){

    /* maschera di segnali */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGKILL);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTSTP);
    if(pthread_sigmask(SIG_SETMASK, &set, NULL)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
    }

    argsCassiere_t* cassa = (argsCassiere_t*)arg;
    struct timeval ts_apertura = {0, 0};
    struct timeval tend_apertura = {0, 0};

    /* inizio a calcolare tempo di apertura */
    gettimeofday(&ts_apertura, NULL);
    while(1){

        /* controllo se la cassa è stata chiusa */
        if(Lock_Acquire(cassa->mtx) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        if(*(cassa->set_close)) break;
        if(Lock_Release(cassa->mtx) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }

        /* controllo se sono arrivati segnali */
        if(sig_hup || sig_quit) break;

        void* test = pop(cassa->coda);
        CHECK_NULL(test);

        argsCliente_t* cliente = (argsCliente_t*)test;

        /* cliente non è più in attesa in coda, risevglio il thread */
        if(Lock_Acquire(cliente->personal) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        *(cliente->set_wait) = 0;
        if(cond_signal(cliente->wait) == -1){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        if(Lock_Release(cliente->personal) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }

        /* fine tempo attesa in coda del cliente */
        gettimeofday(cliente->tend_incoda, NULL);
        
        size_t service_time = T_SERVIZIO(cassa->tempo_fisso, cliente->num_prodotti, cassa->gestione_p);
        if(insertLQueue(cassa->t_servizio, service_time) == -1){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }

        /* gestione invio notifica a direttore */
        size_t t_invio = cassa->t_notifica;
        while(t_invio <= service_time){
            if(Lock_Acquire(cassa->sent) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
            *(cassa->update) = 1;
            *(cassa->notifica) = cassa->coda->clienti;
            if(cond_signal(cassa->sent_cond) == -1){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
            if(Lock_Release(cassa->sent) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
            msleep(t_invio);
            service_time -= t_invio;
            t_invio = cassa->t_notifica;

        }
        msleep(service_time);
        t_invio -= service_time;
        cassa->clienti_serviti++;
    }
    gettimeofday(cassa->tend_apertura, NULL);
    cassa->opening_time = T_OPEN(ts_apertura.tv_sec, ts_apertura.tv_usec, tend_apertura.tv_sec, tend_apertura.tv_usec);
    printf("La cassa %d è rimasta aperta per %.8f secondi\n", cassa->id, (float)cassa->opening_time);
    if(sig_hup){
        while(cassa->coda->clienti != 0){
            void* test = pop(cassa->coda);
            CHECK_NULL(test);
            argsCliente_t* customer = (argsCliente_t*)test;

            gettimeofday(cliente->tend_incoda, NULL);

            size_t service_time = T_SERVIZIO(cassa->tempo_fisso, cliente->num_prodotti, cassa->gestione_p);
            if(insertLQueue(cassa->t_servizio, service_time) == -1){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
            }
            msleep(service_time);
            cassa->clienti_serviti;
        }
        pthread_exit((void*)0);
    }
    else if(sig_quit){
        /* servo solo il primo cliente */
        void* test = pop(cassa->coda);
        CHECK_NULL(test);
        argsCliente_t* customer = (argsCliente_t*)test;

        gettimeofday(cliente->tend_incoda, NULL);

        size_t service_time = T_SERVIZIO(cassa->tempo_fisso, cliente->num_prodotti, cassa->gestione_p);
        if(insertLQueue(cassa->t_servizio, service_time) == -1){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
        }
        msleep(service_time);
        cassa->clienti_serviti++;
        pthread_exit((void*)0);
    }
    /* chiusura standard */
    else{
        /* servo il primo cliente, sposto gli altri */
        void* test = pop(cassa->coda);
        CHECK_NULL(test);
        argsCliente_t* customer = (argsCliente_t*)test;

        gettimeofday(cliente->tend_incoda, NULL);

        size_t service_time = T_SERVIZIO(cassa->tempo_fisso, cliente->num_prodotti, cassa->gestione_p);
        if(insertLQueue(cassa->t_servizio, service_time) == -1){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
        }
        msleep(service_time);
        cassa->cienti_serviti++;
        
        /* tutti gli altri clienti in coda devono cambiare cassa */
        while(cassa->coda->clienti != 0){
            if(Lock_Acquire(customer->personal) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
            *(customer->set_change) = 1;
            if(Lock_Release(customer->personal) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
        }
    }
    pthread_exit((void*)0);
}