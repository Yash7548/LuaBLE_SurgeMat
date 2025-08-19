#include "luatoswrapper.h"
#include "arduinobindings.h"

// External declarations from LuatOS
extern "C"
{
    extern void luat_heap_init(void);
    extern void luat_openlibs(lua_State *L);
    extern int luat_search_module(const char *name, char *filename);
    extern void stopboot(void);
    extern int luat_timer_stop_all(void);

#define LUAT_LOG_TAG "wrapper"
#include "luat_log.h"
}

// Static variables - no struct
static lua_State *L = nullptr;
static ExecutionState currentState = EXEC_STATE_UNINITIALIZED;
static volatile bool stopRequested = false;
static String stopScriptBuffer;
static String lastErrorMessage;

// Module names
static const char *mainModuleName = "main";
static const char *stopModuleName = "stop";
static bool autoRunStopModule = true;

// RTOS handles
static TaskHandle_t luaTaskHandle = nullptr;
static QueueHandle_t commandQueue = nullptr;
static SemaphoreHandle_t stateMutex = nullptr;

// Background task handle
static TaskHandle_t bgTaskHandle = nullptr;

// Callbacks
static OutputCallback outputCallback = nullptr;
static ErrorCallback errorCallback = nullptr;
static RegisterCallback registerCallback = nullptr;

// Command structure
struct Command
{
    enum Type
    {
        CMD_EXEC_STRING,
        CMD_EXEC_FILE,
        CMD_EXEC_MODULE,
        CMD_STOP,
        CMD_DESTROY
    } type;
    String data;
    bool persistent;
    bool autoRestart;
    StopMode stopMode;
};

// Forward declarations
static void lua_task(void *param);
static void lua_bg_task(void *param);
static void execution_hook(lua_State *L, lua_Debug *ar);
 bool create_lua_state();
 void destroy_lua_state();
static void set_execution_state(ExecutionState state);
static ExecutionState get_execution_state();
static bool execute_string_internal(const char *code, bool persistent, bool autoRestart);
static bool execute_file_internal(const char *path, bool persistent, bool autoRestart);
static bool execute_module_internal(const char *module, bool persistent, bool autoRestart);

// Thread-safe state management
static void set_execution_state(ExecutionState state)
{
    if (xSemaphoreTake(stateMutex, portMAX_DELAY) == pdTRUE)
    {
        currentState = state;
        xSemaphoreGive(stateMutex);
    }
}

static ExecutionState get_execution_state()
{
    ExecutionState state = EXEC_STATE_UNINITIALIZED;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        state = currentState;
        xSemaphoreGive(stateMutex);
    }
    return state;
}

// Hook for safe interruption
static void execution_hook(lua_State *L, lua_Debug *ar)
{
    if (stopRequested)
    {
        // Run stop script if provided
        if (!stopScriptBuffer.isEmpty())
        {
            luaL_dostring(L, stopScriptBuffer.c_str());
            stopScriptBuffer.clear();
        }
        else if (autoRunStopModule && stopModuleName)
        {
            // Try to run stop module
            char filename[64];
            if (luat_search_module(stopModuleName, filename) == 0)
            {
                luaL_dofile(L, filename);
            }
        }

        // Clear flag and raise error
        stopRequested = false;
        // Serial.println("*****************LUA SCRIPT STOP*****************");

        luaL_error(L, "Execution stopped");

        set_execution_state(EXEC_STATE_STOPPING);
    }
}

// Lua state management
 bool create_lua_state()
{
    L = lua_newstate(luat_heap_alloc, NULL);
    if (!L)
    {
        lastErrorMessage = "Failed to create Lua state";
        return false;
    }

    // Set panic handler
    lua_atpanic(L, [](lua_State *L) -> int
                {
        const char* msg = lua_tostring(L, -1);
        if (errorCallback) {
            errorCallback(msg);
        }
        return 0; });

    // Open libraries
    luat_openlibs(L);
    registerArduinoBindings(L);
    lua_gc(L, LUA_GCCOLLECT, 0);

    // Register print override
    // lua_register(L, "print", [](lua_State* L) -> int {
    //     if (outputCallback) {
    //         int n = lua_gettop(L);
    //         String output;
    //         for (int i = 1; i <= n; i++) {
    //             if (i > 1) output += "\t";
    //             output += luaL_tolstring(L, i, NULL);
    //             lua_pop(L, 1);
    //         }
    //         outputCallback(output.c_str());
    //     }
    //     return 0;
    // });

    // User registrations
    if (registerCallback)
    {
        registerCallback(L);
    }

    return true;
}

 void destroy_lua_state()
{
    if (L)
    {
     
        lua_close(L);
        L = nullptr;
    }
    luat_timer_stop_all();
}

