#ifndef ergon_vm_h
#define ergon_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64

typedef struct {
  Obj_function *function;
  uint8_t *ip;
  int slots_offset;
} Call_frame;

typedef struct {
  Call_frame frames[FRAMES_MAX];
  int frame_count;
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
