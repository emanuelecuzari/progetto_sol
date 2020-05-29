#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <codaCassa.h>
#include <cliente.h>
#include <cassiere.h>
#include <util.h>
#include <direttore.h>
#include <icl_hash.h>
#include <parsing.h>

#define MAX_INPUT_CONFIG 256
#define DEFAULT_CONFIG "config.txt"
#define OPT_PARAMETER ":f:"
#define BUCKETS 60

/* pid del processo */
pid_t pid;

/*-----------------------------------MACRO-------------------------------------*/
#define CHECK_NULL(x)                       \
        do{                                 \
            if(x == NULL){                  \
                perror("doesn't exist\n");  \
                kill(pid, SIGUSR2);         \
            }                               \
        }while(0)

#define CONFIG_PRINT(s, x)              \
        do{                             \
            printf("===CONFIG===\t");   \
            printf(s, x);               \
        }while(0)
/*-----------------------------------------------------------------------------*/

/*-----------------------------------VARIABILI GLOBALI-------------------------------------*/
pthread_t* thid_cutomers;
pthread_t* thid_checkboxes;
pthread_t thid_direttore;
pthread_mutex_t mtx;            /* mutex principale */
pthread_mutex_t check;          /* mutex per direttore e supermercato */
pthread_mutex_t exit;           /* mutex per direttore e supermercato */
pthread_mutex_t* personal;      /* mutex personale del cliente */
pthread_cond_t* cond;           /* variabile di condizionamento per quando cliente in coda */
pthread_cond_t authorized;      /* variabile di condizionamento per autorizzazione clienti */
pthread_cond_t update_cond;     /* variabile di condizionamento per cassa e direttore, controllo invio/ticezione notifica */
static config_t config;
static Queue_t** code;          /* K code */
static int notifica;            /* notifica tra cassa e direttore */
static int id_cliente = 0;      /* id cliente, sarà num totale clienti entrati */
static int casse_aperte;        /* num casse aperte */
static int out;                 /* flag per comunicazione uscita tra cliente e direttore*/
static int tot_casse;
static bool autorizzazione;     /* richiesta di autorizzazione a uscire tra cliente e direttore */
static bool update;             /* flag per controllo invio ricezione notifica */
static bool* is_out;


static volatile sig_atomic_t supermarket_state; /* stato del supermercato */
volatile sig_atomic_t sig_hup = 0;              /* flag per distinzione segnale arrivato */
volatile sig_atomic_t sig_quit = 0;
/*-------------------------------------------------------------------------------------------*/

/*-----------------------------------FUNZIONI DI UTILITA'-------------------------------------*/
static bool check_supermarket_state(sig_atomic_t state);
static void signal_handler(int sig);
static void cleanup();
/*--------------------------------------------------------------------------------------------*/

