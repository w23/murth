#include <string.h>
#include "nyatomic.h"

void nya_stream_init(void *stream) {
  nya_stream_t *s = (nya_stream_t*)stream;
  s->write = s->read = 0;
}

int nya_stream_write(void *stream, const void *in_data, int chunks) {
  nya_stream_t *s = (nya_stream_t*)stream;
  nya_chunk_t *c = (nya_chunk_t*)in_data;
  for (int i = 0; i < chunks; ++i) {
    const int writepos = s->write;
    const int write_next = (writepos + 1) & NYA_STREAM_BUFFER_MASK;
    if (write_next == s->read) return i;
    memcpy(s->chunks[writepos].data, c[i].data, sizeof(nya_chunk_t));
    __sync_synchronize();
    s->write = write_next;
  }
  return chunks;
}

int nya_stream_read(void *stream, void *out_data, int max_chunks) {
  nya_stream_t *s = (nya_stream_t*)stream;
  nya_chunk_t *c = (nya_chunk_t*)out_data;
  for (int i = 0; i < max_chunks; ++i) {
    const int readpos = s->read;
    const int writepos = s->write;
    __sync_synchronize();
    if (readpos == writepos) return 0;
    memcpy(c[i].data, s->chunks[readpos].data, sizeof(nya_chunk_t));
    s->read = (readpos + 1) & NYA_STREAM_BUFFER_MASK;
  }
  return max_chunks;
}
