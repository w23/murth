#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// single producer, single consumer chunked stream

#define NYA_STREAM_CHUNK_SIZE 16
#define NYA_STREAM_BUFFER_CHUNKS 256
#define NYA_STREAM_BUFFER_MASK (NYA_STREAM_BUFFER_CHUNKS-1)

typedef struct {
  char data[NYA_STREAM_CHUNK_SIZE];
} nya_chunk_t;
typedef struct {
  volatile unsigned write, read;
  nya_chunk_t chunks[NYA_STREAM_BUFFER_CHUNKS];
} nya_stream_t;

extern void nya_stream_init(void *stream);
extern int nya_stream_write(void *stream, const void *in_data, int chunks);
extern int nya_stream_read(void *stream, void *out_data, int max_chunks);

#ifdef __cplusplus
}
#endif
