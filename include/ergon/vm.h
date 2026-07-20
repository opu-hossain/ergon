#ifndef ergon_vm_h
#define ergon_vm_h

#include "chunk.h"
#include "ergon/object.h"
#include "table.h"
#include "value.h"

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
  Value *stack;
  int stack_capacity;
  Value *stack_top;
  Table globals;
  Table strings;
  Table global_names;
  Global_array global_values;
  Obj *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} interpret_result;

extern VM vm;

void init_vm();
void free_vm();
interpret_result interpret(const char *source);
void push(Value value);
Value pop();
void write_global_array(Global_array *array, Global global);

#endif // !ergon_vm_h
