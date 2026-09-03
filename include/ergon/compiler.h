#ifndef ergon_compiler_h
#define ergon_compiler_h

#include "chunk.h"
#include "object.h"
#include "vm.h"
Obj_function *compile(const char *source);

#endif // !DEBUG ergon_compiler_h
