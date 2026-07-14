"""
AssetFileWatcher - Monitors project directories for file changes and imports assets

This module integrates with the AssetDatabase to automatically:
- Import new assets (create .meta files with UUIDs)
- Update existing assets on modification
- Handle asset renames and moves
- Clean up orphaned .meta files on deletion
"""

from pathlib import Path
from typing import Optional, Set, Dict, Callable, List
from PyQt6.QtCore import QObject, QTimer, pyqtSignal, Qt, QFileSystemWatcher
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QProgressBar, QFrame, QApplication
)
from PyQt6.QtGui import QFont

# Import theme manager
from theme import get_theme_manager


# Asset file extensions that should be imported
ASSET_EXTENSIONS = {
    # Images
    '.png', '.jpg', '.jpeg', '.bmp', '.tga', '.gif', '.webp', '.hdr',
    # Audio
    '.wav', '.mp3', '.ogg', '.flac',
    # 3D Models
    '.gltf', '.glb', '.fbx', '.obj', '.dae',
    # Fonts
    '.ttf', '.otf',
    # Animations
    '.spriteanimation',
    # Scripts (for change detection and reload)
    '.lua', '.py', '.rb',
    # Archetypes (definition schemas and instances)
    '.archetype', '.ares',
    # Scenes (for external-edit detection / editor reload; never imported)
    '.scene',
    # Other assets
    '.json', '.xml', '.yaml', '.yml',
}

# Extensions that are watched for change detection only and must NOT be passed to
# the AssetDatabase import pipeline (they are not importable assets). Scripts are
# reloaded by language hosts; scenes are reloaded by the editor session.
NO_IMPORT_EXTENSIONS = {'.py', '.lua', '.rb', '.scene'}

# Directories to skip during scanning
SKIP_DIRECTORIES = {
    '__pycache__', '.git', 'build', 'node_modules', '.vscode',
    '.idea', '.vs', 'cmake-build-debug', 'cmake-build-release'
}

# File patterns to skip
SKIP_PATTERNS = {
    '.meta',  # Meta files themselves
    '.lupine',  # Project files
    '.prefab',  # Prefab files
}


