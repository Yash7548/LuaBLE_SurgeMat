#include "buzzer32.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "esp_log.h"

#ifndef BUZZER_PIN
#define BUZZER_PIN 47 // Default GPIO pin for buzzer speaker
#endif
#ifndef BUZZER_CHANNEL
#define BUZZER_CHANNEL 4 // Default LEDC channel
#endif

#define PWM_RESOLUTION LEDC_TIMER_8_BIT // PWM resolution (8-bit: 0-255)
#define DEFAULT_DUTY 127     // Default duty cycle (50% of 255 for 8-bit)
#define TASK_STACK_SIZE 4096 // Stack size for background task (increased for stability)
#define QUEUE_SIZE 5         // Size of the command queue (reduced to save memory)
#define LEDC_TIMER           LEDC_TIMER_0
#define LEDC_MODE            LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL_NUM     LEDC_CHANNEL_0
#define LEDC_DUTY_RES        LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY       5000 // Frequency in Hertz

// Music note frequencies
#include "notes.h" // Using the provided notes.h

// buzzerCommand types for task queue
typedef enum
{
    CMD_PLAY,
    CMD_STOP,
    CMD_SUCCESS,
    CMD_ERROR,
    CMD_MUSIC,
    CMD_FORCE_STOP,
    CMD_ON,
    CMD_OFF
} CommandType;

// buzzerCommand execution mode
typedef enum
{
    MODE_BACKGROUND, // Return immediately
    MODE_BLOCKING    // Wait for completion
} ExecutionMode;

// Structure for command queue
typedef struct
{
    CommandType type;
    Tone tone;
    char *music;                      // For storing music string
    ExecutionMode mode;               // Execution mode
    SemaphoreHandle_t completion_sem; // For blocking mode
} buzzerCommand;

// Global state
static TaskHandle_t buzzer_task_handle = NULL;
static QueueHandle_t command_queue = NULL;
static lua_State *L_global = NULL;
static int callback_ref_step = LUA_NOREF;
static int callback_ref_done = LUA_NOREF;
static bool is_playing = false;
static bool is_setup = false;
static bool force_stop_flag = false;
static int speed = 80; // ms i think

// Forward declarations
static void buzzer_task(void *pvParameters);
static int l_buzzer_play(lua_State *L);
static int l_buzzer_stop(lua_State *L);
static int l_buzzer_success(lua_State *L);
static int l_buzzer_error(lua_State *L);
static int l_buzzer_play_music(lua_State *L);
static int l_buzzer_is_playing(lua_State *L);
static int l_buzzer_set_callback(lua_State *L);
static int l_buzzer_set_speed(lua_State *L);
static int l_buzzer_force_stop(lua_State *L);
static int l_buzzer_on(lua_State *L);
static int l_buzzer_off(lua_State *L);
static void play_tone(int frequency, int duration);
static void play_sequence(Tone *tone);
static void play_music(const char *music_str);

// C Interface Implementation
bool buzzer_init_c(void)
{
    if (is_setup)
    {
        return true; // Already initialized
    }

    // Initialize LEDC using Arduino framework
    if (!ledcAttach(BUZZER_PIN, 5000, PWM_RESOLUTION)) {
        return false;
    }
    ledcWrite(BUZZER_PIN, 0);

    // Create command queue
    if (command_queue == NULL)
    {
        command_queue = xQueueCreate(QUEUE_SIZE, sizeof(buzzerCommand));
        if (command_queue == NULL)
        {
            return false;
        }
    }

    // Create background task
    if (buzzer_task_handle == NULL)
    {
        BaseType_t res = xTaskCreate(
            buzzer_task,
            "buzzer_task",
            TASK_STACK_SIZE,
            NULL,
            2, // Increased priority for better real-time response
            &buzzer_task_handle);

        if (res != pdPASS)
        {
            return false;
        }
    }

    is_setup = true;
    return true;
}

