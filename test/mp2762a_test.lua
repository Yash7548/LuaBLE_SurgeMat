-- MP2762A Charger IC Test Script
-- This script demonstrates the basic functionality of the MP2762A charger

print("=== MP2762A Charger IC Test ===")

-- Initialize the charger
print("\n1. Initializing charger...")
local success, message = charger.init()
if success then
    print("✓ " .. message)
else
    print("✗ " .. message)
    return
end

-- Check if device is connected
print("\n2. Checking device connection...")
if charger.is_connected() then
    print("✓ Device is connected")
else
    print("✗ Device not found")
    return
end

-- Get initial status
print("\n3. Getting initial status...")
local status = charger.get_status()
if status then
    print("✓ Status retrieved successfully")
    print("  Battery: " .. string.format("%.2fV, %.2fA", status.battery_voltage, status.battery_current))
    print("  Input: " .. string.format("%.2fV, %.2fA", status.input_voltage, status.input_current))
    print("  System: " .. string.format("%.2fV", status.system_voltage))
    print("  Temperature: " .. string.format("%.1f°C", status.junction_temperature))
    print("  Charging State: " .. status.charging_state)
    print("  Fault: " .. status.fault)
else
    print("✗ Failed to get status")
end

-- Configure charger settings
print("\n4. Configuring charger settings...")

-- Set charge current to 2A
local success, message = charger.set_charge_current(2000)
print("Set charge current (2A): " .. (success and "✓" or "✗") .. " " .. message)

-- Set input current limit to 3A
local success, message = charger.set_input_current_limit(3000)
print("Set input current limit (3A): " .. (success and "✓" or "✗") .. " " .. message)

-- Set battery full voltage to 8.4V
local success, message = charger.set_battery_full_voltage(8.4)
print("Set battery full voltage (8.4V): " .. (success and "✓" or "✗") .. " " .. message)

-- Enable charging
local success, message = charger.enable_charging(true)
print("Enable charging: " .. (success and "✓" or "✗") .. " " .. message)

-- Monitor charging for a few cycles
print("\n5. Monitoring charging status...")
for i = 1, 10 do
    local status = charger.get_status()
    if status then
        print(string.format("Cycle %d: Battery=%.2fV, Current=%.2fA, State=%s", 
            i, status.battery_voltage, status.battery_current, status.charging_state))
        
        -- Check for faults
        if status.fault ~= "None" then
            print("⚠ Fault detected: " .. status.fault)
            break
        end
        
        -- Check if charging is complete
        if status.charging_state == "Charge Done" then
            print("✓ Charging completed!")
            break
        end
    else
        print("✗ Failed to get status on cycle " .. i)
        break
    end
    
    -- Wait 1 second between readings
    delay(1000)
end

-- Test OTG functionality
print("\n6. Testing OTG functionality...")
local success, message = charger.enable_otg(true)
print("Enable OTG: " .. (success and "✓" or "✗") .. " " .. message)

if success then
    local success, message = charger.set_otg_voltage(5.1)
    print("Set OTG voltage (5.1V): " .. (success and "✓" or "✗") .. " " .. message)
    
    local success, message = charger.set_otg_current_limit(1500)
    print("Set OTG current limit (1.5A): " .. (success and "✓" or "✗") .. " " .. message)
    
    -- Get status to check OTG state
    local status = charger.get_status()
    if status then
        print("OTG State: " .. status.otg_state)
    end
end

-- Get final measurements
print("\n7. Final measurements...")
local measurements = charger.get_measurements()
if measurements then
    print("✓ Final measurements:")
    print("  Battery Voltage: " .. string.format("%.2fV", measurements.battery_voltage))
    print("  Battery Current: " .. string.format("%.2fA", measurements.battery_current))
    print("  Input Voltage: " .. string.format("%.2fV", measurements.input_voltage))
    print("  Input Current: " .. string.format("%.2fA", measurements.input_current))
    print("  System Voltage: " .. string.format("%.2fV", measurements.system_voltage))
    print("  Junction Temperature: " .. string.format("%.1f°C", measurements.junction_temperature))
    print("  System Power: " .. string.format("%.2fW", measurements.system_power))
else
    print("✗ Failed to get measurements")
end

-- Test fault handling
print("\n8. Testing fault handling...")
local success, message = charger.clear_fault()
print("Clear fault: " .. (success and "✓" or "✗") .. " " .. message)

-- Test reset functionality
print("\n9. Testing reset functionality...")
local success, message = charger.reset()
print("Reset charger: " .. (success and "✓" or "✗") .. " " .. message)

print("\n=== Test Complete ===")

-- Helper function for delay (if not available)
if not delay then
    function delay(ms)
        local start = os.clock()
        while (os.clock() - start) * 1000 < ms do
            -- Wait
        end
    end
end 