#include <stdio.h>
#include <string.h>
#include "SDL2/SDL.h"
#include <algorithm>

static const float PI  = 3.141592653589793238462643383;
static const float TAU = 6.283185307179586476925286766;

// Platform data
SDL_Window *window;
SDL_Renderer *renderer;


// GUI
const int keyboard[3][24] =
{
	{
		SDL_SCANCODE_Q,
		SDL_SCANCODE_W,
		SDL_SCANCODE_E,
		SDL_SCANCODE_R,
		SDL_SCANCODE_T,
		SDL_SCANCODE_Y,
		SDL_SCANCODE_U,
		SDL_SCANCODE_I,
		SDL_SCANCODE_O,
		SDL_SCANCODE_P,
		SDL_SCANCODE_LEFTBRACKET,
		SDL_SCANCODE_RIGHTBRACKET,
		0
	},
	{
		SDL_SCANCODE_A,
		SDL_SCANCODE_S,
		SDL_SCANCODE_D,
		SDL_SCANCODE_F,
		SDL_SCANCODE_G,
		SDL_SCANCODE_H,
		SDL_SCANCODE_J,
		SDL_SCANCODE_K,
		SDL_SCANCODE_L,
		SDL_SCANCODE_SEMICOLON,
		SDL_SCANCODE_APOSTROPHE,
		0
	},
	{
		SDL_SCANCODE_Z,
		SDL_SCANCODE_X,
		SDL_SCANCODE_C,
		SDL_SCANCODE_V,
		SDL_SCANCODE_B,
		SDL_SCANCODE_N,
		SDL_SCANCODE_M,
		SDL_SCANCODE_COMMA,
		SDL_SCANCODE_PERIOD,
		SDL_SCANCODE_SLASH,
		0
	}
};

int rect_size = 30;
int border = 10;



// Command queue
/* We have a list of all possible commands.
 * A command maps its trigger (pressed key, function call, ...) to what it needs to do to play a sound.
 * A command plays a single note, but the same note could be played by different commands.
 * When a command needs to be executed, its index must be inserted in the "command queue".
 * The commands in the "command queue" are then executed and removed from the queue by the renderer.
 * By modifying the parameters in the command, we can control the renderer's behaviour.
 */

struct command_t
{
	int note;
	float pressed_time;
	float released_time;
};

typedef command_t *command_list_t;
typedef int *command_queue_t;

struct command_state_t
{
	int max_commands;

	int all_commands_size;
	command_list_t all_commands;

	int queue_size;
	command_queue_t playing_queue;
};

void command_state_init(command_state_t *state, int max_commands)
{
	state->max_commands = max_commands;

	state->all_commands_size = 0;
	state->all_commands = (command_t*)malloc(max_commands * sizeof(command_t));
	memset(state->all_commands, 0, max_commands * sizeof(command_t));

	state->queue_size = 0;
	state->playing_queue = (int*)malloc(max_commands * sizeof(int));
	memset(state->playing_queue, 0, max_commands * sizeof(int));
}

void command_state_deinit(command_state_t *state)
{
	state->max_commands = 0;

	state->all_commands_size = 0;
	free(state->all_commands);

	state->queue_size = 0;
	free(state->playing_queue);
}

void playing_queue_insert(command_state_t *state, int command_index)
{
	int found = -1;
	for(int i = 0; i < state->queue_size; i++)
	{
		if(state->playing_queue[i] == command_index)
		{
			found = i;
			break;
		}
	}
	if(found == -1)
	{
		// Not found, insert
		state->playing_queue[state->queue_size] = command_index;
		state->queue_size++;
	}
}
void playing_queue_remove(command_state_t *state, int command_index)
{
	int found = -1;
	for(int i = 0; i < state->queue_size; i++)
	{
		if(state->playing_queue[i] == command_index)
		{
			found = i;
			break;
		}
	}
	if(found != -1)
	{
		// Found, remove
		memmove(&state->playing_queue[found], &state->playing_queue[found+1], (state->queue_size - 1 - found) * sizeof(int));
		state->queue_size--;
	}
}

