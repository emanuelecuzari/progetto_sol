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
#include <bool.h>
#include <direttore.h>
#include <icl_hash.h>
#include <sig_handler.h>
#include <parsing.h>

#define MAX_INPUT_CONFIG 256
#define DEFAULT_CONFIG "config.txt"
#define OPT_PARAMETER ":f:"

#define CONFIG_PRINT(s, x)              \
            printf("===CONFIG===\t");   \
            printf(s, x);               \

/* pid del processo */
pid_t pid;

pthread_t* thid_cutomers;
pthread_t* thid_checkboxes;
pthread_t thid_direttore;
pthread_t thid_sig_handler;
pthread_mutex_t mtx;            /* mutex principale */
pthread_mutex_t check;          /* mutex per direttore, handler e supermercato */
pthread_mutex_t exit;           /* mutex per clienti e supermercato */
pthread_cond_t* cond;           /* variabile di condizionamento per quando cliente in coda */
pthread_cond_t authorized;      /* variabile di condizionamento per autorizzazione clienti */
pthread_cond_t update_cond;     /* variabile di condizionamento per cassa e direttore, controllo invio/ticezione notifica */
static config_t config;
static Queue_t** code;          /* K code */
static int notifica;            /* notifica tra cassa e direttore */
static int casse_aperte;        /* num casse aperte */
static int prodotti;
static int out;                 /* flag per comunicazione uscita tra cliente e direttore*/
static int tot_casse;
static bool autorizzazione;     /* richiesta di autorizzazione a uscire tra cliente e direttore */
static bool update;             /* flag per controllo invioIricezione notifica */

int main(int argc, char* argv[]){
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

    
}