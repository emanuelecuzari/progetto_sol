#ifndef (HASH_H)
#define HASH_H

typedef struct hitem{
  int key;              //chiave: corrisponde a indice cassa
  void* data;           //contenuto del hitem
  struct hitem *next;   //next hitem
}hitem_t;

struct hashtable
{
  struct hitem **table;   /* hash table, array of size 2^logsize */
  size_t size; /* log of size of table */
  size_t         mask;    /* (hashval & mask) is position in table */
  struct hitem  *ipos;    /* current item in the array */
  struct reroot *space;   /* space for the hitems */
};
typedef  struct htab  htab;