// Audio
struct ADSR
{
	float amax; // amplitude
	float attack; // time
	float decay; // time
	float sustain; // amplitude
	float release; // time
};

// Time
long long sample_count = 0;
int sample_freq = 48000;
float get_time()
{
	float t = (float)sample_count / sample_freq;
	return t;
}
float get_time_sample_offset(int sample_offset)
{
	float t = (float)(sample_count + sample_offset) / sample_freq;
	return t;
}
void time_advance_by_samples(int samples)
{
	sample_count += samples;
}

// Shared state
bool quit = false;
command_state_t command_state;

// Audio rendering
struct audio_stream
{
	float L;
	float R;
};

ADSR adsr =
{
	.amax = 1.0f,
	.attack = 0.01,
	.decay = 0.1f,
	.sustain = 0.9f,
	.release = 0.5f
};

float compute_amplitude(command_t *command, float time)
{
	float amplitude = 0.0f;
	if(time < command->pressed_time)
	{
		// Too soon
	}
	else if(time < (command->pressed_time + adsr.attack))
	{
		// Attack ramp
		amplitude = (time - command->pressed_time) / adsr.attack * adsr.amax;
	}
	else if(time < (command->pressed_time + adsr.attack + adsr.decay))
	{
		// Decay ramp
		amplitude = (1.0f - (time - (command->pressed_time + adsr.attack)) / adsr.decay) * (adsr.amax - adsr.sustain) + adsr.sustain;
	}
	else if(time < command->released_time)
	{
		// Sustain
		amplitude = adsr.sustain;
	}
	else if(time < (command->released_time + adsr.release))
	{
		// Decay
		amplitude = adsr.sustain * (1.0f - ((time - command->released_time) / adsr.release));
	}

	return amplitude;
}

bool is_playing(command_t *command, float time)
{
	float start_boundary = command->pressed_time;
	float end_boundary = command->released_time + adsr.release;
	return start_boundary <= time && time < end_boundary;
}

void audio_callback(void *userdata, Uint8 *stream_data, int len)
{
	audio_stream *stream = (audio_stream *)stream_data;
	int samples = len / sizeof(audio_stream);

	for(int i = 0; i < samples; i++)
	{
		stream[i].L = 0.0f;
		stream[i].R = 0.0f;
	}

	for(int q = 0; q < command_state.queue_size; q++)
	{
		int command_index = command_state.playing_queue[q];
		command_t command = command_state.all_commands[command_index];

		float freq = pow(2, (float)command.note / 12) * 440;
		for(int s = 0; s < samples; s++)
		{
			// Note
			float t = get_time_sample_offset(s);
			float v = sin(TAU * freq * t) * compute_amplitude(&command, t) * 0.1;

			stream[s].L += v;
			stream[s].R += v;
		}

	}

	time_advance_by_samples(samples);
	// cleanup
	float time = get_time();
	for(int q = 0; q < command_state.queue_size; q++)
	{
		int command_index = command_state.playing_queue[q];
		command_t command = command_state.all_commands[command_index];
		if(!is_playing(&command, time))
		{
			playing_queue_remove(&command_state, command_index);
			q--; // Fix index
		}
	}
}

