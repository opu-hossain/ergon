#ifndef ergon_compiler_h
#define ergon_compiler_h

#include "chunk.h"
#include "object.h"
#include "vm.h"
bool compile(const char *source, Chunk *chunk);

#endif // !DEBUG ergon_compiler_h
