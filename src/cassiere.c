#define _POSIX_C_SOURCE 200112L
#include <cassiere.h>
#include <cliente.h>
#include <codaCassa.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <util.h>
#include <queue.h>

/* flags per distinguere cosa fare quando arriva segnale */
extern volatile sig_atomic_t sig_hup;
extern volatile sig_atomic_t sig_quit;
extern volatile int stop_casse;

/*-----------------------------------------MACRO---------------------------------------*/
#define CHECK_NULL(x)               \
    if (x == NULL) {                \
        perror("CRITICAL ERROR\n"); \
        exit(EXIT_FAILURE);         \
    }
/*-------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------FUNZIONI DI UTILITA'---------------------------------------------------------------------------*/
static inline int calcola_servizio(int fisso, int n, int t_singolo) { return (fisso + n * t_singolo); }
static int invia_notifica(argsCassiere_t* c, int id);
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

void* cassiere(void* arg) {
    argsCassiere_t* cassa = (argsCassiere_t*)arg;
    struct timeval ts_apertura = {0, 0};
    struct timeval tend_apertura = {0, 0};
    double t_incoda;
    int myid = cassa->id;
    size_t t_invio = cassa->t_notifica;

    /* inizio a calcolare tempo di apertura */
    gettimeofday(&ts_apertura, NULL);
    while (1) {

        /* controllo se la cassa è stata chiusa */
        if (Lock_Acquire(cassa->mtx) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
        if (*(cassa->set_close)) {
            #if defined(DEBUG)
                printf("La cassa %d è stata chiusa\n", myid);
            #endif
            if (Lock_Release(cassa->mtx) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
        if (Lock_Release(cassa->mtx) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }

        /* controllo se sono arrivati segnali */
        if (sig_hup || sig_quit) break;

        void* test = pop(cassa->coda);
        if (test == NOCLIENT) break;
        CHECK_NULL(test);

        argsClienti_t* cliente = (argsClienti_t*)test;

        /* fine tempo attesa in coda del cliente */
        gettimeofday(&cliente->tend_incoda, NULL);
        t_incoda = calcola_tempo(cliente->ts_incoda.tv_sec, cliente->ts_incoda.tv_usec, cliente->tend_incoda.tv_sec, cliente->tend_incoda.tv_usec);
        #if defined(DEBUG)
            printf("Il cliente %d è stato in coda per %.8f secondi\n", cliente->id, (float)t_incoda);
        #endif

        double service_time = calcola_servizio(cassa->tempo_fisso, cliente->num_prodotti, cassa->gestione_p);

        /* aggiungo tempo servizio in lista */
        double* temp_tserv = malloc(sizeof(double));
        CHECK_NULL(temp_tserv);
        *temp_tserv = service_time / 1000;
        if(insertLQueue(cassa->tservice_list, temp_tserv) != 0){
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }

        /* gestione invio notifica a direttore */
        while (t_invio <= service_time) {

            if(invia_notifica(cassa, myid) != 0){
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }

            msleep(t_invio);
            service_time -= t_invio;
            t_invio = cassa->t_notifica;
        }
        msleep(service_time);
        t_invio -= service_time;
        cassa->clienti_serviti++;
        cassa->tot_acquisti += cliente->num_prodotti;

        /* cliente non è più in attesa in coda, risevglio il thread */
        if (Lock_Acquire(cliente->personal) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
        *(cliente->set_wait) = 0;
        if (cond_signal(cliente->wait) == -1) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
        if (Lock_Release(cliente->personal) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
    }
    /* chiudo o è arrivato segnale, considero l'invio della notifica come effetuato */
    if (Lock_Acquire(cassa->sent) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    if (cond_signal(cassa->sent_cond) == -1) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    if (Lock_Release(cassa->sent) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    argsClienti_t* customer = NULL;

    if (sig_quit || sig_hup) {
        while (!stop_casse) {
            if (cassa->coda->clienti == 0) continue;

            void* test = pop(cassa->coda);
            if (test == NOCLIENT) break;
            CHECK_NULL(test);
            customer = (argsClienti_t*)test;
            gettimeofday(&customer->tend_incoda, NULL);
            t_incoda = calcola_tempo(customer->ts_incoda.tv_sec, customer->ts_incoda.tv_usec, customer->tend_incoda.tv_sec, customer->tend_incoda.tv_usec);

            if (sig_hup) {
                #if defined(DEBUG)
                    printf("Il cliente %d è stato in coda per %.8f secondi\n", customer->id, (float)t_incoda);
                #endif

                double service_time = calcola_servizio(cassa->tempo_fisso, customer->num_prodotti, cassa->gestione_p);

                /* aggiungo tempo servizio in lista */
                double* temp_tserv = malloc(sizeof(double));
                CHECK_NULL(temp_tserv);
                *temp_tserv = service_time / 1000;
                if(insertLQueue(cassa->tservice_list, temp_tserv) != 0){
                    perror("CRITICAL ERROR\n");
                    exit(EXIT_FAILURE);
                }

                msleep(service_time);
                cassa->clienti_serviti++;
                cassa->tot_acquisti += customer->num_prodotti;
            }

            if (Lock_Acquire(customer->personal) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
            *(customer->set_wait) = 0;
            if (cond_signal(customer->wait) == -1) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
            if (Lock_Release(customer->personal) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    /* chiusura standard */
    else {
        /* tutti gli altri clienti in coda devono cambiare cassa */
        while (1) {
            if (cassa->coda->clienti == 0) break;

            void* test = pop(cassa->coda);
            if (test == NOCLIENT) break;
            CHECK_NULL(test);
            customer = (argsClienti_t*)test;

            if (Lock_Acquire(customer->personal) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
            *(customer->set_change) = 1;
            *(customer->set_wait) = 0;
            if (cond_signal(customer->wait) == -1) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
            if (Lock_Release(customer->personal) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
        }
        cassa->chiusure += 1;
    }
    gettimeofday(&tend_apertura, NULL);
    double tmp_time = calcola_tempo(ts_apertura.tv_sec, ts_apertura.tv_usec, tend_apertura.tv_sec, tend_apertura.tv_usec);
    #if defined(DEBUG)
        printf("La cassa %d è rimasta aperta per %.8f secondi\n", myid, tmp_time);
    #endif
    #if defined(DEBUG)
        printf("La cassa %d ha servito %d clienti\n", myid, cassa->clienti_serviti++);
    #endif
    cassa->opening_time += tmp_time;
    return NULL;
}

static int invia_notifica(argsCassiere_t* c, int id){
    if (Lock_Acquire(c->sent) != 0) {
        return -1;
    }
    #if defined(DEBUG)
        printf("La cassa %d ha inviato la notifica\n", id);
    #endif
    *(c->update) = 1;
    *(c->notifica) = c->coda->clienti;
    if (cond_signal(c->sent_cond) == -1) {
        return -1;
    }
    if (Lock_Release(c->sent) != 0) {
        return -1;
    }
    return 0;
}
    