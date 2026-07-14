#ifndef ergon_object_h
#define ergon_object_h

#include "common.h"
#include "value.h"
#include <stdint.h>

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_STRING(value) ((Obj_string *)AS_OBJ(value))
#define AS_CSTRING(value) (((Obj_string *)AS_OBJ(value))->chars)

typedef enum {
  OBJ_STRING,
} Obj_type;

struct Obj {
  Obj_type type;
  struct Obj *next;
};

struct Obj_string {
  Obj obj;
  int length;
  uint32_t hash;
  char chars[];
};

Obj_string *take_string(char *chars, int length);
Obj_string *copy_string(const char *cahrs, int length);
void print_object(Value value);

static inline bool is_obj_type(Value value, Obj_type type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
