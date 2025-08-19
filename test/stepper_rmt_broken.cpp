#include "stepper_rmt.h"

// Define __containerof macro if not available
#ifndef __containerof
#define __containerof(ptr, type, member) \
    ((type *) ((char *) (ptr) - offsetof(type, member)))
#include <stddef.h>  // for offsetof
#endif

// Motor configurations - initialized with pin definitions from header
MotorConfig motorConfigs[NUM_MOTORS] = {
    // Motor X
    {
        .motor_id = MOTOR_X,
        .step_gpio = MOTOR_X_STEP_PIN,
        .dir_gpio = MOTOR_X_DIR_PIN,
        .counter_gpio = MOTOR_X_COUNTER_PIN,
        .estop_gpio = MOTOR_X_ESTOP_PIN,
        .enable_gpio = MOTOR_X_ENABLE_PIN},
    // Motor Y
    {
        .motor_id = MOTOR_Y,
        .step_gpio = MOTOR_Y_STEP_PIN,
        .dir_gpio = MOTOR_Y_DIR_PIN,
        .counter_gpio = MOTOR_Y_COUNTER_PIN,
        .estop_gpio = MOTOR_Y_ESTOP_PIN,
        .enable_gpio = MOTOR_Y_ENABLE_PIN}};

// Motor states
MotorState motorStates[NUM_MOTORS];

// IDF5 Custom Encoder Implementation for Command Queue
static size_t motorStepEncoderCallback(rmt_encoder_t *encoder, rmt_channel_handle_t channel, 
                                      const void *primary_data, size_t data_size, 
                                      rmt_encode_state_t *ret_state)
{
    stepper_command_encoder_t *step_encoder = __containerof(encoder, stepper_command_encoder_t, base);
    uint8_t motor = step_encoder->motor_id;
    
    if (motor >= NUM_MOTORS) {
        *ret_state = RMT_ENCODING_COMPLETE;
        return 0;
    }
    
    MotorState *state = &motorStates[motor];
    
    // Debug output (remove IRAM_ATTR to allow Serial.printf)
    Serial.printf("Encoder called for motor %d: running=%d, queueCount=%d, cmdIndex=%d, stepsRemaining=%d\n", 
                  motor, state->isRunning, state->queueCount, state->currentCommandIndex, state->stepsRemaining);
    
    // If not running or no commands, complete encoding
    if (!state->isRunning || state->queueCount == 0 || state->currentCommandIndex >= state->queueCount) {
        Serial.printf("Motor %d encoder: completing - not running or no commands\n", motor);
        *ret_state = RMT_ENCODING_COMPLETE;
        return 0;
    }
    
    // If no steps remaining in current command, complete encoding
    if (state->stepsRemaining == 0) {
        Serial.printf("Motor %d encoder: completing - no steps remaining\n", motor);
        *ret_state = RMT_ENCODING_COMPLETE;
        return 0;
    }
    
    // Fill the buffer similar to old motorFillNext logic
    size_t symbols_encoded = 0;
    rmt_symbol_word_t symbols[PULSES_PER_FILL];
    
    // Generate symbols based on current command queue state
    while (symbols_encoded < PULSES_PER_FILL && state->stepsRemaining > 0) {
        // Create a step pulse similar to old implementation
        uint32_t delay_ticks = state->commandQueue[state->currentCommandIndex].delay_us;
        
        // Ensure minimum delay and maximum duration limits
        if (delay_ticks < 2) delay_ticks = 2;        // Minimum 2us total
        if (delay_ticks > 32767) delay_ticks = 32767; // Maximum RMT duration
        
        symbols[symbols_encoded].level0 = 1;           // High pulse
        symbols[symbols_encoded].duration0 = 1;        // 1us high
        symbols[symbols_encoded].level1 = 0;           // Low pulse  
        symbols[symbols_encoded].duration1 = delay_ticks - 1; // Remaining time low
        
        symbols_encoded++;
        
        // Decrement steps remaining
        uint32_t steps = state->stepsRemaining;
        state->stepsRemaining = steps - 1;
        
        // Update position with current direction
        portENTER_CRITICAL_ISR(&state->rmt_spinlock);
        if (state->currentDirection == DIR_CW) {
            int32_t pos = state->currentPosition;
            state->currentPosition = pos - 1;
        } else {
            int32_t pos = state->currentPosition;
            state->currentPosition = pos + 1;
        }
        portEXIT_CRITICAL_ISR(&state->rmt_spinlock);
        
        // If current command is complete, move to next
        if (state->stepsRemaining == 0) {
            if (state->currentCommandIndex + 1 < state->queueCount) {
                // Check for direction change
                if (state->commandQueue[state->currentCommandIndex + 1].direction != state->currentDirection) {
                    // Direction change needed - stop encoding here
                    state->directionChangePending = true;
                    state->nextDirection = state->commandQueue[state->currentCommandIndex + 1].direction;
                    state->nextCommandIndex = state->currentCommandIndex + 1;
                    break;
                } else {
                    // No direction change, continue to next command
                    int idx = state->currentCommandIndex;
                    state->currentCommandIndex = idx + 1;
                    state->stepsRemaining = state->commandQueue[state->currentCommandIndex].steps;
                }
            } else {
                // All commands completed
                break;
            }
        }
    }
    
    if (symbols_encoded > 0) {
        // Use copy encoder to encode the symbols
        rmt_encode_state_t session_state = RMT_ENCODING_RESET;
        size_t encoded_symbols = step_encoder->copy_encoder->encode(step_encoder->copy_encoder, channel, 
                                                                   symbols, symbols_encoded * sizeof(rmt_symbol_word_t), 
                                                                   &session_state);
        *ret_state = session_state;
        
        // If we still have steps remaining, indicate more encoding needed
        if (state->stepsRemaining > 0 && session_state == RMT_ENCODING_COMPLETE) {
            *ret_state = RMT_ENCODING_MEM_FULL;  // Request more buffer space
        }
        
        Serial.printf("Motor %d encoder: generated %d symbols, session_state=%d\n", motor, encoded_symbols, session_state);
        return encoded_symbols;
    }
    
    Serial.printf("Motor %d encoder: no symbols generated, completing\n", motor);
    *ret_state = RMT_ENCODING_COMPLETE;
    return 0;
}

