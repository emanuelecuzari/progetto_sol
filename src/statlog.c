#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <cassiere.h>
#include <cliente.h>
#include <util.h>
#include <statlog.h>
#include <queue.h>

int report(argsCassiere_t* cassa, const char* filename, int k, int num_clienti){
    FILE* fp;
    if((fp = fopen(filename, "a")) == NULL){
        perror("in apertura file di log\n");
        return -1;
    }
    int tot_prodotti = 0;
    void* tmp = NULL;

    /**
     * dati delle casse
     * FORMATO
     * CASSA:id:clienti_serviti:prodotti_elaborati:tempo_apertura:numero_chiusure\n
     * TEMPI DI SERVIZIO
     * [lista tempi servizio]
     * STOP
    */
    for(size_t i = 0; i < k; i++){
        tot_prodotti += cassa[i].tot_acquisti;
        fprintf(fp, "CASSA:%d:%d:%d:%.8f:%d\n",
                cassa[i].id, cassa[i].clienti_serviti, cassa[i].tot_acquisti, cassa[i].opening_time, cassa[i].chiusure);
        fprintf(fp, "TEMPI DI SERVIZIO\n");
        while((tmp = popLQueue(cassa[i].tservice_list)) != NULL){
            fprintf(fp, "%.8f\n", *((double*)tmp));
        }
        fprintf(fp, "STOP\n");
    }
    free(tmp);

    /**
     * dati del supermercato
     * FORMATO
     * SUPERMERCATO:num_clienti_tot:num_prodotti_acquistati_tot\n
    */
    fprintf(fp, "SUPERMERCATO:%d:%d\n", num_clienti, tot_prodotti);
    if(fclose(fp) == EOF){
        perror("CRITICAL\n");
        return -1;
    }

    return 0;
}

int print_cliente(argsClienti_t* cliente, const char* filename){

    FILE* fp;
    if(*(cliente->first_to_write)){
       if((fp = fopen(filename, "w")) == NULL){
        perror("in apertura file di log\n");
        return -1;
        }
        *(cliente->first_to_write) = 0;
    }
    else{
        if((fp = fopen(filename, "a")) == NULL){
            perror("in apertura file di log\n");
            return -1;
        }
    }

    /**
     * dati dei clienti
     * FORMATO
     * CLIENTE:id:tempo_permanenza:tempo_in_coda:numero_cambi:prodtti_acquistati:cassa_scelta\n
    */
    double t_incoda = calcola_tempo(cliente->ts_incoda.tv_sec, cliente->ts_incoda.tv_usec, cliente->tend_incoda.tv_sec, cliente->tend_incoda.tv_usec);
    fprintf(fp, "CLIENTE:%d:%.8f:%.8f:%d:%d:%d\n", cliente->id, cliente->t_supermercato, t_incoda, cliente->cambi, cliente->num_prodotti, cliente->scelta);

    if(fclose(fp) == EOF){
        perror("CRITICAL\n");
        return -1;
    }
    return 0;
}