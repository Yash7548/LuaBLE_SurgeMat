-- Test script for the new C-style charger implementation
-- This demonstrates the RTOS task-based charger system

print("=== MP2762A C-Style Charger Test ===")

-- Initialize the charger (this is now done in C++ during device init)
local success, msg = charger.init()
print("Init result:", success, msg)

-- Check if charger is connected
local connected = charger.is_connected()
print("Charger connected:", connected)

if connected then
    -- Start the RTOS task for continuous monitoring
    local task_success, task_msg = charger.start_task()
    print("Task start result:", task_success, task_msg)
    
    -- Check if task is running
    local task_running = charger.is_task_running()
    print("Task running:", task_running)
    
    -- Get initial status
    local status = charger.get_status()
    print("\n=== Initial Status ===")
    print("Input present:", status.input_present)
    print("Battery present:", status.battery_present)
    print("Charging enabled:", status.charging_enabled)
    print("Charging state:", status.charging_state)
    print("Battery voltage:", status.battery_voltage, "V")
    print("Battery current:", status.battery_current, "mA")
    print("Input voltage:", status.input_voltage, "V")
    print("Junction temperature:", status.junction_temperature, "°C")
    print("Temperature region:", status.temperature_region)
    print("Fault:", status.fault)
    print("Last update time:", status.last_update_time)
    
    -- Test configuration functions
    print("\n=== Testing Configuration ===")
    
    -- Set charge current to 800mA
    local curr_success, curr_msg = charger.set_charge_current(800)
    print("Set charge current (800mA):", curr_success, curr_msg)
    
    -- Set input current limit to 1200mA
    local input_success, input_msg = charger.set_input_current_limit(1200)
    print("Set input current limit (1200mA):", input_success, input_msg)
    
    -- Set battery full voltage to 8.2V
    local volt_success, volt_msg = charger.set_battery_full_voltage(8.2)
    print("Set battery full voltage (8.2V):", volt_success, volt_msg)
    
    -- Enable charging
    local charge_success, charge_msg = charger.enable_charging(true)
    print("Enable charging:", charge_success, charge_msg)
    
    -- Wait a bit for the task to update status
    print("\n=== Waiting for task updates (5 seconds) ===")
    for i = 1, 5 do
        -- Get quick measurements
        local measurements = charger.get_measurements()
        print(string.format("Time %ds - Battery: %.2fV, %.0fmA, Temp: %.1f°C", 
              i, measurements.battery_voltage, measurements.battery_current, measurements.junction_temperature))
        
        -- Sleep for 1 second (this would be done with a proper delay in real usage)
        -- In a real Lua environment, you'd use os.sleep() or similar
        local start_time = os.clock()
        while os.clock() - start_time < 1 do end
    end
    
    -- Get updated status
    print("\n=== Updated Status ===")
    status = charger.get_status()
    print("Charging state:", status.charging_state)
    print("Battery voltage:", status.battery_voltage, "V")
    print("Battery current:", status.battery_current, "mA")
    print("System power:", status.system_power, "mW")
    print("Last update time:", status.last_update_time)
    
    -- Test OTG functionality
    print("\n=== Testing OTG ===")
    local otg_volt_success, otg_volt_msg = charger.set_otg_voltage(5.1)
    print("Set OTG voltage (5.1V):", otg_volt_success, otg_volt_msg)
    
    local otg_curr_success, otg_curr_msg = charger.set_otg_current_limit(1000)
    print("Set OTG current limit (1000mA):", otg_curr_success, otg_curr_msg)
    
    local otg_enable_success, otg_enable_msg = charger.enable_otg(true)
    print("Enable OTG:", otg_enable_success, otg_enable_msg)
    
    -- Wait a moment and check status
    local start_time = os.clock()
    while os.clock() - start_time < 1 do end
    
    status = charger.get_status()
    print("OTG enabled:", status.otg_enabled)
    print("OTG state:", status.otg_state)
    
    -- Disable OTG
    local otg_disable_success, otg_disable_msg = charger.enable_otg(false)
    print("Disable OTG:", otg_disable_success, otg_disable_msg)
    
    -- Test manual status update
    print("\n=== Manual Status Update ===")
    local update_success, update_msg = charger.update_status()
    print("Manual update:", update_success, update_msg)
    
    -- Final status
    status = charger.get_status()
    print("Final charging state:", status.charging_state)
    print("Final fault status:", status.fault)
    
    print("\n=== Test Complete ===")
    print("Note: RTOS task continues running in background")
    print("Task will automatically update status every 1 second")
    print("Interrupts are handled automatically by the task")
    
else
    print("Charger not connected - skipping tests")
end

print("\n=== C-Style API Benefits ===")
print("✓ RTOS task handles continuous monitoring")
print("✓ Interrupt-driven updates (100ms timeout)")
print("✓ Automatic status updates every 1 second")  
print("✓ Thread-safe with mutex protection")
print("✓ No global objects - clean C interface")
print("✓ Compatible with existing Lua API") 