static esp_err_t motorStepEncoderReset(rmt_encoder_t *encoder)
{
    stepper_command_encoder_t *step_encoder = __containerof(encoder, stepper_command_encoder_t, base);
    rmt_encoder_reset(step_encoder->copy_encoder);
    return ESP_OK;
}

static esp_err_t motorStepEncoderDelete(rmt_encoder_t *encoder)
{
    stepper_command_encoder_t *step_encoder = __containerof(encoder, stepper_command_encoder_t, base);
    rmt_del_encoder(step_encoder->copy_encoder);
    free(step_encoder);
    return ESP_OK;
}

esp_err_t createMotorStepEncoder(uint8_t motor)
{
    if (motor >= NUM_MOTORS) return ESP_ERR_INVALID_ARG;
    
    MotorState *state = &motorStates[motor];
    
    stepper_command_encoder_t *step_encoder = (stepper_command_encoder_t*)calloc(1, sizeof(stepper_command_encoder_t));
    if (!step_encoder) {
        return ESP_ERR_NO_MEM;
    }
    
    // Create copy encoder
    rmt_copy_encoder_config_t copy_encoder_config = {};
    esp_err_t ret = rmt_new_copy_encoder(&copy_encoder_config, &step_encoder->copy_encoder);
    if (ret != ESP_OK) {
        free(step_encoder);
        return ret;
    }
    
    // Initialize encoder
    step_encoder->base.encode = motorStepEncoderCallback;
    step_encoder->base.del = motorStepEncoderDelete;
    step_encoder->base.reset = motorStepEncoderReset;
    step_encoder->resolution = RMT_RESOLUTION_HZ;
    step_encoder->motor_id = motor;
    
    state->step_encoder = &step_encoder->base;
    return ESP_OK;
}

void deleteMotorStepEncoder(uint8_t motor)
{
    if (motor >= NUM_MOTORS) return;
    
    MotorState *state = &motorStates[motor];
    if (state->step_encoder) {
        rmt_del_encoder(state->step_encoder);
        state->step_encoder = NULL;
    }
}

