# Lua Serial Script Runner

This example demonstrates how to create an interactive Lua script runner with advanced C++ function bindings.

## Features

1. Basic Operations
   - Script execution via Serial
   - Real-time script control
   - Memory monitoring
   - Error handling

2. Custom C++ Functions
   - Variable argument addition
   - Multiple return values
   - Object creation with methods
   - Mathematical operations

3. Control Commands
   - `STOP` - Stop running script
   - `HELP` - Show command help
   - `MEM` - Display memory usage

## Function Examples

1. Variable Argument Addition:
```lua
-- Add any number of values
sum = add(10, 20, 30, 40)
print("Sum:", sum)  -- Output: 100
```

2. Multiple Return Values:
```lua
-- Get multiple calculations at once
sum, diff, prod, quot = calc_stats(10, 5)
print("Sum:", sum)       -- Output: 15
print("Difference:", diff)   -- Output: 5
print("Product:", prod)      -- Output: 50
print("Quotient:", quot)     -- Output: 2
```

3. Object Creation and Methods:
```lua
-- Create a point and calculate distance
point = create_point(3, 4)
print("X:", point.x)         -- Output: 3
print("Y:", point.y)         -- Output: 4
print("Distance:", point:distance())  -- Output: 5
```

4. Mathematical Constants:
```lua
-- Access built-in constants
print("Pi:", MATH_CONST.pi)
print("e:", MATH_CONST.e)
```

## Command Reference

1. Running Scripts:
   - Type/paste Lua code
   - End with `###`
   - Use STOP to interrupt

2. Available Functions:
   - add(...) - Sum multiple numbers
   - calc_stats(a, b) - Returns sum, difference, product, quotient
   - create_point(x, y) - Creates point object with distance method
   - serial_write(msg) - Print to Serial

3. Global Tables:
   - MATH_CONST - Mathematical constants
   - SYSTEM - System information
   - Arduino - Arduino functions
## Implementation Details

1. C++ Function Return Values:
```cpp
// Single return value
static int l_add_numbers(lua_State* L) {
    double sum = 0;
    int n = lua_gettop(L);
    for(int i = 1; i <= n; i++) {
        sum += luaL_checknumber(L, i);
    }
    lua_pushnumber(L, sum);
    return 1;  // Number of return values
}

// Multiple return values
static int l_calc_stats(lua_State* L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushnumber(L, a + b);  // Sum
    lua_pushnumber(L, a - b);  // Difference
    lua_pushnumber(L, a * b);  // Product
    lua_pushnumber(L, b != 0 ? a / b : 0);  // Quotient
    return 4;  // Number of return values
}
```

2. Object Creation:
```cpp
static int l_create_point(lua_State* L) {
    lua_newtable(L);  // Create table
    // Add properties
    lua_pushstring(L, "x");
    lua_pushnumber(L, luaL_checknumber(L, 1));
    lua_settable(L, -3);
    // Add methods
    lua_pushstring(L, "distance");
    lua_pushcfunction(L, calculate_distance);
    lua_settable(L, -3);
    return 1;  // Return the table
}
```

## Best Practices

1. Error Handling:
   - Use pcall() in Lua scripts
   - Check parameters in C++ functions
   - Provide clear error messages

2. Memory Management:
   - Clean up after long operations
   - Monitor heap usage
   - Use PSRAM for large data

3. Script Development:
   - Test functions individually
   - Use print() for debugging
   - Keep scripts modular
