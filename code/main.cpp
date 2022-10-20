#include <stdio.h>
#include <string.h>
#include "SDL2/SDL.h"
#include <algorithm>

static const float PI  = 3.141592653589793238462643383;
static const float TAU = 6.283185307179586476925286766;

SDL_Window *window;
SDL_Renderer *renderer;

const char *keyboard[3] =
{
	"QWERTYUIOP[]",
	"ASDFGHJKL;'",
	"ZXCVBNM,./"
};

int rect_size = 30;
int border = 10;
bool quit = false;

char pressed_keys[256];
int pressed_count = 0;

char scancode_to_char_map[1024];
int char_to_index_map[256];
void init_map()
{
	memset(&scancode_to_char_map, 0, 1024);
	scancode_to_char_map[SDL_SCANCODE_Q] = 'Q';
	scancode_to_char_map[SDL_SCANCODE_W] = 'W';
	scancode_to_char_map[SDL_SCANCODE_E] = 'E';
	scancode_to_char_map[SDL_SCANCODE_R] = 'R';
	scancode_to_char_map[SDL_SCANCODE_T] = 'T';
	scancode_to_char_map[SDL_SCANCODE_Y] = 'Y';
	scancode_to_char_map[SDL_SCANCODE_U] = 'U';
	scancode_to_char_map[SDL_SCANCODE_I] = 'I';
	scancode_to_char_map[SDL_SCANCODE_O] = 'O';
	scancode_to_char_map[SDL_SCANCODE_P] = 'P';
	scancode_to_char_map[SDL_SCANCODE_LEFTBRACKET] = '[';
	scancode_to_char_map[SDL_SCANCODE_RIGHTBRACKET] = ']';
	scancode_to_char_map[SDL_SCANCODE_A] = 'A';
	scancode_to_char_map[SDL_SCANCODE_S] = 'S';
	scancode_to_char_map[SDL_SCANCODE_D] = 'D';
	scancode_to_char_map[SDL_SCANCODE_F] = 'F';
	scancode_to_char_map[SDL_SCANCODE_G] = 'G';
	scancode_to_char_map[SDL_SCANCODE_H] = 'H';
	scancode_to_char_map[SDL_SCANCODE_J] = 'J';
	scancode_to_char_map[SDL_SCANCODE_K] = 'K';
	scancode_to_char_map[SDL_SCANCODE_L] = 'L';
	scancode_to_char_map[SDL_SCANCODE_SEMICOLON] = ';';
	scancode_to_char_map[SDL_SCANCODE_APOSTROPHE] = '\'';
	scancode_to_char_map[SDL_SCANCODE_Z] = 'Z';
	scancode_to_char_map[SDL_SCANCODE_X] = 'X';
	scancode_to_char_map[SDL_SCANCODE_C] = 'C';
	scancode_to_char_map[SDL_SCANCODE_V] = 'V';
	scancode_to_char_map[SDL_SCANCODE_B] = 'B';
	scancode_to_char_map[SDL_SCANCODE_N] = 'N';
	scancode_to_char_map[SDL_SCANCODE_M] = 'M';
	scancode_to_char_map[SDL_SCANCODE_COMMA] = ',';
	scancode_to_char_map[SDL_SCANCODE_PERIOD] = '.';
	scancode_to_char_map[SDL_SCANCODE_SLASH] = '/';

	memset(&char_to_index_map, 0, 256 * sizeof(int));
	char order[] = "QWERTYUIOP[]ASDFGHJKL;\'ZXCVBNM,./";
	for(int i = 0; i < strlen(order); i++)
		char_to_index_map[order[i]] = i;
}

struct audio_stream
{
	float L;
	float R;
};

