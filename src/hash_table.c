#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "libhash.h"
#include "prime.h"
#include "strdup/strdup.h"

static ht_entry HT_SENTINEL_ENTRY = {NULL, NULL};

static void __ht_insert(hash_table *ht, const char *key, void *value,
                        bool free_value);
static int __ht_delete(hash_table *ht, const char *key, bool free_value);
static void __ht_delete_table(hash_table *ht, bool free_value);

/**
 * Resize the hash table. This implementation has a set capacity;
 * hash collisions rise beyond the capacity and `ht_insert` will fail.
 * To mitigate this, we resize up if the load (measured as the ratio of
 * entries count to capacity) is less than .1, or down if the load exceeds
 * .7. To resize, we create a new table approx. 1/2x or 2x times the current
 * table size, then insert into it all non-deleted entries.
 *
 * @param ht
 * @param base_capacity
 * @return int
 */
static void ht_resize(hash_table *ht, const int base_capacity,
                      bool free_value) {
  if (base_capacity < 0) {
    return;
  }

  hash_table *new_ht = ht_init(base_capacity);

  for (unsigned int i = 0; i < ht->capacity; i++) {
    ht_entry *r = ht->entries[i];

    if (r != NULL && r != &HT_SENTINEL_ENTRY) {
      __ht_insert(new_ht, r->key, r->value, free_value);
    }
  }

  ht->base_capacity = new_ht->base_capacity;
  ht->count = new_ht->count;

  const int tmp_capacity = ht->capacity;

  ht->capacity = new_ht->capacity;
  new_ht->capacity = tmp_capacity;

  ht_entry **tmp_entries = ht->entries;
  ht->entries = new_ht->entries;
  new_ht->entries = tmp_entries;

  ht_delete_table(new_ht);
}

/**
 * Resize the table to a larger size, the first prime subsequent
 * to approx. 2x the base capacity.
 *
 * @param ht
 */
static void ht_resize_up(hash_table *ht, bool free_value) {
  const int new_capacity = ht->base_capacity * 2;

  ht_resize(ht, new_capacity, free_value);
}

/**
 * Resize the table to a smaller size, the first prime subsequent
 * to approx. 1/2x the base capacity.
 *
 * @param ht
 */
static void ht_resize_down(hash_table *ht, bool free_value) {
  const int new_capacity = ht->base_capacity / 2;

  ht_resize(ht, new_capacity, free_value);
}

/**
 * Initialize a new hash table entry with the given k, v pair
 *
 * @param k entry key
 * @param v entry value
 * @return ht_entry*
 */
static ht_entry *ht_entry_init(const char *k, void *v) {
  ht_entry *r = malloc(sizeof(ht_entry));
  r->key = strdup(k);
  r->value = v;

  return r;
}

/**
 * Delete a entry and deallocate its memory
 *
 * @param r entry to delete
 */
static void ht_delete_entry(ht_entry *r, bool free_value) {
  free(r->key);
  if (free_value) {
    free(r->value);
    r->value = NULL;
  }
  free(r);
}

static void __ht_insert(hash_table *ht, const char *key, void *value,
                        bool free_value) {
  if (ht == NULL) {
    return;
  }

  const int load = ht->count * 100 / ht->capacity;
  if (load > 70) {
    ht_resize_up(ht, free_value);
  }

  ht_entry *new_entry = ht_entry_init(key, value);

  int idx = h_resolve_hash(new_entry->key, ht->capacity, 0);

  ht_entry *current_entry = ht->entries[idx];
  int i = 1;

  // i.e. if there was a collision
  while (current_entry != NULL && current_entry != &HT_SENTINEL_ENTRY) {
    // update existing key/value
    if (strcmp(current_entry->key, key) == 0) {
      ht_delete_entry(current_entry, free_value);
      ht->entries[idx] = new_entry;

      return;
    }

    // TODO verify i is 1..
    idx = h_resolve_hash(new_entry->key, ht->capacity, i);
    current_entry = ht->entries[idx];
    i++;
  }

  ht->entries[idx] = new_entry;
  ht->count++;
}

static int __ht_delete(hash_table *ht, const char *key, bool free_value) {
  const int load = ht->count * 100 / ht->capacity;

  if (load < 10) {
    ht_resize_down(ht, free_value);
  }

  int i = 0;
  int idx = h_resolve_hash(key, ht->capacity, i);

  ht_entry *current_entry = ht->entries[idx];

  while (current_entry != NULL && current_entry != &HT_SENTINEL_ENTRY) {
    if (strcmp(current_entry->key, key) == 0) {
      ht_delete_entry(current_entry, free_value);
      ht->entries[idx] = &HT_SENTINEL_ENTRY;

      ht->count--;

      return 1;
    }

    idx = h_resolve_hash(key, ht->capacity, ++i);
    current_entry = ht->entries[idx];
  }

  return 0;
}

static void __ht_delete_table(hash_table *ht, bool free_value) {
  for (unsigned int i = 0; i < ht->capacity; i++) {
    ht_entry *r = ht->entries[i];

    if (r != NULL && r != &HT_SENTINEL_ENTRY) {
      ht_delete_entry(r, free_value);
    }
  }

  free(ht->entries);
  free(ht);
}

hash_table *ht_init(int base_capacity) {
  if (!base_capacity) {
    base_capacity = HT_DEFAULT_CAPACITY;
  }

  hash_table *ht = malloc(sizeof(hash_table));
  ht->base_capacity = base_capacity;

  ht->capacity = next_prime(ht->base_capacity);
  ht->count = 0;
  ht->entries = calloc((size_t)ht->capacity, sizeof(ht_entry *));

  return ht;
}

void ht_insert(hash_table *ht, const char *key, void *value) {
  __ht_insert(ht, key, value, false);
}

void ht_insert_ptr(hash_table *ht, const char *key, void *value) {
  __ht_insert(ht, key, value, true);
}

ht_entry *ht_search(hash_table *ht, const char *key) {
  int idx = h_resolve_hash(key, ht->capacity, 0);
  ht_entry *current_entry = ht->entries[idx];
  int i = 1;

  while (current_entry != NULL && current_entry != &HT_SENTINEL_ENTRY) {
    if (strcmp(current_entry->key, key) == 0) {
      return current_entry;
    }

    idx = h_resolve_hash(key, ht->capacity, i);
    current_entry = ht->entries[idx];
    i++;
  }

  return NULL;
}

void *ht_get(hash_table *ht, const char *key) {
  ht_entry *r = ht_search(ht, key);

  return r ? r->value : NULL;
}

void ht_delete_table(hash_table *ht) { __ht_delete_table(ht, false); }

void ht_delete_table_ptr(hash_table *ht) { __ht_delete_table(ht, true); }

int ht_delete(hash_table *ht, const char *key) {
  return __ht_delete(ht, key, false);
}

int ht_delete_ptr(hash_table *ht, const char *key) {
  return __ht_delete(ht, key, true);
}
