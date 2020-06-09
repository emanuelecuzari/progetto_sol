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
#include <direttore.h>
#include <icl_hash.h>

/* pid del processo */
extern pid_t pid;

/* flags per distinguere cosa fare quando arriva segnale */
extern volatile sig_atomic_t sig_hup;
extern volatile sig_atomic_t sig_quit;

void* direttore(void* arg){

    /* maschera di segnali */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGKILL);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTSTP);
    if(pthread_sigmask(SIG_SETMASK, &set, NULL) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
    }

    argsDirettore_t* director = (argsDirettore_t*)arg;
    int* is_first = 1;

    while(1){

        /* controllo se è arrivato un segnale */
        if(sig_hup || sig_quit) break;

        /* gestione arrivo notifica */
        if(Lock_Acquire(director->sent) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        if(*(director->update) == 1){
            if(*is_first){
                for(size_t i = 0; i < director->casse_tot; i++){
                    if(*(director->cassieri[i].set_close) == 1) continue;
                    void* key = &(director->cassieri[i].id);
                    void* dato = director->cassieri[i].notifica;
                    if(icl_hash_insert(director->hashtable, key, dato) == NULL){
                        perror("CRITICAL ERROR\n");
                        kill(pid, SIGKILL);
                    }
                }
                *is_first = 0;
                *(director->update) = 0;
            }
            else{
                for(size_t i = 0; i < director->casse_tot; i++){
                    if(*(director->cassieri[i].set_close) == 1) continue;
                    void* key = &(director->cassieri[i].id);
                    void* dato = director->cassieri[i].notifica;
                    void* olddata;
                    void* res = icl_hash_find(director->hashtable, key);
                    /* se la cassa non ha ancora aggiornato notifica non aggiorno hash */
                    if(res != NULL && *(int*)res==*(int*)dato) continue;
                    /* aggiorno hash */
                    if(icl_hash_update_insert(h, key, dato, &olddata) == NULL){
                        perror("CRITICAL ERROR\n");
                        kill(pid, SIGKILL);
                    }
                }
            }
            *(director->update) = 0;
        }
        else{
            if(cond_wait(director->sent_cond, director->sent) == -1){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
        }
        if(Lock_Release(director->sent) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }

        /* chiusura casse */
        if(Lock_Acquire(director->mtx) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        int cntClose = 0;
        for(size_t i = 0; i < director->casse_tot; i++){
            int key = director->cassieri[i].id;
            void* res = icl_hash_find(director->hashtable, &key);
            if(res == NULL) continue;
            int* clienti_in_coda = (int*)res;

            /* non chiudo la cassa se è l'unica aperta */
            if(*(director->casse_aperte) == MIN_OPEN) break;

            /* chiudo una cassa solo se ce ne se sono almeno boundClose con al più un cliente */
            if(*clienti_in_coda >= 0 && *clienti_in_coda <= 1){
                cntClose++;
                if(!(*(director->cassieri[i].set_close)) && cntClose >= director->boundClose){
                    *(director->cassieri[i].set_close) = 1;
                    *(director->casse_chiuse)++;
                    *(director->casse_aperte)--;
                }
            }
        }
        if(Lock_Release(director->mtx) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }

        /* apertura casse */
        if(Lock_Acquire(director->mtx) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        for(size_t i = 0; i < director->casse_tot; i++){
            int key = director->cassieri[i].id;
            void* res = icl_hash_find(director->hashtable, &key);
            int* clienti_in_coda = (int*)res;
            if(res == NULL) continue;

            /* controllo se in una cassa aperta ci sono più clienti del bound */
            if(!(*(director->cassieri[i].set_close)) && *clienti_in_coda >= director->boundOpen){

                /* controllo gli stati delle casse per aprirne una che era chiusa */
                for(size_t i = 0; i < director->casse_tot; i++){
                    if(*(director->cassieri[i].set_close)){
                        if(pthread_create(&director->thid_casse[i], NULL, cassiere, &director->cassieri[i]) != 0){
                            perror("CRITICAL ERROR\n");
                            kill(pid, SIGKILL);
                        }
                        *(director->cassieri[i].set_close) = 0;
                        *(director->casse_chiuse)--;
                        *(director->casse_aperte)++;
                    }
                }
            }
        }
        if(Lock_Release(director->mtx) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }

        /* autorizzazione uscita cliente */
        if(Lock_Acquire(director->ask_auth)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }

        /** 
         * autorizza tutti i clienti all'uscita: 
         * gli unici per cui l'autorizzazione è rilevante sono i clienti che non acquistano 
        */
        for(size_t i = 0; i < director->tot_clienti; i++){
            if(!(*(director->autorizzazione))) *(director->autorizzazione) = 1;
        }

        /* risevglio tutti i thread in attesa di autorizzazione */
        if(pthread_cond_broadcast(director->ask_auth_cond) == -1){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        if(Lock_Release(director->mtx)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
    }

    /* chiusura di tutte le casse */
    if(Lock_Acquire(director->mtx) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
    }
    for(size_t i = 0; i < director->casse_tot; i++){
        if(!(*(director->cassieri[i].set_close))){
            *(director->cassieri[i].set_close) = 1;
        }
    }
    if(Lock_Release(director->mtx) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
    }

    /* autorizzazione dei clienti rimasti */
    if(Lock_Acquire(director->ask_auth)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
    }
    for(size_t i = 0; i < director->tot_clienti; i++){
        if(!(*(director->autorizzazione))) *(director->autorizzazione) = 1;
    }
    if(pthread_cond_broadcast(director->ask_auth_cond) == -1){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
    }
    if(Lock_Release(director->mtx)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
    }
    pthread_exit((void*)0);
}