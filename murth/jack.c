#include <stdlib.h>
#include <stdio.h>
#include <jack/jack.h>
#include "synth.h"

static const char *client_name = "murth";
static const char *output_port_name = "output";
static jack_port_t *port_output = 0;
static jack_client_t *client = 0;

#define RUNTIME_ASSERT(condition) \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d: assert(%s) failed\n", __FILE__, __LINE__, #condition);\
    abort(); \
  }

static int process_callback(jack_nframes_t nframes, void *param) {
  murth_synthesize(jack_port_get_buffer(port_output, nframes), nframes);
  return 0;
}

static void shutdown_callback(void *param) { exit(0); }

void jack_audio_init(int *samplerate) {
  jack_status_t status;
  client = jack_client_open(client_name, JackNullOption, &status, NULL);
  RUNTIME_ASSERT(client != NULL);
  jack_set_process_callback(client, process_callback, 0);
  jack_on_shutdown(client, shutdown_callback, 0);
  *samplerate = jack_get_sample_rate(client);
  port_output = jack_port_register(client, output_port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput | JackPortIsTerminal, 0);
  RUNTIME_ASSERT(port_output != NULL);
}

void jack_audio_start() {
  RUNTIME_ASSERT(0 == jack_activate(client));
  // autoconnect
  const char **phys_out = jack_get_ports(client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
  RUNTIME_ASSERT(phys_out != NULL);
  for (int i = 0; phys_out[i] != 0; ++i)
    RUNTIME_ASSERT(0 == jack_connect(client, jack_port_name(port_output), phys_out[i]));
  jack_free(phys_out);
}

void jack_audio_close() {
  jack_client_close(client);
}
