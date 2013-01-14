/* hash.c -- hash table implementation
** 
** Copyright (C) 2005-2009 Collecta, Inc. 
**
**  This software is provided AS-IS with no warranty, either express
**  or implied.
**
**  This software is distributed under license and may not be copied,
**  modified or distributed except as expressly authorized under the
**  terms of the license contained in the file LICENSE.txt in this
**  distribution.
*/

/** @file
 *  Hash tables.
 */

#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "zmalloc.h"

/* private types */
typedef struct _hashentry_t hashentry_t;

struct _hashentry_t {
    hashentry_t *next;
    char *key;
    void *value;
};

struct _Hash {
    unsigned int ref;
    hash_free_func free_fun;
    int length;
    int num_keys;
    hashentry_t **entries;
};

struct _hash_iterator_t {
    unsigned int ref;
    Hash *table;
    hashentry_t *entry;
    int index;
};
   
/** allocate and initialize a new hash table */
Hash *hash_new(const int size, hash_free_func free_fun)
{
    Hash *result = NULL;

    result = zmalloc(sizeof(Hash));
    if (result != NULL) {
	result->entries = zmalloc(size * sizeof(hashentry_t *));
	if (result->entries == NULL) {
	    zfree(result);
	    return NULL;
	}
	memset(result->entries, 0, size * sizeof(hashentry_t *));
	result->length = size;

	result->free_fun = free_fun;
	result->num_keys = 0;
	/* give the caller a reference */
	result->ref = 1;
    }
    
    return result;
}

/** obtain a new reference to an existing hash table */
Hash *hash_clone(Hash * const table)
{
    table->ref++;
    return table;
}

/** release a hash table that is no longer needed */
void hash_release(Hash * const table)
{
    hashentry_t *entry, *next;
    int i;
    
    if (table->ref > 1)
	table->ref--;
    else {
	for (i = 0; i < table->length; i++) {
	    entry = table->entries[i];
	    while (entry != NULL) {
		next = entry->next;
		zfree(entry->key);
		if (table->free_fun) table->free_fun(entry->value);
		zfree(entry);
		entry = next;
	    }
	}
	zfree(table->entries);
	zfree(table);
    }
}

/** hash a key for our table lookup */
static int _hash_key(Hash *table, const char *key)
{
   int hash = 0;
   int shift = 0;
   const char *c = key;

   while (*c != '\0') {
	/* assume 32 bit ints */
	hash ^= ((int)*c++ << shift);
	shift += 8;
	if (shift > 24) shift = 0;
   }

   return hash % table->length;
}

/** add a key, value pair to a hash table.
 *  each key can appear only once; the value of any
 *  identical key will be replaced
 */
int hash_add(Hash *table, const char * const key, void *data)
{
   hashentry_t *entry = NULL;
   int index = _hash_key(table, key);

   /* drop existing entry, if any */
   hash_drop(table, key);

   /* allocate and fill a new entry */
   entry = zmalloc(sizeof(hashentry_t));
   if (!entry) return -1;
   entry->key = zstrdup(key);
   if (!entry->key) {
       zfree(entry);
       return -1;
   }
   entry->value = data;
   /* insert ourselves in the linked list */
   /* TODO: this leaks duplicate keys */
   entry->next = table->entries[index];
   table->entries[index] = entry;
   table->num_keys++;

   return 0;
}

/** look up a key in a hash table */
void *hash_get(Hash *table, const char *key)
{
   hashentry_t *entry;
   int index = _hash_key(table, key);
   void *result = NULL;

   /* look up the hash entry */
   entry = table->entries[index];
   while (entry != NULL) {
	/* traverse the linked list looking for the key */
	if (!strcmp(key, entry->key)) {
	  /* match */
	  result = entry->value;
	  return result;
	}
	entry = entry->next;
   }
   /* no match */
   return result;
}

/** delete a key from a hash table */
int hash_drop(Hash *table, const char *key)
{
   hashentry_t *entry, *prev;
   int index = _hash_key(table, key);

   /* look up the hash entry */
   entry = table->entries[index];
   prev = NULL;
   while (entry != NULL) {
	/* traverse the linked list looking for the key */
	if (!strcmp(key, entry->key)) {
	  /* match, remove the entry */
	  zfree(entry->key);
	  if (table->free_fun) table->free_fun(entry->value);
	  if (prev == NULL) {
	    table->entries[index] = entry->next;
	  } else {
	    prev->next = entry->next;
	  }
	  zfree(entry);
	  table->num_keys--;
	  return 0;
	}
	prev = entry;
	entry = entry->next;
   }
   /* no match */
   return -1;
}

int hash_num_keys(Hash *table)
{
    return table->num_keys;
}

/** allocate and initialize a new iterator */
hash_iterator_t *hash_iter_new(Hash *table)
{
    hash_iterator_t *iter;

    iter = zmalloc(sizeof(*iter));
    if (iter != NULL) {
	iter->ref = 1;
	iter->table = hash_clone(table);
	iter->entry = NULL;
	iter->index = -1;
    }

    return iter;
}


/** release an iterator that is no longer needed */
void hash_iter_release(hash_iterator_t *iter)
{
    iter->ref--;

    if (iter->ref <= 0) {
        hash_release(iter->table);
        zfree(iter);
    }
}

/** return the next hash table key from the iterator.
    the returned key should not be freed */
const char * hash_iter_next(hash_iterator_t *iter)
{
    Hash *table = iter->table;
    hashentry_t *entry = iter->entry;
    int i = iter->index + 1;

    /* advance until we find the next entry */
    if (entry != NULL) entry = entry->next;
    if (entry == NULL) {
	/* we're off the end of list, search for a new entry */
	while (i < iter->table->length) {
	    entry = table->entries[i];
	    if (entry != NULL) {
		iter->index = i;
		break;
	    }
	    i++;
	}
    }

    if ((entry == NULL) || (i >= table->length)) {
	/* no more keys! */
	return NULL;
    }

    /* remember our current match */
    iter->entry = entry;
    return entry->key;
}

