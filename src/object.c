#include <stdio.h>
#include <string.h>

#include "ergon/memory.h"
#include "ergon/object.h"
#include "ergon/value.h"
#include "ergon/vm.h"

#define ALLOCATE_OBJ(type, object_type)                                        \
  (type *)allocate_object(sizeof(type), object_type)

static Obj *allocate_object(size_t size, Obj_type type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;

  object->next = vm.objects;
  vm.objects = object;
  return object;
}

static Obj_string *allocate_string(int length) {
  Obj_string *string = (Obj_string *)allocate_object(
      sizeof(Obj_string) + length + 1, OBJ_STRING);
  string->length = length;
  return string;
}

Obj_string *take_string(char *chars, int length) {
  Obj_string *string = allocate_string(length);
  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';
  FREE_ARRAY(char, chars, length + 1);
  return string;
}

Obj_string *copy_string(const char *chars, int length) {
  Obj_string *string = allocate_string(length);
  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';
  return string;
}

void print_object(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  }
}
