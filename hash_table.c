#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "hash_table.h"

uint64_t string_hash(void* key)
{
    unsigned char* str = *(unsigned char**) key;
    uint64_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

uint64_t integer_hash(void* key)
{
    uint64_t x = *(uint32_t*)key;
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

uint64_t fnv_hash_1a_64 ( void *key, size_t len)
{
    unsigned char *p = (unsigned char*)key;
    uint64_t h = 0xcbf29ce484222325ULL;
    int i;


    for ( i = 0; i < len; i++ )
      h = ( h ^ p[i] ) * 0x100000001b3ULL;

   return h;
}

static void* get_entry(hash_table* table, size_t entry_no)
{
    size_t offset = table->entry_size * entry_no;

    return table->table + offset;
} 


static size_t* get_entry_psl(hash_table* table, size_t entry_no)
{
    return (size_t*)(get_entry(table, entry_no));
}

static void* get_entry_key(hash_table* table, size_t entry_no)
{
    return get_entry(table, entry_no) + sizeof(size_t);

}

static void* get_entry_value(hash_table* table, size_t entry_no)
{
    return get_entry(table, entry_no)+sizeof(size_t) + table->key_size;
}



static size_t* get_spare_psl(hash_table* table)
{
    return (size_t*)table->spare;
}

static void* get_spare_key(hash_table* table)
{
    return table->spare + sizeof(size_t);
}

static void* get_spare_value(hash_table* table)
{
    return table->spare + sizeof(size_t) + table->key_size;
}

static int hash_table_compare(hash_table* table, void* key1, void* key2)
{
    if(table->compare)
    {
        return table->compare(key1, key2);
    }
    return memcmp(key1, key2, table->key_size);
}



static uint64_t hash_table_hash(hash_table* table, void* key)
{
    if(table->hash)
    {
        return table->hash(key);
    }
    return fnv_hash_1a_64(key, table->key_size);
}


hash_table* hash_table_new(size_t key_size, size_t value_size, int (*compare)(void*, void*), uint64_t (*hash)(void* key), size_t cap)
{
    return hash_table_new_ex(key_size, value_size, compare, hash, cap, NULL, NULL, NULL, NULL);
}


hash_table* hash_table_new_ex(size_t key_size, size_t value_size, int (*compare)(void*, void*), uint64_t (*hash)(void* key), size_t cap,
void* (*key_dup)(void*), void* (*value_dup)(void*), void (*key_free)(void* key), void (*value_free)(void* value))
{
    hash_table* table = malloc(sizeof(hash_table));
    if(!table)
    {
        fprintf(stderr, "Error allocating hash_table");
        return NULL;
    }
    table->key_size = key_size;
    table->value_size = value_size;
    table->compare = compare;
    table->hash = hash;
    table->key_dup = key_dup;
    table->value_dup = value_dup;
    // if no user supplied free function, just use stdlib free
    if(key_dup && !key_free)
    {
        table->key_free = free;
    }
    else
    {
        table->key_free = key_free;
    }
    if(value_dup && !value_free)
    {
        table->value_free = free;
    }
    else
    {
        table->value_free = value_free;
    }
    table->size = 0;
    table->cap = 16;
    while(table->cap < cap)
    {
        // double cap until its bigger than needed capacity
        table->cap <<= 1;
    }
    table->resize = (table->cap * (LOAD_FACTOR * 100))/100;
    table->entry_size = sizeof(size_t) + key_size + value_size;
    // align size of entries to cpu word size
    if(table->entry_size & (sizeof(uintptr_t)-1))
    {
        table->entry_size += sizeof(uintptr_t) - (table->entry_size & (sizeof(uintptr_t)-1));
    }
    // allocate one more entry for spare
    table->table = malloc(table->entry_size * table->cap + table->entry_size*2);
    if(!table->table)
    {
        if(!table)
        {
            fprintf(stderr, "Error allocating hash_table table");
            free(table);
            return NULL;
        }
    }
    table->spare = table->table + table->entry_size * table->cap;
    table->temp = table->spare + table->entry_size;
    memset(table->table,0, table->entry_size * table->cap+table->entry_size*2);
    return table;
}


int hash_table_resize(hash_table* table, size_t new_size)
{
    int new_cap = table->cap;
    while(new_cap < new_size)
    {
        new_cap <<= 1;
    }

    hash_table* table2 = hash_table_new_ex(table->key_size, table->value_size, table->compare, table->hash,
    new_cap, table->key_dup, table->value_dup, table->key_free, table->value_free);

    if(!table2)
    {
        fprintf(stderr, "Error allocating new table\n");
        return -1;
    }

    // rehash all entries

    for(int i = 0; i < table->cap; i++)
    {
        // empty entry
        if(!*get_entry_psl(table, i))
        {
            continue;
        }
        memcpy(table2->spare, get_entry(table, i), table2->entry_size);
        *get_spare_psl(table2) = 1;
        size_t index = hash_table_hash(table, get_entry_key(table, i)) & (table2->cap - 1);
        while(*get_entry_psl(table2, index))
        {

            if(*get_spare_psl(table2) > *get_entry_psl(table2,index))
            {
                memcpy(table2->temp, get_entry(table2, index), table2->entry_size);
                memcpy(get_entry(table2, index), table2->spare, table2->entry_size);
                memcpy(table2->spare, table2->temp, table2->entry_size);
            }

            (*get_spare_psl(table2))++;
            index = (index + 1) & (table2->cap - 1);
        }
        memcpy(get_entry(table2, index), table2->spare, table2->entry_size);
    }

    table->cap = table2->cap;
    table->resize = (table->cap*(LOAD_FACTOR*100))/100;
    free(table->table);
    table->table = table2->table;
    table->spare = table2->spare;
    table->temp = table2->temp;
    free(table2);


    return 0;

}


void* hash_table_get(hash_table* table, void* key)
{
    // modulo by table size to get a index inside table
    size_t index = hash_table_hash(table,key) & (table->cap - 1);
    int psl = 1;
    while(psl <= *get_entry_psl(table, index) && *get_entry_psl(table, index))
    {
        // entry was found
        if(hash_table_compare(table, key, get_entry_key(table, index)) == 0)
        {
            return get_entry_value(table, index);
        }
    
        psl++;
        index = (index + 1) & (table->cap - 1);
    }
    return NULL;
}



int hash_table_set(hash_table* table, void* key, void* value)
{
    if(table->size >= table->resize)
    {
        // adding entry will make table load factor go over maximum specified
        // resize table to twice its size
        if(hash_table_resize(table, table->cap << 1) == -1)
        {
            return -1;
        }
    }

    void* new_value = value;
    if(table->value_dup)
    {
        new_value = table->value_dup(value);
        memcpy(get_spare_value(table), new_value, table->value_size);
        free(new_value);
    }
    else
    {
        memcpy(get_spare_value(table), new_value, table->value_size);
    }
    // first search for the key to see if all we have to do is update the value
    size_t index = hash_table_hash(table,key) & (table->cap - 1);
    *get_spare_psl(table) = 1;

    while(*get_spare_psl(table) <= *get_entry_psl(table, index) && *get_entry_psl(table, index))
    {
        // entry was found
        if(hash_table_compare(table, key, get_entry_key(table, index)) == 0)
        {
            
            memcpy(get_entry_value(table,index),get_spare_value(table), table->value_size);
            return 0;
        }
    
        (*get_spare_psl(table))++;
        index = (index + 1) & (table->cap - 1);
    }
    // key is not in table, so new key must be created
    void* new_key = key;
    if(table->key_dup)
    {
        new_key = table->key_dup(key);
        memcpy(get_spare_key(table),new_key, table->key_size);
        free(new_key);
    }
    else
    {
        memcpy(get_spare_key(table),new_key, table->key_size);
    }

    // go until you find an empty slot
    while(*get_entry_psl(table, index))
    {
        // entry at index hashes after key, so entry should be replaced
        if(*get_spare_psl(table) > *get_entry_psl(table, index))
        {
            // swap entry and spare
            memcpy(table->temp, get_entry(table,index), table->entry_size);
            memcpy(get_entry(table, index), table->spare, table->entry_size);
            memcpy(table->spare, table->temp, table->entry_size);
        }
        (*get_spare_psl(table))++;
        index = (index + 1) & (table->cap - 1);
    }

    memcpy(get_entry(table,index), table->spare, table->entry_size);
    table->size++;
    return 0;
}

void hash_table_remove(hash_table* table, void* key)
{

}

void hash_table_free(hash_table* table)
{
    if(table->key_free || table->value_free)
    {
        for(int i = 0; i < table->cap; i++)
        {
            if(*get_entry_psl(table, i))
            {
                if(table->key_free)
                {

                    table->key_free(get_entry_key(table, i));
                    
                    
                }
                if(table->value_free)
                {
                    table->value_free(get_entry_value(table, i));
                }
            }
        }
    }

    free(table->table);
    free(table);
}