// IDF5 TX Done Callback - replaces interrupt handling
bool IRAM_ATTR motorRmtTxDoneCallback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_ctx)
{
    uint8_t motor = (uint8_t)(uintptr_t)user_ctx;
    if (motor >= NUM_MOTORS) return false;
    
    MotorState *state = &motorStates[motor];
    
    portENTER_CRITICAL_ISR(&state->rmt_spinlock);
    
    if (state->directionChangePending) {
        // Handle direction change
        motorSetDirection(motor, state->nextDirection);
        state->directionChangePending = false;
        
        // Move to the specific command that needs direction change
        state->currentCommandIndex = state->nextCommandIndex;
        state->nextCommandIndex = -1;
        
        // Check if we have more commands to execute
        if (state->currentCommandIndex < state->queueCount) {
            state->stepsRemaining = state->commandQueue[state->currentCommandIndex].steps;
            
            // Continue transmission will happen when next transmit is called
            portEXIT_CRITICAL_ISR(&state->rmt_spinlock);
            
            // Give semaphore to continue execution
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(state->rmt_sem, &xHigherPriorityTaskWoken);
            return xHigherPriorityTaskWoken == pdTRUE;
        } else {
            // No more commands, end execution
            state->isRunning = false;
        }
    } else {
        // Normal end of transmission
        if (state->currentCommandIndex + 1 < state->queueCount) {
            // More commands available - continue in main execution loop
            // This will be handled by startMotorExecution loop
        } else {
            // All commands completed
            state->isRunning = false;
        }
    }
    
    portEXIT_CRITICAL_ISR(&state->rmt_spinlock);
    
    // Signal main task that transmission is complete
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(state->rmt_sem, &xHigherPriorityTaskWoken);
    return xHigherPriorityTaskWoken == pdTRUE;
}

void initMotors()
{
    Serial.println("Initializing motors with IDF5 RMT...");

    // Initialize both motors
    for (uint8_t motor = 0; motor < NUM_MOTORS; motor++)
    {
        initMotor(motor);
        Serial.printf("Motor %s initialized with IDF5\n", getMotorName(motor));
    }

    Serial.println("All motors initialized successfully with IDF5");
}

void initMotor(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    Serial.printf("Initializing %s motor with IDF5...\n", getMotorName(motor));

    // Initialize motor state - same as before
    MotorState *state = &motorStates[motor];

    // Initialize spinlocks
    state->estop_spinlock = portMUX_INITIALIZER_UNLOCKED;
    state->realtime_pos_spinlock = portMUX_INITIALIZER_UNLOCKED;
    state->rmt_spinlock = portMUX_INITIALIZER_UNLOCKED;

    // Initialize queue
    state->queueHead = 0;
    state->queueTail = 0;
    state->queueCount = 0;

    // Initialize execution state
    state->isRunning = false;
    state->currentPosition = 0;
    state->currentSteps = 0;
    state->stepsRemaining = 0;
    state->currentCommandIndex = -1;
    state->currentDirection = DIR_CW;
    state->directionChangePending = false;
    state->nextDirection = DIR_CW;
    state->nextCommandIndex = -1;
    state->transmissionComplete = false;

    // Initialize emergency stop
    state->emergencyStopEnabled = false;
    state->emergencyStopTriggered = false;
    state->needsReinitAfterEstop = false;

    // Initialize enable state
    state->motorEnabled = false;

    // Initialize real-time position
    state->realtimePosition = 0;

    // Initialize IDF5 RMT variables
    state->rmt_channel = NULL;
    state->step_encoder = NULL;
    state->rmt_mem_ptr = NULL;
    state->whichHalf = 0;

    // Create semaphore
    state->rmt_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(state->rmt_sem);

    // Initialize direction pin
    pinMode(motorConfigs[motor].dir_gpio, OUTPUT);
    digitalWrite(motorConfigs[motor].dir_gpio, DIR_CW);

    // Initialize enable pin (active low - disabled by default)
    pinMode(motorConfigs[motor].enable_gpio, OUTPUT);
    digitalWrite(motorConfigs[motor].enable_gpio, HIGH); // HIGH = disabled

    // Initialize motor components with IDF5
    initMotorRMT(motor);
    initMotorRealtimeCounter(motor);

    // Handle emergency stop reinit if needed
    handleMotorEmergencyStopReinit(motor);

    Serial.printf("%s motor initialized with IDF5\n", getMotorName(motor));
}