// Internal execution functions
static bool execute_string_internal(const char *code, bool persistent, bool autoRestart)
{
    if (!L)
    {
        if (!create_lua_state())
        {
            return false;
        }
    }

    // Install hook for interruption

    int result = luaL_dostring(L, code);
   lua_wrapper_print_memory_usage();
        LLOGD("*****************LUA SCRIPT STOP*****************");
    // Remove hook

    if (result != LUA_OK)
    {
        lastErrorMessage = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (errorCallback)
        {
            errorCallback(lastErrorMessage.c_str());
        }

        // Auto restart if requested
        if (autoRestart && !stopRequested)
        {
            destroy_lua_state();
            create_lua_state();
            return execute_string_internal(code, persistent, false); // Only retry once CHANGE
        }

        return false;
    }

    // Clean up if not persistent
    // if (!persistent) {
    lua_gc(L, LUA_GCCOLLECT, 0);
    // }

    return true;
}

static bool execute_file_internal(const char *path, bool persistent, bool autoRestart)
{
    if (!L)
    {
        if (!create_lua_state())
        {
            return false;
        }
    }

    // Install hook for interruption
    // lua_sethook(L, execution_hook, LUA_MASKCOUNT, 1000);

    int result = luaL_dofile(L, path);
   lua_wrapper_print_memory_usage();
        LLOGD("*****************LUA SCRIPT STOP*****************");
    // Remove hook
    lua_sethook(L, nullptr, 0, 0);

    if (result != LUA_OK)
    {
        lastErrorMessage = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (errorCallback)
        {
            errorCallback(lastErrorMessage.c_str());
        }

        // Auto restart if requested
        if (autoRestart && !stopRequested)
        {
            destroy_lua_state();
            create_lua_state();
            return execute_file_internal(path, persistent, false); // Only retry once
        }

        return false;
    }

    // Clean up if not persistent
    // if (!persistent) {
    lua_gc(L, LUA_GCCOLLECT, 0);
    // }

    return true;
}

static bool execute_module_internal(const char *module, bool persistent, bool autoRestart)
{
    char filename[64];
    if (luat_search_module(module, filename) == 0)
    {
        return execute_file_internal(filename, persistent, autoRestart);
    }

    lastErrorMessage = "Module not found: ";
    lastErrorMessage += module;

    if (errorCallback)
    {
        errorCallback(lastErrorMessage.c_str());
    }

    return false;
}

// Task main loop
static void lua_task(void *param)
{
    Command cmd;

    // Initialize LuatOS system
    bootloader_random_enable();

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        r = nvs_flash_init();
    }

    luat_heap_init();
    esp_event_loop_create_default();
    luat_fs_init();
    stopboot();

    // Initialize Lua state
    create_lua_state();
    set_execution_state(EXEC_STATE_READY);

    while (true)
    {
        if (xQueueReceive(commandQueue, &cmd, portMAX_DELAY) == pdTRUE)
        {
            switch (cmd.type)
            {
            case Command::CMD_EXEC_STRING:
                set_execution_state(EXEC_STATE_RUNNING);
                execute_string_internal(cmd.data.c_str(), cmd.persistent, cmd.autoRestart);
                if (get_execution_state() == EXEC_STATE_RUNNING)
                {
                    set_execution_state(EXEC_STATE_READY);
                }
                break;

            case Command::CMD_EXEC_FILE:
                set_execution_state(EXEC_STATE_RUNNING);
                execute_file_internal(cmd.data.c_str(), cmd.persistent, cmd.autoRestart);
                if (get_execution_state() == EXEC_STATE_RUNNING)
                {
                    set_execution_state(EXEC_STATE_READY);
                }
                break;

            case Command::CMD_EXEC_MODULE:
                set_execution_state(EXEC_STATE_RUNNING);
                execute_module_internal(cmd.data.c_str(), cmd.persistent, cmd.autoRestart);
                if (get_execution_state() == EXEC_STATE_RUNNING)
                {
                    set_execution_state(EXEC_STATE_READY);
                }
                break;

            case Command::CMD_STOP:
                // Process stop based on mode
                if (cmd.stopMode == STOP_CLEAN)
                {
                    destroy_lua_state();
                    create_lua_state();
                    }
                    set_execution_state(EXEC_STATE_READY);
                
                break;

            case Command::CMD_DESTROY:
                destroy_lua_state();
                vTaskDelete(NULL);
                return;
            }
        }
    }
}

// Background task for async execution
static void lua_bg_task(void *param)
{
    String *code = (String *)param;

    // Create separate Lua state for background execution
    lua_State *bgL = lua_newstate(luat_heap_alloc, NULL);
    if (bgL)
    {
        luat_openlibs(bgL);
        registerArduinoBindings(bgL);

        if (registerCallback)
        {
            registerCallback(bgL);
        }

        // Execute the code
        int result = luaL_dostring(bgL, code->c_str());

        if (result != LUA_OK)
        {
            const char *error = lua_tostring(bgL, -1);
            if (errorCallback)
            {
                errorCallback(error);
            }
        }

        lua_close(bgL);
    }

    delete code;
    bgTaskHandle = nullptr;
    vTaskDelete(NULL);
}

