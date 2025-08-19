-- FastAccelStepper Style Motor Test
-- Tests the FastAccelStepper-inspired RMT implementation

print("=== FastAccelStepper Motor Test ===")

-- Test X Motor Enable/Disable
print("Step 1: Testing X motor enable/disable...")
print("Before enable - is_enabled:", xm.is_enabled())
xm.enable()
print("After enable - is_enabled:", xm.is_enabled())

-- Clear queue first
print("\nStep 2: Clearing queue...")
xm.clear_queue()
print("Queue cleared, count:", xm.get_queue_count())

-- Add test commands with different delays and directions
print("\nStep 3: Adding FastAccelStepper test commands...")

-- Small movements with various timings
print("Adding command 1: 2000us delay, 20 steps, CW")
xm.add_command(2000, 20, "cw")
print("Queue count:", xm.get_queue_count())

print("Adding command 2: 1500us delay, 15 steps, CCW") 
xm.add_command(1500, 15, "ccw")
print("Queue count:", xm.get_queue_count())

print("Adding command 3: 3000us delay, 25 steps, CW")
xm.add_command(3000, 25, "cw")
print("Queue count:", xm.get_queue_count())

-- Get initial status
print("\nStep 4: Initial status check...")
local status = xm.status()
print("Status details:")
for k, v in pairs(status) do
    print("  " .. k .. ":", v)
end

-- Start execution
print("\nStep 5: Starting FastAccelStepper execution...")
print("About to call xm.start()...")
xm.start()
print("xm.start() called - check Serial monitor for detailed FastAccelStepper logs")

-- Monitor execution with shorter intervals
print("\nStep 6: Monitoring FastAccelStepper execution...")
for i = 1, 40 do
    -- Short wait for more responsive monitoring
    os.sleep(150)  -- Wait 150ms
    
    local current_status = xm.status()
    print(string.format("Check %d - Running: %s, Position: %d, Queue: %d", 
          i, current_status.running and "Yes" or "No", 
          current_status.position, current_status.queue_count))
    
    -- Exit early if completed
    if not current_status.running and current_status.queue_count == 0 then
        print("✓ FastAccelStepper execution completed successfully!")
        break
    end
    
    -- Timeout check
    if i >= 40 then
        print("⚠ Timeout - motor still running after 6 seconds")
        break
    end
end

-- Final comprehensive status
print("\nStep 7: Final status check...")
local final_status = xm.status()
print("Final status details:")
for k, v in pairs(final_status) do
    print("  " .. k .. ":", v)
end

print("\nStep 8: Testing another sequence...")
-- Test immediate second sequence
xm.clear_queue()
xm.add_command(1000, 10, "ccw")
xm.add_command(1000, 10, "cw")
print("Added second sequence, starting...")
xm.start()

-- Quick monitoring
for i = 1, 20 do
    os.sleep(100)
    local status = xm.status()
    if not status.running and status.queue_count == 0 then
        print("✓ Second sequence completed!")
        break
    end
end

print("\n=== FastAccelStepper Test Complete ===")
print("Check Serial output for:")
print("- 'FastAccelStepper RMT transmission started'") 
print("- 'Filled buffer with N symbols'")
print("- 'Added FAS command' messages")
print("- No 'garbage data' errors")