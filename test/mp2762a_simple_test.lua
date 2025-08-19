-- Simple MP2762A Charger Test Script
-- Tests basic functionality with updated pin configuration

print("=== MP2762A Charger Simple Test ===")

-- Initialize charger
print("\n1. Initializing charger...")
local success, message = charger.init()
print("Result:", success, message)

if not success then
    print("Failed to initialize charger. Exiting.")
    return
end

-- Check connection
print("\n2. Checking connection...")
local connected = charger.is_connected()
print("Connected:", connected)

if not connected then
    print("Charger not connected. Exiting.")
    return
end

-- Get initial status
print("\n3. Getting initial status...")
local status = charger.get_status()
if status then
    print("Input present:", status.input_present)
    print("Battery present:", status.battery_present)
    print("Charging state:", status.charging_state)
    print("Battery voltage:", string.format("%.2fV", status.battery_voltage))
    print("Input voltage:", string.format("%.2fV", status.input_voltage))
    print("Power good pin:", status.power_good)
else
    print("Failed to get status")
end

-- Test measurements
print("\n4. Getting measurements...")
local measurements = charger.get_measurements()
if measurements then
    print("Battery voltage:", string.format("%.2fV", measurements.battery_voltage))
    print("Battery current:", string.format("%.2fmA", measurements.battery_current))
    print("Input voltage:", string.format("%.2fV", measurements.input_voltage))
    print("Input current:", string.format("%.2fmA", measurements.input_current))
    print("System voltage:", string.format("%.2fV", measurements.system_voltage))
    print("Junction temp:", string.format("%.1f°C", measurements.junction_temperature))
    print("System power:", string.format("%.2fW", measurements.system_power))
else
    print("Failed to get measurements")
end

-- Test configuration
print("\n5. Testing configuration...")
success, message = charger.set_charge_current(1000)  -- 1A
print("Set charge current:", success, message)

success, message = charger.set_input_current_limit(1500)  -- 1.5A
print("Set input current limit:", success, message)

success, message = charger.set_battery_full_voltage(8.4)  -- 8.4V for 2S
print("Set battery full voltage:", success, message)

-- Test enable/disable
print("\n6. Testing enable/disable...")
success, message = charger.enable_charging(true)
print("Enable charging:", success, message)

success, message = charger.enable_termination(true)
print("Enable termination:", success, message)

success, message = charger.enable_safety_timer(true)
print("Enable safety timer:", success, message)

-- Get final status
print("\n7. Getting final status...")
status = charger.get_status()
if status then
    print("Charging enabled:", status.charging_enabled)
    print("Charging state:", status.charging_state)
    print("Fault:", status.fault)
    print("Temperature region:", status.temperature_region)
end

print("\n=== Test Complete ===") 