// Public API implementations
bool lua_wrapper_init(size_t stackSize, int priority)
{
    // Create synchronization objects
    commandQueue = xQueueCreate(10, sizeof(Command));
    stateMutex = xSemaphoreCreateMutex();

    if (!commandQueue || !stateMutex)
    {
        return false;
    }

    // Create task
    // BaseType_t result = xTaskCreate(
    //     lua_task,
    //     "LuaTask",
    //     stackSize,
    //     nullptr,
    //     priority,
    //     &luaTaskHandle);

    BaseType_t result = xTaskCreatePinnedToCore(
        lua_task,
        "LuaTask",
        stackSize,
        nullptr,
        priority,
        &luaTaskHandle,
        1);

    if (result != pdPASS)
    {
        vQueueDelete(commandQueue);
        vSemaphoreDelete(stateMutex);
        return false;
    }

    while (lua_wrapper_get_state() != EXEC_STATE_READY)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return true;
}

void lua_wrapper_destroy()
{
    if (commandQueue && luaTaskHandle)
    {
        Command cmd = {
            .type = Command::CMD_DESTROY};
        xQueueSend(commandQueue, &cmd, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (commandQueue)
    {
        vQueueDelete(commandQueue);
        commandQueue = nullptr;
    }

    if (stateMutex)
    {
        vSemaphoreDelete(stateMutex);
        stateMutex = nullptr;
    }
}

bool lua_wrapper_exec_string(const char *code, bool persistent, bool autoRestart)
{
    if (!commandQueue || get_execution_state() == EXEC_STATE_UNINITIALIZED)
    {
        return false;
    }

    Command cmd = {
        .type = Command::CMD_EXEC_STRING,
        .data = code,
        .persistent = persistent,
        .autoRestart = autoRestart};

    return xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE;
}

bool lua_wrapper_exec_file(const char *path, bool persistent, bool autoRestart)
{
    if (!commandQueue || get_execution_state() == EXEC_STATE_UNINITIALIZED)
    {
        return false;
    }

    Command cmd = {
        .type = Command::CMD_EXEC_FILE,
        .data = path,
        .persistent = persistent,
        .autoRestart = autoRestart};

    return xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE;
}

bool lua_wrapper_exec_module(const char *module, bool persistent, bool autoRestart)
{
    if (!commandQueue || get_execution_state() == EXEC_STATE_UNINITIALIZED)
    {
        return false;
    }

    Command cmd = {
        .type = Command::CMD_EXEC_MODULE,
        .data = module,
        .persistent = persistent,
        .autoRestart = autoRestart};

    return xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE;
}

bool lua_wrapper_exec_string_bg(const char *code, bool persistent)
{
    if (bgTaskHandle != nullptr)
    {
        return false; // Background task already running
    }

    String *codeCopy = new String(code);

    BaseType_t result = xTaskCreatePinnedToCore(
        lua_bg_task,
        "LuaBgTask",
        8192,
        codeCopy,
        1,
        &bgTaskHandle,
        1);

    if (result != pdPASS)
    {
        delete codeCopy;
        return false;
    }

    return true;
}

bool lua_wrapper_stop(const char *stopScript, StopMode mode)
{
    ExecutionState state = get_execution_state();
    if (state != EXEC_STATE_RUNNING)
    {
        return false;
    }

    // Set stop request
    stopRequested = true;
    stopScriptBuffer = stopScript ? stopScript : "";

    // Queue cleanup command
    Command cmd = {
        .type = Command::CMD_STOP,
        .data = "",
        .stopMode = mode};
        
    lua_sethook(L, execution_hook, LUA_MASKCOUNT, 1);
    return xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE;
}

bool lua_wrapper_is_running()
{
    return get_execution_state() == EXEC_STATE_RUNNING;
}

ExecutionState lua_wrapper_get_state()
{
    return get_execution_state();
}

const char *lua_wrapper_get_error()
{
    return lastErrorMessage.c_str();
}

// Configuration setters
void lua_wrapper_set_output_cb(OutputCallback cb)
{
    outputCallback = cb;
}

void lua_wrapper_set_error_cb(ErrorCallback cb)
{
    errorCallback = cb;
}

void lua_wrapper_set_register_cb(RegisterCallback cb)
{
    registerCallback = cb;
}

void lua_wrapper_set_main_module(const char *name)
{
    mainModuleName = name;
}

void lua_wrapper_set_stop_module(const char *name)
{
    stopModuleName = name;
}

void lua_wrapper_print_memory_usage()
{
    luat_os_print_heapinfo("Wrpper Memory Usage");
}