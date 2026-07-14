#ifndef ergon_table_h
#define ergon_table_h

#include "common.h"
#include "value.h"
#include <stdint.h>

typedef struct {
  Obj_string *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table;

void init_table(Table *table);
void free_table(Table *table);
void table_add_all(Table *from, Table *to);
Obj_string *table_find_string(Table *table, const char *chars, int length,
                              uint32_t hash);
bool table_get(Table *table, Obj_string *key, Value *value);
bool table_set(Table *table, Obj_string *key, Value value);
bool table_delete(Table *table, Obj_string *key);

#endif