bool buzzer_play_tone_c(int freq, int duration, bool blocking)
{
    if (!is_setup && !buzzer_init_c())
    {
        return false;
    }

    Tone tone = {
        .frequency = freq,
        .duration = duration,
        .pause = 0,
        .repetitions = 1};

    buzzerCommand cmd = {
        .type = CMD_PLAY,
        .tone = tone,
        .music = NULL,
        .mode = blocking ? MODE_BLOCKING : MODE_BACKGROUND,
        .completion_sem = NULL};

    if (blocking)
    {
        cmd.completion_sem = xSemaphoreCreateBinary();
        if (cmd.completion_sem == NULL)
        {
            return false;
        }
    }

    if (xQueueSend(command_queue, &cmd, portMAX_DELAY) != pdPASS)
    {
        if (blocking && cmd.completion_sem != NULL)
        {
            vSemaphoreDelete(cmd.completion_sem);
        }
        return false;
    }

    if (blocking)
    {
        xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
        vSemaphoreDelete(cmd.completion_sem);
    }

    return true;
}

bool buzzer_play_sequence_c(Tone *tone, bool blocking)
{
    if (!is_setup && !buzzer_init_c())
    {
        return false;
    }

    buzzerCommand cmd = {
        .type = CMD_PLAY,
        .tone = *tone,
        .music = NULL,
        .mode = blocking ? MODE_BLOCKING : MODE_BACKGROUND,
        .completion_sem = NULL};

    if (blocking)
    {
        cmd.completion_sem = xSemaphoreCreateBinary();
        if (cmd.completion_sem == NULL)
        {
            return false;
        }
    }

    if (xQueueSend(command_queue, &cmd, portMAX_DELAY) != pdPASS)
    {
        if (blocking && cmd.completion_sem != NULL)
        {
            vSemaphoreDelete(cmd.completion_sem);
        }
        return false;
    }

    if (blocking)
    {
        xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
        vSemaphoreDelete(cmd.completion_sem);
    }

    return true;
}

bool buzzer_play_music_c(const char *music_str, bool blocking)
{
    if (!is_setup && !buzzer_init_c())
    {
        return false;
    }

    if (strlen(music_str) < 2)
    {
        return false;
    }

    buzzerCommand cmd = {
        .type = CMD_MUSIC,
        .music = strdup(music_str),
        .mode = blocking ? MODE_BLOCKING : MODE_BACKGROUND,
        .completion_sem = NULL};

    if (cmd.music == NULL)
    {
        return false;
    }

    if (blocking)
    {
        cmd.completion_sem = xSemaphoreCreateBinary();
        if (cmd.completion_sem == NULL)
        {
            free(cmd.music);
            return false;
        }
    }

    if (xQueueSend(command_queue, &cmd, portMAX_DELAY) != pdPASS)
    {
        free(cmd.music);
        if (blocking && cmd.completion_sem != NULL)
        {
            vSemaphoreDelete(cmd.completion_sem);
        }
        return false;
    }

    if (blocking)
    {
        xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
        vSemaphoreDelete(cmd.completion_sem);
    }

    return true;
}

void buzzer_force_stop_c(void)
{
    if (!is_setup)
    {
        return;
    }

    buzzerCommand cmd = {
        .type = CMD_FORCE_STOP,
        .mode = MODE_BLOCKING,
        .completion_sem = xSemaphoreCreateBinary()};

    if (cmd.completion_sem == NULL)
    {
        return;
    }

    xQueueSend(command_queue, &cmd, portMAX_DELAY);
    xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
    vSemaphoreDelete(cmd.completion_sem);
}

void buzzer_set_speed_c(int new_speed)
{
    if (new_speed < 1)
    {
        new_speed = 1; // Minimum speed
    }
    speed = new_speed;
}

void buzzer_on_c(int frequency)
{
    if (!is_setup && !buzzer_init_c())
    {
        return;
    }

    if (frequency <= 0 || frequency > 20000)
    {
        return; // Invalid frequency
    }

    // For real-time sensor data, bypass queue and set tone directly
    // This provides immediate response for sensor anomaly detection
    ledcWriteTone(BUZZER_PIN, frequency);
    
    // Reset force stop flag since we're starting new tone
    force_stop_flag = false;
}

void buzzer_off_c(void)
{
    if (!is_setup)
    {
        return;
    }

    // Emergency stop - set flag immediately
    force_stop_flag = true;
    
    // Clear the command queue immediately
    xQueueReset(command_queue);
    
    // Stop buzzer immediately at hardware level
    ledcWrite(BUZZER_PIN, 0);
    
    // Reset playing state
    is_playing = false;
}

