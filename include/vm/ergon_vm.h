#ifndef ergon_vm_h
#define ergon_vm_h

#include "../ergon/ergon_chunks.h"
#include "../ergon/ergon_value.h"

#define STACK_MAX 256

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
  Value stack[STACK_MAX];
  Value *stack_top;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} interpret_result;

void init_vm();
void free_vm();
interpret_result interpret(Chunk *chunk);
void push(Value value);
Value pop();

#endif // !ergon_vm_h
