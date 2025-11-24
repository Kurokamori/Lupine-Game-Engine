"""
Example usage of EditorBridge for PyQt-based editor

This demonstrates how to:
1. Initialize the EditorBridge with a graphics backend
2. Create render views for PyQt widgets
3. Load and manage scenes
4. Discover and create nodes/components
5. Edit node properties
6. Save changes
"""

import sys
from PyQt6.QtWidgets import QApplication, QMainWindow, QWidget
from PyQt6.QtCore import QTimer
import lupine_engine as le

class EditorWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Lupine Editor - EditorBridge Example")
        self.setGeometry(100, 100, 1280, 720)
        
        # Create editor session
        self.session = le.EditorSession()
        self.session.initialize()
        
        # Get editor bridge
        self.bridge = self.session.get_editor_bridge()
        
        # Initialize rendering with OpenGL backend
        self.bridge.initialize(le.GraphicsBackend.OpenGL)
        
        # Create a render widget (placeholder for now)
        self.render_widget = QWidget(self)
        self.setCentralWidget(self.render_widget)
        
        # Create render view for the widget
        window_handle = int(self.render_widget.winId())
        self.view_id = self.bridge.create_render_view(
            window_handle, 
            self.render_widget.width(), 
            self.render_widget.height()
        )
        
        print(f"Created render view: {self.view_id}")
        
        # Setup render timer
        self.render_timer = QTimer()
        self.render_timer.timeout.connect(self.render_frame)
        self.render_timer.start(16)  # ~60 FPS
        
        # Example: Create a new scene
        self.setup_example_scene()
    
    def setup_example_scene(self):
        """Create an example scene with nodes and components"""
        
        # Create a new scene (or load existing)
        # For now, we'll create nodes programmatically
        
        # Discover available types
        print("\n=== Available Node Types ===")
        node_types = self.bridge.get_node_types()
        for type_info in node_types:
            print(f"  - {type_info.type_name} (built-in: {type_info.is_built_in})")
        
        print("\n=== Available Component Types ===")
        component_types = self.bridge.get_component_types()
        for type_info in component_types:
            print(f"  - {type_info.type_name} (built-in: {type_info.is_built_in})")
        
        # Create a root node
        root = self.bridge.create_node("Node3D", "SceneRoot")
        if root:
            print(f"\nCreated root node: {root.get_name()}")
            
            # Create a child node
            child = self.bridge.create_node("Node3D", "ChildNode")
            if child:
                self.bridge.add_node(child, root)
                print(f"Added child node: {child.get_name()}")
                
                # Create and add a component
                component = self.bridge.create_component("Component")
                if component:
                    self.bridge.add_component(child, component)
                    print(f"Added component to child node")
                
                # Get node properties
                props_json = self.bridge.get_node_properties(child)
                print(f"\nChild node properties: {props_json}")
        
        # Check if scene is dirty
        if self.bridge.is_active_scene_dirty():
            print("\nScene has unsaved changes")
    
    def render_frame(self):
        """Render a frame"""
        if self.view_id:
            self.bridge.render_view(self.view_id)
    
    def resizeEvent(self, event):
        """Handle window resize"""
        super().resizeEvent(event)
        if hasattr(self, 'view_id') and self.view_id:
            self.bridge.resize_render_view(
                self.view_id,
                self.render_widget.width(),
                self.render_widget.height()
            )
    
    def closeEvent(self, event):
        """Clean up on close"""
        self.render_timer.stop()
        
        # Destroy render view
        if hasattr(self, 'view_id') and self.view_id:
            self.bridge.destroy_render_view(self.view_id)
        
        # Shutdown bridge and session
        self.bridge.shutdown()
        self.session.shutdown()
        
        super().closeEvent(event)

def main():
    app = QApplication(sys.argv)
    window = EditorWindow()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()