// Initialize the buzzer module
static int l_buzzer_init(lua_State *L)
{
    L_global = L;

    // Initialize buzzer system using C interface
    if (!buzzer_init_c())
    {
        return luaL_error(L, "Failed to initialize buzzer system");
    }

    // Return the buzzer table
    lua_newtable(L);

    // Register functions
    lua_pushcfunction(L, l_buzzer_play);
    lua_setfield(L, -2, "play");

    lua_pushcfunction(L, l_buzzer_stop);
    lua_setfield(L, -2, "stop");

    lua_pushcfunction(L, l_buzzer_force_stop);
    lua_setfield(L, -2, "force_stop");

    lua_pushcfunction(L, l_buzzer_success);
    lua_setfield(L, -2, "success");

    lua_pushcfunction(L, l_buzzer_error);
    lua_setfield(L, -2, "error");

    lua_pushcfunction(L, l_buzzer_play_music);
    lua_setfield(L, -2, "play_music");

    lua_pushcfunction(L, l_buzzer_is_playing);
    lua_setfield(L, -2, "is_playing");

    lua_pushcfunction(L, l_buzzer_set_callback);
    lua_setfield(L, -2, "set_callback");

    lua_pushcfunction(L, l_buzzer_set_speed);
    lua_setfield(L, -2, "set_speed");

    lua_pushcfunction(L, l_buzzer_on);
    lua_setfield(L, -2, "on");

    lua_pushcfunction(L, l_buzzer_off);
    lua_setfield(L, -2, "off");
    
    // Create notes table
    lua_newtable(L);

    // Add all notes to the table
    // Example for a few notes - you can add more from notes.h
    lua_pushinteger(L, NOTE_C4);
    lua_setfield(L, -2, "C4");

    lua_pushinteger(L, NOTE_D4);
    lua_setfield(L, -2, "D4");

    lua_pushinteger(L, NOTE_E4);
    lua_setfield(L, -2, "E4");

    lua_pushinteger(L, NOTE_F4);
    lua_setfield(L, -2, "F4");

    lua_pushinteger(L, NOTE_G4);
    lua_setfield(L, -2, "G4");

    lua_pushinteger(L, NOTE_A4);
    lua_setfield(L, -2, "A4");

    lua_pushinteger(L, NOTE_B4);
    lua_setfield(L, -2, "B4");

    // Add all octaves
    for (int octave = 1; octave <= 8; octave++)
    {
        char note_name[4];

        sprintf(note_name, "C%d", octave);
        lua_pushinteger(L, NOTE_C1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "D%d", octave);
        lua_pushinteger(L, NOTE_D1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "E%d", octave);
        lua_pushinteger(L, NOTE_E1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "F%d", octave);
        lua_pushinteger(L, NOTE_F1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "G%d", octave);
        lua_pushinteger(L, NOTE_G1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "A%d", octave);
        lua_pushinteger(L, NOTE_A1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);

        sprintf(note_name, "B%d", octave);
        lua_pushinteger(L, NOTE_B1 * (1 << (octave - 1)));
        lua_setfield(L, -2, note_name);
    }

    // Add notes table to buzzer table
    lua_setfield(L, -2, "notes");

    return 1;
}

