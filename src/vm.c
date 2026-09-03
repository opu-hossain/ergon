#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../include/ergon/chunk.h"
#include "../include/ergon/common.h"
#include "../include/ergon/compiler.h"
#include "../include/ergon/debug.h"
#include "../include/ergon/memory.h"
#include "../include/ergon/object.h"
#include "../include/ergon/table.h"
#include "../include/ergon/value.h"
#include "../include/ergon/vm.h"

VM vm;

static Value clock_native(int arg_count, Value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

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

static void reset_stack() {
  vm.stack_top = vm.stack;
  vm.frame_count = 0;
}

static void runtime_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frame_count - 1; i >= 0; i--) {
    Call_frame *frame = &vm.frames[i];
    Obj_function *function = frame->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ",
            get_line(&function->chunk, (int)instruction));
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  reset_stack();
}

static void define_native(const char *name, Native_fn function) {
  Obj_string *native_name = copy_string(name, (int)strlen(name));
  push(OBJ_VAL(native_name));
  push(OBJ_VAL(new_native(function)));

  Value index;
  uint8_t slot;

  if (table_get(&vm.global_names, native_name, &index)) {
    slot = (uint8_t)AS_NUMBER(index);
  } else {
    Global global = {
        .name = native_name,
        .value = NIL_VAL,
        .is_defined = false,
        .is_const = false,
    };
    write_global_array(&vm.global_values, global);
    slot = (uint8_t)(vm.global_values.count - 1);
    table_set(&vm.global_names, native_name, NUMBER_VAL((double)slot));
  }

  vm.global_values.globals[slot].value = vm.stack[1];
  vm.global_values.globals[slot].is_defined = true;

  pop();
  pop();
}

void init_vm() {
  reset_stack();
  vm.objects = NULL;
  init_table(&vm.strings);

  init_table(&vm.global_names);
  vm.global_values.count = 0;
  vm.global_values.capacity = 0;
  vm.global_values.globals = NULL;

  define_native("clock", clock_native);
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

static bool call(Obj_function *function, int arg_count) {
  if (arg_count != function->arity) {
    runtime_error("Expected %d arguments but got %d.", function->arity,
                  arg_count);
    return false;
  }

  if (vm.frame_count == FRAMES_MAX) {
    runtime_error("Stack overflow.");
    return false;
  }

  Call_frame *frame = &vm.frames[vm.frame_count++];
  frame->function = function;
  frame->ip = function->chunk.code;
  frame->slots_offset = (int)(vm.stack_top - vm.stack) - arg_count - 1;

  return true;
}

static bool call_value(Value callee, int arg_count) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_FUNCTION:
      return call(AS_FUNCTION(callee), arg_count);
    case OBJ_NATIVE: {
      Native_fn native = AS_NATIVE(callee);
      Value result = native(arg_count, vm.stack_top - arg_count);
      vm.stack_top -= arg_count + 1;
      push(result);
      return true;
    }
    default:
      break;
    }
  }
  runtime_error("Can only call functions and classes.");
  return false;
}

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
  Call_frame *frame = &vm.frames[vm.frame_count - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
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
    disassemble_instruction(&frame->function->chunk,
                            (int)(frame->ip - frame->function->chunk.code));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
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
      push(vm.stack[frame->slots_offset + slot]);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      vm.stack[frame->slots_offset + slot] = peek(0);
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
    case OP_DIVIDE:
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
      frame->ip += offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (is_falsey(peek(0)))
        frame->ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      frame->ip -= offset;
      break;
    }
    case OP_CALL: {
      int arg_count = READ_BYTE();
      if (!call_value(peek(arg_count), arg_count)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frame_count - 1];
      break;
    }
    case OP_RETURN: {
      Value result = pop();
      vm.frame_count--;
      if (vm.frame_count == 0) {
        pop();
        return INTERPRET_OK;
      }

      vm.stack_top = vm.stack + frame->slots_offset;
      push(result);
      frame = &vm.frames[vm.frame_count - 1];
      break;
    }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

interpret_result interpret(const char *source) {
  Obj_function *function = compile(source);
  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  call(function, 0);

  return run();
}