int main(int argc, char* argv[]){
    int sig;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTSTP);
    /* la signal mask è set; i segnali bloccati sono quelli in set */
    if(pthread_sigmask(SIG_SETMASK, &set, NULL)!=0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }

    /* installo signal handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    /* ignoro SIGINT E SIGTSTP */
    sa.sa_handler = SIG_IGN;
    sa.sa_mask = set;
    if(sigaction(SIGINT, &sa, NULL) == -1){
        perror("sigaction SIGINT\n");
    }
    if(sigaction(SIGTSTP, &sa, NULL) == -1){
        perror("sigaction SIGTSTP\n");
    }
    /* gestione diversa per altri segnali */
    sa.sa_handler = signal_handler;
    if(sigaction(SIGHUP, &sa, NULL) == -1){
        perror("sigaction SIGHUP\n");
    }
    if(sigaction(SIGQUIT, &sa, NULL) == -1){
        perror("sigaction SIGQUIT\n");
    }
    if(sigaction(SIGUSR2, &sa, NULL) == -1){
        perror("sigaction SIGUSR2\n");
    }
    
    /**
     * seme generato randomicamente
     * usato per tempo fisso cassa e
     * indice coda cassa in cui va cliente
    */
    unsigned int seed = time(NULL);
    srand(seed);

    /* salvo pid processo */
    pid = getpid();

    if(argc != 3 && argc != 1){
        fprintf(stderr, "usage: %s -f nome_config\n", argv[0]);
        fprintf(stderr, "usage default: %s [-f nome_config]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char config_file[MAX_INPUT_CONFIG];
    int opt;
    
    /* setting del file di config */
    if(argc == 3){
        if(opt = getopt(argc, argv, OPT_PARAMETER) != -1){
            switch(opt){
                case 'c':{
                    if(sscanf(optarg, "%[^\n]", config_file)) != 1){
                        perror("in sscanf\n");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                default:{
                    fprintf(stderr, "No option found\n");
                    break;
                }
            }
        }
        else{
            perror("in getopt\n");
            exit(EXIT_FAILURE);
        }
    }
    else{
        strcpy(config_file, DEFAULT_CONFIG);
    }
    memset(&config, 0, sizeof(config_t));
    if(parse(config_file, config) == -1){
        fprintf(stderr, "Error while parsing file\n");
        exit(EXIT_FAILURE);
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
    printf("===END CONFIG===\n");

    /* dichiarazione strutture dati utili */
    directorArgs_t director;                        /* struttura del direttore */
    Cassa_t array_casse[config.k];                  /* strutture delle casse */
    clientArgs_t* array_clienti[config.c];           /* strutture dei clienti */
    stato_cassa_opt array_stato_cassa[config.k];    /* array stati casse */
    client_state_opt array_stato_cliente[config.c]; /* array stati clienti */ 
    supermarket_state = ATTIVO;
    icl_hash_t* hash_direttore;

    /* allocazione */
    thid_cutomers = malloc(config.c * sizeof(pthread_t));
    CHECK_NULL(thid_cutomers);
    thid_checkboxes = malloc(config.k * sizeof(pthread_t));
    CHECK_NULL(thid_checkboxes);
    personal = malloc(config.c * sizeof(pthread_mutex_t));
    CHECK_NULL(personal);
    cond = malloc(config.c * sizeof(pthread_cond_t));
    CHECK_NULL(cond);

    /* inizializzazione */
    if(pthread_mutex_init(&mtx, NULL) != 0){
        perror("init mutex mtx\n");
        kill(pid, SIGUSR2);
    }
    if(pthread_mutex_init(&check, NULL) != 0){
        perror("init mutex check\n");
        kill(pid, SIGUSR2);
    }
    if(pthread_mutex_init(&exit, NULL) != 0){
        perror("init mutex exit\n");
        kill(pid, SIGUSR2);
    }
    for(size_t i = 0; i < config.c; i++){
        if(pthread_mutex_init(&personal[i], NULL) != 0){
        perror("init mutex personal\n");
        kill(pid, SIGUSR2);
        }
    }
    if(pthread_cond_init(&authorized, NULL) != 0){
        perror("init cond authorized\n");
        kill(pid, SIGUSR2);casse
    }
    if(pthread_cond_init(&update_cond, NULL) != 0){
        perror("init cond update_cond\n");
        kill(pid, SIGUSR2);
    }
    for(size_t i = 0; i < config.c; i++){
        if(pthread_cond_init(&cond[i], NULL) != 0){
        perror("init cond\n");
        kill(pid, SIGUSR2);
        }
    }

    code = (Queue_t**)malloc(config.k * sizeof(Queue_t*));
    CHECK_NULL(code);
    for(size_t i = 0; i < config.k; i++){
        CHECK_NULL(code[i] = initQueue(config.c));
    }

    CHECK_NULL(hash_direttore = icl_hash_create(BUCKETS, NULL, NULL));

    autorizzazione = false;
    update = false;
    out = 0;
    tot_casse = config.k;
    casse_aperte = config.casse_start;
    notifica = 0;

    for(size_t i = 0; i < tot_casse; i++){
        array_stato_cassa[i] = (i < casse_aperte) ? OPEN : CLOSED;
    }
    for(size_t i = 0; i < config.c; i++){
        array_stato_cliente[i] = IN_MARKET;
        is_out[i] = false;
    }

    /* creazione del thread cassa */
    for(size_t i = 0; i < tot_casse; i++){
        array_casse[i].id = i+1;
        array_casse[i].notifica = &notifica;
        array_casse[i].t_trasmissione = config.intervallo;
        array_casse[i].gest_prod = config.t_gestione_p;
        array_casse[i].t_fisso = rand_r(&seed) % (MAX_REGULAR_TIME - MIN_REGULAR_TIME) + MIN_REGULAR_TIME;
        array_casse[i].coda = code[i];
        array_casse[i].mtx = &mtx;
        array_casse[i].cond = cond[i];
        array_casse[i].update_cond = &update_cond;
        array_casse[i].state = array_stato_cassa[i];
        if(array_stato_cassa[i] == OPEN){
            if(pthread_create(&thid_checkboxes[i], NULL, Cassiere, array_casse[i]) != 0){
                perror("in create director\n");
                kill(pid, SIGUSR2);
            }
        }
    }

    /* creazione thread cliente */
    for(size_t i = 0; i < config.c; i++){
        array_clienti[i].id = id_cliente++;
        array_clienti[i].prodotti = rand_r(&seed) % config.p;
        array_clienti[i].num_casse = tot_casse;
        array_clienti[i].casse_aperte = casse_aperte;
        array_clienti[i].out = 0;
        array_clienti[i].tot_uscite = 0;
        array_clienti[i].seed = seed;
        array_clienti[i].t_acquisti = rand_r(&seed) % (config.t - T_MIN) + T_MIN;
        array_clienti[i].autorizzazione = false;
        array_clienti[i].mtx = &mtx;
        array_clienti[i].exit = &exit;
        array_clienti[i].cond = cond[i];
        array_clienti[i].authorized = &authorized;
        array_clienti[i].cashbox_queue = code[i];
        array_clienti[i].state = array_stato_cliente[i];
        array_clienti[i].cashbox_state = array_stato_cassa[i];
        if(pthread_create(&thid_cutomers[i], NULL, cliente, array_clienti[i]) != 0){
            perror("in create director\n");
            kill(pid, SIGUSR2);
        }
    }
    
    /* creazione thread direttore */
    director.tot_casse = tot_casse;
    director.e = config.e;
    director.boundClose = config.s1;
    director.boundOpen = config.s2;
    director.casse_chiuse = &(tot_casse - casse_aperte);
    director.casse_aperte = &casse_aperte;
    director.tot_clienti = &config.c;
    director.update = &update;
    director.is_out = is_out;
    director.checkbox = array_casse;
    director.stato_casse = array_stato_cassa;
    director.hashtable = hash_direttore;
    /* manca array clienti */
    director.stato = director_state;
    director.thid_casse = thid_checkboxes;
    director.thid_clienti = thid_cutomers;
    director.mtx = &mtx;
    director.check = &check;
    director.update_cond = &update_cond;
    if(pthread_create(&thid_direttore, NULL, Direttore, &director) != 0){
        perror("in create director\n");
        kill(pid, SIGUSR2);
    }

    while(1){
        if(!check_supermarket_state(supermarket_state)) break;

        /* autorizzazione entrata cliente */
        if(Lock_Acquire(&exit)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        while(*(director.conta_uscite) < config.e){
            if(cond_wait(&exit, &exit_cond) == -1){
                perror("CRITICAL ERROR\n");
                kill(pid, SIGUSR2);
            }
        }
        /* riciclo thread cliente uscito(terminato) */
        for(size_t i = 0; i <= config.c; i++){
            if(director->is_out[i] == true){
                if(i < config.e){
                    if(pthread_join(&thid_customers[i], NULL) != 0){
                        perror("CRITICAL ERROR\n");
                        kill(pid, SIGUSR2);
                    }
                    array_clienti[i] = malloc(sizeof(clientArgs_t));
                    array_stato_cliente[i] = IN_MARKET;
                    CHECK_NULL(array_clienti[i]);
                    array_clienti[i].id = id_cliente++;
                    array_clienti[i].prodotti = rand_r(&seed) % config.p;
                    array_clienti[i].num_casse = tot_casse;
                    array_clienti[i].casse_aperte = casse_aperte;
                    array_clienti[i].out = 0;
                    array_clienti[i].tot_uscite = 0;
                    array_clienti[i].seed = seed;
                    array_clienti[i].t_acquisti = rand_r(&seed) % (config.t - T_MIN) + T_MIN;
                    array_clienti[i].autorizzazione = false;
                    array_clienti[i].mtx = &mtx;
                    array_clienti[i].exit = &exit;
                    array_clienti[i].cond = cond[i];
                    array_clienti[i].authorized = &authorized;
                    array_clienti[i].cashbox_queue = code[i];
                    array_clienti[i].state = array_stato_cliente[i];
                    array_clienti[i].cashbox_state = array_stato_cassa[i];
                    if(pthread_create(&direttore->thid_clienti[i], NULL, cliente, array_clienti[i]) != 0){
                        perror("CRITICAL ERROR\n");
                        kill(pid, SIGUSR2);
                    }
                }
            }
        }
        printf("Sono entrati %d nuovi clienti", i+1);
        if(Lock_Release(&direttore->exit)!=0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }

        /* gestione segnali */
        if(sigwait(&set, &sig) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGKILL);
        }
        if(sig_hup){
            fprintf(stdout, "Ricevuto SIGHUP!\n");
            sig_hup = 0;
        }
        if(sig_quit){
            fprintf(stdout, "Ricevuto SIGQUIT!\n");
            sig_quit = 0;
        }
    }
    /* attesa terminazione clienti, casse e direttore */
    for(size_t i = 0; i < config.k; i++){
        if(pthread_join(&thid_checkboxes[i], NULL) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
    }
    for(size_t i = 0; i < config.c; i++){
        if(pthread_join(&thid_customers[i], NULL) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
    }
    if(pthread_join(&thid_direttore, NULL) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    cleanup();
    if(icl_hash_destroy(hash_direttore, free, free) == -1){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    fprintf(stdout, "Supermercato chiuso!\n");
    return 0;
}

/**
 * funzione per controllare stato del supermercato
 * @param state stato del supermercato
 * @return true se stato ATTIVO, false altrimenti
*/
static bool check_supermarket_state(sig_atomic_t state){
    if(Lock_Acquire(&check) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    if(state != ATTIVO){
        if(Lock_Acquire(&check) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
        return false;
    }
    if(Lock_Acquire(&check) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    return true;
}

/**
 * funzione per liberare memoria
*/
static void cleanup(){
    if(thid_checkboxes) free(thid_checkboxes);
    if(thid_cutomers) free(thid_cutomers);

    if(pthread_mutex_destroy(&mtx) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    if(pthread_mutex_destroy(&check) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    if(pthread_mutex_destroy(&exit) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    for(size_t i = 0; i < config.c; i++){
        if(pthread_mutex_destroy(&personal[i]) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
    }
    free(personal);
    for(size_t i = 0; i < config.c; i++){
        if(pthread_cond_destroy(&cond[i]) != 0){
            perror("CRITICAL ERROR\n");
            kill(pid, SIGUSR2);
        }
    }
    free(cond);
    if(pthread_cond_destroy(&authorized) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }
    if(pthread_cond_destroy(&update_cond) != 0){
        perror("CRITICAL ERROR\n");
        kill(pid, SIGUSR2);
    }

    for(size_t i = 0; i < config.k; i++){
        if(code[i]) delQueue(code[i]);
    }
    if(code) free(code);

    if(is_out) free(is_out);
}

/**
 * funzione per gestione di diversi tipi di segnale
 * @param sig segnale da gestire
 * @return true se segnale è SIGHUP o SIGQUIT, false altrimenti
*/
static void signal_handler(int sig){
    switch(sig){
        case SIGHUP: {
            supermarket_state = INT_SIGHUP;
            sig_hup = 1;
        }
        case SIGQUIT: {
            supermarket_state = INT_SIGQUIT;
            sig_quit = 1;
        }
        case SIGUSR2: {
            kill(pid, SIGKILL);
        }
        default: {
            abort();
        }
    }
}