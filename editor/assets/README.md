# Editor Assets

## Icon Packs

Icon packs are organized in subfolders under `icons/`. Each icon pack folder should contain 8 PNG files for the scene tree view and enable columns:

### Required Icons (20x20px recommended)

**Visibility States:**
- `visible.png` - Node is visible and parent is visible
- `invisible.png` - Node is invisible and parent is visible
- `visible-parent-invisible.png` - Node itself is visible but parent is invisible
- `invisible-parent-invisible.png` - Node is invisible and parent is invisible

**Enabled States:**
- `enabled.png` - Node is enabled and parent is enabled
- `disabled.png` - Node is disabled and parent is enabled
- `enabled-parent-disabled.png` - Node itself is enabled but parent is disabled
- `disabled-parent-disabled.png` - Node is disabled and parent is disabled

### Built-in Icon Packs

- `dark/` - Icons for dark themes
- `light/` - Icons for light themes

### Custom Icon Packs

You can create custom icon packs by:

1. Create a new folder under `icons/` (e.g., `icons/custom/`)
2. Add all 8 required PNG files to the folder
3. The icon pack will be automatically detected by the editor
4. Select it in the Theme Editor dialog (Editor → Project Settings → Editor tab)

### Registering Icon Packs Programmatically

```python
from theme import get_icon_pack_manager

icon_pack_manager = get_icon_pack_manager()
icon_pack_manager.register_icon_pack("My Custom Pack", "/path/to/icon/folder")
```
