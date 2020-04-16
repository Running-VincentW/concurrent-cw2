#ifndef __HASHTABLE_H
#define __HASHTABLE_H

typedef struct {
    char* key;
    char* value;
} ht_item;

typedef struct {
    int size;
    int count;
    ht_item** items;
} ht_hash_table;

extern ht_hash_table* ht_new();
extern void ht_del_hash_table(ht_hash_table* ht);
#endiftmp