class AssetImportPopup(QFrame):
    """
    A popup overlay that shows asset import progress.
    Similar to Godot's reimport progress indicator.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("AssetImportPopup")
        self._setup_ui()
        self._current_assets: List[str] = []
        self._processed_count = 0
        self._total_count = 0

        # Auto-hide timer
        self._hide_timer = QTimer(self)
        self._hide_timer.setSingleShot(True)
        self._hide_timer.timeout.connect(self._fade_out)

    def _setup_ui(self):
        """Setup the popup UI"""
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint | Qt.WindowType.Tool)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, False)

        # Get theme colors
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        colors = theme.colors if theme else None

        # Use theme colors or fallback to defaults
        bg_color = colors.surface if colors else "#2d2d30"
        border_color = colors.border if colors else "#3e3e42"
        text_color = colors.text_primary if colors else "#cccccc"
        title_color = colors.text_on_accent if colors else "#ffffff"
        accent_color = colors.accent_color if colors else "#0e639c"
        secondary_text = colors.text_secondary if colors else "#9cdcfe"

        # Style the frame
        self.setStyleSheet(f"""
            QFrame#AssetImportPopup {{
                background-color: {bg_color};
                border: 1px solid {border_color};
                border-radius: 6px;
            }}
            QLabel {{
                color: {text_color};
            }}
            QLabel#TitleLabel {{
                color: {title_color};
                font-weight: bold;
            }}
            QLabel#StatusLabel {{
                color: {secondary_text};
                font-size: 11px;
            }}
            QProgressBar {{
                background-color: {border_color};
                border: none;
                border-radius: 3px;
                height: 6px;
            }}
            QProgressBar::chunk {{
                background-color: {accent_color};
                border-radius: 3px;
            }}
        """)

        # Store accent color for icon
        self._accent_color = accent_color

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(6)

        # Header with icon and title
        header_layout = QHBoxLayout()
        header_layout.setSpacing(8)

        # Icon label (using unicode symbol)
        icon_label = QLabel("⟳")
        icon_label.setFont(QFont("Segoe UI", 14))
        icon_label.setStyleSheet(f"color: {accent_color};")
        header_layout.addWidget(icon_label)

        # Title
        self._title_label = QLabel("Importing Assets...")
        self._title_label.setObjectName("TitleLabel")
        header_layout.addWidget(self._title_label)
        header_layout.addStretch()

        layout.addLayout(header_layout)

        # Progress bar
        self._progress_bar = QProgressBar()
        self._progress_bar.setMinimum(0)
        self._progress_bar.setMaximum(100)
        self._progress_bar.setValue(0)
        self._progress_bar.setTextVisible(False)
        self._progress_bar.setFixedHeight(6)
        layout.addWidget(self._progress_bar)

        # Status label
        self._status_label = QLabel("Scanning for changes...")
        self._status_label.setObjectName("StatusLabel")
        layout.addWidget(self._status_label)

        # Set fixed size
        self.setFixedWidth(280)
        self.adjustSize()

    def show_importing(self, total_assets: int = 0):
        """Show the popup when import starts"""
        self._hide_timer.stop()
        self._total_count = total_assets
        self._processed_count = 0

        if total_assets > 0:
            self._title_label.setText(f"Importing Assets ({total_assets})")
            self._progress_bar.setMaximum(total_assets)
        else:
            self._title_label.setText("Scanning for Changes...")
            self._progress_bar.setMaximum(0)  # Indeterminate

        self._status_label.setText("Scanning project directory...")
        self._progress_bar.setValue(0)

        self._position_popup()
        self.show()
        self.raise_()
        QApplication.processEvents()

    def update_progress(self, current: int, total: int, asset_path: str):
        """Update progress with current asset being processed"""
        self._processed_count = current
        self._total_count = total

        if total > 0:
            self._title_label.setText(f"Importing Assets ({current}/{total})")
            self._progress_bar.setMaximum(total)
            self._progress_bar.setValue(current)

        # Show just the filename
        filename = Path(asset_path).name if asset_path else ""
        self._status_label.setText(f"Processing: {filename}")
        QApplication.processEvents()

    def show_complete(self, imported_count: int):
        """Show completion message and auto-hide"""
        self._title_label.setText("Import Complete")
        self._progress_bar.setMaximum(100)
        self._progress_bar.setValue(100)

        if imported_count > 0:
            self._status_label.setText(f"✓ {imported_count} asset(s) imported")
        else:
            self._status_label.setText("✓ All assets up to date")

        # Auto-hide after 2 seconds
        self._hide_timer.start(2000)
        QApplication.processEvents()

    def show_error(self, message: str):
        """Show an error message"""
        self._hide_timer.stop()
        self._title_label.setText("Import Error")
        self._status_label.setText(f"✗ {message}")
        self._progress_bar.setValue(0)

        # Auto-hide after 4 seconds for errors
        self._hide_timer.start(4000)
        QApplication.processEvents()

    def _fade_out(self):
        """Hide the popup"""
        self.hide()

    def _position_popup(self):
        """Position the popup in the bottom-right corner of the parent or screen"""
        parent = self.parent()
        if parent and hasattr(parent, 'geometry'):
            parent_rect = parent.geometry()
            x = parent_rect.right() - self.width() - 20
            y = parent_rect.bottom() - self.height() - 40
            self.move(x, y)
        else:
            # Position relative to primary screen
            screen = QApplication.primaryScreen()
            if screen:
                screen_rect = screen.availableGeometry()
                x = screen_rect.right() - self.width() - 20
                y = screen_rect.bottom() - self.height() - 40
                self.move(x, y)


class AssetFileWatcher(QObject):
    """
    Watches project directories for file changes and triggers asset imports.

    Signals:
        asset_imported(str): Emitted when an asset is imported (path)
        asset_updated(str): Emitted when an asset is updated (path)
        asset_deleted(str): Emitted when an asset is deleted (path)
        scan_started(): Emitted when scanning/importing begins
        scan_progress(int, int, str): Emitted during scan (current, total, path)
        scan_complete(): Emitted when a full scan completes
    """

    asset_imported = pyqtSignal(str)  # path
    asset_updated = pyqtSignal(str)   # path
    asset_deleted = pyqtSignal(str)   # path
    scan_started = pyqtSignal()
    scan_progress = pyqtSignal(int, int, str)  # current, total, path
    scan_complete = pyqtSignal()

    def __init__(self, project_root: str, parent=None, show_popup: bool = True):
        super().__init__(parent)

        self.project_root = Path(project_root)
        self.watcher = QFileSystemWatcher()
        self.pending_changes: Set[str] = set()
        self.debounce_timer = QTimer()
        self.debounce_timer.setSingleShot(True)
        self.debounce_timer.timeout.connect(self._process_pending_changes)
        self.debounce_delay = 500  # ms

        # Track known assets for detecting new/deleted files
        self.known_assets: Dict[str, float] = {}  # path -> mtime

        # Asset import callback - set by editor to call C++ AssetDatabase
        self._import_callback: Optional[Callable[[str], bool]] = None
        self._delete_callback: Optional[Callable[[str], bool]] = None

        # Import popup
        self._show_popup = show_popup
        self._popup: Optional[AssetImportPopup] = None
        if show_popup and parent:
            self._popup = AssetImportPopup(parent)

        # Connect our own signals to popup
        if self._popup:
            self.scan_started.connect(lambda: self._popup.show_importing())
            self.scan_progress.connect(self._popup.update_progress)

    def set_import_callback(self, callback: Callable[[str], bool]):
        """Set callback for importing assets (should call AssetDatabase.ImportAsset)"""
        self._import_callback = callback

    def set_delete_callback(self, callback: Callable[[str], bool]):
        """Set callback for handling deleted assets"""
        self._delete_callback = callback

    def set_popup_parent(self, parent: QWidget):
        """Set or change the popup's parent widget"""
        if self._show_popup:
            if self._popup:
                self._popup.setParent(parent)
            else:
                self._popup = AssetImportPopup(parent)
                self.scan_started.connect(lambda: self._popup.show_importing())
                self.scan_progress.connect(self._popup.update_progress)

    def start(self):
        """Start watching project directories"""
        print(f"[FileWatcher] Starting file watcher for project: {self.project_root}")
        self._setup_watchers()
        self.watcher.directoryChanged.connect(self._on_directory_changed)
        self.watcher.fileChanged.connect(self._on_file_changed)
        print(f"[FileWatcher] Signals connected, starting initial scan...")
        # Initial scan
        self.scan_all()
        print(f"[FileWatcher] Initial scan complete, watcher is active")

    def check_for_changes(self):
        """Manually check for file changes - useful when QFileSystemWatcher misses events"""
        if not self.project_root or not self.project_root.exists():
            return

        print(f"[FileWatcher] Manual check for changes triggered")

        # Check all known assets for modifications
        changes_found = []

        for file_path in self.project_root.rglob('*'):
            if not file_path.is_file():
                continue
            if not self._should_import(file_path):
                continue

            str_path = str(file_path)
            current_mtime = file_path.stat().st_mtime
            known_mtime = self.known_assets.get(str_path, 0)

            if str_path not in self.known_assets:
                # New file
                print(f"[FileWatcher] Manual check: New file {str_path}")
                self.known_assets[str_path] = current_mtime
                self.asset_imported.emit(str_path)
                changes_found.append(str_path)
            elif current_mtime > known_mtime:
                # Modified file
                print(f"[FileWatcher] Manual check: Modified {str_path}")
                self.known_assets[str_path] = current_mtime
                self.asset_updated.emit(str_path)
                changes_found.append(str_path)

        if changes_found:
            print(f"[FileWatcher] Manual check found {len(changes_found)} changes")
        else:
            print(f"[FileWatcher] Manual check: No changes detected")
        
    def stop(self):
        """Stop watching and cleanup all resources"""
        # Stop timers
        self.debounce_timer.stop()

        # Disconnect signals (may fail if not connected)
        try:
            self.watcher.directoryChanged.disconnect()
        except (TypeError, RuntimeError):
            pass
        try:
            self.watcher.fileChanged.disconnect()
        except (TypeError, RuntimeError):
            pass

        # Remove all watched paths
        dirs = self.watcher.directories()
        if dirs:
            self.watcher.removePaths(dirs)
        files = self.watcher.files()
        if files:
            self.watcher.removePaths(files)

        # Clear tracked data to free memory
        self.pending_changes.clear()
        self.known_assets.clear()

        # Cleanup popup
        if self._popup:
            self._popup._hide_timer.stop()
            self._popup.close()
            self._popup = None

        # Clear callbacks to break reference cycles
        self._import_callback = None
        self._delete_callback = None
        
    def scan_all(self, silent: bool = False):
        """Perform a full scan and import only assets that need reimporting"""
        if not self.project_root.exists():
            return

        # Try to get AssetDatabase for checking if assets need reimport
        asset_db = None
        try:
            import lupine_engine as le
            asset_db = le.AssetDatabase.get_instance()
            if not asset_db.is_initialized():
                asset_db = None
        except Exception:
            pass

        # Collect all assets first, then filter to only those needing import
        all_assets = list(self._find_all_assets())

        # Filter to only assets that need reimporting
        # Scripts and scenes are tracked for change detection but not imported
        assets_to_import = []
        for asset_path in all_assets:
            path_str = str(asset_path)
            is_no_import = asset_path.suffix.lower() in NO_IMPORT_EXTENSIONS

            # Always track the file with its mtime for change detection
            if path_str not in self.known_assets and asset_path.exists():
                self.known_assets[path_str] = asset_path.stat().st_mtime

            # Scripts and scenes don't need importing, only change detection
            if is_no_import:
                continue

            needs_import = True
            if asset_db:
                try:
                    needs_import = asset_db.needs_reimport(path_str)
                except Exception:
                    needs_import = True

            if needs_import:
                assets_to_import.append(asset_path)

        total = len(assets_to_import)
        imported_count = 0

        if total > 0 and not silent:
            self.scan_started.emit()
            if self._popup:
                self._popup.show_importing(total)

        for i, asset_path in enumerate(assets_to_import):
            if not silent:
                self.scan_progress.emit(i + 1, total, str(asset_path))

            if self._import_asset(str(asset_path)):
                imported_count += 1

        if self._popup and not silent:
            if imported_count > 0:
                self._popup.show_complete(imported_count)
            else:
                # No assets needed importing, hide popup quickly
                self._popup.hide()

        self.scan_complete.emit()
        
    def _setup_watchers(self):
        """Setup file system watchers for project directories"""
        if not self.project_root.exists():
            print(f"[FileWatcher] Project root does not exist: {self.project_root}")
            return

        dirs_to_watch = [str(self.project_root)]

        for subdir in self.project_root.rglob('*'):
            if subdir.is_dir() and subdir.name not in SKIP_DIRECTORIES:
                dirs_to_watch.append(str(subdir))

        print(f"[FileWatcher] Setting up watchers for {len(dirs_to_watch)} directories")
        for d in dirs_to_watch:
            print(f"[FileWatcher]   Watching: {d}")

        failed = self.watcher.addPaths(dirs_to_watch)
        if failed:
            print(f"[FileWatcher] WARNING: Failed to add {len(failed)} paths: {failed}")
        else:
            print(f"[FileWatcher] All {len(dirs_to_watch)} paths added successfully")

        # Verify what's being watched
        watched_dirs = self.watcher.directories()
        print(f"[FileWatcher] Currently watching {len(watched_dirs)} directories")
        
    def _find_all_assets(self):
        """Find all importable asset files in the project"""
        for file_path in self.project_root.rglob('*'):
            if self._should_import(file_path):
                yield file_path
                
    def _should_import(self, path: Path) -> bool:
        """Check if a file should be imported as an asset"""
        if not path.is_file():
            return False

        # Check extension
        suffix = path.suffix.lower()
        if suffix not in ASSET_EXTENSIONS:
            return False

        # Skip certain patterns
        if suffix in SKIP_PATTERNS:
            return False

        # Skip files in skipped directories
        for part in path.parts:
            if part in SKIP_DIRECTORIES:
                return False

        return True

    def _on_directory_changed(self, directory: str):
        """Handle directory change event"""
        print(f"[FileWatcher] Directory changed: {directory}")
        self.pending_changes.add(directory)
        self.debounce_timer.stop()
        self.debounce_timer.start(self.debounce_delay)

    def _on_file_changed(self, file_path: str):
        """Handle file change event"""
        print(f"[FileWatcher] File changed: {file_path}")
        self.pending_changes.add(file_path)
        self.debounce_timer.stop()
        self.debounce_timer.start(self.debounce_delay)

    def _process_pending_changes(self):
        """Process all pending changes after debounce period"""
        changes = self.pending_changes.copy()
        self.pending_changes.clear()

        print(f"[FileWatcher] Processing {len(changes)} pending changes: {changes}")

        if not changes:
            return

        # Count how many actual asset changes we have
        asset_changes = []
        for change in changes:
            path = Path(change)
            print(f"[FileWatcher] Checking change: {change}, is_dir={path.is_dir()}, is_file={path.is_file()}")
            if path.is_dir():
                # Collect assets from directory - recursively check for new or modified files
                if path.exists():
                    # Use rglob to check all files recursively, not just direct children
                    for file_path in path.rglob('*'):
                        if not file_path.is_file():
                            continue
                        if self._should_import(file_path):
                            str_path = str(file_path)
                            current_mtime = file_path.stat().st_mtime if file_path.exists() else 0
                            known_mtime = self.known_assets.get(str_path, 0)

                            if str_path not in self.known_assets:
                                # New file
                                print(f"[FileWatcher] New file detected: {str_path}")
                                asset_changes.append(('new', str_path))
                            elif current_mtime > known_mtime:
                                # Modified file
                                print(f"[FileWatcher] Modified file detected: {str_path} (mtime: {known_mtime} -> {current_mtime})")
                                asset_changes.append(('modified', str_path))
                            else:
                                print(f"[FileWatcher] File unchanged: {str_path} (mtime: {current_mtime})")
            elif path.is_file() and self._should_import(path):
                print(f"[FileWatcher] Direct file change: {path}")
                asset_changes.append(('file', str(path)))
            elif not path.exists():
                asset_changes.append(('deleted', str(path)))

        print(f"[FileWatcher] Total asset changes: {len(asset_changes)}")
        if not asset_changes:
            return

        # Show popup for batch processing (only for importable assets to avoid spam)
        total = len(asset_changes)
        importable_changes = [c for c in asset_changes if Path(c[1]).suffix.lower() not in NO_IMPORT_EXTENSIONS]

        if self._popup and len(importable_changes) > 0:
            self._popup.show_importing(len(importable_changes))

        imported_count = 0
        for i, (change_type, change_path) in enumerate(asset_changes):
            path = Path(change_path)
            is_no_import = path.suffix.lower() in NO_IMPORT_EXTENSIONS

            if self._popup and not is_no_import:
                self._popup.update_progress(i + 1, total, change_path)

            if change_type == 'deleted':
                self._handle_deleted_asset(change_path)
            elif change_type == 'new':
                # New file from directory scan: import unless it is change-only
                # (scripts/scenes), but always track and announce it.
                if is_no_import:
                    if path.exists():
                        self.known_assets[change_path] = path.stat().st_mtime
                elif self._import_asset(change_path):
                    imported_count += 1
                self.asset_imported.emit(change_path)
            elif change_type == 'modified':
                # Modified file - update mtime and emit signal
                if path.exists():
                    self.known_assets[change_path] = path.stat().st_mtime
                self.asset_updated.emit(change_path)
            else:
                # File changed/updated (direct file change)
                if is_no_import:
                    if path.exists():
                        self.known_assets[change_path] = path.stat().st_mtime
                elif self._import_asset(change_path):
                    imported_count += 1
                self.asset_updated.emit(change_path)

        if self._popup and len(importable_changes) > 0:
            self._popup.show_complete(imported_count)

    def _scan_directory(self, directory: Path):
        """Scan a directory for new assets"""
        if not directory.exists():
            return

        for file_path in directory.iterdir():
            if self._should_import(file_path):
                str_path = str(file_path)
                if str_path not in self.known_assets:
                    # New asset
                    self._import_asset(str_path)
                    self.asset_imported.emit(str_path)

    def _import_asset(self, path: str) -> bool:
        """Import an asset using the callback. Returns True if newly imported."""
        if self._import_callback:
            try:
                success = self._import_callback(path)
                if success:
                    mtime = Path(path).stat().st_mtime if Path(path).exists() else 0
                    was_new = path not in self.known_assets
                    self.known_assets[path] = mtime
                    return was_new
            except Exception as e:
                print(f"Failed to import asset {path}: {e}")
                if self._popup:
                    self._popup.show_error(f"Failed: {Path(path).name}")
                return False
        else:
            # Just track it without importing
            if Path(path).exists():
                was_new = path not in self.known_assets
                self.known_assets[path] = Path(path).stat().st_mtime
                return was_new
        return False

    def _handle_deleted_asset(self, path: str):
        """Handle a deleted asset"""
        if path in self.known_assets:
            del self.known_assets[path]

        if self._delete_callback:
            try:
                self._delete_callback(path)
            except Exception as e:
                print(f"Failed to handle deleted asset {path}: {e}")

        self.asset_deleted.emit(path)

