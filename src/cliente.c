#define _POSIX_C_SOURCE 200112L
#include <cassiere.h>
#include <cliente.h>
#include <codaCassa.h>
#include <pthread.h>
#include <signal.h>
#include <statlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <util.h>

/*-----------------------------------------MACRO---------------------------------------*/
#define CHECK_NULL(x)               \
    if (x == NULL) {                \
        perror("CRITICAL ERROR\n"); \
        exit(EXIT_FAILURE);         \
    }
/*-------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------FUNZIONI DI UTILITA'---------------------------------------------------------------------------*/
static int wait_to_pay(argsClienti_t* buyer);
static int wait_authorize(argsClienti_t* buyer);
static int exit_market(argsClienti_t* buyer);
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

void* cliente(void* arg) {
    struct timeval in_supermarket = {0, 0};
    struct timeval out_supermarket = {0, 0};

    argsClienti_t* client = (argsClienti_t*)arg;
    int buying = client->t_acquisti;
    unsigned int seme = client->seed;
    int myid = client->id;

    /* inizio a calcolare tempo in supermercato */
    gettimeofday(&in_supermarket, NULL);
    msleep(buying);

    if (client->num_prodotti > 0) {

        /* array di 1 o 0 */
        int tmp[client->casse_tot];

        while (1) {
            if (Lock_Acquire(client->mtx) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
            if (*(client->set_signal)) {
                #if defined(DEBUG)
                    printf("Signal out\n");
                #endif
                if (Lock_Release(client->mtx) != 0) {
                    perror("CRITICAL ERROR\n");
                    exit(EXIT_FAILURE);
                }
                return NULL;
            }

            int scelta = -1;
            int cnt = 0;
            /* inizializzo tmp */
            for (size_t i = 0; i < client->casse_tot; i++) {
                tmp[i] = 0;
            }

            /* ciclo per scelta e verifica apertura/esistenza cassa */
            while (1) {
                scelta = 1 + (rand_r(&seme) % (client->casse_tot));

                /* verifico se in precedenza ho già controllata la cassa scelta */
                if (tmp[scelta - 1]) continue;
                tmp[scelta - 1] = 1;

                /* ho trovato una cassa aperta */
                if (*(client->cassieri[scelta - 1].set_close) == 0) break;

                cnt++;
                /* ho controllato tutte le casse */
                if (cnt == client->casse_tot) {
                    scelta = -1;
                    break;
                }
            }

            if (scelta == -1) {
                if (Lock_Release(client->mtx) != 0) {
                    perror("CRITICAL ERROR\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }

            client->scelta = scelta;
            *(client->set_wait) = 1;
            /* inizio a calcolare tempo in coda */
            gettimeofday(&client->ts_incoda, NULL);

            /* inserisco */
            if (insert(client->code[scelta - 1], client) == -1) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
            #if defined(DEBUG)
                printf("Il cliente %d è in coda alla cassa %d\n", myid, client->scelta);
            #endif
            if (Lock_Release(client->mtx) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }

            /* attesa turno */
            if(wait_to_pay(client) != 0){
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }

            /* se cliente deve cambiare cassa, rieseguo ciclo */
            if (*(client->set_change) == 0) {
                break;
            } else {
                *(client->set_change) = 0;
                client->cambi++;
            }
        }
        #if defined(DEBUG)
            printf("Il cliente %d ha cambiato %d volte cassa\n", myid, client->cambi);
        #endif
    } 
    else {
        
        //cliente non ha acquistato, richiede autorizzazione a uscire
        if(wait_authorize(client) != 0){
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
    }

    /* gestione uscita cliente */
    #if defined(DEBUG)
        printf("Il cliente %d si sta preparando a uscire\n", myid);
    #endif

    if(exit_market(client) != 0){
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }

    gettimeofday(&out_supermarket, NULL);
    client->t_supermercato = calcola_tempo(in_supermarket.tv_sec, in_supermarket.tv_usec, out_supermarket.tv_sec, out_supermarket.tv_usec);
    #if defined(DEBUG)
        printf("Il cliente %d è stato %.8f secondi nel supermercato\n", myid, client->t_supermercato);
    #endif
    print_cliente(client, client->log_name);
    return NULL;
}

static int wait_to_pay(argsClienti_t* buyer){
    if (Lock_Acquire(buyer->personal) != 0) {
        return -1;
    }
    while (*(buyer->set_wait)) {
        if (cond_wait(buyer->wait, buyer->personal) == -1) {
            return -1;
        }
    }
    if (Lock_Release(buyer->personal) != 0) {
        return -1;
    }
    return 0;
}

static int wait_authorize(argsClienti_t* buyer){
    if (Lock_Acquire(buyer->ask_auth) != 0) {
        return -1;
    }
    while (!(*(buyer->autorizzazione))) {
        if (cond_wait(buyer->ask_auth_cond, buyer->ask_auth) == -1) {
            return -1;
        }
    }
    #if defined(DEBUG)
        printf("autorizzazione concessa\n");
    #endif
    if (Lock_Release(buyer->ask_auth) != 0) {
        return -1;
    }
    return 0;
}

static int exit_market(argsClienti_t* buyer){
    if (Lock_Acquire(buyer->out) != 0) {
        return -1;
    }
    *(buyer->is_out) = 1;
    *(buyer->num_uscite) += 1;
    if (cond_signal(buyer->out_cond) == -1) {
        return -1;
    }

    if (Lock_Release(buyer->out) != 0) {
        return -1;
    }
    return 0;
}