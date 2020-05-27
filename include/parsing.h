/* interfaccia per definizione parsing del file di configurazione */

#if !defined(PARSING_H)
#define PARSING_H

#define MAXARGS 256
#define MAX_INPUT 128

typedef struct config{
    int k;                          /* casse totali */
    int c;                          /* num max clienti */
    int e;                          /* compreso tra 0 e c, num clienti che possono entrare/usicre alla volta */
    int t;                          /* max tempo per acqusiti */
    int p;                          /* max num prodotti acquistabili */
    int s1;                         /* soglia per chiusura cassa */
    int s2;                         /* soglia per apertura cassa */ 
    int intervallo;                 /* intervallo di tempo per trasmissione notifica */
    int t_gestione_p;               /* tempo elaborazione di un prodotto*/
    int casse_start;                /* casse aperte all'inizio */
    char filename[MAX_INPUT];       /* nome file di log */
}config_t;

int parse(char* config_filename, config_t* config);
/**
 * funzione che svolge parsing del file di configurazione
 * @param config_filename nome del file di configurazione
 * @param config struttura contenente i dati a cui asscoiare i valori presenti nel file
 * @return 0 success, -1 failure
*/

int set_val(void* value, icl_hash_t* h, char* key, bool flag);
/**
 * funzione che setta il valore della variabile nella struttura
 * @param value elemento della struttura dati cui associare il valore letto nel file
 * @param h hash table contenente i valori letti dal file
 * @param key chiave che devo cercare nell'hash per prelevare il valore da assegnare
 * @param flag booleano per distinguere stringhe da valori interi contenuti nel file
 * @return 0 success, -1 failure
*/