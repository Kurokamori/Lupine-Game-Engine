"""
Base Editor Panel Class
All editor panels inherit from this class
"""

from PyQt6.QtWidgets import QDockWidget, QWidget, QVBoxLayout
from PyQt6.QtCore import Qt, pyqtSignal


class EditorPanel(QDockWidget):
    """Base class for all editor panels"""
    
    # Signals
    panel_closed = pyqtSignal(str)  # Emits panel ID when closed
    panel_state_changed = pyqtSignal(str, dict)  # Emits panel ID and state data
    
    def __init__(self, title: str, parent=None):
        super().__init__(title, parent)
        self.setAllowedAreas(Qt.DockWidgetArea.AllDockWidgetAreas)
        self.setFeatures(
            QDockWidget.DockWidgetFeature.DockWidgetMovable |
            QDockWidget.DockWidgetFeature.DockWidgetFloatable |
            QDockWidget.DockWidgetFeature.DockWidgetClosable
        )
        
        # Allow the panel to be a top-level window
        self.setFloating(False)  # Start docked
        self.topLevelChanged.connect(self._on_top_level_changed)
        
        # Create content widget
        self.content_widget = QWidget()
        self.content_layout = QVBoxLayout()
        self.content_layout.setContentsMargins(0, 0, 0, 0)
        self.content_widget.setLayout(self.content_layout)
        self.setWidget(self.content_widget)
        
        # Setup panel-specific content
        self._setup_panel()
    
    def _on_top_level_changed(self, top_level: bool):
        """Handle when panel becomes floating/docked"""
        # Don't manually set window flags - let Qt handle docking behavior
        # This allows panels to be properly re-docked after floating
        pass
    
    def _setup_panel(self):
        """Override this to setup panel content"""
        pass
    
    def get_panel_id(self) -> str:
        """Get unique identifier for this panel"""
        return self.__class__.__name__
    
    def get_state(self) -> dict:
        """
        Get the current state of this panel for serialization
        Override this to save panel-specific state
        """
        return {
            "id": self.get_panel_id(),
            "title": self.windowTitle(),
            "visible": self.isVisible(),
            "floating": self.isFloating(),
        }
    
    def set_state(self, state: dict):
        """
        Restore panel state from serialized data
        Override this to restore panel-specific state
        """
        if "visible" in state:
            self.setVisible(state["visible"])
        if "floating" in state:
            self.setFloating(state["floating"])
    
    def closeEvent(self, event):
        """Handle panel close event"""
        self.panel_closed.emit(self.get_panel_id())
        super().closeEvent(event)
