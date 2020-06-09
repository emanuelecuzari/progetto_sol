

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
                int* tmp = (int*)icl_hash_find(h, key);     \
                if(*tmp <= 0){                              \         
                    perror("in SET_INT");                   \
                    icl_hash_destroy(h , free, free);       \
                    return -1;                              \
                }                                           \
                config_param = *tmp;                        \
            }while(0)

#define SET_CHAR(config_param, h, key)                      \
            do{                                             \
                char* tmp = (char*)icl_hash_find(h, key);   \
                int len = strlen(tmp);                      \         
                strncpy(config_name, tmp, len);             \
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
    char* tag;
    char* valore;
    while(fgets(buf, MAXARGS, fp) != NULL){
        if(buf[0] = ">") continue;
        tag = malloc(MAXINPUT * sizeof(char));
        FREE_ON_FAIL(tag, NULL, hash);
        valore = malloc(MAXINPUT * sizeof(char));
        FREE_ON_FAIL(valore, NULL, hash);
        if(sscanf(buf, "%[^:]:%[^\n]", tag, valore) != 2){
            perror("in sscanf\n");
            return -1;
        }
        int len_tag = strlen(tag);
        int len_val = strlen(valore);
        tag[len_tag + 1] = '\0';
        valore[len_val + 1] = '\0';
        FREE_ON_FAIL(icl_hash_insert(hash, tag, valore), -1, hash);
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