void initMotorRMT(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorConfig *config = &motorConfigs[motor];
    MotorState *state = &motorStates[motor];

    Serial.printf("%s motor configuring IDF5 RMT channel...\n", getMotorName(motor));

    // Create IDF5 TX Channel
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = config->step_gpio,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = RMT_MEM_BLOCK_SIZE,
        .trans_queue_depth = 4,
        .intr_priority = 0,
        .flags = {
            .invert_out = false,
            .with_dma = false,
            .io_loop_back = false,
            .io_od_mode = false
        }
    };

    esp_err_t err = rmt_new_tx_channel(&tx_chan_config, &state->rmt_channel);
    if (err != ESP_OK) {
        Serial.printf("%s motor IDF5 RMT channel creation failed: %s\n", getMotorName(motor), esp_err_to_name(err));
        return;
    }
    Serial.printf("%s motor IDF5 RMT channel created successfully\n", getMotorName(motor));

    // Create custom encoder for command queue
    err = createMotorStepEncoder(motor);
    if (err != ESP_OK) {
        Serial.printf("%s motor encoder creation failed: %s\n", getMotorName(motor), esp_err_to_name(err));
        rmt_del_channel(state->rmt_channel);
        state->rmt_channel = NULL;
        return;
    }

    // Register TX done callback
    rmt_tx_event_callbacks_t cbs = {
        .on_trans_done = motorRmtTxDoneCallback,
    };
    
    err = rmt_tx_register_event_callbacks(state->rmt_channel, &cbs, (void*)(uintptr_t)motor);
    if (err != ESP_OK) {
        Serial.printf("%s motor callback registration failed: %s\n", getMotorName(motor), esp_err_to_name(err));
        deleteMotorStepEncoder(motor);
        rmt_del_channel(state->rmt_channel);
        state->rmt_channel = NULL;
        return;
    }

    // Enable the channel
    err = rmt_enable(state->rmt_channel);
    if (err != ESP_OK) {
        Serial.printf("%s motor RMT enable failed: %s\n", getMotorName(motor), esp_err_to_name(err));
        deleteMotorStepEncoder(motor);
        rmt_del_channel(state->rmt_channel);
        state->rmt_channel = NULL;
        return;
    }

    Serial.printf("%s motor IDF5 RMT initialized - GPIO: %d\n", getMotorName(motor), config->step_gpio);
}

void IRAM_ATTR motorSetDirection(uint8_t motor, uint8_t direction)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];
    MotorConfig *config = &motorConfigs[motor];

    state->currentDirection = direction;
    digitalWrite(config->dir_gpio, direction);
    
    // Add direction setup delay
    delayMicroseconds(DIR_SETUP_TIME_US);
}

void startMotorExecution(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];
    MotorConfig *config = &motorConfigs[motor];

    if (state->isRunning || state->queueCount == 0 || !state->motorEnabled)
    {
        if (state->isRunning)
        {
            Serial.printf("%s motor already running!\n", getMotorName(motor));
        }
        else if (state->queueCount == 0)
        {
            Serial.printf("%s motor queue is empty!\n", getMotorName(motor));
        }
        else if (!state->motorEnabled)
        {
            Serial.printf("%s motor is disabled! Use enable command first.\n", getMotorName(motor));
        }
        return;
    }

    // Take semaphore
    xSemaphoreTake(state->rmt_sem, portMAX_DELAY);

    // Initialize execution state
    state->currentCommandIndex = 0;
    state->stepsRemaining = state->commandQueue[0].steps;

    // Set direction for first command
    uint8_t newDirection = state->commandQueue[0].direction;
    state->currentDirection = newDirection;
    motorSetDirection(motor, state->currentDirection);

    Serial.printf("%s motor direction set to: %s\n",
                  getMotorName(motor), state->currentDirection == DIR_CW ? "CW" : "CCW");

    state->isRunning = true;
    state->whichHalf = 0;
    state->directionChangePending = false;
    state->nextCommandIndex = -1;
    state->transmissionComplete = false;

    // Start IDF5 transmission with custom encoder
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,  // Single transmission, encoder handles the steps
    };

    // Create dummy data for the encoder (encoder will generate actual RMT symbols)
    uint32_t dummy_data = 1;  // Just to pass valid data pointer

    Serial.printf("%s motor starting RMT transmission...\n", getMotorName(motor));

    // Transmission loop - handles command queue execution
    while (state->isRunning && state->currentCommandIndex < state->queueCount) {
        // Start transmission with dummy data (encoder will generate real symbols)
        esp_err_t err = rmt_transmit(state->rmt_channel, state->step_encoder, &dummy_data, sizeof(dummy_data), &tx_config);
        if (err != ESP_OK) {
            Serial.printf("%s motor transmission failed: %s\n", getMotorName(motor), esp_err_to_name(err));
            break;
        }

        Serial.printf("%s motor transmission started, waiting for completion...\n", getMotorName(motor));

        // Wait for transmission completion
        xSemaphoreTake(state->rmt_sem, portMAX_DELAY);
        
        Serial.printf("%s motor transmission completed, checking for more commands...\n", getMotorName(motor));
        
        // Handle direction changes or continue to next commands
        if (state->directionChangePending) {
            // Direction change was handled in callback, continue
            Serial.printf("%s motor direction change pending, continuing...\n", getMotorName(motor));
            continue;
        }
        
        // Check if we need to continue with more commands
        if (state->currentCommandIndex + 1 < state->queueCount && state->stepsRemaining == 0) {
            state->currentCommandIndex++;
            state->stepsRemaining = state->commandQueue[state->currentCommandIndex].steps;
            
            Serial.printf("%s motor moving to command %d, steps: %d\n", getMotorName(motor), state->currentCommandIndex, state->stepsRemaining);
            
            // Check for direction change
            if (state->commandQueue[state->currentCommandIndex].direction != state->currentDirection) {
                motorSetDirection(motor, state->commandQueue[state->currentCommandIndex].direction);
            }
        } else if (state->stepsRemaining == 0) {
            // All commands completed
            Serial.printf("%s motor all commands completed\n", getMotorName(motor));
            break;
        }
    }

    state->isRunning = false;
    xSemaphoreGive(state->rmt_sem);

    Serial.printf("%s motor execution completed\n", getMotorName(motor));
}

