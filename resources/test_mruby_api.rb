# test_mruby_api.rb
# Comprehensive mRuby scripting API test

#@export health int 100
#@export speed float 5.5
#@export player_name string "Player"
#@export is_active bool true

# Test counter
$frame_count = 0
$test_results = []

def on_awake
    Lupine.log_info("mRuby: on_awake() called")
    $test_results << "on_awake"
    
    # Test logging
    Lupine.log_info("Testing mRuby logging functions")
    Lupine.log_warning("This is a warning")
    Lupine.log_error("This is an error (test)")
end

def on_ready
    Lupine.log_info("mRuby: on_ready() called")
    $test_results << "on_ready"
    
    # Test utility functions
    dt = Lupine.get_delta_time()
    Lupine.log_info("Delta time: #{dt}")
    
    rand = Lupine.random_range(0.0, 100.0)
    Lupine.log_info("Random value: #{rand}")
    
    # Test child count
    child_count = Lupine.get_child_count()
    Lupine.log_info("Child count: #{child_count}")
end

def on_input(delta)
    # Test input functions
    if Lupine.is_action_pressed("jump")
        Lupine.log_info("mRuby: Jump action pressed!")
    end
    
    if Lupine.is_action_just_pressed("fire")
        Lupine.log_info("mRuby: Fire action just pressed!")
    end
    
    axis_val = Lupine.get_axis("horizontal")
    if axis_val != 0.0
        Lupine.log_info("mRuby: Horizontal axis: #{axis_val}")
    end
end

def on_process(delta)
    $frame_count += 1
    
    if $frame_count == 1
        Lupine.log_info("mRuby: on_process() called - frame #{$frame_count}")
        $test_results << "on_process"
    end
    
    # Test export variables
    if $frame_count == 2
        Lupine.log_info("Health: #{$health}")
        Lupine.log_info("Speed: #{$speed}")
        Lupine.log_info("Player name: #{$player_name}")
        Lupine.log_info("Is active: #{$is_active}")
    end
    
    # Test audio functions
    if $frame_count == 3
        volume = Lupine.get_bus_volume("Master")
        Lupine.log_info("Master bus volume: #{volume}")
        
        Lupine.set_bus_volume("SFX", 0.8)
        Lupine.log_info("Set SFX bus volume to 0.8")
    end
    
    # Modify export variables
    if $frame_count == 4
        $health -= 10
        $speed += 0.5
        $player_name = "Hero"
        $is_active = !$is_active
        
        Lupine.log_info("Modified export variables")
    end
end

def on_physics_process(delta)
    if $frame_count == 1
        Lupine.log_info("mRuby: on_physics_process() called")
        $test_results << "on_physics_process"
    end
end

def on_render
    if $frame_count == 1
        Lupine.log_info("mRuby: on_render() called")
        $test_results << "on_render"
    end
end

def on_destroy
    Lupine.log_info("mRuby: on_destroy() called")
    $test_results << "on_destroy"
    
    # Print test summary
    Lupine.log_info("=== mRuby Test Summary ===")
    Lupine.log_info("Total frames: #{$frame_count}")
    Lupine.log_info("Lifecycle hooks called: #{$test_results.length}")
    $test_results.each_with_index do |result, i|
        Lupine.log_info("  #{i + 1}. #{result}")
    end
    Lupine.log_info("Final health: #{$health}")
    Lupine.log_info("Final speed: #{$speed}")
    Lupine.log_info("Final player_name: #{$player_name}")
    Lupine.log_info("Final is_active: #{$is_active}")
    Lupine.log_info("=== mRuby Test Complete ===")
end

