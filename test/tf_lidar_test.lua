-- TF-LiDAR Test Script
-- This script tests the simplified TF-LiDAR wrapper functionality

print("=== TF-LiDAR Test Script ===")

-- Test start
print("1. Testing start...")
if tf.start() then
    print("   ✓ TF-LiDAR started successfully")
else
    print("   ✗ TF-LiDAR start failed")
    return
end

-- Test isRunning
print("2. Testing isRunning...")
if tf.isRunning() then
    print("   ✓ TF-LiDAR is running")
else
    print("   ✗ TF-LiDAR is not running")
end

-- Test data reading
print("3. Testing data reading...")
local data_count = 0
local max_attempts = 10

for i = 1, max_attempts do
    local data = tf.read()
    if data then
        if data.valid then
            print(string.format("   ✓ Data %d: Distance=%d cm, Flux=%d, Temp=%d°C", 
                i, data.distance, data.flux, data.temperature))
            data_count = data_count + 1
        else
            print(string.format("   - Data %d: Invalid data", i))
        end
    else
        print(string.format("   - Data %d: No data available", i))
    end
    delay(100)  -- Wait 100ms between reads
end

print(string.format("   Data collection: %d/%d successful reads", data_count, max_attempts))

-- Test continuous reading
print("4. Testing continuous reading...")
local start_time = millis()
local continuous_count = 0
local max_continuous = 20

while continuous_count < max_continuous and (millis() - start_time) < 3000 do
    local data = tf.read()
    if data and data.valid then
        continuous_count = continuous_count + 1
        if continuous_count % 5 == 0 then  -- Print every 5th reading
            print(string.format("   Continuous %d: Distance=%d cm", 
                continuous_count, data.distance))
        end
    end
    delay(50)  -- Read every 50ms
end

print(string.format("   Continuous reading: %d valid readings in %d ms", 
    continuous_count, millis() - start_time))

-- Test stop
print("5. Testing stop...")
if tf.stop() then
    print("   ✓ TF-LiDAR stopped successfully")
else
    print("   ✗ TF-LiDAR stop failed")
end

-- Final status check
print("6. Final status check...")
if tf.isRunning() then
    print("   ✗ TF-LiDAR is still running")
else
    print("   ✓ TF-LiDAR is stopped")
end

-- Test reading after stop
print("7. Testing read after stop...")
local data = tf.read()
if data then
    print("   - Data still available after stop (last reading)")
else
    print("   - No data available after stop")
end

print("=== TF-LiDAR Test Completed ===")
print(string.format("Summary: %d successful reads, %d continuous reads", data_count, continuous_count)) 