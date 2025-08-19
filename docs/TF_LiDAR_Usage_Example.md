# TF-LiDAR Lua Wrapper Usage Guide

## Overview
The TF-LiDAR wrapper provides a simple interface to the Benewake TFMini-Plus LiDAR sensor with high-frequency data collection at 250Hz (4ms intervals).

## Features
- **High-frequency data collection**: 250Hz sampling rate (4ms intervals)
- **Simple API**: Just 4 essential functions
- **Thread-safe operations**: FreeRTOS-based implementation
- **Shared variable access**: Direct access to latest sensor data

## API Reference

### Basic Operations
```lua
-- Start data collection
tf.start()

-- Stop data collection
tf.stop()

-- Check if sensor is running
if tf.isRunning() then
    print("TF-LiDAR is collecting data")
end

-- Read latest data
local data = tf.read()
if data then
    print("Distance: " .. data.distance .. " cm")
    print("Flux: " .. data.flux)
    print("Temperature: " .. data.temperature .. " °C")
    print("Valid: " .. tostring(data.valid))
end
```

## Complete Example
```lua
-- Complete TF-LiDAR usage example
function main()
    print("Starting TF-LiDAR example...")
    
    -- Start data collection
    if not tf.start() then
        print("Failed to start TF-LiDAR")
        return
    end
    
    print("TF-LiDAR started successfully")
    print("Data will be collected at 250Hz")
    
    -- Keep running for 10 seconds
    local start_time = millis()
    while (millis() - start_time) < 10000 do
        -- Read data
        local data = tf.read()
        if data and data.valid then
            print(string.format("Distance: %d cm, Flux: %d, Temp: %d°C", 
                data.distance, data.flux, data.temperature))
        else
            print("No valid data available")
        end
        
        delay(1000)  -- Wait 1 second between reads
    end
    
    -- Stop data collection
    tf.stop()
    
    print("TF-LiDAR example completed")
end

-- Run the example
main()
```

## Data Structure
The `tf.read()` function returns a table with the following fields:
```lua
{
    distance = 123,      -- Distance in centimeters
    flux = 456,          -- Signal strength
    temperature = 25,    -- Temperature in Celsius
    valid = true         -- Data validity flag
}
```

## Error Handling
```lua
-- Check for errors in data
local data = tf.read()
if data then
    if data.valid then
        print("Valid data received")
    else
        print("Invalid data - check sensor connection")
    end
else
    print("No data available")
end
```

## Performance Notes
- **Timer-based collection**: Data is collected every 4ms (250Hz) using FreeRTOS task
- **Shared variables**: Latest data is stored in shared variables for immediate access
- **Non-blocking reads**: All read operations are non-blocking
- **Single task**: One background task handles all data collection

## Hardware Configuration
The TF-LiDAR sensor should be connected to:
- **TX**: GPIO 18 (LIDAR_TX)
- **RX**: GPIO 17 (LIDAR_RX)
- **Power**: 5V
- **Ground**: GND

## Troubleshooting
1. **No data received**: Check serial connections and power supply
2. **Invalid data**: Ensure sensor is properly positioned and has clear line of sight
3. **High error rates**: Check for interference or poor signal conditions
4. **Start fails**: Verify hardware connections and serial port availability 