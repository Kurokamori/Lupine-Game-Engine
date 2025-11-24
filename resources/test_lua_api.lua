-- test_lua_api.lua
-- Comprehensive Lua scripting API test

--@export health int 100
--@export speed float 5.5
--@export player_name string "Player"
--@export is_active bool true

-- Test counter
frame_count = 0
test_results = {}

function on_awake()
    Lupine.log_info("Lua: on_awake() called")
    table.insert(test_results, "on_awake")
    
    -- Test logging
    Lupine.log_info("Testing Lua logging functions")
    Lupine.log_warning("This is a warning")
    Lupine.log_debug("This is a debug message")
end

function on_ready()
    Lupine.log_info("Lua: on_ready() called")
    table.insert(test_results, "on_ready")
    
    -- Test utility functions
    local dt = Lupine.get_delta_time()
    Lupine.log_info("Delta time: " .. tostring(dt))
    
    local rand = Lupine.random_range(0.0, 100.0)
    Lupine.log_info("Random value: " .. tostring(rand))
    
    local rand_int = Lupine.random_range_int(1, 10)
    Lupine.log_info("Random int: " .. tostring(rand_int))
    
    -- Test math helpers
    local lerp_val = Lupine.lerp(0.0, 10.0, 0.5)
    Lupine.log_info("Lerp(0, 10, 0.5) = " .. tostring(lerp_val))
    
    local clamp_val = Lupine.clamp(15.0, 0.0, 10.0)
    Lupine.log_info("Clamp(15, 0, 10) = " .. tostring(clamp_val))
end

function on_input(delta)
    -- Test input functions
    if Lupine.is_action_pressed("jump") then
        Lupine.log_info("Lua: Jump action pressed!")
    end
    
    if Lupine.is_action_just_pressed("fire") then
        Lupine.log_info("Lua: Fire action just pressed!")
    end
    
    local axis_val = Lupine.get_axis("horizontal")
    if axis_val ~= 0.0 then
        Lupine.log_info("Lua: Horizontal axis: " .. tostring(axis_val))
    end
    
    -- Test raw input
    if Lupine.is_key_pressed(32) then  -- Space key
        Lupine.log_info("Lua: Space key pressed!")
    end
    
    if Lupine.is_mouse_button_pressed(0) then  -- Left mouse button
        Lupine.log_info("Lua: Left mouse button pressed!")
    end
end

function on_process(delta)
    frame_count = frame_count + 1
    
    if frame_count == 1 then
        Lupine.log_info("Lua: on_process() called - frame " .. tostring(frame_count))
        table.insert(test_results, "on_process")
        
        -- Test node functions
        local child_count = Lupine.get_child_count()
        Lupine.log_info("Child count: " .. tostring(child_count))
    end
    
    -- Test export variables
    if frame_count == 2 then
        Lupine.log_info("Health: " .. tostring(health))
        Lupine.log_info("Speed: " .. tostring(speed))
        Lupine.log_info("Player name: " .. player_name)
        Lupine.log_info("Is active: " .. tostring(is_active))
    end
    
    -- Test audio functions
    if frame_count == 3 then
        local volume = Lupine.get_bus_volume("Master")
        Lupine.log_info("Master bus volume: " .. tostring(volume))
        
        Lupine.set_bus_volume("SFX", 0.8)
        Lupine.log_info("Set SFX bus volume to 0.8")
        
        Lupine.set_bus_muted("Music", true)
        Lupine.log_info("Muted Music bus")
    end
    
    -- Modify export variables
    if frame_count == 4 then
        health = health - 10
        speed = speed + 0.5
        player_name = "Hero"
        is_active = not is_active
        
        Lupine.log_info("Modified export variables")
    end
end

function on_physics_process(delta)
    if frame_count == 1 then
        Lupine.log_info("Lua: on_physics_process() called")
        table.insert(test_results, "on_physics_process")
    end
end

function on_render()
    if frame_count == 1 then
        Lupine.log_info("Lua: on_render() called")
        table.insert(test_results, "on_render")
    end
end

function on_destroy()
    Lupine.log_info("Lua: on_destroy() called")
    table.insert(test_results, "on_destroy")
    
    -- Print test summary
    Lupine.log_info("=== Lua Test Summary ===")
    Lupine.log_info("Total frames: " .. tostring(frame_count))
    Lupine.log_info("Lifecycle hooks called: " .. tostring(#test_results))
    for i, result in ipairs(test_results) do
        Lupine.log_info("  " .. tostring(i) .. ". " .. result)
    end
    Lupine.log_info("Final health: " .. tostring(health))
    Lupine.log_info("Final speed: " .. tostring(speed))
    Lupine.log_info("Final player_name: " .. player_name)
    Lupine.log_info("Final is_active: " .. tostring(is_active))
    Lupine.log_info("=== Lua Test Complete ===")
end

