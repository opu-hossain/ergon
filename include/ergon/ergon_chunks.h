#ifndef ergon_chunks_h
#define ergon_chunks_h

#include "ergon_common.h"
#include "ergon_value.h"

typedef enum {
  OP_CONSTANT,
  OP_RETURN,
} OpCode;

typedef struct {
  int offset;
  int line;
} LineStart;

typedef struct {
  int count;
  int capacity;
  uint8_t *code;
  value_array constants;

  int line_count;
  int line_capacity;
  LineStart *lines;
} Chunk;

void init_chunk(Chunk *chunk);
void write_chunk(Chunk *chunk, uint8_t byte, int line);
void free_chunk(Chunk *chunk);
int add_constant(Chunk *chunk, Value value);
int get_line(Chunk *chunk, int instruction);

#endif // !ergon_chunks_h