void stopMotorExecution(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];

    if (!state->isRunning)
    {
        Serial.printf("%s motor not running!\n", getMotorName(motor));
        return;
    }

    // Stop transmission
    if (state->rmt_channel) {
        rmt_disable(state->rmt_channel);
        rmt_enable(state->rmt_channel);  // Re-enable for next use
    }

    state->isRunning = false;
    xSemaphoreGive(state->rmt_sem);

    Serial.printf("%s motor execution stopped\n", getMotorName(motor));
}

// Keep all other existing functions unchanged...
// (addMotorCommand, clearMotorQueue, printMotorStatus, etc.)

void addMotorCommand(uint8_t motor, uint32_t delay_us, uint32_t steps, uint8_t direction)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];

    if (state->queueCount >= MAX_COMMANDS)
    {
        Serial.printf("%s motor queue full!\n", getMotorName(motor));
        return;
    }

    state->commandQueue[state->queueTail].delay_us = delay_us;
    state->commandQueue[state->queueTail].steps = steps;
    state->commandQueue[state->queueTail].direction = direction;
    int tail = state->queueTail;
    state->queueTail = (tail + 1) % MAX_COMMANDS;
    int count = state->queueCount;
    state->queueCount = count + 1;
}

void clearMotorQueue(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];

    state->queueHead = 0;
    state->queueTail = 0;
    state->queueCount = 0;
    state->currentCommandIndex = -1;
    Serial.printf("%s motor queue cleared\n", getMotorName(motor));
}

void printMotorStatus(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];

    Serial.printf("\n--- %s Motor Status (IDF5) ---\n", getMotorName(motor));
    Serial.printf("Enabled: %s\n", state->motorEnabled ? "Yes" : "No");
    Serial.printf("Running: %s\n", state->isRunning ? "Yes" : "No");
    Serial.printf("Position: %d\n", state->currentPosition);
    Serial.printf("Real-time position: %d\n", state->realtimePosition);
    Serial.printf("Position difference: %d\n", state->currentPosition - state->realtimePosition);
    Serial.printf("Current direction: %s\n", state->currentDirection == DIR_CW ? "CW" : "CCW");
    Serial.printf("Queue count: %d\n", state->queueCount);
    Serial.printf("Emergency stop enabled: %s\n", state->emergencyStopEnabled ? "Yes" : "No");
    Serial.printf("Emergency stop triggered: %s\n", state->emergencyStopTriggered ? "Yes" : "No");

    if (state->queueCount > 0)
    {
        Serial.printf("Queued commands:\n");
        for (int i = 0; i < state->queueCount; i++)
        {
            int idx = (state->queueHead + i) % MAX_COMMANDS;
            Serial.printf("  [%d] delay=%dus, steps=%d, direction=%s\n",
                          i, state->commandQueue[idx].delay_us, state->commandQueue[idx].steps,
                          state->commandQueue[idx].direction == DIR_CW ? "CW" : "CCW");
        }
    }

    if (state->isRunning && state->currentCommandIndex >= 0)
    {
        Serial.printf("Currently executing command %d\n", state->currentCommandIndex);
        Serial.printf("Steps remaining: %d\n", state->stepsRemaining);
    }
}