// Input
int scancode_to_command_index_map[1024];
void keyboard_input_init()
{
	int key_sequence[] =
	{
		SDL_SCANCODE_Q,
		SDL_SCANCODE_W,
		SDL_SCANCODE_E,
		SDL_SCANCODE_R,
		SDL_SCANCODE_T,
		SDL_SCANCODE_Y,
		SDL_SCANCODE_U,
		SDL_SCANCODE_I,
		SDL_SCANCODE_O,
		SDL_SCANCODE_P,
		SDL_SCANCODE_LEFTBRACKET,
		SDL_SCANCODE_RIGHTBRACKET,
		SDL_SCANCODE_A,
		SDL_SCANCODE_S,
		SDL_SCANCODE_D,
		SDL_SCANCODE_F,
		SDL_SCANCODE_G,
		SDL_SCANCODE_H,
		SDL_SCANCODE_J,
		SDL_SCANCODE_K,
		SDL_SCANCODE_L,
		SDL_SCANCODE_SEMICOLON,
		SDL_SCANCODE_APOSTROPHE,
		SDL_SCANCODE_Z,
		SDL_SCANCODE_X,
		SDL_SCANCODE_C,
		SDL_SCANCODE_V,
		SDL_SCANCODE_B,
		SDL_SCANCODE_N,
		SDL_SCANCODE_M,
		SDL_SCANCODE_COMMA,
		SDL_SCANCODE_PERIOD,
		SDL_SCANCODE_SLASH,
		0
	};

	for(int i = 0; i < 1024; i++)
		scancode_to_command_index_map[i] = -1;

	int key_count = 0;
	for(int i = 0; key_sequence[i] != 0; i++)
	{
		scancode_to_command_index_map[key_sequence[i]] = i;
		key_count++;
	}

	// Init commands related to keyboard
	int base = command_state.all_commands_size;
	command_state.all_commands_size += key_count;
	for(int i = 0; i < key_count; i++)
	{
		command_state.all_commands[base + i].note = i - 12;
		command_state.all_commands[base + i].pressed_time = 0.0f;
		command_state.all_commands[base + i].released_time = 0.0f;
	}
}



int main(int argc, char *argv[])
{
	printf("Starting Steven...\n");

	// Program init
	command_state_init(&command_state, 256);
	keyboard_input_init();

	// SDL Init
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


	// Main loop
	while(!quit)
	{
		// Input processing
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
					if(event.key.repeat)
						continue;

					int command_index = scancode_to_command_index_map[event.key.keysym.scancode];
					if(command_index >= 0)
					{
						command_state.all_commands[command_index].pressed_time = get_time();
						command_state.all_commands[command_index].released_time = +INFINITY;
						playing_queue_insert(&command_state, command_index);
					}
				} break;

				case SDL_KEYUP:
				{
					int command_index = scancode_to_command_index_map[event.key.keysym.scancode];
					if(command_index >= 0)
					{
						command_state.all_commands[command_index].released_time = get_time();
					}
				} break;
			}
		}

		// GUI Rendering
		SDL_SetRenderDrawColor(renderer, 24, 26, 27, 0xFF);
		SDL_RenderClear(renderer);

		// Draw keyboard
		for(int r = 0; r < 3; r++)
		{
			for(int c = 0; keyboard[r][c] != 0; c++)
			{
				int key = keyboard[r][c];

				SDL_Rect rect;
				rect.x = border + r*rect_size/2 + c * (rect_size + border);
				rect.y = border + r * (rect_size + border);
				rect.w = rect_size;
				rect.h = rect_size;

				int command_index = scancode_to_command_index_map[key];
				float pressed_time = command_state.all_commands[command_index].pressed_time;
				float released_time = command_state.all_commands[command_index].released_time;
				float current_time = get_time();
				if(command_index >= 0)
				{
					command_t command = command_state.all_commands[command_index];
					if(is_playing(&command, current_time))
					{
						int intensity = 0xFF * fabs(compute_amplitude(&command, current_time));
						SDL_SetRenderDrawColor(renderer, intensity, intensity, intensity, 0xFF);
						SDL_RenderFillRect(renderer, &rect);
					}
				}

				SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
				SDL_RenderDrawRect(renderer, &rect);
			}
		}

		SDL_RenderPresent(renderer);
	}

	// SDL deinit
	command_state_deinit(&command_state);
	SDL_CloseAudioDevice(dev);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
