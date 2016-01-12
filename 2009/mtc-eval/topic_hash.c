#include "def.h"
#include "hashtable.h"

DEFINE_HASHTABLE_INSERT(insert_topic, char, _topic);
DEFINE_HASHTABLE_SEARCH(search_topic, char, _topic);
DEFINE_HASHTABLE_REMOVE(remove_topic, char, _topic);

unsigned int topic_hash (void *ky)
{
	return sdbm((char *)ky);
}

int topic_keycmp (void *k1, void *k2)
{
	return (0 == strcmp((char *)k1, (char *)k2));
}
