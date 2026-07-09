#include <stdio.h>
#include <stdlib.h>

#include "../include/ergon/ergon_chunks.h"
#include "../include/ergon/ergon_memory.h"

void init_chunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  init_value_array(&chunk->constants);

  chunk->line_count = 0;
  chunk->line_capacity = 0;
  chunk->lines = NULL;
}

void write_chunk(Chunk *chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code =
        GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->count++;

  if (chunk->line_count > 0 &&
      chunk->lines[chunk->line_count - 1].line == line) {
    return;
  }

  if (chunk->line_capacity < chunk->line_count + 1) {
    int old_line_capacity = chunk->line_capacity;
    chunk->line_capacity = GROW_CAPACITY(old_line_capacity);
    chunk->lines = GROW_ARRAY(LineStart, chunk->lines, old_line_capacity,
                              chunk->line_capacity);
  }

  LineStart *line_start = &chunk->lines[chunk->line_count++];
  line_start->offset = chunk->count - 1;
  line_start->line = line;
}

void free_chunk(Chunk *chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(LineStart, chunk->lines, chunk->line_capacity);
  free_value_array(&chunk->constants);
  init_chunk(chunk);
}

int add_constant(Chunk *chunk, Value value) {
  write_value_array(&chunk->constants, value);
  return chunk->constants.count - 1;
}

int get_line(Chunk *chunk, int instruction) {
  for (int i = 0; i < chunk->line_count - 1; i++) {
    if (chunk->lines[i + 1].offset > instruction) {
      return chunk->lines[i].line;
    }
  }
  return chunk->lines[chunk->line_count - 1].line;
}