long long sample_count = 0;
int sample_freq = 48000;
void audio_callback(void *userdata, Uint8 *stream_data, int len)
{
	audio_stream *stream = (audio_stream *)stream_data;
	int samples = len / sizeof(audio_stream);

	for(int i = 0; i < samples; i++)
	{
		stream[i].L = 0.0f;
		stream[i].R = 0.0f;
	}

	for(int k = 0; k < pressed_count; k++)
	{
		int index = char_to_index_map[pressed_keys[k]];
		//float freq = 440 / 16 * (1+index);
		float freq = pow(2, (float)(index + - 13) / 12) * 440;
		for(int s = 0; s < samples; s++)
		{
			// Nota
			float t = (float)(sample_count + s) / sample_freq;
			float v = sin(TAU * freq * t) * 0.1;

			// Distorsore
			// float mod_freq = 5.0f;
			// float gain = 10.0f * (1 + sin(TAU * mod_freq * t)) / 2;
			// v = std::min(v * gain, 0.1f);

			stream[s].L += v;
			stream[s].R += v;
		}

	}

	sample_count += samples;
}

int main()
{
	printf("Starting Steven...\n");
	init_map();

	if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to initialize SDL: %s", SDL_GetError());
		return 1;
	}

	window = SDL_CreateWindow("Steven", 0, 0, 500, 150, 0);
	if(window == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
		return 1;
	}
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if(renderer == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
		return 1;
	}

	SDL_AudioDeviceID dev;
	{
		SDL_AudioSpec want, have;

		SDL_memset(&want, 0, sizeof(want));
		want.freq = sample_freq;
		want.format = AUDIO_F32;
		want.channels = 2;
		want.samples = 512;
		want.callback = audio_callback;

		dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
		if(dev == 0)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio: %s", SDL_GetError());
			return 2;
		}
		if(have.format != want.format)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open audio device with FLOAT32 format.");
			return 2;
		}
		SDL_PauseAudioDevice(dev, 0);
	}



	while(!quit)
	{
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_QUIT:
				{
					quit = true;
				} break;
				case SDL_KEYDOWN:
				{
					if(!event.key.repeat)
					{
						char key = scancode_to_char_map[event.key.keysym.scancode];
						if(key != 0)
						{
							pressed_keys[pressed_count] = key;
							pressed_count++;
						}
					}
				} break;
				case SDL_KEYUP:
				{
					char key = scancode_to_char_map[event.key.keysym.scancode];
					if(key != 0)
					{
						int pos;
						for(pos = 0; pos < pressed_count; pos++)
						{
							if(pressed_keys[pos] == key)
								break;
						}
						if(pos != pressed_count)
						{
							memmove(&pressed_keys[pos], &pressed_keys[pos + 1], pressed_count - pos - 1);
							pressed_count--;
						}
					}
				} break;
			}
		}

		SDL_SetRenderDrawColor(renderer, 24, 26, 27, 0xFF);
		SDL_RenderClear(renderer);

		// Draw keyboard outline
		SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
		for(int r = 0; r < 3; r++)
		{
			int len = strlen(keyboard[r]);
			for(int c = 0; c < len; c++)
			{
				SDL_Rect rect;
				rect.x = border + r*rect_size/2 + c * (rect_size + border);
				rect.y = border + r * (rect_size + border);
				rect.w = rect_size;
				rect.h = rect_size;
				SDL_RenderDrawRect(renderer, &rect);
			}
		}

		// Draw pressed keys
		for(int i = 0; i < pressed_count; i++)
		{
			char key = pressed_keys[i];
			bool found = false;
			int found_r, found_c;
			for(int r = 0; r < 3 && !found; r++)
			{
				int len = strlen(keyboard[r]);
				for(int c = 0; c < len && !found; c++)
				{
					if(keyboard[r][c] == key)
					{
						found = true;
						found_r = r;
						found_c = c;
					}
				}
			}
			if(found)
			{
				SDL_Rect rect;
				rect.x = border + found_r*rect_size/2 + found_c * (rect_size + border);
				rect.y = border + found_r * (rect_size + border);
				rect.w = rect_size;
				rect.h = rect_size;
				SDL_RenderFillRect(renderer, &rect);
			}
		}

		SDL_RenderPresent(renderer);
	}

	SDL_CloseAudioDevice(dev);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
