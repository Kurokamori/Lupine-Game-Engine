# =============================================================================
# Networking CMake Configuration for Lupine Engine
# =============================================================================
#
# Configures the optional networking / multiplayer subsystem. When enabled it
# defines LUPINE_HAS_NETWORKING and selects the native transport library (ENet,
# reliable UDP). The web (WebSocket) transport on Emscripten uses the browser's
# built-in WebSocket API (emscripten/websocket.h) and needs no extra package.
#
# The subsystem is designed so a build with networking DISABLED still compiles
# and links fully: the manager + in-process loopback transport are pure C++ and
# the ENet/WebSocket transports degrade to no-ops. Non-networked games are never
# affected - there is zero runtime cost until a session is started.
#
# Set LUPINE_ENABLE_NETWORKING=OFF to drop the native transport dependency.
# =============================================================================

option(LUPINE_ENABLE_NETWORKING "Enable the networking/multiplayer subsystem" ON)

set(LUPINE_HAS_NETWORKING FALSE)
set(LUPINE_HAS_WEBSOCKET FALSE)
set(LUPINE_ENET_TARGET "" CACHE INTERNAL "Resolved ENet link target")
set(LUPINE_WS_TARGET "" CACHE INTERNAL "Resolved WebSocket link target")

if(LUPINE_ENABLE_NETWORKING)
    if(EMSCRIPTEN)
        # Browser WebSocket transport uses emscripten/websocket.h (no package).
        set(LUPINE_HAS_NETWORKING TRUE)
        set(LUPINE_HAS_WEBSOCKET TRUE)
        add_definitions(-DLUPINE_HAS_NETWORKING)
        message(STATUS "Networking: enabled (Emscripten WebSocket client)")
    else()
        # vcpkg ships ENet as the 'unofficial-enet' package exporting the
        # 'unofficial::enet::enet' target.
        find_package(unofficial-enet CONFIG QUIET)
        if(unofficial-enet_FOUND)
            set(LUPINE_HAS_NETWORKING TRUE)
            set(LUPINE_ENET_TARGET unofficial::enet::enet CACHE INTERNAL "Resolved ENet link target")
            add_definitions(-DLUPINE_HAS_NETWORKING)
            add_definitions(-DLUPINE_HAS_ENET)
            message(STATUS "Networking: enabled (ENet)")
        else()
            message(WARNING "Networking: ENet (unofficial-enet) not found - networking disabled")
        endif()

        # Optional native WebSocket transport (browser cross-play). When absent,
        # the WebSocket transport degrades to a no-op; ENet still works.
        find_package(ixwebsocket CONFIG QUIET)
        if(ixwebsocket_FOUND AND LUPINE_HAS_NETWORKING)
            set(LUPINE_HAS_WEBSOCKET TRUE)
            set(LUPINE_WS_TARGET ixwebsocket::ixwebsocket CACHE INTERNAL "Resolved WebSocket link target")
            add_definitions(-DLUPINE_HAS_WEBSOCKET)
            message(STATUS "Networking: native WebSocket transport enabled (ixwebsocket)")
        else()
            message(STATUS "Networking: native WebSocket transport disabled (ixwebsocket not found)")
        endif()
    endif()
else()
    message(STATUS "Networking: disabled by LUPINE_ENABLE_NETWORKING=OFF")
endif()
