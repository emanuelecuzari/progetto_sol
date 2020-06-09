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
#define TIME(start_sec, start_usec, end_sec, end_usec)                              \
            return ((end_sec * 1000 + end_usec) - (start_sec * 1000 + start_usec))

#define CHECK_NULL(x)                       \
            if(x == NULL){                  \
                perror("CRITICAL ERROR\n"); \
                kill(pid, SIGKILL);         \
            }
/*-------------------------------------------------------------------------------------*/

void* cliente(void* arg){

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

    struct timeval in_supermarket = {0, 0};
    struct timeval out_supermarket = {0, 0};

    argsCliente_t* client = (argsCliente_t*)arg;

    /* inizio a calcolare tempo in supermercato */
    gettimeofday(&in_supermarket, NULL);
    msleep(client->t_acquisti);

    if(client->num_prodotti > 0){

        /* inizio a calcolare tempo in coda */
        gettimeofday(client->ts_incoda, NULL);
        while(1){

            /* se arriva segnale, esco */
            if(sig_hup || sig_quit) break;

            if(Lock_Acquire(client->mtx) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }

            int scelta = -1;
            int cnt = 0;
            /* array di 1 o 0 */
            int* tmp = malloc(sizeof(int));
            CHECK_NULL(tmp);

            /* ciclo per scelta e verifica apertura/esistenza cassa */
            while(1){
                scelta = 1 + (rand_r(&client->seed) % (client->casse_tot));

                /* verifico se in precedenza ho già controllata la cassa scelta */
                if(tmp[scelta - 1]) continue;
                tmp[scelta - 1] == 1;
                
                /* ho trovato una cassa aperta */
                if(*(client->cassieri[scelta - 1].set_close) == 0) break;
                
                /* ho controllato tutte le casse */
                if(cnt == client->casse_tot - 1){
                    scelta = -1;
                    break;
                }
                cnt++;
            }

            if(scelta == -1) break;

            /* inserisco */
            if(insert(client->code[scelta - 1], client) == -1){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
            *(client->set_wait) = 1;

            /* calcolo inizio tempo in coda */
            gettimeofday(&client->ts_incoda, NULL);

            if(Lock_Release(client->ask_auth) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }

            /* attesa turno */
            if(Lock_Acquire(client->personal) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
            while(*(client->set_wait)){
                if(cond_wait(client->wait, client->personal) == -1){
                    perror("CRITICAL ERROR\n");
                    kill(pid, SIGKILL);
                }
            }
            if(Lock_Release(client->personal) != 0){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
            /* se cliente deve cambiare cassa, rieseguo ciclo */
            if(*(client->set_change) == 0) break;
            else{
                client->cambi++;
            }
        }
    }
    else{
        /* cliente non ha acquistato, richiede autorizzazione a uscire */
        if(Lock_Acquire(client->ask_auth) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        while(!(*(client->autorizzazione))){
            if(cond_wait(client->ask_auth_cond, client->ask_auth) == -1){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGKILL);
            }
        }
        if(Lock_Release(client->ask_auth) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
    }
    /* gestione uscita cliente */
    gettimeofday(&out_supermarket, NULL);
    client->t_supermercato = TIME(in_supermarket.tv_sec, in_supermarket.tv_usec, out_supermarket.tv_sec, out_supermarket.tv_usec);
    if(Lock_Acquire(client->out) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
    }
    /* controllo che le uscite siano esattamente E */
    while(*(client->num_uscite) < client->e){
        client->is_out = 1;
        *(client->num_uscite)++;
        if(cond_signal(client->out_cond){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
    }
    if(Lock_Release(client->out) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGKILL);
    }
    pthread_exit((void*)0);
}