// Background task for playing sounds
static void buzzer_task(void *pvParameters)
{
    buzzerCommand cmd;
    const TickType_t queue_timeout = pdMS_TO_TICKS(100); // 100ms timeout

    for (;;)
    {
        // Optional: Monitor stack usage (uncomment for debugging)
        // UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
        // if (stack_remaining < 512) {
        //     Serial.printf("Warning: Buzzer task low stack: %d bytes\\n", stack_remaining * sizeof(StackType_t));
        // }
        
        // Use timeout instead of portMAX_DELAY to prevent blocking forever
        if (xQueueReceive(command_queue, &cmd, queue_timeout) == pdTRUE)
        {
            is_playing = true;

            switch (cmd.type)
            {
            case CMD_FORCE_STOP:
                // Force stop overrides everything
                force_stop_flag = true;
                ledcWrite(BUZZER_PIN, 0);
                is_playing = false;
                if (cmd.mode == MODE_BLOCKING && cmd.completion_sem != NULL)
                {
                    xSemaphoreGive(cmd.completion_sem);
                }
                continue; // Skip normal completion handling

            case CMD_ON:
                if (cmd.tone.frequency > 0 && cmd.tone.frequency <= 20000) {
                    ledcWriteTone(BUZZER_PIN, cmd.tone.frequency);
                }
                break;

            case CMD_OFF:
                ledcWrite(BUZZER_PIN, 0);
                break;

            case CMD_PLAY:
                force_stop_flag = false;
                play_sequence(&cmd.tone);
                break;

            case CMD_STOP:
                force_stop_flag = true;
                ledcWrite(BUZZER_PIN, 0);
                break;

            case CMD_SUCCESS:
                force_stop_flag = false;
                play_sequence(&cmd.tone);
                break;

            case CMD_ERROR:
                force_stop_flag = false;
                play_sequence(&cmd.tone);
                break;

            case CMD_MUSIC:
                if (cmd.music != NULL)
                {
                    force_stop_flag = false;
                    play_music(cmd.music);
                    free(cmd.music); // Free the allocated string
                    cmd.music = NULL; // Prevent double free
                }
                break;
            }

            is_playing = false;

            // Signal completion for blocking mode
            if (cmd.mode == MODE_BLOCKING && cmd.completion_sem != NULL)
            {
                xSemaphoreGive(cmd.completion_sem);
            }
        }
        
        // Add small delay to prevent task from consuming too much CPU
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Play a single tone (optimized for stack safety)
static void play_tone(int frequency, int duration)
{
    // Validate frequency range
    if (frequency <= 200 || frequency > 20000)
    {
        frequency = 3000; // Default frequency if invalid
    }

    // Check for early stop
    if (force_stop_flag) return;

    // Play the tone
    ledcWriteTone(BUZZER_PIN, frequency);

    // Wait for duration in small increments to allow interruption
    int remaining_duration = duration;
    const int check_interval = 50; // Increased to 50ms for better performance
    
    while (remaining_duration > 0 && !force_stop_flag) {
        int delay_time = (remaining_duration > check_interval) ? check_interval : remaining_duration;
        vTaskDelay(pdMS_TO_TICKS(delay_time));
        remaining_duration -= delay_time;
    }

    // Turn off the tone
    ledcWrite(BUZZER_PIN, 0);
}

// Play a sequence of tones (optimized for stack safety)
static void play_sequence(Tone *tone)
{
    if (tone == NULL) return;
    
    // Limit repetitions to prevent excessive stack usage
    int max_reps = (tone->repetitions > 100) ? 100 : tone->repetitions;
    
    for (int i = 0; i < max_reps && !force_stop_flag; i++)
    {
        play_tone(tone->frequency, tone->duration);

        // Check for force stop before pause
        if (force_stop_flag) break;

        // Pause between tones if needed
        if (tone->pause > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(tone->pause));
        }
    }
}

// Play a music string (e.g., "C41D42E43") - optimized for stack safety
static void play_music(const char *music_str)
{
    if (music_str == NULL) return;
    
    int len = strlen(music_str);
    if (len == 0 || len > 200) return; // Limit music string length for safety
    
    int i = 0;
    int note_count = 0;
    const int max_notes = 50; // Limit number of notes to prevent stack issues

    while (i < len && !force_stop_flag && note_count < max_notes)
    {
        // Extract note
        char note = music_str[i++];
        if (i >= len) break;

        // Extract duration factor
        char duration_char = music_str[i++];
        int duration_factor = duration_char - '0';
        
        // Validate duration factor
        if (duration_factor < 1 || duration_factor > 9) {
            duration_factor = 1; // Default duration
        }

        // Find the note frequency
        int frequency = 0;
        switch (note)
        {
        case 'C': case 'c': frequency = NOTE_C4; break;
        case 'D': case 'd': frequency = NOTE_D4; break;
        case 'E': case 'e': frequency = NOTE_E4; break;
        case 'F': case 'f': frequency = NOTE_F4; break;
        case 'G': case 'g': frequency = NOTE_G4; break;
        case 'A': case 'a': frequency = NOTE_A4; break;
        case 'B': case 'b': frequency = NOTE_B4; break;
        default: continue; // Skip invalid notes
        }

        // Check for force stop before playing
        if (force_stop_flag) break;

        // Play the note with validated parameters
        int note_duration = speed * duration_factor;
        if (note_duration > 5000) note_duration = 5000; // Max 5 second per note
        
        play_tone(frequency, note_duration);
        note_count++;
        
        // Check for force stop before pause
        if (force_stop_flag) break;
        
        // Pause between notes (safer calculation)
        int pause_time = speed / 7; // Simplified calculation
        if (pause_time > 0 && pause_time < 1000) {
            vTaskDelay(pdMS_TO_TICKS(pause_time));
        }
    }
}

static int l_buzzer_set_speed(lua_State *L)
{
    int _speed = luaL_checkinteger(L, 1);
    Serial.print("Playing __speed: ");
    Serial.print(_speed);
    // speed = constrain(speed, 0, 10000);
    speed = _speed;

    Serial.print("Playing After assign speed: ");
    Serial.println(speed);
    return 0;
}

// Lua: buzzer.play(options)
static int l_buzzer_play(lua_State *L)
{
    // Default values
    Tone tone = {
        .frequency = 3000, // Default frequency
        .duration = 1000,  // Default duration in ms
        .pause = 0,        // Default pause in ms
        .repetitions = 1   // Default repetitions
    };

    bool blocking = false; // Default to non-blocking mode

    // Parse options table if provided
    if (lua_istable(L, 1))
    {
        // Get frequency
        lua_getfield(L, 1, "freq");
        if (lua_isnumber(L, -1))
        {
            tone.frequency = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // Get play duration
        lua_getfield(L, 1, "play_duration");
        if (lua_isnumber(L, -1))
        {
            tone.duration = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // Get pause duration
        lua_getfield(L, 1, "pause_duration");
        if (lua_isnumber(L, -1))
        {
            tone.pause = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // Get repetitions
        lua_getfield(L, 1, "times");
        if (lua_isnumber(L, -1))
        {
            tone.repetitions = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // Get blocking mode
        lua_getfield(L, 1, "blocking");
        if (lua_isboolean(L, -1))
        {
            blocking = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        // Get callbacks
        lua_getfield(L, 1, "on_step");
        if (lua_isfunction(L, -1))
        {
            // Replace old callback if exists
            if (callback_ref_step != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_step);
            }

            // Store callback reference
            callback_ref_step = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }

        lua_getfield(L, 1, "on_done");
        if (lua_isfunction(L, -1))
        {
            // Replace old callback if exists
            if (callback_ref_done != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_done);
            }

            // Store callback reference
            callback_ref_done = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }
    }

    if (!buzzer_play_sequence_c(&tone, blocking))
    {
        return luaL_error(L, "Failed to play tone");
    }

    return 0;
}

// Lua: buzzer.stop()
static int l_buzzer_stop(lua_State *L)
{
    buzzerCommand cmd = {
        .type = CMD_STOP,
        .mode = MODE_BLOCKING,
        .completion_sem = xSemaphoreCreateBinary()};

    if (cmd.completion_sem == NULL)
    {
        return luaL_error(L, "Failed to create semaphore");
    }

    if (xQueueSend(command_queue, &cmd, portMAX_DELAY) != pdPASS)
    {
        vSemaphoreDelete(cmd.completion_sem);
        return luaL_error(L, "Failed to send stop command");
    }

    xSemaphoreTake(cmd.completion_sem, portMAX_DELAY);
    vSemaphoreDelete(cmd.completion_sem);
    return 0;
}

// Lua: buzzer.force_stop()
static int l_buzzer_force_stop(lua_State *L)
{
    buzzer_force_stop_c();
    return 0;
}

// Lua: buzzer.on(frequency) - optimized for real-time sensor data
static int l_buzzer_on(lua_State *L)
{
    // Default frequency is 1000Hz if no parameter provided
    int frequency = 1000;
    
    // Check if frequency parameter was provided
    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1))
    {
        frequency = lua_tointeger(L, 1);
        
        // Validate frequency range for real-time use
        if (frequency <= 0 || frequency > 20000)
        {
            return luaL_error(L, "Invalid frequency: %d (must be 1-20000 Hz)", frequency);
        }
    }

    // Call C function directly for maximum speed
    buzzer_on_c(frequency);
    
    return 0;
}

// Lua: buzzer.off() - emergency stop everything
static int l_buzzer_off(lua_State *L)
{
    // Call C function directly for immediate response
    buzzer_off_c();
    
    return 0;
}

// Lua: buzzer.success(options)
static int l_buzzer_success(lua_State *L)
{
    // Default success tone
    Tone tone = {
        .frequency = 2700, // Higher frequency for success
        .duration = 100,   // Short beeps
        .pause = 25,       // Short pause
        .repetitions = 2   // Two beeps
    };

    bool blocking = false; // Default to non-blocking mode

    // Parse options table if provided
    if (lua_istable(L, 1))
    {
        lua_getfield(L, 1, "freq");
        if (lua_isnumber(L, -1))
        {
            tone.frequency = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "play_duration");
        if (lua_isnumber(L, -1))
        {
            tone.duration = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "pause_duration");
        if (lua_isnumber(L, -1))
        {
            tone.pause = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "times");
        if (lua_isnumber(L, -1))
        {
            tone.repetitions = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "blocking");
        if (lua_isboolean(L, -1))
        {
            blocking = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "on_step");
        if (lua_isfunction(L, -1))
        {
            if (callback_ref_step != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_step);
            }
            callback_ref_step = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }

        lua_getfield(L, 1, "on_done");
        if (lua_isfunction(L, -1))
        {
            if (callback_ref_done != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_done);
            }
            callback_ref_done = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }
    }

    if (!buzzer_play_sequence_c(&tone, blocking))
    {
        return luaL_error(L, "Failed to play success tone");
    }

    return 0;
}

