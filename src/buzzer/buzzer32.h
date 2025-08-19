#ifndef BUZZER32_H
#define BUZZER32_H

#include "Global/global.h"
#include <stdbool.h>

// Structure to represent a buzzer tone
typedef struct buzzer_tone_struct {
    int frequency;
    int duration;
    int pause;
    int repetitions;
} Tone;

// C Interface Functions
bool buzzer_init_c(void);
bool buzzer_play_tone_c(int freq, int duration, bool blocking= false);
bool buzzer_play_sequence_c(Tone* tone, bool blocking= false);
bool buzzer_play_music_c(const char* music_str, bool blocking= false);
void buzzer_force_stop_c(void);
void buzzer_set_speed_c(int new_speed);
void buzzer_on_c(int frequency);
void buzzer_off_c(void);
// Module registration function for use with require()
int luaopen_buzzer32(lua_State* L);

// Function to register the buzzer module globally
// Call this during Lua state initialization
// void register_buzzer32_global(lua_State* L);

#endif // BUZZER32_H
