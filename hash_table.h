#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stddef.h>
#include <stdint.h>

#ifndef LOAD_FACTOR
#define LOAD_FACTOR 0.6
#endif


// good hash function for strings
// dbj2 hash
uint64_t string_hash(void* key);
// good hash function for integers
uint64_t integer_hash(void* key);

// used if not specifed otherwise
uint64_t fnv_hash_1a_64 ( void *key, size_t len);

typedef struct hash_table
{
    // table consists of entries which consist of
    // probe sequence length (sizeof(size_t))
    // key struct (key_size)
    // value struct (value_size)
    // the proble sequence length is part of a addressing scheme called robinhood hashing
    // probe sequence length indicates distance from hashed location
    // entries with low psl(rich) are replaced with entries with high psl(poor)
    // entry actually stores psl + 1 so that a psl of zero can be used to indicate an empty entry
    void* table;
    // temporary storage for swapping
    void* temp;
    void* spare;
    // capacity of the table, always a power of two
    size_t cap;
    // number of entries currently
    size_t size;
    // size at which to increase table size
    size_t resize;

    // user supplied comparision function
    // should follow strcmp conventions, i.e returns 0 for equality
    int (*compare)(void*, void*);
    // user supplied key duplication function
    // if the key is a char* we would want to duplicate it so that the user can continue to use the string
    void* (*key_dup)(void*);
    // user supplied value duplication function
    void* (*value_dup)(void*);
    // size of each value object
    size_t value_size;
    // size of each key object
    size_t key_size;

    size_t entry_size;
    
    uint64_t (*hash)(void* key);
    // if duplication was required, specialized free function can be supplied
    void (*key_free)(void* key);
    void (*value_free)(void* value);



} hash_table;





// initializes hash table
hash_table* hash_table_new(size_t key_size, size_t value_size, int (*compare)(void*, void*), uint64_t (*hash)(void* key), size_t cap);

// extended initialization
hash_table* hash_table_new_ex(size_t key_size, size_t value_size, int (*compare)(void*, void*), uint64_t (*hash)(void* key), size_t cap,
void* (*key_dup)(void*), void* (*value_dup)(void*), void (*key_free)(void* key), void (*value_free)(void* value));


// new size is not guaranteed to be new_size, but it will be greater or equal to it
int hash_table_resize(hash_table* table, size_t new_size);

void* hash_table_get(hash_table* table, void* key);

int hash_table_set(hash_table* table, void* key, void* value);

void hash_table_remove(hash_table* table, void* key);

void hash_table_free(hash_table* table);


#endif