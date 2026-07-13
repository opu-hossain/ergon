#ifndef ergon_compiler_h
#define ergon_compiler_h

#include "ergon/chunk.h"
#include "ergon/object.h"
#include "ergon/vm.h"
bool compile(const char *source, Chunk *chunk);

#endif // !DEBUG ergon_compiler_h
