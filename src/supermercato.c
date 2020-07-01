#define _POSIX_C_SOURCE 200112L
#include <cassiere.h>
#include <cliente.h>
#include <codaCassa.h>
#include <direttore.h>
#include <icl_hash.h>
#include <parsing.h>
#include <pthread.h>
#include <signal.h>
#include <statlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <util.h>

/* flags per distinguere cosa fare quando arriva segnale */
volatile sig_atomic_t sig_hup = 0;
volatile sig_atomic_t sig_quit = 0;

/* flag che dice alle casse di chiudere perchè non c'è più alcun cliente */
volatile int stop_casse = 0;

/*-----------------------------------------VARIABILI GLOBALI---------------------------------------*/
static pthread_mutex_t mtx;          /* mutex principale  */
static pthread_mutex_t sent;         /* mutex tra direttore e cassieri */
static pthread_mutex_t* personal;    /* muetx esclusiva del cliente */
static pthread_mutex_t ask_auth;     /* mutex tra direttore e cliente */
static pthread_mutex_t out;          /* mutex tra cliente e supermercato */
static pthread_cond_t ask_auth_cond; /* var di cond. tra direttore e cliente */
static pthread_cond_t out_cond;      /* var di cond. tra cliente e supermercato*/
static pthread_cond_t* wait;         /* var di cond. per attesa turno in coda */
static pthread_cond_t sent_cond;     /* var cond. tra cassieri e direttore */
static int* update;                  /* array per controllare che tutte le casse abbiano notificato */
static int* notifica;
static int* autorizzazione;          /* array di autorizzazioni per uscita clienti */
static int num_uscite = 0;
static int casse_aperte;
static int casse_chiuse;
static int first_to_write = 1;
static int id_cliente = 0;           /* corrisponderà al numero tottale di clienti entrati */
static Queue_t** code;
static pthread_t* thid_casse;        /* array di thid casse */
static pthread_t* thid_clienti;      /* array di thid clienti */
static pthread_t thid_direttore;     /* thid del direttore */
static config_t config;              /* struttura contenente i dati di configurazione */
/*-------------------------------------------------------------------------------------------------*/

/*-----------------------------------------MACRO---------------------------------------*/
#define DEFAULT_CONFIG "config.txt"
#define NBUCKETS 60

#define CHECK_NULL(x)               \
    if (x == NULL) {                \
        perror("CRITICAL ERROR\n"); \
        exit(EXIT_FAILURE);         \
    }

#define CONFIG_PRINT(s, x)        \
    do {                          \
        printf("===CONFIG===\t"); \
        printf(s, x);             \
    } while (0)

#define INIT_MUTEX(x)                        \
    if (pthread_mutex_init(x, NULL) == -1) { \
        perror("in init\n");                 \
        exit(EXIT_FAILURE);                  \
    }

#define INIT_COND(x)                        \
    if (pthread_cond_init(x, NULL) == -1) { \
        perror("in init\n");                \
        exit(EXIT_FAILURE);                 \
    }

/*-------------------------------------------------------------------------------------*/

/*--------------------------------------------------------FUNZIONI DI UTILITA'--------------------------------------------------------*/
static void cleanup();
static void signal_handler(int sig);
static void set_signal_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction SIGHUP\n");
    }
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("sigaction SIGQUIT\n");
    }
}
/*------------------------------------------------------------------------------------------------------------------------------------*/