// Lua: buzzer.error(options)
static int l_buzzer_error(lua_State *L)
{
    // Default error tone
    Tone tone = {
        .frequency = 200, // Low frequency for error
        .duration = 1000, // Long beep
        .pause = 0,       // No pause
        .repetitions = 1  // One beep
    };

    bool blocking = false; // Default to non-blocking mode

    // Parse options table if provided
    if (lua_istable(L, 1))
    {
        lua_getfield(L, 1, "freq");
        if (lua_isnumber(L, -1))
        {
            tone.frequency = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "play_duration");
        if (lua_isnumber(L, -1))
        {
            tone.duration = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "times");
        if (lua_isnumber(L, -1))
        {
            tone.repetitions = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "blocking");
        if (lua_isboolean(L, -1))
        {
            blocking = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "on_step");
        if (lua_isfunction(L, -1))
        {
            if (callback_ref_step != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_step);
            }
            callback_ref_step = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }

        lua_getfield(L, 1, "on_done");
        if (lua_isfunction(L, -1))
        {
            if (callback_ref_done != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_done);
            }
            callback_ref_done = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            lua_pop(L, 1);
        }
    }

    if (!buzzer_play_sequence_c(&tone, blocking))
    {
        return luaL_error(L, "Failed to play error tone");
    }

    return 0;
}

