#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "codaCassa.h"
#include "cliente.h"
#include "cassiere.h"
#include "util.h"
#include "bool.h"

/* pid del processo supermecato */
extern pid_t pid;

/*-----------------------------------FUNZIONI DI UTILITA'-------------------------------------*/
static void to_queue(clientArgs_t* customer, unsigend int* seed);
static void no_stuff(clientArgs_t* customer);
/*--------------------------------------------------------------------------------------------*/

void* cliente(void* arg){
    clientArgs_t* customer=(clientArgs_t*)arg;
    unsigned int seme=customer->seed;             //seme per generare randomicamente cassa in cui andare
    int myid=customer->thid;
    int prod=customer->prodotti;
    int casse_attive=customer->casse_aperte;
    long buying=customer->t_acquisti;
    int tot=customer->num_casse;
    client_state_opt* state=customer->state;

    /* maschera di segnali */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGINT);
    if(pthread_sigmask(SIG_SETMASK, &set, NULL)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }

    /* cliente fa acquisti */
    msleep(buying);

    if(prod > 0){
        while(1){
            /* inserisco in coda il cliente */
            to_queue(customer, &seme);

            /* cliente aspetta il suo turno */
            if(Lock_Acquire(&customer->mtx)!=0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
            while(state==IN_QUEUE){
                if(cond_wait(&customer->cond)!=0){
                    perror("CRITICAL ERROR\n");
                    kill(pid, SIGUSR2);
                }
            }
            if(Lock_Release(&customer->mtx)!=0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }

            /* se cliente si deve spostare rieseguo ciclo, break altrimenti*/
            if(state!=CHANGE) break;
            else printf("Il cliente %d deve cambiare cassa\n", myid);

        }
    }
    else{
        no_stuff(customer);
        printf("Il cliente %d esce senza comprare\n", myid);
    }

    /* gestione uscita */
    if(Lock_Acquire(&customer->exit)!=0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
    }
    *(customer->tot_uscite)+=1;
    customer->out=1;
    if(Lock_Release(&customer->exit)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    pthread_exit((void*)0);
}

/**
 * funzione per mettere in coda un cliente
 * @param customer cliente da mettere in coda
 * @param seed seme per generare indice cassa scelta randomicamente 
*/
static void to_queue(clientArgs_t* customer, unsigend int* seed){
    if(Lock_Acquire(&customer->mtx)!=0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
    }
    /* il supermercato deve sempre avere almeno una cassa all'attivo */
    if(customer->casse_aperte==0){
        perror("CRITICAL ERROR: infrazione regolamento!");
        kill(pid, SIGUSR2);
    }
    /* scelgo radomicamente la cassa */
    int indice=1 + (rand_r(seed) % (customer->casse_aperte));
    for(int i=1; i <= tot; i++){
        if(customer->cashbox_state[i-1]==OPEN ){
            if(indice==1){
                if(insert(customer->cashbox_queue[i-1], customer)==-1){
                    kill(pid, SIGUSR2);
                }
                customer->state=IN_QUEUE;
                printf("Il cliente %d è in coda alla cassa %d\n", myid, i);
                break;
            }
            else{
                indice--;
            }
        }
    }
    if(Lock_Release(&customer->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
}

/**
 * funzione per attesa autorizzazione a uscire se cliente non compra
 * @param customer cliente che richiede autorizzazione a uscire
*/
static void no_stuff(clientArgs_t* customer){
    if(Lock_Acquire(&customer->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    if(!(customer->autorizzazione)){
        if(cond_wait(&customer->authorized, &customer->mtx)==-1){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
    }
    if(Lock_Release(&customer->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
}