#include "def.h"
#include "hashtable.h"

DEFINE_HASHTABLE_INSERT(insert_some, char, struct doc_value);
DEFINE_HASHTABLE_SEARCH(search_some, char, struct doc_value);
DEFINE_HASHTABLE_REMOVE(remove_some, char, struct doc_value);

unsigned long sdbm (unsigned char *str)
{
	unsigned long hash = 0;
	int c;

	while (c = *str++)
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash;
}

unsigned int doc_hash (void *ky)
{
	return sdbm((char *)ky);
}

int doc_keycmp (void *k1, void *k2)
{
	return (0 == strcmp((char *)k1, (char *)k2));
}
