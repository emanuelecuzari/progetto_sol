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
pid_t pid;

/* flags per distinguere cosa fare quando arriva segnale */
volatile sig_atomic_t sig_hup;
volatile sig_atomic_t sig_quit;

/*-----------------------------------------VARIABILI GLOBALI---------------------------------------*/
pthread_mutex_t mtx;                    /* mutex principale  */
pthread_mutex_t sent;                   /* mutex tra direttore e cassieri */
pthread_mutex_t* personal;              /* muetx esclusiva del cliente */
pthread_mutex_t ask_auth;               /* mutex tra direttore e cliente */
pthread_mutex_t out;                    /* mutex tra cliente e supermercato */
pthread_cond_t ask_auth_cond;           /* var di cond. tra direttore e cliente */
pthread_cond_t out_cond;                /* var di cond. tra cliente e supermercato*/
pthread_cond_t* wait;                   /* var di cond. per attesa turno in coda */
pthread_cond_t sent_cond;               /* var cond. tra cassieri e direttore */
static int update;                                   
static int notifica;
static int autorizzazione;              
static int num_uscite;                  
static int casse_aperte;                
static int casse_chiuse;                
static Queue_t** code;               
static LQueue_t* t_servizio;
static pthread_t* thid_casse;           /* array di thid casse */
static pthread_t* thid_clienti;         /* array di thid clienti */
/*-------------------------------------------------------------------------------------------------*/

/*-----------------------------------------MACRO---------------------------------------*/
#define CHECK_NULL(x)                       \
            if(x == NULL){                  \
                perror("CRITICAL ERROR\n"); \
                kill(pid, SIGKILL);         \
            }
/*-------------------------------------------------------------------------------------*/

int main(int argc, char* argv[]){

}
