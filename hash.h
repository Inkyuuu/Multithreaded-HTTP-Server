#include "rwlock.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct Entry Entry;

typedef struct hash hash_t;

hash_t *create_hash(size_t size);

void free_hash(hash_t **hash);

int find_entry(hash_t *hash, const char *uri);

rwlock_t *get_lock(hash_t *hash, const char *uri);
// returns true if it was added successfully
bool add_entry(hash_t *hash, const char *uri);

void remove_entry(hash_t *hash, const char *uri);