// Lua: buzzer.play_music(music_str, blocking)
static int l_buzzer_play_music(lua_State *L)
{
    const char *music_str = luaL_checkstring(L, 1);
    bool blocking = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : false;

    if (!buzzer_play_music_c(music_str, blocking))
    {
        return luaL_error(L, "Failed to play music");
    }

    return 0;
}

// Lua: buzzer.is_playing()
static int l_buzzer_is_playing(lua_State *L)
{
    lua_pushboolean(L, is_playing);
    return 1;
}

// Lua: buzzer.set_callback("step"|"done", function)
static int l_buzzer_set_callback(lua_State *L)
{
    const char *type = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (strcmp(type, "step") == 0)
    {
        // Replace old callback if exists
        if (callback_ref_step != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_step);
        }

        // Store callback reference
        lua_pushvalue(L, 2); // Copy the function to the top
        callback_ref_step = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else if (strcmp(type, "done") == 0)
    {
        // Replace old callback if exists
        if (callback_ref_done != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, callback_ref_done);
        }

        // Store callback reference
        lua_pushvalue(L, 2); // Copy the function to the top
        callback_ref_done = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        return luaL_error(L, "Invalid callback type: %s", type);
    }

    return 0;
}

// Module registration function
int luaopen_buzzer32(lua_State *L)
{
    // Create buzzer table directly in the global environment
    l_buzzer_init(L);

    // Set the returned table in the global environment as "buzzer"
    lua_setglobal(L, "buzzer");

    // Return nothing (we've already set it as a global)
    return 0;
}
