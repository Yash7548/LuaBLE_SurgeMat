#include "luat_base.h"
#include "luat_wdt.h"

#include "esp_task_wdt.h"

#include "luat_log.h"
#define LUAT_LOG_TAG "wdt"

static uint32_t task_added = 0;

int luat_wdt_init(size_t timeout){
    if (task_added != 0)
        return 0;
    luat_wdt_set_timeout(timeout);
    esp_task_wdt_add(NULL);
    task_added = 1;
    return 0;
}

int luat_wdt_feed(void){
    return esp_task_wdt_reset();
}

int luat_wdt_set_timeout(size_t timeout) {
    // Arduino ESP32 still provides access to the ESP-IDF task watchdog
    // However, the API might be slightly different depending on your Arduino ESP32 core version
    
    #if ESP_ARDUINO_VERSION_MAJOR >= 2
    // For newer Arduino ESP32 core versions (2.x+)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = timeout,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    return esp_task_wdt_reconfigure(&twdt_config);
    #else
    // For older Arduino ESP32 core versions
    // Older versions might not have esp_task_wdt_reconfigure
    // You might need to deinitialize and reinitialize the watchdog
    esp_task_wdt_deinit();
    esp_task_wdt_config_t twdt_cfg = {
        .timeout_ms     = timeout,                // e.g. 5000 ms
        .idle_core_mask = BIT(0) | BIT(1),           // monitor idle tasks on both cores
        .trigger_panic  = true                       // crash & backtrace on timeout
    };

    esp_task_wdt_init(&twdt_cfg);   // new signature
    // esp_task_wdt_init(timeout / 1000.0, true); // Convert ms to seconds
    return 0;
    #endif
}
int luat_wdt_close(void){
    esp_task_wdt_delete(NULL);
    return esp_task_wdt_deinit();
}