int main(int argc, char* argv[]) {

    /* installo signal handler */
    set_signal_handler();

    /**
     * seme generato randomicamente
     * usato per tempo fisso cassa e
     * indice coda cassa in cui va cliente
    */
    unsigned int seed = time(NULL);

    if (argc != 3 && argc != 1) {
        fprintf(stderr, "usage: %s -f config_filname\n", argv[0]);
        fprintf(stderr, "usage default: %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int opt;
    memset(&config, 0, sizeof(config_t));

    /* decisione di quale file fare il parsing */
    if (argc == 3) {
        if ((opt = getopt(argc, argv, ":f:")) != -1) {
            switch (opt) {
                case 'f': {
                    if (parse(optarg, &config) == -1) {
                        perror("in parse\n");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                default: {
                    fprintf(stdout, "No correct option found\n");
                    break;
                }
            }
        } else {
            perror("in getopt\n");
            exit(EXIT_FAILURE);
        }
    } else {
        if (parse(DEFAULT_CONFIG, &config) == -1) {
            perror("in parse\n");
            exit(EXIT_FAILURE);
        }
    }

    /* stampa di configurazione */
    CONFIG_PRINT("casse totali: %d\n", config.k);
    CONFIG_PRINT("clienti totali: %d\n", config.c);
    CONFIG_PRINT("entrate/uscite per volta: %d\n", config.e);
    CONFIG_PRINT("max tempo acquisti: %d\n", config.t);
    CONFIG_PRINT("max num prodotti per cliente: %d\n", config.p);
    CONFIG_PRINT("soglia chiusura: %d\n", config.s1);
    CONFIG_PRINT("soglia apertura: %d\n", config.s2);
    CONFIG_PRINT("intervallo notifica: %d\n", config.intervallo);
    CONFIG_PRINT("tempo gestione di un prodotto: %d\n", config.t_gestione_p);
    CONFIG_PRINT("casse iniziali: %d\n", config.casse_start);
    CONFIG_PRINT("nome file di log: %s\n", config.filename);
    printf("\t\t---END CONFIG---\n");

    /* dichiarazione strutture utili */
    argsDirettore_t director;                /* struttura del direttore */
    argsCassiere_t array_cassiere[config.k]; /* array di strutture dei cassieri */
    argsClienti_t array_cliente[config.c];   /* array di strutture dei clienti */
    int set_close[config.k];                 /* array di 1 o 0 per indicare chiusura di una cassa */
    int set_wait[config.c];                  /* array di 1 o 0 per indicare cliente in attesa in coda */
    int set_change[config.c];                /* array di 1 o 0 per indicare cliente che deve cambiare cassa */
    int set_signal;                          /* flag per indicare uscita dovuta a segnale */
    int is_out[config.c];                    /* array di 1 o 0 per indicare se cliente è uscito */

    /* allocazione variabili */
    thid_casse = malloc(config.k * sizeof(pthread_t));
    CHECK_NULL(thid_casse);
    thid_clienti = malloc(config.c * sizeof(pthread_t));
    CHECK_NULL(thid_clienti);
    personal = malloc(config.c * sizeof(pthread_mutex_t));
    CHECK_NULL(personal);
    wait = malloc(config.c * sizeof(pthread_cond_t));
    CHECK_NULL(wait);
    code = (Queue_t**)malloc(config.k * sizeof(Queue_t*));
    CHECK_NULL(code);
    autorizzazione = malloc(config.c * sizeof(int));
    CHECK_NULL(autorizzazione);
    notifica = malloc(config.k * sizeof(int));
    CHECK_NULL(notifica);
    update = malloc(config.k * sizeof(int));
    CHECK_NULL(update);

    /* inizializzazione */
    INIT_MUTEX(&mtx);
    INIT_MUTEX(&sent);
    INIT_MUTEX(&ask_auth);
    INIT_MUTEX(&out);
    INIT_COND(&sent_cond);
    INIT_COND(&ask_auth_cond);
    INIT_COND(&out_cond);
    for (size_t i = 0; i < config.c; i++) {
        INIT_MUTEX(&personal[i]);
    }
    for (size_t i = 0; i < config.c; i++) {
        INIT_COND(&wait[i]);
    }

    num_uscite = 0;
    set_signal = 0;
    casse_aperte = config.casse_start;
    casse_chiuse = config.k - config.casse_start;

    for (size_t i = 0; i < config.k; i++) {
        /* apro solo il num di casse specificato nel file di config */
        if (i < config.casse_start) {
            set_close[i] = 0;
        } else
            set_close[i] = 1;
        code[i] = initQueue(config.c);
        CHECK_NULL(code[i]);
        notifica[i] = 0;
        update[i] = 0;
    }

    for (size_t i = 0; i < config.c; i++) {
        set_wait[i] = 0;
        set_change[i] = 0;
        autorizzazione[i] = 0;
        is_out[i] = 0;
    }

    /* init strutture dati delle casse */
    for (size_t i = 0; i < config.k; i++) {
        array_cassiere[i].id = i + 1;
        array_cassiere[i].chiusure = 0;
        array_cassiere[i].clienti_serviti = 0;
        array_cassiere[i].tot_acquisti = 0;
        array_cassiere[i].tempo_fisso = (rand_r(&seed) % (MAX - MIN)) + MIN;
        array_cassiere[i].gestione_p = config.t_gestione_p;
        array_cassiere[i].opening_time = 0;
        array_cassiere[i].t_notifica = config.intervallo;
        array_cassiere[i].coda = code[i];
        array_cassiere[i].mtx = &mtx;
        array_cassiere[i].sent = &sent;
        array_cassiere[i].sent_cond = &sent_cond;
        array_cassiere[i].update = &update[i];
        array_cassiere[i].set_close = &set_close[i];
        array_cassiere[i].notifica = &notifica[i];
        if (!set_close[i]) {
            if (pthread_create(&thid_casse[i], NULL, cassiere, &array_cassiere[i]) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    /* init struttura dati del direttore */
    director.casse_tot = config.k;
    director.boundClose = config.s1;
    director.boundOpen = config.s2;
    director.tot_clienti = config.c;
    director.notifica = notifica;
    director.mtx = &mtx;
    director.ask_auth = &ask_auth;
    director.sent = &sent;
    director.out = &out;
    director.sent_cond = &sent_cond;
    director.ask_auth_cond = &ask_auth_cond;
    director.out_cond = &out_cond;
    director.casse_aperte = &casse_aperte;
    director.casse_chiuse = &casse_chiuse;
    director.update = update;
    director.autorizzazione = autorizzazione;
    director.cassieri = array_cassiere;
    director.thid_casse = thid_casse;
    if (pthread_create(&thid_direttore, NULL, direttore, &director) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }

    /* init strutture dati dei clienti */
    for (size_t i = 0; i < config.c; i++) {
        array_cliente[i].id = id_cliente++;
        array_cliente[i].cambi = 0;
        array_cliente[i].casse_tot = config.k;
        array_cliente[i].num_prodotti = rand_r(&seed) % config.p;
        array_cliente[i].seed = seed;
        array_cliente[i].first_to_write = &first_to_write;
        array_cliente[i].scelta = 0;
        array_cliente[i].ts_incoda = (struct timeval){0, 0};
        array_cliente[i].tend_incoda = (struct timeval){0, 0};
        array_cliente[i].t_acquisti = (rand_r(&seed) % (config.t - MIN_T_ACQUISTI)) + MIN_T_ACQUISTI;
        array_cliente[i].t_supermercato = 0;
        array_cliente[i].t_servizio = 0;
        array_cliente[i].mtx = &mtx;
        array_cliente[i].personal = &personal[i];
        array_cliente[i].ask_auth = &ask_auth;
        array_cliente[i].out = &out;
        array_cliente[i].ask_auth_cond = &ask_auth_cond;
        array_cliente[i].out_cond = &out_cond;
        array_cliente[i].wait = &wait[i];
        array_cliente[i].is_out = &is_out[i];
        array_cliente[i].autorizzazione = &autorizzazione[i];
        array_cliente[i].set_change = &set_change[i];
        array_cliente[i].set_wait = &set_wait[i];
        array_cliente[i].set_signal = &set_signal;
        array_cliente[i].num_uscite = &num_uscite;
        array_cliente[i].cassieri = array_cassiere;
        array_cliente[i].code = code;
        array_cliente[i].log_name = config.filename;
        if (pthread_create(&thid_clienti[i], NULL, cliente, &array_cliente[i]) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        if (sig_hup) {
            fprintf(stdout, "Ricevuto SIGHUP!\n");
            break;
        }
        if (sig_quit) {
            fprintf(stdout, "Ricevuto SIGQUIT!\n");
            break;
        }

        /* attesa entrata nuovi clienti */
        if (Lock_Acquire(&out) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }

        while (num_uscite < config.e) {
            if (cond_wait(&out_cond, &out) == -1) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
        }
        #if defined(DEBUG)
            printf("Sono usciti %d clienti\n", num_uscite);
        #endif

        /* riciclo i threads cliente controllando che non ne entrino più di E */
        int newIn = 0;
        for (size_t i = 0; i < config.c; i++) {
            if (is_out[i] == 1) {
                if (pthread_join(thid_clienti[i], NULL) != 0) {
                    perror("CRITICAL ERROR\n");
                    exit(EXIT_FAILURE);
                }
                set_wait[i] = 0;
                set_change[i] = 0;
                autorizzazione[i] = 0;
                is_out[i] = 0;
                array_cliente[i].id = id_cliente++;
                array_cliente[i].cambi = 0;
                array_cliente[i].casse_tot = config.k;
                array_cliente[i].num_prodotti = rand_r(&seed) % config.p;
                array_cliente[i].seed = seed;
                array_cliente[i].first_to_write = &first_to_write;
                array_cliente[i].scelta = 0;
                array_cliente[i].ts_incoda = (struct timeval){0, 0};
                array_cliente[i].tend_incoda = (struct timeval){0, 0};
                array_cliente[i].t_acquisti = (rand_r(&seed) % (config.t - MIN_T_ACQUISTI)) + MIN_T_ACQUISTI;
                array_cliente[i].t_supermercato = 0;
                array_cliente[i].t_servizio = 0;
                array_cliente[i].mtx = &mtx;
                array_cliente[i].personal = &personal[i];
                array_cliente[i].ask_auth = &ask_auth;
                array_cliente[i].out = &out;
                array_cliente[i].ask_auth_cond = &ask_auth_cond;
                array_cliente[i].out_cond = &out_cond;
                array_cliente[i].wait = &wait[i];
                array_cliente[i].is_out = &is_out[i];
                array_cliente[i].autorizzazione = &autorizzazione[i];
                array_cliente[i].set_change = &set_change[i];
                array_cliente[i].set_wait = &set_wait[i];
                array_cliente[i].set_signal = &set_signal;
                array_cliente[i].num_uscite = &num_uscite;
                array_cliente[i].cassieri = array_cassiere;
                array_cliente[i].code = code;
                array_cliente[i].log_name = config.filename;
                if (pthread_create(&thid_clienti[i], NULL, cliente, &array_cliente[i]) != 0) {
                    perror("CRITICAL ERROR\n");
                    exit(EXIT_FAILURE);
                }
                newIn++;
            }
            if (newIn == config.e) {
                num_uscite -= config.e;
                break;
            }
        }
        #if defined(DEBUG)
            printf("Sono entrati %d nuovi clienti\n", newIn);
        #endif
        if (Lock_Release(&out) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
    }

    /* avvisa clienti di non mettersi in coda */
    if (Lock_Acquire(&mtx) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    set_signal = 1;
    if (Lock_Release(&mtx) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }

    /* attesa terminazione clienti, casse e direttore */
    #if defined(DEBUG)
        printf("Attesa clienti\n");
    #endif
    for (size_t i = 0; i < config.c; i++) {
        if (pthread_join(thid_clienti[i], NULL) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
    }
    stop_casse = 1;
    for (size_t i = 0; i < config.k; i++) {
        if (*(array_cassiere[i].set_close) == 0) {
            insert(array_cassiere[i].coda, NOCLIENT);
            if (pthread_join(thid_casse[i], NULL) != 0) {
                perror("CRITICAL ERROR\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    if (pthread_join(thid_direttore, NULL) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }

    report(array_cassiere, config.filename, config.k, id_cliente);

    /* funzione di pulizia memoria allocata */
    cleanup();
    fprintf(stdout, "Supermercato chiuso!\n");
    return 0;
}

/**
 * funzione per liberare memoria
*/
static void cleanup() {
    if (thid_casse) free(thid_casse);
    if (thid_clienti) free(thid_clienti);

    if (pthread_mutex_destroy(&mtx) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&out) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&sent) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&ask_auth) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < config.c; i++) {
        if (pthread_mutex_destroy(&personal[i]) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
    }
    free(personal);

    for (size_t i = 0; i < config.c; i++) {
        if (pthread_cond_destroy(&wait[i]) != 0) {
            perror("CRITICAL ERROR\n");
            exit(EXIT_FAILURE);
        }
    }
    free(wait);

    if (pthread_cond_destroy(&ask_auth_cond) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_destroy(&sent_cond) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_cond_destroy(&out_cond) != 0) {
        perror("CRITICAL ERROR\n");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < config.k; i++) {
        if (code[i]) delQueue(code[i]);
    }
    if (code) free(code);

    if (autorizzazione) free(autorizzazione);
}

/**
 * funzione per gestione di diversi tipi di segnale
 * @param sig segnale da gestire
*/
static void signal_handler(int sig) {
    switch (sig) {
        case SIGHUP: {
            sig_hup = 1;
            break;
        }
        case SIGQUIT: {
            sig_quit = 1;
            break;
        }
        default: {
            abort();
            break;
        }
    }
}