// Keep all the existing helper and utility functions unchanged...
void initMotorEmergencyStop(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorConfig *config = &motorConfigs[motor];

    pinMode(config->estop_gpio, INPUT);
    if (motor == MOTOR_X)
    {
        attachInterrupt(digitalPinToInterrupt(config->estop_gpio), []()
                        {
                            motorEmergencyStopISR(MOTOR_X); }, RISING);
    }

    if (motor == MOTOR_Y)
    {
        attachInterrupt(digitalPinToInterrupt(config->estop_gpio), []()
                        {
                            motorEmergencyStopISR(MOTOR_Y); }, RISING);
    }
}

void deinitMotorEmergencyStop(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;
    MotorConfig *config = &motorConfigs[motor];
    detachInterrupt(digitalPinToInterrupt(config->estop_gpio));
}

void IRAM_ATTR motorEmergencyStopISR(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];

    portENTER_CRITICAL_ISR(&state->estop_spinlock);

    if (state->emergencyStopEnabled)
    {
        state->emergencyStopTriggered = true;
        state->needsReinitAfterEstop = true;
        state->emergencyStopEnabled = false;

        // Stop RMT transmission immediately
        if (state->isRunning && state->rmt_channel)
        {
            rmt_disable(state->rmt_channel);
            state->isRunning = false;

            // Signal main task
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(state->rmt_sem, &xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken == pdTRUE)
            {
                portYIELD_FROM_ISR();
            }
        }
    }

    portEXIT_CRITICAL_ISR(&state->estop_spinlock);
}

void enableMotorEmergencyStop(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;
    initMotorEmergencyStop(motor);
    MotorState *state = &motorStates[motor];
    portENTER_CRITICAL(&state->estop_spinlock);
    state->emergencyStopEnabled = true;
    portEXIT_CRITICAL(&state->estop_spinlock);
    Serial.printf("%s motor emergency stop enabled\n", getMotorName(motor));
}

void disableMotorEmergencyStop(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];
    deinitMotorEmergencyStop(motor);
    portENTER_CRITICAL(&state->estop_spinlock);
    state->emergencyStopEnabled = false;
    portEXIT_CRITICAL(&state->estop_spinlock);
    Serial.printf("%s motor emergency stop disabled\n", getMotorName(motor));
}

void handleMotorEmergencyStopReinit(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];
    MotorConfig *config = &motorConfigs[motor];

    if (state->needsReinitAfterEstop)
    {
        Serial.printf("%s motor emergency stop was triggered - reinitializing IDF5...\n", getMotorName(motor));

        // Stop any ongoing operations
        if (state->isRunning)
        {
            stopMotorExecution(motor);
        }

        // Clear the queue
        clearMotorQueue(motor);

        // Reinitialize IDF5 RMT
        if (state->rmt_channel) {
            rmt_disable(state->rmt_channel);
            deleteMotorStepEncoder(motor);
            rmt_del_channel(state->rmt_channel);
            state->rmt_channel = NULL;
        }
        initMotorRMT(motor);

        // Restore direction pin state
        pinMode(config->dir_gpio, OUTPUT);
        digitalWrite(config->dir_gpio, state->currentDirection);
        Serial.printf("%s motor direction pin restored to: %s\n",
                      getMotorName(motor), state->currentDirection == DIR_CW ? "CW" : "CCW");

        // Reset flags
        portENTER_CRITICAL(&state->estop_spinlock);
        state->emergencyStopTriggered = false;
        state->needsReinitAfterEstop = false;
        portEXIT_CRITICAL(&state->estop_spinlock);

        Serial.printf("%s motor reinitialized after emergency stop with IDF5\n", getMotorName(motor));
    }
}

