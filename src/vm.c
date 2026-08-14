#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ergon/chunk.h"
#include "ergon/common.h"
#include "ergon/compiler.h"
#include "ergon/debug.h"
#include "ergon/memory.h"
#include "ergon/object.h"
#include "ergon/table.h"
#include "ergon/value.h"
#include "ergon/vm.h"

VM vm;

void write_global_array(Global_array *array, Global global) {
  if (array->capacity < array->count + 1) {
    int old_capacity = array->capacity;
    array->capacity = GROW_CAPACITY(old_capacity);
    array->globals =
        GROW_ARRAY(Global, array->globals, old_capacity, array->capacity);
  }
  array->globals[array->count] = global;
  array->count++;
}

static void reset_stack() { vm.stack_top = vm.stack; }

static void runtime_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = get_line(vm.chunk, (int)instruction);
  fprintf(stderr, "[line %d] in script\n]", line);
}

void init_vm() {
  reset_stack();
  vm.objects = NULL;
  init_table(&vm.strings);

  init_table(&vm.global_names);
  vm.global_values.count = 0;
  vm.global_values.capacity = 0;
  vm.global_values.globals = NULL;
}

void free_vm() {
  free_table(&vm.strings);
  free_table(&vm.global_names);
  FREE_ARRAY(Global, vm.global_values.globals, vm.global_values.capacity);
  free_objects();
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

static Value peek(int distance) { return vm.stack_top[-1 - distance]; }

static bool is_falsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  Obj_string *b = AS_STRING(pop());
  Obj_string *a = AS_STRING(pop());

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  Obj_string *result = take_string(chars, length);
  push(OBJ_VAL(result));
}

static interpret_result run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_SHORT() (vm.ip += 2, (uint16_t)(vm.ip[-2]) << 8) | vm.ip[-1]
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(value_type, op)                                              \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtime_error("Operands must be nubmers.");                              \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(value_type(a op b));                                                  \
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
    case OP_EQUAL: {
      Value a = pop();
      Value b = pop();
      push(BOOL_VAL(values_equal(a, b)));
      break;
    }
    case OP_NOT_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(!values_equal(a, b)));
      break;
    }
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_GREATER_EQUAL:
      BINARY_OP(BOOL_VAL, >=);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_LESS_EQUAL:
      BINARY_OP(BOOL_VAL, <=);
      break;
    case OP_NIL:
      push(NIL_VAL);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true));
      break;
    case OP_FALSE:
      push(BOOL_VAL(false));
      break;
    case OP_POP:
      pop();
      break;
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(vm.stack[slot]);
      break;
    }
    case OP_SET_LOCAL: {

      uint8_t slot = READ_BYTE();
      vm.stack[slot] = peek(0);
      break;
    }
    case OP_DEFINE_GLOBAL: {
      uint8_t slot = READ_BYTE();
      vm.global_values.globals[slot].value = peek(0);
      vm.global_values.globals[slot].is_defined = true;
      pop();
      break;
    }
    case OP_GET_GLOBAL: {
      uint8_t slot = READ_BYTE();
      Global *global = &vm.global_values.globals[slot];
      if (!global->is_defined) {
        runtime_error("Undefined variable '%s'.", global->name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(global->value);
      break;
    }
    case OP_SET_GLOBAL: {
      uint8_t slot = READ_BYTE();
      Global *global = &vm.global_values.globals[slot];
      if (!global->is_defined) {
        runtime_error("Undefined variable '%s'.", global->name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      global->value = peek(0);
      break;
    }
    case OP_ADD:
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate();
      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
      } else {
        runtime_error("Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_DEVIDE:
      BINARY_OP(NUMBER_VAL, /);
      break;
    case OP_NOT:
      push(BOOL_VAL(is_falsey(pop())));
      break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(0))) {
        runtime_error("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
      break;
    case OP_PRINT:
      print_value(pop());
      printf("\n");
      break;
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      vm.ip += offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (is_falsey(peek(0)))
        vm.ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      vm.ip -= offset;
      break;
    }
    case OP_RETURN:
      // Exit interpreter
      return INTERPRET_OK;
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

interpret_result interpret(const char *source) {
  Chunk chunk;
  init_chunk(&chunk);

  if (!compile(source, &chunk)) {
    free_chunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;

  interpret_result result = run();

  free_chunk(&chunk);
  return result;
}
