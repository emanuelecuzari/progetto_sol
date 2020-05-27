#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <cliente.h>
#include <cassiere.h>
#include <util.h>
#include <bool.h>
#include <direttore.h>
#include <icl_hash.h>

/* pid del processo supermecato */
extern pid_t pid;

extern volatile int sig_hup;

/*-----------------------------------FUNZIONI DI UTILITA'-------------------------------------*/
static bool check_state(directorArgs_t* director, director_state_opt* s);
static int accept_notify(directorArgs_t* director, icl_hash_t* h);
static void auth_exit(directorArgs_t* director, clientArgs_t* customer);
/*--------------------------------------------------------------------------------------------*/

void* Direttore(void* arg){
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

    directorArgs_t* direttore=(directorArgs_t*)arg;
    director_state_opt* state = direttore->stato;
    check_state(direttore, state);

    while(state==ATTIVO){
        /* controllo che non sia arrivato un segnale */
        if(!check_state(direttore, state)) break;
        
        /* lettura notifica */
        icl_hash_t* hash = direttore->hashtable;
        if(accept_notify(direttore, hash) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }

        /* chiusura casse */
        if(Lock_Acquire(&direttore->mtx)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        for(size_t i = 0; i < direttore->tot_casse; i++){
            int key = direttore->checkbox[i]->id;
            void* res = icl_hash_find(direttore->hashtable, &key);
            if(res == NULL) continue;
            /* non chiudo la cassa se è l'unica aperta */
            if(direttore->casse_aperte == MIN_APERTURA) break;
            /* controllo se la cassa ha esattamente un cliente, perchè se non ne ha ed è aperta, il thread termina(vedi cassiere.c) */
            if(direttore->checkbox[i]->state==OPEN && res == 1 && direttore->casse_aperte >= direttore->boundClose){
                *(direttore->checkbox[i]->state) == CLOSING;
                *(direttore->state[i]) == CLOSING;
                *(direttore->casse_chiuse)++;
                *(direttore->casse_aperte)--;
                printf("Il direttore ha chiuso la cassa %d\n", direttore->checkbox[i]->id);
            }
        }
        if(Lock_Release(&direttore->mtx)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }

        /* apertura casse */
        if(Lock_Acquire(&direttore->mtx)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        for(size_t i = 0; i < direttore->tot_casse, i++){
            int key = direttore->checkbox[i]->id;
            void* res = icl_hash_find(direttore->hashtable, &key);
            if(res == NULL) continue;
            /* controllo se in una cassa aperta ci sono più clienti del bound */
            if(direttore->checkbox[i]->state==OPEN && res >= direttore->boundOpen)
            /* controllo gli stati delle casse per aprirne una che era chiusa */
                for(size_t i = 0; i < direttore->tot_casse, i++){
                    if(direttore->stato_casse[i] == CLOSED){
                        if(pthread_create(&direttore->thid_casse[i], NULL, Cassiere, direttore->checkbox[i])!=0){
                            perror("CRITICAL ERROR\n");
                            kill(pid, SIGUSR2);
                        }
                        direttore->stato_casse[i] == OPEN;
                        *(direttore->checkbox[i]->state) == OPEN;
                        *(direttore->casse_chiuse)--;
                        *(direttore->casse_aperte)++;
                        printf("Il direttore ha aperto la cassa %d\n", i+1);
                    }
                }
            }
        }
        if(Lock_Release(&direttore->mtx)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }

        /* autirizzazione uscita cliente */
        clientArgs_t* cliente = direttore->customer;
        auth_exit(direttore, cliente);

        /* autorizzazione entrata cliente */
        if(Lock_Acquire(&direttore->mtx)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        for(size_t i = 0; i < conta_uscite; i++){
            if(pthread_create(&direttore->thid_clienti[i], NULL, cliente, direttore->customer[i]) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
        }
        if(Lock_Release(&direttore->mtx)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
    }
    /* chiudo tutte le casse */
    if(Lock_Acquire(&direttore->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    for(size_t i = 0; i < direttore->tot_casse; i++){
        if(direttore->stato_casse[i] != CLOSED){
            if(sig_hup){
                direttore->stato_casse[i] == CLOSING_MARKET;
                *(direttore->checkbox[i]->state) == CLOSING_MARKET;
            }
            else{
                direttore->stato_casse[i] == SHUT_DOWN;
                *(direttore->checkbox[i]->state) == SHUT_DOWN;
            }
        }
    }
    if(Lock_Release(&direttore->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }

    /* se ci sono clienti in attesa uscita, li autorizzo */
    auth_exit(direttore, cliente);
    return NULL;
}

static bool check_state(directorArgs_t* director, director_state_opt* s){
    if(Lock_Acquire(&director->check)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    if(*s != ATTIVO){
        if(Lock_Release(&direttore->check)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        return false;
    }
    if(Lock_Release(&direttore->check)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    return true;
}

static int accept_notify(directorArgs_t* director, icl_hash_t* h){
    if(Lock_Acquire(&director->mtx)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
    }
    if(director->update==true){
        for(size_t i = 0; i <= director->tot_casse; i++){
            if(director->stato_casse[i]==CLOSED) continue;
            int key = director->checkbox[i]->id;
            int* dato = director->checkbox[i]->notifica;
            void** olddata;
            /* se la cassa non ha ancora aggiornato notifica non aggiorno hash */
            void* res=icl_hash_find(h, &key);
            if(res != NULL && (int*)res==dato) continue;
            /* aggiorno hash */
            if(icl_hash_update_insert(h, &key, dato, olddata) == NULL){
                return -1;
            }
        }
        printf("Il direttore ha ricevuto la notifica: dati aggiornati!\n");
        *(director->update)=false;
    }
    else{
        if(cond_wait(director->update_cond, director->mtx) == -1){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
    }
    if(Lock_Release(&director->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    return 0;
}

static void auth_exit(directorArgs_t* director, clientArgs_t* customer){
    if(Lock_Acquire(&director->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    int conta_uscite = 0;
    for(size_t i = 0; i < director->tot_clienti; i++){
        if(customer[i]->out){
            customer[i]->autorizzazione = true;
            if(cond_signal(customer[i]->authorized)==-1){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
            conta_uscite++;
        }
    }
    if(Lock_Release(&director->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
}