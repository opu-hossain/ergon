#include <stdio.h>

#include "../include/compiler/ergon_compiler.h"
#include "../include/ergon/ergon_common.h"
#include "../include/ergon/ergon_debug.h"
#include "../include/ergon/ergon_memory.h"
#include "../include/vm/ergon_vm.h"

VM vm;

static void reset_stack() { vm.stack_top = vm.stack; }

void init_vm() { reset_stack(); }

void free_vm() {
  // todo
}

void push(Value value) {
  if (vm.stack_top - vm.stack == vm.stack_capacity) {
    int old_capacity = vm.stack_capacity;
    int offset = vm.stack_top - vm.stack;

    vm.stack_capacity = GROW_CAPACITY(old_capacity);
    vm.stack = GROW_ARRAY(Value, vm.stack, old_capacity, vm.stack_capacity);

    vm.stack_top = vm.stack + offset;
  }

  *vm.stack_top = value;
  vm.stack_top++;
}

Value pop() {
  vm.stack_top--;
  return *vm.stack_top;
}

static interpret_result run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op)                                                          \
  do {                                                                         \
    double b = *(vm.stack_top - 1);                                            \
    double a = *(vm.stack_top - 2);                                            \
    vm.stack_top--;                                                            \
    *(vm.stack_top - 1) = a op b;                                              \
  } while (false)
  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("        ");
    for (Value *slot = vm.stack; slot < vm.stack_top; slot++) {
      printf("[ ");
      print_value(*slot);
      printf(" ]");
    }
    printf("\n");
    disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT:
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    case OP_ADD:
      BINARY_OP(+);
      break;
    case OP_SUBTRACT:
      BINARY_OP(-);
      break;
    case OP_MULTIPLY:
      BINARY_OP(*);
      break;
    case OP_DEVIDE:
      BINARY_OP(/);
      break;

    case OP_NEGATE:
      *(vm.stack_top - 1) = -*(vm.stack_top - 1);
      break;
    case OP_RETURN:
      print_value(pop());
      printf("\n");
      return INTERPRET_OK;
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

interpret_result interpret(const char *source) {
  compile(source);
  return INTERPRET_OK;
}
