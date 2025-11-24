# Test Python Script Component
# This script demonstrates export properties and lifecycle hooks

# Export properties - these will be exposed in the editor
#@export health int 100
#@export speed float 5.0
#@export player_name str "Player"
#@export is_active bool True

# Script initialization
health = 100
speed = 5.0
player_name = "Player"
is_active = True

# Note: print() is disabled in embedded Python to avoid stdout issues
# Use the engine's logging system instead when available

# Lifecycle hooks
def on_awake():
    """Called when the component is first initialized"""
    pass  # Python on_awake called

def on_ready():
    """Called when the node is ready"""
    pass  # Python on_ready called

def on_process(delta_time):
    """Called every frame"""
    global health
    # Simulate health decay
    health = max(0, health - 1)

def on_physics_process(delta_time):
    """Called every physics frame"""
    pass

def on_input(delta_time):
    """Called for input handling"""
    pass

def on_render():
    """Called during render"""
    pass

def on_destroy():
    """Called when component is destroyed"""
    pass  # Python on_destroy called

# Custom function
def take_damage(amount):
    """Custom function to take damage"""
    global health
    health = max(0, health - amount)
    return health

def heal(amount):
    """Custom function to heal"""
    global health
    health = min(200, health + amount)
    return health
