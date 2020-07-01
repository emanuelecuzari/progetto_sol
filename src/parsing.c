#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <icl_hash.h>
#include <parsing.h>
#include <util.h>

#define NBUCKETS 40
#define TEMPO_MIN 10

/*-----------------------------------------MACRO---------------------------------------*/
#define CHECK_NULL(x)                       \
            if(x == NULL){                  \
                perror("doesn't exist\n");  \
                return -1;                  \
            }

#define FREE_ON_FAIL(x, fail, h)                    \
            if(x == fail){                          \
                icl_hash_destroy(h , free, free);   \
                return -1;                          \
            }

#define SET_INT(config_param, h, key)                       \
            do{                                             \
                int x;                                      \
                char* tmp = (char*)icl_hash_find(h, key);   \
                if(sscanf(tmp, "%d", &x) != 1){             \
                    perror("in sscanf\n");                  \
                    return -1;                              \
                }                                           \
                if(x <= 0){                                 \
                    perror("in SET_INT");                   \
                    icl_hash_destroy(h , free, free);       \
                    return -1;                              \
                }                                           \
                config_param = x;                           \
            }while(0)

#define SET_CHAR(config_param, h, key)                      \
            do{                                             \
                char* tmp = (char*)icl_hash_find(h, key);   \
                int len = strlen(tmp);                      \
                strncpy(config_param, tmp, len);             \
            }while(0)
/*-------------------------------------------------------------------------------------*/

int parse(char* config_filename, config_t* config){
    FILE* fp;
    icl_hash_t* hash;
    hash = icl_hash_create(NBUCKETS, NULL, NULL);
    CHECK_NULL(hash);
    if((fp = fopen(config_filename, "r")) == NULL){
        perror("file not open\n");
        return -1;
    }
    char buf[MAXARGS];
    char* tag = NULL;
    char* valore = NULL;
    while(fgets(buf, MAXARGS, fp) != NULL){
        if(buf[0] == '>') continue;
        tag = malloc(MAX_INPUT * sizeof(char));
        FREE_ON_FAIL(tag, NULL, hash);
        valore = malloc(MAX_INPUT * sizeof(char));
        FREE_ON_FAIL(valore, NULL, hash);
        if(sscanf(buf, "%[^:]:%[^\n]", tag, valore) != 2){
            perror("in sscanf\n");
            return -1;
        }
        FREE_ON_FAIL(icl_hash_insert(hash, tag, valore), NULL, hash);
    }
    FREE_ON_FAIL(fclose(fp), EOF, hash);

    SET_INT(config->k, hash, "K");
    SET_INT(config->c, hash, "C");
    SET_INT(config->e, hash, "E");
    SET_INT(config->t, hash, "T");
    SET_INT(config->p, hash, "P");
    SET_INT(config->s1, hash, "S1");
    SET_INT(config->s2, hash, "S2");
    SET_INT(config->intervallo, hash, "INTERVALLO");
    SET_INT(config->t_gestione_p, hash, "TEMPO_GESTIONE_PROD");
    SET_INT(config->casse_start, hash, "CASSE_INIZIALI");
    SET_CHAR(config->filename, hash, "FILENAME");

    /* controlli sui valori */
    if(config->casse_start > config->k){
        fprintf(stderr, "K deve essere maggiore di CASSE_INIZIALI!\n");
        return -1;
    }
    if(config->e > config->c){
        fprintf(stderr, "E deve essere minore di C!\n");
        return -1;
    }
    if(config->t <= TEMPO_MIN){
        fprintf(stderr, "T deve essere maggiore di TEMPO_MIN!\n");
        return -1;
    }
    CHECK_NULL(config->filename);
    return 0;
}