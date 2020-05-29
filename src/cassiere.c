#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <codaCassa.h>
#include <cliente.h>
#include <cassiere.h>
#include <util.h>
#include <direttore.h>

/*pid del processo supermercato*/
extern pid_t pid;

/*-----------------------------------FUNZIONI DI UTILITA'-------------------------------------*/
static bool check_state(Cassa_t* c, stato_cassa_opt* s);
static void svuota(Cassa_t* c, stato_cassa_opt* s);
static void set_customer_state(clientArgs_t* cliente, client_state_opt* s);
static void notify(Cassa_t* c);
/*--------------------------------------------------------------------------------------------*/

void* Cassiere(void* arg){
    int servi_primo = 1;                        /* flag */
    int t_invio_dati=cassa->t_trasmissione;
    long t_apertura;
    long t_servizio;                            //tempo di servizio cliente --> T(n)= tempo fisso + (num_prodotti * t_gestione_prodotto);
    struct timeval start_open = {0, 0};        //inizio
    /* ---argomenti thread--- */
    Cassa_t* cassa=((Cassa_t*)arg);
    int myid=cassa->id;
    long t_prodotto=cassa->gest_prod;
    long t_base=cassa->t_fisso;
    Queue_t* q=cassa->coda;
    stato_cassa_opt* stato=cassa->stato;


    /* maschera di segnali */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTSTP);
    if(pthread_sigmask(SIG_SETMASK, &set, NULL)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }

    /*con gettimeofday() calcolo il tempo di apertura cassa*/
    if(stato==OPEN) gettimeofday(&start_open, NULL);
    while(1){
        /* controllo lo stato della cassa */
        bool check = check_state(&cassa, &stato);
        if(!check) break;

        /* se cassa aperta ma coda vuota, invio lo stesso notifica */
        if(isEmpty(cassa->coda)==0){
            notify(cassa);
            continue;
        }

        /* servo cliente */
        void* client=pop(cassa->coda);
        if(client==NULL){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        clientArgs_t* cliente=(clientArgs_t*)client;
        gettimeofday(&cliente->tend_incoda);
        t_servizio=t_base + (cliente->prodotti * t_prodotto);
        set_customer_state(cliente, PAYING);

        /** gestione notifica 
         * spezzo la msleep in due casi:
         *  1.t_invio_dati <= t_servizio
         *  2.t_invio_dati >= t_servizio
        */
        while(t_invio_dati <= t_servizio){
            msleep(t_invio_dati);
            notify(cassa);               
            t_servizio-=t_invio_dati;
            t_invio_dati=cassa->t_trasmissione;
        }
        msleep(t_servizio);
        t_invio_dati= t_invio_dati-t_servizio;
        set_customer_state(cliente, PAID);
        cassa->served++;
    }
    /* chiusura cassa */
    cassa->closed++;
    struct timeval end_open = {0, 0};
    gettimeofday(&end_open, NULL);
    t_apertura=(end_open.tv_sec * 1000 + end_open.tv_usec)-(start_open.tv_sec * 1000 + start_open.tv_usec);
    while(1){
        if(isEmpty(coda->cassa) == 0) break;
        void* client = pop(coda->cassa);
        if(client==NULL){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        clientArgs_t* cliente=(clientArgs_t*)client;
        gettimeofday(&cliente->tend_incoda);
        if(*stato == CLOSING_MARKET){
            if(Lock_Acquire(&cliente->personal) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
            t_servizio=t_base + (cliente->prodotti * t_prodotto);
            set_customer_state(cliente, PAYING);
            if(Lock_Release(&cliente->personal) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
            msleep(t_servizio);
            set_customer_state(cliente, PAID);
            cassa->served++;
        }
        else{
            /* se SIGQUIT, servo comunque il primo cliente */
            if(servi_primo){
                if(Lock_Acquire(&cliente->personal) != 0){
                    perror("CRITICAL ERROR\n");
                    kill(pid, SIGUSR2);
                }
                t_servizio=t_base + (cliente->prodotti * t_prodotto);
                set_customer_state(cliente, PAYING);
                if(Lock_Release(&cliente->personal) != 0){
                    perror("CRITICAL ERROR\n");
                    kill(pid, SIGUSR2);
                }
                msleep(t_servizio);
                set_customer_state(cliente, PAID);
                cassa->served++;
                servi_primo = 0;
            }
            else set_customer_state(cliente, SIGNAL_OUT);
        }
    }
    printf("cassa %d chiusa fino a nuova apertura\n", myid);
    return NULL;
}

/**
 * funzione che controlla lo stato della cassa
 * @param c cassa di cui controllo lo stato
 * @param s stato della cassa
 * @return true se stato è ancora OPEN
 * @return false altrimenti
*/
static bool check_state(Cassa_t* c, stato_cassa_opt* s){
    if(Lock_Acquire(&c->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    if(*s==CLOSING){
        svuota(c, s);
    }
    if(*s == CLOSED) pthread_exit((void*)0);
    if(*s!=OPEN){
        if(Lock_Release(&c->mtx)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        return false;
    }
    if(Lock_Release(&c->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    return true;
}

/**
 * funzione che svuota cassa
 * @param c cassa da svuotare
 * @param s stato della cassa
*/
static void svuota(Cassa_t* c, stato_cassa_opt* s){
    void* client;
    long t_servizio;
    while(!isEmpty(c->coda)){
        client=pop(c->coda);
        if(client==NULL){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        clientArgs_t* cliente=(clientArgs_t*)client;
        /* cassa in chiusura, servo primo cliente e sposto gli altri */
        if(*s==CLOSING){
            set_customer_state(cliente, PAYING);
            t_servizio=c->t_fisso + (cliente->prodotti * c->gest_prod);
            msleep(t_servizio);
            set_customer_state(cliente, PAID);
            *s==CLOSED;
        }
        else if(*s==CLOSED){
            set_customer_state(cliente, CHANGE);
        }
    }
}

/**
 * funzione per settare stato cliente
 * @param cliente cliente di cui devo settare lo stato
 * @param s stato da settare
*/
static void set_customer_state(clientArgs_t* cliente, client_state_opt* s){
    if(Lock_Acquire(&cliente->personal)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    if(cliente->state==IN_QUEUE) cliente->state=s;
    else{
        cliente->state=s;
        if(cond_signal(&cliente->cond)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
    }
    if(Lock_Release(&cliente->personal)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
}

/**
 * funzione per preparazione notifica
 * @param c cassa che invia notifica a direttore
*/
static void notify(Cassa_t* c){
    if(Lock_Acquire(&c->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    directorArgs_t* d;
    *(c->notifica)=c->coda->clienti;
    *(d->update)=true;
    if(cond_signal(c->update_cond) == -1){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    if(Lock_Release(&c->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
}