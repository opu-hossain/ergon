#ifndef ergon_vm_h
#define ergon_vm_h

#include "chunk.h"
#include "value.h"

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
  Value *stack;
  int stack_capacity;
  Value *stack_top;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} interpret_result;

void init_vm();
void free_vm();
interpret_result interpret(const char *source);
void push(Value value);
Value pop();

#endif // !ergon_vm_h
