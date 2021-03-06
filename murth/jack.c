#include <stdlib.h>
#include <stdio.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include "murth.h"

static const char *client_name = "murth";
static jack_port_t *audio_output = 0, *midi_input = 0;
static jack_client_t *client = 0;

#define RUNTIME_ASSERT(condition) \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d: assert(%s) failed\n", __FILE__, __LINE__, #condition);\
    abort(); \
  }

static int process_callback(jack_nframes_t nframes, void *param) {
  void *midi_buf = jack_port_get_buffer(midi_input, nframes);
  jack_nframes_t nmidi_events = jack_midi_get_event_count(midi_buf);
  for (int i = 0; i < nmidi_events; ++i) {
    jack_midi_event_t event;
    jack_midi_event_get(&event, midi_buf, i);
    murth_process_raw_midi(event.buffer, event.size);
  }
  murth_synthesize(jack_port_get_buffer(audio_output, nframes), nframes);
  return 0;
}

static void shutdown_callback(void *param) { exit(0); }

void jack_audio_init(int *samplerate) {
  jack_status_t status;
  client = jack_client_open(client_name, JackNoStartServer, &status, NULL);
  RUNTIME_ASSERT(client != NULL);
  jack_set_process_callback(client, process_callback, 0);
  jack_on_shutdown(client, shutdown_callback, 0);
  *samplerate = jack_get_sample_rate(client);
  audio_output = jack_port_register(client, "audio_output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput | JackPortIsTerminal, 0);
  RUNTIME_ASSERT(audio_output != NULL);
  midi_input = jack_port_register(client, "midi_input", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  RUNTIME_ASSERT(midi_input != NULL);
}

void jack_audio_start() {
  RUNTIME_ASSERT(0 == jack_activate(client));
  // autoconnect audio
  const char **phys_out = jack_get_ports(client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsPhysical | JackPortIsInput);
  RUNTIME_ASSERT(phys_out != NULL);
  for (int i = 0; phys_out[i] != 0; ++i)
    RUNTIME_ASSERT(0 == jack_connect(client, jack_port_name(audio_output), phys_out[i]));
  // autoconnect midi
  const char **midi_in = jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);
  if(midi_in != NULL) {
    for (int i = 0; midi_in[i] != 0; ++i)
      fprintf(stderr, "%s -> = %d\n", midi_in[i], jack_connect(client, midi_in[i], jack_port_name(midi_input)));
    jack_free(midi_in);
  }
}

void jack_audio_close() {
  jack_client_close(client);
}
