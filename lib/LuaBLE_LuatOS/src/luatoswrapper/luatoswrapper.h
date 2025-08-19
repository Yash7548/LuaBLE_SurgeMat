#ifndef LUATOSWRAPPER_H
#define LUATOSWRAPPER_H

#include <Arduino.h>
#include <functional>
#include <string>
#include <bootloader_random.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
/* #include "loadlib.h" */   /* keep commented if not needed */
#include "luat_log.h"
#include "luat_base.h"
#include "luat_timer.h"
#include "luat_uart.h"
#include "luat_malloc.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "luat_rtos.h"
#include "luat_fs.h"
#include <time.h>
#include <sys/time.h>

#ifdef __cplusplus
}   /* extern "C" */
#endif

// ────────────────────────────────────────
// Execution states
enum ExecutionState {
    EXEC_STATE_UNINITIALIZED,
    EXEC_STATE_READY,
    EXEC_STATE_RUNNING,
    EXEC_STATE_STOPPING,
    EXEC_STATE_ERROR
};

enum StopMode {
    STOP_CLEAN,      // Destroy state after stop
    STOP_PERSISTENT  // Keep state after stop
};

// Callbacks
typedef void (*OutputCallback)(const char* output);
typedef void (*ErrorCallback)(const char* error);
typedef void (*RegisterCallback)(lua_State* L);

// Initialization
bool lua_wrapper_init(size_t stackSize = 16384, int priority = 1);
void lua_wrapper_destroy();

// Execution
bool lua_wrapper_exec_string(const char* code, bool persistent = false, bool autoRestart = false);
bool lua_wrapper_exec_file(const char* path, bool persistent = false, bool autoRestart = false);
bool lua_wrapper_exec_module(const char* module, bool persistent = false, bool autoRestart = false);
bool lua_wrapper_exec_string_bg(const char* code, bool persistent = false);

// Control
bool lua_wrapper_stop(const char* stopScript = nullptr, StopMode mode = STOP_CLEAN);
bool lua_wrapper_is_running();
ExecutionState lua_wrapper_get_state();
const char* lua_wrapper_get_error();

// Configuration
void lua_wrapper_set_output_cb(OutputCallback cb);
void lua_wrapper_set_error_cb(ErrorCallback cb);
void lua_wrapper_set_register_cb(RegisterCallback cb);
void lua_wrapper_set_main_module(const char* name);
void lua_wrapper_set_stop_module(const char* name);
bool create_lua_state();
void destroy_lua_state();
// Utility
void lua_wrapper_print_memory_usage();

#endif  /* LUATOSWRAPPER_H */