void initMotorRealtimeCounter(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorConfig *config = &motorConfigs[motor];
    MotorState *state = &motorStates[motor];

    // Install GPIO ISR service (only once)
    static bool isr_service_installed = false;
    if (!isr_service_installed)
    {
        gpio_install_isr_service(0);
        isr_service_installed = true;
    }

    gpio_set_intr_type(config->counter_gpio, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(config->counter_gpio, motorRealtimeStepISR, (void *)(uintptr_t)motor);

    // Initialize real-time position to 0
    portENTER_CRITICAL(&state->realtime_pos_spinlock);
    state->realtimePosition = 0;
    portEXIT_CRITICAL(&state->realtime_pos_spinlock);

    Serial.printf("%s motor real-time step counter initialized\n", getMotorName(motor));
    Serial.printf("Monitoring step pin GPIO_%d (Falling edge)\n", config->counter_gpio);
}

void IRAM_ATTR motorRealtimeStepISR(void *arg)
{
    uint8_t motor = (uint8_t)(uintptr_t)arg;
    if (motor >= NUM_MOTORS)
        return;

    MotorState *state = &motorStates[motor];
    MotorConfig *config = &motorConfigs[motor];

    // Read current direction pin state
    if (digitalRead(config->dir_gpio) == DIR_CW)
    {
        int32_t pos = state->realtimePosition;
        state->realtimePosition = pos - 1; // Clockwise decrements
    }
    else
    {
        int32_t pos = state->realtimePosition;
        state->realtimePosition = pos + 1; // Counter-clockwise increments
    }
}

// Keep all existing getter/setter functions unchanged
int32_t getMotorPosition(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return 0;
    return motorStates[motor].currentPosition;
}

int32_t getMotorRealtimePosition(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return 0;
    return motorStates[motor].realtimePosition;
}

void resetMotorPosition(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;
    if (motorStates[motor].isRunning)
    {
        Serial.printf("Cannot reset %s motor position while running!\n", getMotorName(motor));
        return;
    }
    motorStates[motor].currentPosition = 0;
    Serial.printf("%s motor position reset to 0\n", getMotorName(motor));
}

void resetMotorRealtimePosition(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;
    MotorState *state = &motorStates[motor];
    if (state->isRunning)
    {
        Serial.printf("Cannot reset %s motor real-time position while running!\n", getMotorName(motor));
        return;
    }
    portENTER_CRITICAL(&state->realtime_pos_spinlock);
    state->realtimePosition = 0;
    portEXIT_CRITICAL(&state->realtime_pos_spinlock);
    Serial.printf("%s motor real-time position reset to 0\n", getMotorName(motor));
}

bool isMotorRunning(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return false;
    return motorStates[motor].isRunning;
}

uint8_t getMotorDirection(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return DIR_CW;
    return motorStates[motor].currentDirection;
}

int getMotorQueueCount(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return 0;
    return motorStates[motor].queueCount;
}

// Helper functions
const char *getMotorName(uint8_t motor)
{
    switch (motor)
    {
    case MOTOR_X:
        return "X";
    case MOTOR_Y:
        return "Y";
    default:
        return "Unknown";
    }
}

uint8_t getMotorFromName(const char *name)
{
    if (strcmp(name, "X") == 0 || strcmp(name, "x") == 0)
        return MOTOR_X;
    if (strcmp(name, "Y") == 0 || strcmp(name, "y") == 0)
        return MOTOR_Y;
    return NUM_MOTORS; // Invalid motor
}

// Motor enable/disable functions
void enableMotor(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorConfig *config = &motorConfigs[motor];
    MotorState *state = &motorStates[motor];

    // Enable motor (active low - LOW = enabled)
    digitalWrite(config->enable_gpio, LOW);
    state->motorEnabled = true;

    Serial.printf("%s motor enabled\n", getMotorName(motor));
}

void disableMotor(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return;

    MotorConfig *config = &motorConfigs[motor];
    MotorState *state = &motorStates[motor];

    // Stop motor if running
    if (state->isRunning)
    {
        stopMotorExecution(motor);
    }

    // Disable motor (active low - HIGH = disabled)
    digitalWrite(config->enable_gpio, HIGH);
    state->motorEnabled = false;

    Serial.printf("%s motor disabled\n", getMotorName(motor));
}

bool isMotorEnabled(uint8_t motor)
{
    if (motor >= NUM_MOTORS)
        return false;
    return motorStates[motor].motorEnabled;
}

// Keep the motorFillNext function for compatibility (now used in encoder)
void IRAM_ATTR motorFillNext(uint8_t motor)
{
    // This function is now integrated into the encoder callback
    // Keeping for compatibility but actual logic is in motorStepEncoderCallback
}