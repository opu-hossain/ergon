#ifndef ergon_object_h
#define ergon_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"
#include <stdint.h>

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)

#define AS_STRING(value) ((Obj_string *)AS_OBJ(value))
#define AS_CSTRING(value) (((Obj_string *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((Obj_function *)AS_OBJ(value))
#define AS_CLOSURE(value) ((Obj_closure *)AS_OBJ(value))
#define AS_NATIVE(value) (((Obj_native *)AS_OBJ(value))->function)

typedef enum {
  OBJ_STRING,
  OBJ_FUNCTION,
  OBJ_CLOSURE,
  OBJ_UPVALUE,
  OBJ_NATIVE,
} Obj_type;

typedef struct {
  Obj_string *name;
  Value value;
  bool is_defined;
  bool is_const;
} Global;

typedef struct {
  int count;
  int capacity;
  Global *globals;
} Global_array;

struct Obj {
  Obj_type type;
  struct Obj *next;
};

typedef struct {
  Obj obj;
  int arity;
  int upvalue_count;
  Chunk chunk;
  Obj_string *name;
} Obj_function;

typedef Value (*Native_fn)(int arg_count, Value *args);

typedef struct {
  Obj obj;
  Native_fn function;
} Obj_native;

typedef struct Obj_upvalue {
  Obj obj;
  Value *location;
  Value closed;
  struct Obj_upvalue *next;
} Obj_upvalue;

typedef struct {
  Obj obj;
  Obj_function *function;
  Obj_upvalue **upvalues;
  int upvalue_count;
} Obj_closure;

struct Obj_string {
  Obj obj;
  int length;
  uint32_t hash;
  char chars[];
};

Obj_function *new_function();
Obj_closure *new_closure(Obj_function *function);
Obj_upvalue *new_upvalue(Value *slot);
Obj_native *new_native(Native_fn function);
Obj_string *take_string(char *chars, int length);
Obj_string *copy_string(const char *cahrs, int length);
void print_object(Value value);

static inline bool is_obj_type(Value value, Obj_type type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
