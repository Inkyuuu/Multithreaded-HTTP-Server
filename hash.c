#include "hash.h"
typedef struct Entry {
    const char *uri;
    rwlock_t *rw;
} Entry;

typedef struct hash {
    Entry **elements;
    size_t num_elements;
    size_t size;
} hash_t;

hash_t *create_hash(size_t size) {
    hash_t *hash = calloc(1, sizeof(hash_t));
    Entry **elements = calloc(size, sizeof(Entry*));
    hash->elements = elements;
    hash->num_elements = 0;
    hash->size = size;
    return hash;
}

void free_hash(hash_t **hash) {
    free(*hash);
    if (*hash != NULL) {
        *hash = NULL;
    }
}

int find_entry(hash_t *hash, const char *uri) {
    int ans = -1;
    for (size_t i = 0; i < hash->num_elements; i++) {
        if (strcmp(uri, hash->elements[i]->uri) == 0) {
            ans = i;
        }
    }
    return ans;
}

rwlock_t *get_lock(hash_t *hash, const char *uri) {
    int ind = find_entry(hash, uri);
    if (ind > -1) { // if found
        return hash->elements[ind]->rw;
    }
    return NULL;
}
// returns true if it was added successfully
bool add_entry(hash_t *hash, const char *uri) {
    if (hash->num_elements < hash->size) {
        // if the uri exists in hash already, use the already existing lock
        Entry *entry = calloc(1, sizeof(Entry));
        entry->uri = uri;
        rwlock_t *rw;
        int ind = find_entry(hash, uri);
        if (ind > -1) { // if the uri has been found, use the current lock
            rw = hash->elements[ind]->rw;
        } else { // uri not found, create new lock
            rw = rwlock_new(READERS, 1);
        }
        entry->rw = rw;
        hash->elements[hash->num_elements] = entry;
        hash->num_elements++;
        return true;
    }
    return false;
}

void remove_entry(hash_t *hash, const char *uri) {
    size_t ind = find_entry(hash, uri);
    if (ind > -1) {
        Entry *entry = hash->elements[ind];
        hash->elements[ind] = NULL;
        hash->num_elements--;
        for (size_t i = ind; i < hash->num_elements; i++) {
            hash->elements[ind] = hash->elements[ind + 1];
        }
        // search elements for duplicates, if none exist free the lock for that uri
        if (find_entry(hash, uri) < 0) {
            rwlock_t *rw = entry->rw;
            rwlock_delete(&rw);
        }
    }
}
