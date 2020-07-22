#define _POSIX_C_SOURCE 200112L
#include <cassiere.h>
#include <cliente.h>
#include <codaCassa.h>
#include <direttore.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <util.h>

#define X 5

/* flags per distinguere cosa fare quando arriva segnale */
extern volatile sig_atomic_t sig_hup;
extern volatile sig_atomic_t sig_quit;

/*--------------------------------------------------------FUNZIONI DI UTILITA'--------------------------------------------------------*/
static int wait_notify(argsDirettore_t* dir);
static int authorize(argsDirettore_t* dir);
/*------------------------------------------------------------------------------------------------------------------------------------*/

void* direttore(void* arg) {
    argsDirettore_t* director = (argsDirettore_t*)arg;
    struct timeval t = {0, 0};
    struct timeval* aggiorna; /* array con tempi aggiornamento casse */
    int open = *(director->casse_aperte);
    int close = *(director->casse_chiuse);

    aggiorna = calloc(director->casse_tot, sizeof(struct timeval));

    while (1) {
        int cntClose = 0;
        int indexClose = -1;
        int indexOpen = -1;
        int canOpen = 0;

        /* controllo se è arrivato un segnale */
        if (sig_hup || sig_quit) {
            break;
        }

        /* gestione arrivo notifica */
        if (Lock_Acquire(director->sent) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }

        /* attendo notifica da parte di tutte le casse */
        if(wait_notify(director) != 0){
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }

        /* reset array notifiche */
        for (size_t i = 0; i < director->casse_tot; i++) {
            director->update[i] = 0;
        }

        #if defined(DEBUG)
            printf("La notifica è stata ricevuta!\n");
        #endif

        /**
         * chiudo la cassa con num clienti <= 1 e aperta da almeno X secondi
         * se ce ne sono più o esattamente boundClose con clienti <= 1
         * 
         * apro la cassa chiusa da più tempo
        */

        gettimeofday(&t, NULL);
        for (size_t i = 0; i < director->casse_tot; i++) {
            if (*(director->cassieri[i].set_close) == 0) {
                if (director->notifica[i] <= 1) {
                    cntClose++;
                    if (*(director->casse_aperte) > MIN_OPEN && cntClose >= director->boundClose) {
                        double diff = calcola_tempo(aggiorna[i].tv_sec, aggiorna[i].tv_usec, t.tv_sec, t.tv_usec);
                        if (diff > X) {
                            indexClose = i;
                        }
                    }
                } else {
                    if (director->notifica[i] >= director->boundOpen) {
                        canOpen = 1;
                    }
                }
            } else {
                if (indexOpen == -1)
                    indexOpen = i;
                else {
                    double evaluate = calcola_tempo(aggiorna[indexOpen].tv_sec, aggiorna[indexOpen].tv_usec, aggiorna[i].tv_sec, aggiorna[i].tv_usec);
                    if (evaluate > 0) indexOpen = i;
                }
            }
        }
        if (Lock_Release(director->sent) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }

        /* chiudo se possibile */
        if (indexClose != -1) {
            if (Lock_Acquire(director->mtx) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
            insert(director->cassieri[indexClose].coda, NOCLIENT);
            *(director->cassieri[indexClose].set_close) = 1;
            *(director->casse_chiuse) += 1;
            *(director->casse_aperte) -= 1;
            close = *(director->casse_chiuse);
            open = *(director->casse_aperte);
            #if defined(DEBUG)
                printf("Chiude la cassa %d\n", indexClose + 1);
                printf("Ci sono ancora %d casse aperte e %d casse chiuse\n", open, close);
            #endif

            if (Lock_Release(director->mtx) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }

            if (pthread_join(director->thid_casse[indexClose], NULL) == -1) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }

            /* update del tempo di ultimo aggiornamento per la cassa indexClose */
            aggiorna[indexClose] = t;
        }

        /* apro se possibile */
        if (canOpen && indexOpen != -1) {
            if (Lock_Acquire(director->mtx) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }

            *(director->cassieri[indexOpen].set_close) = 0;
            *(director->casse_chiuse) -= 1;
            *(director->casse_aperte) += 1;
            close = *(director->casse_chiuse);
            open = *(director->casse_aperte);
            if (pthread_create(&director->thid_casse[indexOpen], NULL, cassiere, &director->cassieri[indexOpen]) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
            #if defined(DEBUG)
                printf("Apre la cassa %d\n", indexOpen + 1);
                printf("Ci sono ancora %d casse aperte e %d casse chiuse\n", open, close);
            #endif

            if (Lock_Release(director->mtx) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }

            aggiorna[indexOpen] = t;
        }

        /* autorizzazione uscita cliente */
        if(authorize(director) != 0){
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
    }

    #if defined(DEBUG)
        printf("Segnale arrivato: direttore chiude tutto\n");
    #endif

    /* autorizzazione dei clienti rimasti */
    if(authorize(director) != 0){
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }

    return NULL;
}

static int wait_notify(argsDirettore_t* dir){
    for (size_t i = 0; i < dir->casse_tot; i++) {
        while ((dir->update[i] == 0) && (*(dir->cassieri[i].set_close) == 0) && !(sig_hup || sig_quit)) {
            if (cond_wait(dir->sent_cond, dir->sent) == -1) {
                return -1;
            }
        }
    }
    return 0;
}

static int authorize(argsDirettore_t* dir){
     if (Lock_Acquire(dir->ask_auth) != 0) {
        return -1;
    }

    /** 
     * autorizza tutti i clienti all'uscita: 
     * gli unici per cui l'autorizzazione è rilevante sono i clienti che non acquistano 
    */
    for (size_t i = 0; i < dir->tot_clienti; i++) {
        if (!(dir->autorizzazione[i])) dir->autorizzazione[i] = 1;
    }

    /* risevglio tutti i thread in attesa di autorizzazione */
    if (pthread_cond_broadcast(dir->ask_auth_cond) == -1) {
        return -1;
    }
    #if defined(DEBUG)
        printf("Clienti autorizzati\n");
    #endif
    if (Lock_Release(dir->ask_auth) != 0) {
        return -1;
    }
    return 0;
}