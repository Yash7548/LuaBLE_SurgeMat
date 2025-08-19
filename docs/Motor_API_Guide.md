# Motor Control API Documentation

## Overview

This document provides comprehensive examples and usage patterns for the Motor Control API, including RMT stepper control, TMC driver configuration, buzzer feedback, and BLE communication.

## Table of Contents

1. [Quick Start](#quick-start)
2. [Motor Control API (xm, ym)](#motor-control-api)
3. [TMC Driver API (xd, yd)](#tmc-driver-api)
4. [Buzzer Integration](#buzzer-integration)
5. [BLE Communication](#ble-communication)
6. [Complete Examples](#complete-examples)
7. [Troubleshooting](#troubleshooting)

---

## Quick Start

```lua
-- Basic motor movement example
xm.enable()                        -- Enable X motor
xm.add_command(1000, 200, "cw")   -- Move 200 steps clockwise
xm.start()                         -- Begin execution

-- Check status
print("Running:", xm.is_running())
print("Position:", xm.get_position())
```

---

## Motor Control API

### Available Motors
- **xm** - X Motor (RMT stepper control)
- **ym** - Y Motor (RMT stepper control)

### Enable/Disable Control

```lua
-- Enable motors
xm.enable()
ym.enable()

-- Check if enabled
if xm.is_enabled() then
    print("X motor is ready")
end

-- Disable motors (stops and disables)
xm.disable()
ym.disable()
```

### Movement Commands

```lua
-- Add movement commands to queue
-- Syntax: add_command(delay_us, steps, direction)

xm.add_command(1000, 100, "cw")    -- 100 steps clockwise, 1ms between steps
xm.add_command(1500, 50, "ccw")    -- 50 steps counter-clockwise, 1.5ms between steps

-- Direction options:
xm.add_command(1000, 100, "cw")    -- String: clockwise
xm.add_command(1000, 100, "ccw")   -- String: counter-clockwise
xm.add_command(1000, 100, 0)       -- Number: 0 = clockwise
xm.add_command(1000, 100, 1)       -- Number: 1 = counter-clockwise
xm.add_command(1000, 100, DIR.CW)  -- Constant: clockwise
xm.add_command(1000, 100, DIR.CCW) -- Constant: counter-clockwise
```

### Execution Control

```lua
-- Start executing queued commands (non-blocking by default)
xm.start()                     -- Non-blocking: returns immediately
xm.start(false)               -- Non-blocking: returns immediately  
xm.start(true)                -- Blocking: waits until all commands complete

-- Check if running
while xm.is_running() do
    print("Position:", xm.get_position())
    print("Queue remaining:", xm.get_queue_count())
    delay(100)
end

-- Stop execution immediately
xm.stop()

-- Clear all queued commands
xm.clear_queue()
```

### Blocking vs Non-Blocking Execution

The motor system supports both blocking and non-blocking execution modes:

```lua
-- Non-blocking execution (default)
xm.add_command(1000, 200, "cw")
xm.start()                     -- Returns immediately
print("Motor started!")        -- This prints right away

-- Check status while running
while xm.is_running() do
    print("Position:", xm.get_position())
    delay(100)
end
print("Motor finished!")

-- Blocking execution
xm.add_command(1000, 200, "cw")
xm.start(true)                 -- Waits until all commands complete
print("Motor finished!")       -- This prints only after completion

-- Practical example: synchronized movement
xm.add_command(1000, 100, "cw")
ym.add_command(1000, 100, "cw")
xm.start(true)                 -- Start X motor and wait
ym.start(true)                 -- Start Y motor and wait
print("Both motors completed!")
```

**Use Cases:**
- **Non-blocking**: Parallel operations, status monitoring, responsive UI
- **Blocking**: Sequential operations, precise timing, simple scripts

### Position Tracking

```lua
-- Get current positions
local rmt_pos = xm.get_position()          -- RMT-tracked position
local real_pos = xm.get_realtime_position() -- Hardware-tracked position

print("RMT Position:", rmt_pos)
print("Real Position:", real_pos) 
print("Difference:", rmt_pos - real_pos)

-- Reset positions (motor must be stopped)
if not xm.is_running() then
    xm.reset_position()
    xm.reset_realtime_position()
end
```

### Status Information

```lua
-- Get comprehensive status
local status = xm.status()
print("Motor Status:")
print("  Enabled:", status.enabled)
print("  Running:", status.running)
print("  Position:", status.position)
print("  Real-time Position:", status.realtime_position)
print("  Direction:", status.direction)
print("  Queue Count:", status.queue_count)
print("  Motor Name:", status.name)
```

### Emergency Stop

```lua
-- Enable emergency stop monitoring
xm.enable_estop()
ym.enable_estop()

-- Disable emergency stop
xm.disable_estop()
ym.disable_estop()
```

---

## TMC Driver API

### Available Drivers
- **xd** - X Motor TMC Driver
- **yd** - Y Motor TMC Driver

### Current Configuration

```lua
-- Set motor current
-- Syntax: set_current(ma, hold_percent)
xd.set_current(800, 50)  -- 800mA run current, 50% hold current
yd.set_current(600, 30)  -- 600mA run current, 30% hold current

-- Common current values:
-- - NEMA 17: 400-1200mA
-- - NEMA 23: 800-2800mA
-- - Hold: 20-50% of run current
```

### Microstepping

```lua
-- Set microstepping resolution
xd.set_microsteps(16)  -- 16 microsteps per full step
yd.set_microsteps(32)  -- 32 microsteps per full step

-- Valid values: 1, 2, 4, 8, 16, 32, 64, 128, 256
-- Higher values = smoother movement, lower torque
-- Lower values = higher torque, more vibration
```

### StealthChop vs SpreadCycle

```lua
-- Enable StealthChop (quiet operation)
xd.enable_stealthchop(true)   -- Quiet but less powerful
yd.enable_stealthchop(false)  -- Powerful but noisier (SpreadCycle)

-- StealthChop: Good for low-speed, quiet operation
-- SpreadCycle: Better for high-speed, high-torque applications
```

### StallGuard (Sensorless Homing)

```lua
-- Enable StallGuard detection
xd.enable_stallguard(true)
xd.set_stall_threshold(10)  -- Sensitivity: -64 to +63

-- Monitor stall detection
local stall_result = xd.get_stall_result()
print("StallGuard Result:", stall_result)

-- Lower threshold = more sensitive
-- Higher result = more load/resistance
```

### Driver Diagnostics

```lua
-- Check driver version
local version = xd.get_version()
if version then
    print("TMC Driver Version:", version)
else
    print("Driver communication failed")
end

-- Check for errors
if xd.has_error() then
    print("Driver has errors!")
    print("Status:", xd.get_status())
end

-- Get detailed status
print("Driver Status:", xd.get_status())
```

### Homing Configuration

```lua
-- Configure for sensorless homing
xd.setup_homing()  -- Sets appropriate current and StallGuard settings
yd.setup_homing()

-- Re-configure with default settings
xd.config(false)   -- Apply default config
xd.config(true)    -- Reinitialize driver completely
```

---

## Buzzer Integration

### Basic Sounds

```lua
-- Success sound after movement
xm.start()
while xm.is_running() do
    delay(100)
end
piezo.success()  -- Play success tone

-- Error sound
piezo.error()

-- Custom tone
piezo.play({
    freq = 1000,           -- 1kHz frequency
    play_duration = 500,   -- 500ms duration
    times = 2,             -- Repeat twice
    blocking = true        -- Wait for completion
})
```

### Movement Feedback

```lua
-- Audio feedback during movement
piezo.set_callback("step", function(data)
    if data.playing then
        print("Beep: " .. data.freq .. "Hz")
    end
end)

-- Play music during long movements
xm.add_command(500, 1000, "cw")  -- Long movement
xm.start()
piezo.play_music("C4D4E4F4G4A4B4", false)  -- Non-blocking music
```

---

## BLE Communication

### Basic Output

```lua
-- Send status to BLE client
ble_print("Motor X Position: " .. xm.get_position())
ble_print("Motor Y Position: " .. ym.get_position())

-- Send formatted status
local x_status = xm.status()
ble_print("X Motor - Enabled: " .. tostring(x_status.enabled) .. 
          ", Running: " .. tostring(x_status.running))
```

### Real-time Monitoring

```lua
-- Monitor motors and send updates via BLE
function monitor_motors()
    while true do
        if xm.is_running() or ym.is_running() then
            local status = {
                x_pos = xm.get_position(),
                y_pos = ym.get_position(),
                x_running = xm.is_running(),
                y_running = ym.is_running()
            }
            
            ble_print("Status: X=" .. status.x_pos .. " Y=" .. status.y_pos)
            
            if status.x_running then
                ble_print("X motor running, queue: " .. xm.get_queue_count())
            end
            
            if status.y_running then
                ble_print("Y motor running, queue: " .. ym.get_queue_count())
            end
        end
        delay(500)  -- Update every 500ms
    end
end
```

---

## Complete Examples

### Example 1: Basic Movement with Feedback

```lua
-- Configure and move X motor with audio/BLE feedback
function basic_movement_demo()
    ble_print("=== Basic Movement Demo ===")
    
    -- Configure driver
    xd.set_current(800, 50)
    xd.set_microsteps(16)
    xd.enable_stealthchop(true)
    
    -- Enable motor
    xm.enable()
    ble_print("X motor enabled")
    
    -- Add movement commands
    xm.add_command(1000, 200, "cw")   -- Move right
    xm.add_command(2000, 100, "ccw")  -- Move left slower
    
    ble_print("Commands queued: " .. xm.get_queue_count())
    
    -- Start movement with audio feedback
    piezo.play({freq = 2000, play_duration = 100})  -- Start beep
    xm.start()
    
    -- Monitor progress
    while xm.is_running() do
        ble_print("Position: " .. xm.get_position() .. 
                  ", Queue: " .. xm.get_queue_count())
        delay(200)
    end
    
    -- Completion feedback
    piezo.success()
    ble_print("Movement complete! Final position: " .. xm.get_position())
end

basic_movement_demo()
```

### Example 2: Dual Motor Coordination

```lua
-- Coordinate X and Y motors for diagonal movement
function diagonal_movement()
    ble_print("=== Diagonal Movement Demo ===")
    
    -- Configure both drivers
    xd.set_current(800, 50)
    yd.set_current(800, 50)
    xd.set_microsteps(16)
    yd.set_microsteps(16)
    
    -- Enable both motors
    xm.enable()
    ym.enable()
    
    -- Add synchronized commands
    local steps = 300
    local speed = 1200  -- microseconds between steps
    
    xm.add_command(speed, steps, "cw")
    ym.add_command(speed, steps, "cw")
    
    -- Start both motors simultaneously
    ble_print("Starting diagonal movement...")
    piezo.play_music("C4E4G4", false)  -- Background music
    
    xm.start()
    ym.start()
    
    -- Monitor both motors
    while xm.is_running() or ym.is_running() do
        ble_print("X: " .. xm.get_position() .. " Y: " .. ym.get_position())
        delay(300)
    end
    
    piezo.success()
    ble_print("Diagonal movement complete!")
    ble_print("Final - X: " .. xm.get_position() .. " Y: " .. ym.get_position())
end

diagonal_movement()
```

### Example 3: Sensorless Homing

```lua
-- Perform sensorless homing using StallGuard
function sensorless_homing()
    ble_print("=== Sensorless Homing Demo ===")
    
    -- Configure for homing
    xd.setup_homing()
    xd.enable_stallguard(true)
    xd.set_stall_threshold(5)  -- Sensitive threshold
    
    xm.enable()
    
    -- Home X axis
    ble_print("Homing X axis...")
    local homing_speed = 2000  -- Slower for homing
    local max_steps = 1000     -- Safety limit
    
    -- Add homing command
    xm.add_command(homing_speed, max_steps, "ccw")  -- Move towards home
    xm.start()
    
    -- Monitor StallGuard during homing
    while xm.is_running() do
        local stall_result = xd.get_stall_result()
        ble_print("Homing... StallGuard: " .. stall_result)
        
        -- Check for stall (low values indicate contact)
        if stall_result < 50 then
            ble_print("Stall detected! Stopping...")
            xm.stop()
            break
        end
        
        delay(100)
    end
    
    -- Set home position
    xm.reset_position()
    xm.reset_realtime_position()
    
    piezo.success()
    ble_print("Homing complete! Position reset to 0")
    
    -- Return to normal operation
    xd.config(false)  -- Restore normal settings
end

sensorless_homing()
```

### Example 4: Motor Diagnostics

```lua
-- Comprehensive motor and driver diagnostics
function motor_diagnostics()
    ble_print("=== Motor Diagnostics ===")
    
    -- Check driver communication
    local x_version = xd.get_version()
    local y_version = yd.get_version()
    
    if x_version then
        ble_print("X Driver Version: " .. x_version)
    else
        ble_print("X Driver: Communication Failed!")
        piezo.error()
        return
    end
    
    if y_version then
        ble_print("Y Driver Version: " .. y_version)
    else
        ble_print("Y Driver: Communication Failed!")
        piezo.error()
        return
    end
    
    -- Check for driver errors
    if xd.has_error() then
        ble_print("X Driver Errors: " .. xd.get_status())
        piezo.error()
    else
        ble_print("X Driver: OK")
    end
    
    if yd.has_error() then
        ble_print("Y Driver Errors: " .. yd.get_status())
        piezo.error()
    else
        ble_print("Y Driver: OK")
    end
    
    -- Test movement and position tracking
    ble_print("Testing position tracking...")
    
    xm.enable()
    xm.reset_position()
    xm.reset_realtime_position()
    
    -- Small test movement
    xm.add_command(1000, 50, "cw")
    xm.start()
    
    while xm.is_running() do
        delay(50)
    end
    
    local rmt_pos = xm.get_position()
    local real_pos = xm.get_realtime_position()
    local difference = math.abs(rmt_pos - real_pos)
    
    ble_print("Position Test Results:")
    ble_print("  RMT Position: " .. rmt_pos)
    ble_print("  Real Position: " .. real_pos)
    ble_print("  Difference: " .. difference)
    
    if difference <= 2 then
        ble_print("Position tracking: PASSED")
        piezo.success()
    else
        ble_print("Position tracking: FAILED")
        piezo.error()
    end
    
    -- Detailed status
    ble_print("X Driver Status: " .. xd.get_status())
    ble_print("Y Driver Status: " .. yd.get_status())
    
    local x_status = xm.status()
    ble_print("X Motor Status:")
    ble_print("  Enabled: " .. tostring(x_status.enabled))
    ble_print("  Running: " .. tostring(x_status.running))
    ble_print("  Position: " .. x_status.position)
    ble_print("  Direction: " .. x_status.direction)
end

motor_diagnostics()
```

### Example 5: Interactive Control Loop

```lua
-- Interactive motor control with BLE commands
function interactive_control()
    ble_print("=== Interactive Motor Control ===")
    ble_print("Commands:")
    ble_print("  'x100' - Move X motor 100 steps CW")
    ble_print("  'x-50' - Move X motor 50 steps CCW")
    ble_print("  'y200' - Move Y motor 200 steps CW")
    ble_print("  'status' - Show motor status")
    ble_print("  'home' - Home both motors")
    ble_print("  'stop' - Emergency stop")
    ble_print("  'exit' - Exit control mode")
    
    -- Initialize motors
    xd.set_current(800, 50)
    yd.set_current(800, 50)
    xm.enable()
    ym.enable()
    
    piezo.play({freq = 1500, play_duration = 200})
    
    -- This would be expanded with actual command parsing
    -- For demonstration, showing the command structure
    
    -- Example command handlers:
    function handle_move_command(axis, steps)
        local direction = steps > 0 and "cw" or "ccw"
        steps = math.abs(steps)
        
        if axis == "x" then
            xm.add_command(1000, steps, direction)
            xm.start()
            ble_print("Moving X " .. steps .. " steps " .. direction)
        elseif axis == "y" then
            ym.add_command(1000, steps, direction)
            ym.start()
            ble_print("Moving Y " .. steps .. " steps " .. direction)
        end
    end
    
    function handle_status_command()
        local x_status = xm.status()
        local y_status = ym.status()
        
        ble_print("Motor Status:")
        ble_print("X - Pos: " .. x_status.position .. 
                  ", Running: " .. tostring(x_status.running))
        ble_print("Y - Pos: " .. y_status.position .. 
                  ", Running: " .. tostring(y_status.running))
    end
    
    function handle_home_command()
        ble_print("Homing both motors...")
        if not xm.is_running() then xm.reset_position() end
        if not ym.is_running() then ym.reset_position() end
        piezo.success()
        ble_print("Home position set")
    end
    
    function handle_stop_command()
        xm.stop()
        ym.stop()
        piezo.play({freq = 500, play_duration = 300})
        ble_print("Emergency stop executed")
    end
    
    ble_print("Interactive control ready!")
end

interactive_control()
```

### Example 6: Speed Testing

```lua
-- Test different movement speeds and provide feedback
function speed_testing()
    ble_print("=== Speed Testing Demo ===")
    
    -- Configure motor
    xd.set_current(800, 50)
    xd.set_microsteps(16)
    xm.enable()
    
    -- Test different speeds
    local speeds = {500, 1000, 1500, 2000, 3000}  -- microseconds between steps
    local test_steps = 100
    
    for i, speed in ipairs(speeds) do
        ble_print("Testing speed " .. i .. ": " .. speed .. "us per step")
        
        -- Reset position for consistent measurement
        xm.reset_position()
        
        -- Time the movement
        local start_time = millis()
        
        xm.add_command(speed, test_steps, "cw")
        xm.start()
        
        while xm.is_running() do
            delay(10)
        end
        
        local end_time = millis()
        local duration = end_time - start_time
        
        ble_print("  Duration: " .. duration .. "ms")
        ble_print("  Final Position: " .. xm.get_position())
        
        piezo.play({freq = 1000 + (i * 200), play_duration = 100})
        delay(500)  -- Pause between tests
    end
    
    piezo.success()
    ble_print("Speed testing complete!")
end

speed_testing()
```

### Example 7: Pattern Movements

```lua
-- Create geometric patterns with coordinated movement
function pattern_movements()
    ble_print("=== Pattern Movement Demo ===")
    
    -- Configure both motors
    xd.set_current(800, 50)
    yd.set_current(800, 50)
    xd.set_microsteps(16)
    yd.set_microsteps(16)
    xm.enable()
    ym.enable()
    
    -- Square pattern
    function draw_square(size, speed)
        ble_print("Drawing square pattern...")
        
        -- Side 1: Right
        xm.add_command(speed, size, "cw")
        -- Side 2: Up  
        ym.add_command(speed, size, "cw")
        -- Side 3: Left
        xm.add_command(speed, size, "ccw")
        -- Side 4: Down
        ym.add_command(speed, size, "ccw")
        
        xm.start()
        delay(100)  -- Slight delay between axes
        ym.start()
        
        while xm.is_running() or ym.is_running() do
            ble_print("X: " .. xm.get_position() .. " Y: " .. ym.get_position())
            delay(300)
        end
    end
    
    -- Circle approximation
    function draw_circle(radius, segments)
        ble_print("Drawing circle pattern...")
        
        local angle_step = (2 * math.pi) / segments
        local speed = 1000
        
        for i = 0, segments-1 do
            local angle = i * angle_step
            local x_step = math.floor(radius * math.cos(angle))
            local y_step = math.floor(radius * math.sin(angle))
            
            if x_step > 0 then
                xm.add_command(speed, math.abs(x_step), "cw")
            elseif x_step < 0 then
                xm.add_command(speed, math.abs(x_step), "ccw")
            end
            
            if y_step > 0 then
                ym.add_command(speed, math.abs(y_step), "cw")
            elseif y_step < 0 then
                ym.add_command(speed, math.abs(y_step), "ccw")
            end
        end
        
        xm.start()
        ym.start()
        
        while xm.is_running() or ym.is_running() do
            delay(200)
        end
    end
    
    -- Execute patterns
    piezo.play_music("C4D4E4", false)
    
    draw_square(200, 1000)
    delay(1000)
    
    piezo.play_music("E4F4G4", false)
    draw_circle(100, 8)
    
    piezo.success()
    ble_print("Pattern movements complete!")
    ble_print("Final X: " .. xm.get_position() .. " Y: " .. ym.get_position())
end

pattern_movements()
```

---

## Troubleshooting

### Common Issues

1. **Motor not moving:**
   ```lua
   -- Check if motor is enabled
   if not xm.is_enabled() then
       xm.enable()
   end
   
   -- Check queue
   if xm.get_queue_count() == 0 then
       ble_print("No commands in queue")
   end
   ```

2. **Driver communication failed:**
   ```lua
   if not xd.get_version() then
       ble_print("Check TMC driver wiring and power")
       xd.config(true)  -- Try reinitializing
   end
   ```

3. **Position tracking mismatch:**
   ```lua
   local diff = math.abs(xm.get_position() - xm.get_realtime_position())
   if diff > 5 then
       ble_print("Position mismatch detected: " .. diff)
       -- Check for missed steps or mechanical issues
   end
   ```

### Debug Functions

```lua
-- Debug helper functions
function debug_motor_state(motor_name, motor, driver)
    ble_print("=== " .. motor_name .. " Motor Debug ===")
    
    local status = motor.status()
    ble_print("Enabled: " .. tostring(status.enabled))
    ble_print("Running: " .. tostring(status.running))
    ble_print("Position: " .. status.position)
    ble_print("Real Position: " .. status.realtime_position)
    ble_print("Queue: " .. status.queue_count)
    
    local version = driver.get_version()
    if version then
        ble_print("Driver Version: " .. version)
        ble_print("Driver Status: " .. driver.get_status())
        ble_print("Has Error: " .. tostring(driver.has_error()))
    else
        ble_print("Driver: Communication Failed")
    end
end

-- Usage:
debug_motor_state("X", xm, xd)
debug_motor_state("Y", ym, yd)
```

### Performance Testing

```lua
-- Test motor performance and timing accuracy
function performance_test()
    ble_print("=== Performance Test ===")
    
    xm.enable()
    xm.reset_position()
    xm.reset_realtime_position()
    
    local test_steps = 1000
    local target_speed = 1000  -- 1ms per step
    
    ble_print("Testing " .. test_steps .. " steps at " .. target_speed .. "us/step")
    
    local start_time = millis()
    xm.add_command(target_speed, test_steps, "cw")
    xm.start()
    
    local start_pos = xm.get_position()
    
    while xm.is_running() do
        -- Monitor during execution
        local current_pos = xm.get_position()
        local progress = current_pos - start_pos
        local percentage = (progress / test_steps) * 100
        
        if percentage % 25 == 0 then  -- Report every 25%
            ble_print("Progress: " .. math.floor(percentage) .. "%")
        end
        
        delay(50)
    end
    
    local end_time = millis()
    local actual_duration = end_time - start_time
    local expected_duration = (test_steps * target_speed) / 1000  -- Convert to ms
    
    local final_rmt = xm.get_position()
    local final_real = xm.get_realtime_position()
    
    ble_print("Performance Results:")
    ble_print("  Expected Duration: " .. expected_duration .. "ms")
    ble_print("  Actual Duration: " .. actual_duration .. "ms")
    ble_print("  Timing Error: " .. (actual_duration - expected_duration) .. "ms")
    ble_print("  RMT Steps: " .. (final_rmt - start_pos))
    ble_print("  Real Steps: " .. final_real)
    ble_print("  Step Accuracy: " .. (test_steps - math.abs(final_real - (final_rmt - start_pos))))
    
    if math.abs(actual_duration - expected_duration) < 50 then
        ble_print("Timing: PASSED")
        piezo.success()
    else
        ble_print("Timing: FAILED")
        piezo.error()
    end
end

performance_test()
```

---

## API Reference Summary

### Motor Control (xm, ym)
- `enable()` / `disable()` / `is_enabled()`
- `start()` / `stop()` / `is_running()`
- `add_command(delay_us, steps, direction)`
- `clear_queue()` / `get_queue_count()`
- `get_position()` / `get_realtime_position()`
- `reset_position()` / `reset_realtime_position()`
- `status()` / `enable_estop()` / `disable_estop()`

### TMC Driver (xd, yd)
- `set_current(ma, hold_percent)` / `set_microsteps(steps)`
- `enable_stallguard(enable)` / `set_stall_threshold(threshold)` / `get_stall_result()`
- `enable_stealthchop(enable)` / `setup_homing()` / `config(reinit)`
- `get_version()` / `has_error()` / `get_status()`

### Constants
- `DIR.CW` (0) / `DIR.CCW` (1)

### Piezo
- `piezo.play(options)` / `piezo.stop()` / `piezo.success()` / `piezo.error()`
- `piezo.play_music(string)` / `piezo.set_speed(speed)`

### BLE
- `ble_print(string)` - Send text to connected BLE client

### Arduino Functions Available in Lua
- `delay(ms)` / `millis()` / `micros()`
- `print(...)` / `math.abs()` / `math.floor()` / `tostring()`

---

## Quick Reference Card

```lua
-- Quick Setup
xd.set_current(800, 50)      -- Set current
xd.set_microsteps(16)        -- Set resolution
xm.enable()                  -- Enable motor

-- Basic Movement
xm.add_command(1000, 200, "cw")  -- Queue command
xm.start()                       -- Start execution

-- Status Check
print("Running:", xm.is_running())
print("Position:", xm.get_position())
ble_print("Status sent to phone")

-- Audio Feedback
piezo.success()              -- Success sound
piezo.error()                -- Error sound
piezo.play_music("C4D4E4")   -- Play melody

-- Emergency
xm.stop()                    -- Stop immediately
xm.clear_queue()             -- Clear commands
```

This documentation provides a comprehensive guide to using the Motor Control API with practical examples that demonstrate real-world usage patterns, testing procedures, and troubleshooting techniques. 