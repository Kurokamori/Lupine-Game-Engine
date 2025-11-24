-- Test Lua Script Component
-- This script demonstrates export properties and lifecycle hooks

-- Export properties - these will be exposed in the editor
--@export health int 100
--@export speed float 5.0
--@export player_name string "Player"
--@export is_active bool true

-- Script initialization
print("Lua script loaded!")

-- Lifecycle hooks
function on_awake()
    -- Called when the component is first initialized
    print("Lua on_awake called! Player: " .. player_name .. ", Health: " .. health)
end

function on_ready()
    -- Called when the node is ready
    print("Lua on_ready called! Speed: " .. speed .. ", Active: " .. tostring(is_active))
end

function on_process(delta_time)
    -- Called every frame
    -- Simulate health decay
    health = math.max(0, health - 1)
    if health % 20 == 0 then
        print("Lua on_process: Health = " .. health .. ", Delta = " .. delta_time)
    end
end

function on_physics_process(delta_time)
    -- Called every physics frame
end

function on_input(delta_time)
    -- Called for input handling
end

function on_render()
    -- Called during render
end

function on_destroy()
    -- Called when component is destroyed
    print("Lua on_destroy called!")
end

-- Custom functions
function take_damage(amount)
    -- Custom function to take damage
    health = math.max(0, health - amount)
    print("Player took " .. amount .. " damage! Health: " .. health)
    return health
end

function heal(amount)
    -- Custom function to heal
    health = math.min(200, health + amount)
    print("Player healed " .. amount .. "! Health: " .. health)
    return health
end
