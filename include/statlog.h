/* interfaccia per definizione del file di log */

#if !defined(STATLOG_H)
#define STATLOG_H

int report(argsCassiere_t* cassa, const char* filename, int k, int num_clienti);
/**
 * funzione che stampa resoconto del supermercato
 * @param cassa array di cassieri
 * @param filename nome del file di log su cui salvo dati
 * @param k numero casse totale
 * @param num_clienti numero clienti totale del supermercato
 * @return -1 failure, 0 success
*/

int print_cliente(argsClienti_t* cliente, const char* filename);
/**
 * funzione che stampa i dati relativi a un cliente
 * @param cliente che sta mettendo i suoi dati sul file di log
 * @param filename nome del file di log
 * @return -1 failure, 0 success
*/

#endif /* STATLOG_H */