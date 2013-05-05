// cc -std=c99 -lpthread -o testnya testnya.c nyatomic.c && ./testnya
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "nyatomic.h"

#define CHUNKS (256*1024)

nya_stream_t stream;

void *thread_consumer(void *param) {
  int counter = 0;
  for (int i = 0; i < CHUNKS; ++i) {
    int values[4];
    while(0 == nya_stream_read(&stream, values, 1));
    printf("r (%d %d %d %d)\n", values[0], values[1], values[2], values[3]);
#define CHECK(i) if(values[i]!=counter++){exit(counter);}
    CHECK(0);
    CHECK(1);
    CHECK(2);
    CHECK(3);
  }
}

int main(int argc, char *argv[]) {
  nya_stream_init(&stream);
  pthread_t consumer;
  if (0 != pthread_create(&consumer, NULL, thread_consumer, NULL)) {
    printf("pthread\n");
    return -1;
  }
  int counter = 0;
  for (int i = 0; i < CHUNKS; ++i) {
    int values[4];
    values[0] = counter++; values[1] = counter++; values[2] = counter++; values[3] = counter++;
    printf("w [%d %d %d %d]\n", values[0], values[1], values[2], values[3]);
    while(0 == nya_stream_write(&stream, values, 1));
  }
  pthread_join(consumer, 0);
  return 0;
}
