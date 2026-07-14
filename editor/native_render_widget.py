"""
Native Render Widget

A QWidget that hosts a foreign (engine-owned) rendering surface through its
native window handle (winId()). The engine draws into this window with its own
graphics API (D3D11/D3D12/Vulkan/OpenGL), so Qt must never paint or erase the
surface itself.

A plain QWidget does not know this. On every paint or erase event Qt fills the
widget with its background brush. When that brush is an opaque colour (the black
the viewport uses) the surface flashes to that colour for one Qt frame before the
engine's render timer redraws it. That single-frame fill is exactly the "flicker
to black" seen when the viewport is resized or when the animation timeline is
scrubbed (scrubbing calls QWidget.update() on the viewport, which schedules a Qt
repaint of this surface).

Setting the attributes below hands the surface fully to the engine:

* WA_NativeWindow      - guarantees a real native child window backs the widget.
* WA_PaintOnScreen     - the widget paints directly to screen with a non-Qt
                         engine; Qt does not back it with its own buffer.
* WA_NoSystemBackground- Qt does not erase the background on paint/resize.
* WA_OpaquePaintEvent  - the surface is fully opaque, so Qt never needs to clear
                         it before painting.

paintEngine() returning None tells Qt this widget has no Qt paint engine, which
(together with WA_PaintOnScreen) stops Qt from compositing/clearing the surface.
An empty paintEvent() makes the contract explicit: all drawing is owned by the
engine. With these in place Qt never overwrites the engine's output, so the
black flicker disappears on resize, update(), and sibling repaints alike.

Pre-render appearance
---------------------
Because Qt no longer paints the surface, the bare native window shows whatever
Windows uses to erase it. The Win32 default is a *white* brush, so the surface
would flash white at creation before the engine presents its first frame.

WM_ERASEBKGND is handled in two distinct regimes, tracked by `_rendered_once`:

* Before the first engine frame - fill the client area with `surface_color`, so
  initial show is the dark backdrop instead of white.
* After the first engine frame - suppress the erase entirely (claim it handled,
  paint nothing). The previously presented frame stays on screen (Windows/DWM
  keeps showing it, stretched, until the engine presents the new size), so a
  resize does NOT flash to the surface colour. Filling here would repaint the
  surface colour on every resize-erase, which for the black viewport is exactly
  the black flicker this whole widget exists to remove.

Call notify_rendered() once the engine has presented a frame to flip regimes.
"""

import sys
import ctypes
from ctypes import wintypes

from PyQt6.QtWidgets import QWidget
from PyQt6.QtCore import Qt


_IS_WINDOWS = sys.platform == "win32"
_WM_ERASEBKGND = 0x0014

if _IS_WINDOWS:
    _user32 = ctypes.windll.user32
    _gdi32 = ctypes.windll.gdi32

    _user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    _user32.GetClientRect.restype = wintypes.BOOL
    _user32.FillRect.argtypes = [wintypes.HDC, ctypes.POINTER(wintypes.RECT), wintypes.HBRUSH]
    _user32.FillRect.restype = ctypes.c_int
    _gdi32.CreateSolidBrush.argtypes = [wintypes.COLORREF]
    _gdi32.CreateSolidBrush.restype = wintypes.HBRUSH
    _gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
    _gdi32.DeleteObject.restype = wintypes.BOOL


class NativeRenderWidget(QWidget):
    """QWidget whose surface is rendered exclusively by the engine, not by Qt.

    `surface_color` is the RGB (0-255) colour Windows fills the surface with
    before/while the engine has not yet drawn it (initial show, resize-expose).
    Make it match the engine's clear colour so the pre-render backdrop is
    seamless rather than a white flash.
    """

    def __init__(self, parent: QWidget = None, surface_color: tuple = (0, 0, 0)):
        super().__init__(parent)

        r, g, b = surface_color
        self._surface_color = (int(r) & 0xFF, int(g) & 0xFF, int(b) & 0xFF)

        # False until the engine has presented its first frame. While False the
        # surface is filled with surface_color on erase (no white flash); once True
        # the erase is suppressed so a resize keeps the last frame instead of
        # flashing the surface colour. Flipped by notify_rendered().
        self._rendered_once = False

        self.setAttribute(Qt.WidgetAttribute.WA_NativeWindow, True)
        self.setAttribute(Qt.WidgetAttribute.WA_PaintOnScreen, True)
        self.setAttribute(Qt.WidgetAttribute.WA_NoSystemBackground, True)
        self.setAttribute(Qt.WidgetAttribute.WA_OpaquePaintEvent, True)
        self.setAutoFillBackground(False)

        # Force creation of the backing native window now so winId() is valid and
        # stable before the engine binds a swapchain to it.
        self.winId()

    def paintEngine(self):
        """No Qt paint engine: the engine owns this surface."""
        return None

    def paintEvent(self, event):
        """The engine draws every pixel; Qt must not paint this surface."""
        pass

    def notify_rendered(self):
        """Mark that the engine has presented at least one frame.

        After this, WM_ERASEBKGND is suppressed rather than filled, so resizing
        keeps the last presented frame on screen instead of flashing the surface
        colour. Call from the render loop right after a successful render_view().
        """
        self._rendered_once = True

    def nativeEvent(self, eventType, message):
        """Erase to the surface colour (not Windows' default white) before render.

        Handles WM_ERASEBKGND so the native window shows `surface_color` until the
        engine presents its first frame, and in any region newly exposed by a
        grow-resize. Returns unhandled for every other message.

        The window handle and device context come straight from the message
        struct (msg.hWnd / msg.wParam). This deliberately never calls winId()
        here: winId() can force native-window recreation, which would re-enter
        this handler and crash. The whole body is guarded so a native-call hiccup
        can never abort Qt during event dispatch.

        Unhandled messages return (False, 0) rather than forwarding
        super().nativeEvent()'s result. Returning the base QWidget.nativeEvent()
        tuple back through a Python override corrupts the stack under PyQt6
        (fail-fast 0xC0000409 crash); the base implementation does nothing and
        returns False anyway, so (False, 0) is the correct, safe equivalent.
        """
        try:
            if _IS_WINDOWS and bytes(eventType) == b"windows_generic_MSG":
                msg = wintypes.MSG.from_address(int(message))
                if msg.message == _WM_ERASEBKGND:
                    # Before the first frame: paint the surface colour (avoids the
                    # white default). After: claim the erase but paint nothing, so
                    # the last presented frame stays visible through a resize.
                    if not self._rendered_once:
                        self._erase_to_surface_color(msg.hWnd, wintypes.HDC(msg.wParam))
                    return True, 1
        except Exception:
            pass
        return False, 0

    def _erase_to_surface_color(self, hwnd, hdc):
        """Fill the window's client rect with `surface_color` via GDI."""
        rect = wintypes.RECT()
        if not _user32.GetClientRect(hwnd, ctypes.byref(rect)):
            return
        r, g, b = self._surface_color
        colorref = wintypes.COLORREF(r | (g << 8) | (b << 16))
        brush = _gdi32.CreateSolidBrush(colorref)
        if not brush:
            return
        try:
            _user32.FillRect(hdc, ctypes.byref(rect), brush)
        finally:
            _gdi32.DeleteObject(brush)
