#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <icl_hash.h>
#include <parsing.h>

#define NBUCKETS 40
#define TEMPO_MIN 10

/* NOTA: il do...while(0) serve alla macro se il suo corpo non è un single statement */

/* macro per controllare se NULL */
#define CHECK_NULL(x)                       \
        do{                                 \
            if(x == NULL){                  \
                perror("doesn't exist\n");  \
                return -1;                  \
            }                               \
        }while(0)

/** 
 * macro per controllare se x fallisce
 * se vero, dealloca hash e le sue entry
 * x può essere una funzione o una variabile 
*/
#define CHECK_AND_DESTROY(x, fail, hash)            \
        do{                                         \
            if(x == fail){                          \
                perror("ERROR\n");                  \
                icl_hash_destroy(hash, free, free); \
                return -1;                          \
            }                                       \
        }while(0)

int set_val(void* value, icl_hash_t* h, char* key, bool flag){
    CHECK_NULL(h);
    CHECK_NULL(value);
    CHECK_NULL(key);
    void* tmp = icl_hash_find(h, key);
    CHECK_NULL(tmp);
    /* il dato nell'hash è una stringa */
    if(flag){
        int len = strlen(tmp);
        memcpy((char*)value, (char*)tmp, len*sizeof(char));
        return 0;
    }
    if((int*)tmp <= 0){
        perror("negative value!\n");
        return -1;
    }
    *((int*)value) = (int*)tmp;
    return 0;
}

int parse(char* config_filename, config_t* config){
    FILE* fp;
    icl_hash_t* hashtable;
    CHECK_NULL((hashtable=icl_hash_create(NBUCKETS, NULL, NULL)));
    char nome[MAX_INPUT];
    char valore[MAX_INPUT];
    char* real_name = NULL;
    char* real_val = NULL;
    if((fp = fopen(config_filename, "r")) == NULL){
        perror("in apertura file\n");
        return -1;
    }
    char buf[MAXARGS];
    while(fgets(buf, MAXARGS, fp) != NULL){
        if(buffer[0] == '@') continue;
        memset(nome, '\0', MAX_INPUT);
        memset(valore, '\0', MAX_INPUT);
        if(sscanf(buf, "%127[^:]:%127[^\n]", nome, valore) != 2){
            perror("in sscanf\n");
            return -1;
        }
        int len_nome = strlen(nome);
        int len_valore = strlen(valore);
        real_name = calloc(len_nome + 1 , sizeof(char));
        real_val = calloc(len_valore + 1, sizeof(char));
        CHECK_AND_DESTROY(real_name, NULL, hashtable);
        CHECK_AND_DESTROY(real_val, NULL, hashtable);
        memcpy(real_name, nome, len_nome*sizeof(char));
        memcpy(real_val, valore, len_valore*sizeof(char));
        CHECK_AND_DESTROY(icl_hash_insert(hashtable, real_name, real_val), NULL, hashtable);
    }
    CHECK_AND_DESTROY(fclose(fp), EOF, hashtable);

    CHECK_AND_DESTROY(set_val(&config->k, hashtable, "K", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->c, hashtable, "C", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->e, hashtable, "E", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->t, hashtable, "T", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->p, hashtable, "P", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->s1, hashtable, "S1", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->s2, hashtable, "S2", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->intervallo, hashtable, "INTERVALLO", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->t_gestione_p, hashtable, "TEMPO_GESTIONE_PROD", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->casse_start, hashtable, "CASSE_INIZIALI", false), -1, hashtable);
    CHECK_AND_DESTROY(set_val(&config->filename, hashtable, "FILENAME", true), -1, hashtable);

    if(icl_hash_destroy(hashtable, free, free) == -1) return -1;

    /* controlli di validità sui valori letti */
    /* da set_val so già che x>0 */
    if(config->t > TEMPO_MIN){
        perror("TEMPO_MIN deve essere maggiore di t\n");
        return -1;
    }
    if(config->c <= config->e){
        perror("C deve essere maggiore di E\n");
        return -1;
    }
    if(config->casse_start > config->c){
        perror("Il numero di casse iniziali aperte deve essere minore o uguale a C!\n");
        return -1;
    }
    if(config->s1 >= config->c){
        perror("S1 deve essere minore di C!\n");
        return -1;
    }
    return 0;
}