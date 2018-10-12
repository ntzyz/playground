#include <SDL.h>
#include <math.h>
#include <FLAC/stream_decoder.h>
#include <pthread.h>

#undef main
#define SDL_main main

void feed_audio_stream(void *userdata, uint8_t *stream, int length);

void cleanup_event_queue() {
  static SDL_Event e;
  while (SDL_PollEvent(&e)) {
    ;
  }
}

struct pcm_buffer_node {
  size_t sample_count;
  int32_t **buffer;
  struct pcm_buffer_node *next;
};

struct audio_session {
  uint32_t sample_offset;
  uint32_t channels;
  struct pcm_buffer_node *head;
  struct pcm_buffer_node *current;
};

struct decode_session {
  uint32_t freq;
  uint32_t channels;
  struct pcm_buffer_node *head;
  struct pcm_buffer_node *tail;
  struct audio_session *playback_session;
};

void init_sdl_audio(int channels, int freq, struct audio_session **audio_session) {
  *audio_session = (struct audio_session *) malloc(sizeof(struct audio_session));
  (*audio_session)->sample_offset = 0;
  (*audio_session)->channels = channels;

  SDL_AudioSpec wanted_spec = {
    .freq = freq,
    .format = AUDIO_S16LSB,
    .channels = channels,
    .callback = feed_audio_stream,
    .userdata = *audio_session,
  };

  if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
    puts(SDL_GetError());
  }
}

void start_sdl_audio_playback(struct decode_session *decode_session) {
  printf("playback started!\n");
  SDL_PauseAudio(0);
}

void feed_audio_stream(void *userdata, uint8_t *stream, int length) {
  int i = 0, channel;
  struct audio_session *session = (struct audio_session *) userdata;

  int16_t *real_stream = (int16_t *)stream;
  int real_length = length * sizeof(uint8_t) / sizeof(int16_t) / session->channels;

  memset(stream, 0x00, length);

  for (i = 0; i != real_length; i++) {
    for (channel = 0; channel != session->channels; channel++) {
      if (session->sample_offset >= session->current->sample_count) {
        session->sample_offset = 0;
        if (session->current->next) {
          session->current = session->current->next;
        } else {
          return;
        }
      }
      real_stream[i * session->channels + channel] = session->current->buffer[channel][session->sample_offset] 
>> 8;
    }
    session->sample_offset ++;
  }
}

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
                                              const FLAC__int32 * const buffer[], void *client_data) {
  struct decode_session *session = (struct decode_session *) client_data;
  struct pcm_buffer_node *node = (struct pcm_buffer_node *) malloc(sizeof(struct pcm_buffer_node));
  int i;

  printf("."); fflush(stdout);
  memset(node, 0, sizeof(struct pcm_buffer_node));
  node->buffer = (int32_t **) malloc(frame->header.channels * sizeof(int32_t *));
  node->sample_count = frame->header.blocksize;
  node->next = NULL;

  // each channel require buffer for their pcm data.
  for (i = 0; i != frame->header.channels; i++) {
    node->buffer[i] = (int32_t *) malloc(frame->header.blocksize * sizeof(int32_t));
    memcpy(node->buffer[i], buffer[i], frame->header.blocksize * sizeof(int32_t));
  }

  if (session->head == NULL) {
    // first frame, begin playback
    session->head = node;
    session->tail = node;

    session->playback_session->head = node;
    session->playback_session->current = node;
    start_sdl_audio_playback(session);
  } else {
    session->tail->next = node;
    session->tail = node;
  }

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void 
*client_data) {
  struct decode_session *session = (struct decode_session *) client_data;
	/* print some stats */
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		/* save for later */
		uint64_t total_samples = metadata->data.stream_info.total_samples;
		uint32_t sample_rate = metadata->data.stream_info.sample_rate;
		uint32_t channels = metadata->data.stream_info.channels;
		uint32_t bps = metadata->data.stream_info.bits_per_sample;

    session->channels = channels;
    session->freq = sample_rate;

    init_sdl_audio(channels, sample_rate, &(session->playback_session));

		fprintf(stderr, "sample rate    : %u Hz\n", sample_rate);
		fprintf(stderr, "channels       : %u\n", channels);
		fprintf(stderr, "bits per sample: %u\n", bps);
		fprintf(stderr, "total samples  : %" PRIu64 "\n", total_samples);
	}
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void 
*client_data) {
	fprintf(stderr, "Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

struct decode_session *init_flac(const char *filename) {
  FLAC__StreamDecoder *decoder = 0;
  FLAC__StreamDecoderInitStatus init_status;
  struct decode_session *decode_session = (struct decode_session *) malloc(sizeof(struct decode_session));
  memset(decode_session, 0, sizeof(struct decode_session));

  decoder = FLAC__stream_decoder_new();
  FLAC__stream_decoder_set_md5_checking(decoder, true);
	init_status = FLAC__stream_decoder_init_file(decoder, filename, write_callback, metadata_callback, 
error_callback, decode_session);

  FLAC__stream_decoder_process_until_end_of_stream(decoder);

  return decode_session;
}

void flac_decoder_thread_entry(void *userdata) {
  struct decode_session **p_decode_session = (struct decode_session **)userdata;

  *p_decode_session = init_flac("/home/kasora/01. Der Zigeunerbaron Einzugsmarsch 2.flac");
}

int SDL_main() {
  SDL_Init(SDL_INIT_EVERYTHING);

  SDL_Surface *screen = SDL_SetVideoMode(500, 500, 32, SDL_SWSURFACE);
  SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 255, 255, 128));
  SDL_Flip(screen);

  struct decode_session *decode_session;
  pthread_t decode_thread;
  pthread_create(&decode_thread, NULL, flac_decoder_thread_entry, (void *) &decode_session);

  SDL_Event e;
  for (;;) {
    SDL_PollEvent(&e);
    if (e.type == SDL_QUIT) {
      break;
    }

    cleanup_event_queue();
    SDL_Delay(30);
  }

  SDL_Quit();
  return 0;
}