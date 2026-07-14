/**
 * @file lupine_extension_interface.h
 * @brief Lupine Engine - Native C++ Extension ABI ("LupineExtension")
 *
 * This is the stable C ABI boundary between the engine host (the runtime
 * executable, the console, or the editor's embedded engine module) and a
 * compiled native extension shipped as a .dll / .so / .dylib. It is the
 *
 * Why a C ABI: the engine's core (lupine_core) is a STATIC library, so each
 * host binary embeds its own copy of the engine singletons (TypeRegistry, ...).
 * A plugin therefore cannot statically link the engine and call C++ APIs
 * directly without mutating a different registry the host never sees. Instead,
 * at load time the host hands the plugin a table of C function pointers
 * (LupineHostInterface) bound to the *host's* live singletons, and the plugin
 * registers its component classes back through that table. Only C POD types,
 * opaque handles and UTF-8 JSON strings ever cross the boundary, which keeps it
 * ABI-stable across compilers, runtimes and engine versions.
 *
 * Plugin authors do not use this header directly; they subclass lupine::ext::
 * Component from the C++ SDK (lupine/ext/Extension.hpp), which wraps everything
 * declared here. This header is the contract both sides compile against.
 *
 * Dynamic value marshalling mirrors the engine's existing reflection bridge
 * (lc_reflection): scalars marshal as JSON scalars; Vec2/Vec3/Vec4 as
 * {"x","y",...}; Color as {"r","g","b","a"}; method arguments as a JSON array.
 * Property descriptors marshal as the JSON produced by
 * lupine::core::PropertyDescriptor::Serialize().
 *
 * Ownership of strings: any char* RETURNED to the plugin by a host function is
 * newly allocated and owned by the caller; release it with host->string_free.
 * Any const char* PASSED as an argument is borrowed for the duration of the
 * call only.
 */

#ifndef LUPINE_EXTENSION_INTERFACE_H
#define LUPINE_EXTENSION_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform / calling convention
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
    #define LUPINE_EXT_EXPORT __declspec(dllexport)
    #define LUPINE_EXT_IMPORT __declspec(dllimport)
    #define LUPINE_EXT_CALL   __cdecl
#else
    #define LUPINE_EXT_EXPORT __attribute__((visibility("default")))
    #define LUPINE_EXT_IMPORT
    #define LUPINE_EXT_CALL
#endif

/**
 * ABI version. Incremented on any breaking change to the structs or function
 * signatures below. The host refuses to load a plugin whose reported version
 * does not match.
 *
 * Version history:
 *   1 - Initial lifecycle/property/scene-access ABI.
 *   2 - Added the rendering bridge: LupineRenderContextHandle, the
 *       build_draw_commands / get_render_bounds vtable slots, the
 *       LUPINE_HOOK_DRAW flag, and the render_draw_* / debug_draw_* host
 *       functions. All additions are append-only at the end of their structs.
 *   3 - Added the debug_draw_rect_2d host function (editor-only 2D rectangle),
 *       completing parity with the scripting EditorDraw* family. Append-only.
 */
#define LUPINE_EXTENSION_ABI_VERSION 3u

/** Boolean marshalled across the ABI (0 = false, nonzero = true). */
typedef int32_t LupineBool;

/* ============================================================================
 * Opaque handles
 * ============================================================================ */

/** Handle to a host-side scene-tree node (lupine::core::Node). */
typedef struct LupineNodeOpaque*      LupineNodeHandle;
/** Handle to a host-side component (lupine::core::Component). For a native
 *  extension component this is its NativeComponentWrapper. */
typedef struct LupineComponentOpaque* LupineComponentHandle;
/** The plugin's own per-instance C++ object (created by the class vtable). */
typedef void* LupineInstance;
/** The plugin's optional top-level state, returned from lupine_extension_init. */
typedef void* LupineExtensionHandle;
/** Opaque registration context handed to the plugin at init time. */
typedef void* LupineRegistrationContext;

/**
 * Handle to the host-side render context (lupine::RenderContext) active during a
 * build_draw_commands call. It is the single render bridge: pass it to the
 * render_draw_* host functions to record runtime-visible draw geometry. The
 * handle is valid ONLY for the duration of the build_draw_commands call that
 * received it; never retain it past that call. Editor-only debug primitives use
 * the global debug_draw_* host functions instead and need no handle.
 */
typedef struct LupineRenderContextOpaque* LupineRenderContextHandle;

/** Forward declaration; the full definition appears further below. Declared here
 *  so the host interface can reference it by pointer. */
struct LupineComponentClassDesc;

/* ============================================================================
 * Log levels (match lupine::Logger severities)
 * ============================================================================ */

typedef enum LupineLogLevel {
    LUPINE_LOG_TRACE = 0,
    LUPINE_LOG_DEBUG = 1,
    LUPINE_LOG_INFO  = 2,
    LUPINE_LOG_WARN  = 3,
    LUPINE_LOG_ERROR = 4
} LupineLogLevel;

/* ============================================================================
 * Lifecycle hook flags
 *
 * A class advertises which hooks it implements so the host can skip the
 * cross-boundary call entirely for hooks the plugin does not use (important for
 * the per-frame hooks). A vtable slot whose flag is not set is never invoked
 * and may be NULL.
 * ============================================================================ */

typedef enum LupineHookFlags {
    LUPINE_HOOK_NONE             = 0,
    LUPINE_HOOK_AWAKE            = 1 << 0,
    LUPINE_HOOK_READY            = 1 << 1,
    LUPINE_HOOK_UPDATE           = 1 << 2,
    LUPINE_HOOK_PROCESS          = 1 << 3,
    LUPINE_HOOK_PHYSICS_PROCESS  = 1 << 4,
    LUPINE_HOOK_INPUT            = 1 << 5,
    LUPINE_HOOK_RENDER           = 1 << 6,
    LUPINE_HOOK_LATE_UPDATE      = 1 << 7,
    LUPINE_HOOK_ENTER_TREE       = 1 << 8,
    LUPINE_HOOK_EXIT_TREE        = 1 << 9,
    LUPINE_HOOK_DESTROY          = 1 << 10,
    LUPINE_HOOK_INPUT_EVENT      = 1 << 11,
    LUPINE_HOOK_PROPERTY_CHANGED = 1 << 12,
    LUPINE_HOOK_CALL_METHOD      = 1 << 13,
    /**
     * The class records runtime draw geometry via build_draw_commands. When set,
     * the host treats the component as an IRenderableComponent: it is gathered
     * during the render pass, its build_draw_commands slot is invoked with the
     * active LupineRenderContextHandle, and (absent get_render_bounds) a
     * node-transform bounding box is used for frustum culling. (ABI v2.)
     */
    LUPINE_HOOK_DRAW             = 1 << 14
} LupineHookFlags;

/* ============================================================================
 * Host interface — function pointers the host provides to the plugin
 *
 * Bound to the host's live engine singletons. Stable for the lifetime of the
 * loaded extension. The plugin must check `struct_size` / `abi_version` before
 * use; the host always fills the whole struct for the advertised ABI version.
 * ============================================================================ */

typedef struct LupineHostInterface {
    uint32_t struct_size;   /**< sizeof(LupineHostInterface) as the host sees it. */
    uint32_t abi_version;   /**< LUPINE_EXTENSION_ABI_VERSION of the host. */

    /* ---- Memory ---- */
    /** Release a string previously returned by a host function. NULL is a no-op. */
    void (LUPINE_EXT_CALL *string_free)(char* str);
    /**
     * Duplicate a NUL-terminated string into host-owned storage. Use this for any
     * string the PLUGIN returns to the HOST (e.g. a call_method result) so that the
     * allocation and its eventual free both happen on the host side of the ABI,
     * never across the C runtime boundary. The host frees it internally.
     * @return host-owned copy (do NOT free it yourself), or NULL if `s` is NULL.
     */
    char* (LUPINE_EXT_CALL *string_dup)(const char* s);

    /* ---- Logging ---- */
    /** Emit a log line through the engine logger. `category` may be NULL. */
    void (LUPINE_EXT_CALL *log)(int32_t level, const char* category, const char* message);

    /* ---- Class registration (call only from lupine_extension_init) ---- */
    /**
     * Register a component class with the host type registry. The host deep-copies
     * everything it needs from `desc` (including the strings and the vtable), so the
     * plugin may free or reuse `desc` after the call returns.
     * @return nonzero on success, zero on failure (e.g. duplicate name, bad ABI).
     */
    LupineBool (LUPINE_EXT_CALL *register_component_class)(
        LupineRegistrationContext ctx, const struct LupineComponentClassDesc* desc);

    /* ---- Per-instance property declaration (call only from define_properties) ---- */
    /**
     * Declare an exported property on `self`. `descriptor_json` must match
     * lupine::core::PropertyDescriptor::Serialize() (keys: name, type, default,
     * hint, description, group). The host owns and stores the property; its value
     * round-trips through the inspector and scene serialization automatically.
     */
    void (LUPINE_EXT_CALL *define_property)(LupineComponentHandle self, const char* descriptor_json);

    /* ---- This component's own exported property values ---- */
    /** @return newly allocated JSON value string, or NULL if no such property. */
    char* (LUPINE_EXT_CALL *get_property_json)(LupineComponentHandle self, const char* name);
    /** @return nonzero on success, zero if missing or value_json is invalid. */
    LupineBool (LUPINE_EXT_CALL *set_property_json)(LupineComponentHandle self, const char* name, const char* value_json);

    /* ---- Scene-tree access (consume surface) ---- */
    /** @return the node this component is attached to, or NULL if detached. */
    LupineNodeHandle (LUPINE_EXT_CALL *get_owner_node)(LupineComponentHandle self);
    /** @return newly allocated node name string, or NULL. */
    char* (LUPINE_EXT_CALL *node_get_name)(LupineNodeHandle node);
    /** @return newly allocated node path string (e.g. "/root/Player"), or NULL. */
    char* (LUPINE_EXT_CALL *node_get_path)(LupineNodeHandle node);
    /** @return the first component of `type_name` on `node`, or NULL. */
    LupineComponentHandle (LUPINE_EXT_CALL *node_get_component_by_type)(LupineNodeHandle node, const char* type_name);
    /** @return a child node by (relative) path, or NULL. */
    LupineNodeHandle (LUPINE_EXT_CALL *node_get_node)(LupineNodeHandle node, const char* path);
    /** @return newly allocated JSON value for a node-level property, or NULL. */
    char* (LUPINE_EXT_CALL *node_get_property_json)(LupineNodeHandle node, const char* name);
    /** @return nonzero on success. */
    LupineBool (LUPINE_EXT_CALL *node_set_property_json)(LupineNodeHandle node, const char* name, const char* value_json);

    /* ---- Other component access (siblings, queried components) ---- */
    /** @return newly allocated runtime type name of `comp`, or NULL. */
    char* (LUPINE_EXT_CALL *component_get_type_name)(LupineComponentHandle comp);
    /** @return newly allocated JSON value for a property of `comp`, or NULL. */
    char* (LUPINE_EXT_CALL *component_get_property_json)(LupineComponentHandle comp, const char* name);
    /** @return nonzero on success. */
    LupineBool (LUPINE_EXT_CALL *component_set_property_json)(LupineComponentHandle comp, const char* name, const char* value_json);
    /**
     * Invoke a named control method on `comp` (generic CallMethod dispatch).
     * @param args_json a JSON array of arguments, or NULL for none.
     * @return newly allocated JSON result, or NULL when the method returns nothing.
     */
    char* (LUPINE_EXT_CALL *component_call_method_json)(LupineComponentHandle comp, const char* method, const char* args_json);

    /* ========================================================================
     * Rendering bridge (ABI v2, append-only)
     *
     * Two families of recorders. The render_draw_* functions enqueue real,
     * runtime-visible geometry and are valid ONLY when called (directly or
     * indirectly) from a build_draw_commands hook, passing the
     * LupineRenderContextHandle that hook received. The debug_draw_* functions
     * enqueue editor-only visualization (they are no-ops in a runtime build);
     * call them from on_render. Vectors are passed as flat floats; colors as
     * four floats (r, g, b, a). Blend modes mirror the engine:
     * 0=Alpha, 1=Additive, 2=Multiply, 3=Opaque, 4=Overlay.
     * ===================================================================== */

    /** Filled 2D/3D quad. texture_id 0 means untextured. */
    void (LUPINE_EXT_CALL *render_draw_quad)(
        LupineRenderContextHandle ctx,
        float pos_x, float pos_y, float pos_z,
        float size_x, float size_y,
        float r, float g, float b, float a,
        int32_t blend_mode);

    /** Filled quad sampling a host texture handle (see component texture APIs). */
    void (LUPINE_EXT_CALL *render_draw_textured_quad)(
        LupineRenderContextHandle ctx,
        float pos_x, float pos_y, float pos_z,
        float size_x, float size_y,
        float r, float g, float b, float a,
        uint32_t texture_id,
        int32_t blend_mode);

    /** Axis-aligned 2D rectangle (top-left origin), equivalent to a zero-radius
     *  rounded rect. */
    void (LUPINE_EXT_CALL *render_draw_rect)(
        LupineRenderContextHandle ctx,
        float pos_x, float pos_y,
        float size_x, float size_y,
        float r, float g, float b, float a,
        int32_t blend_mode);

    /** 2D rounded rectangle (top-left origin) with a uniform corner radius. */
    void (LUPINE_EXT_CALL *render_draw_rounded_rect)(
        LupineRenderContextHandle ctx,
        float pos_x, float pos_y,
        float size_x, float size_y,
        float corner_radius,
        float r, float g, float b, float a,
        int32_t blend_mode);

    /** 2D sprite with pivot, rotation, tint, UV sub-rect, layer and blend mode. */
    void (LUPINE_EXT_CALL *render_draw_sprite)(
        LupineRenderContextHandle ctx,
        uint32_t texture_id,
        float pos_x, float pos_y,
        float size_x, float size_y,
        float pivot_x, float pivot_y,
        float rotation,
        float r, float g, float b, float a,
        float uv_min_x, float uv_min_y,
        float uv_max_x, float uv_max_y,
        int32_t layer,
        int32_t blend_mode);

    /** Line segment between two points with a pixel thickness. */
    void (LUPINE_EXT_CALL *render_draw_line)(
        LupineRenderContextHandle ctx,
        float start_x, float start_y, float start_z,
        float end_x, float end_y, float end_z,
        float r, float g, float b, float a,
        float thickness);

    /** Filled or outlined circle centered at a world point. */
    void (LUPINE_EXT_CALL *render_draw_circle)(
        LupineRenderContextHandle ctx,
        float center_x, float center_y, float center_z,
        float radius,
        float r, float g, float b, float a,
        LupineBool filled);

    /** Regular SDF polygon (sides>=3) centered at a 2D point, optionally rotated. */
    void (LUPINE_EXT_CALL *render_draw_polygon)(
        LupineRenderContextHandle ctx,
        float center_x, float center_y,
        float radius,
        int32_t sides,
        float r, float g, float b, float a,
        float rotation,
        int32_t blend_mode);

    /** Wireframe or filled axis-aligned box centered at a world point. */
    void (LUPINE_EXT_CALL *render_draw_box)(
        LupineRenderContextHandle ctx,
        float center_x, float center_y, float center_z,
        float size_x, float size_y, float size_z,
        float r, float g, float b, float a,
        LupineBool wireframe);

    /** @return nonzero when editor debug drawing is currently available (always
     *  zero in a runtime build). Call before the debug_draw_* functions to skip
     *  work that would otherwise be discarded. */
    LupineBool (LUPINE_EXT_CALL *debug_draw_available)(void);

    /** Editor-only debug line. */
    void (LUPINE_EXT_CALL *debug_draw_line)(
        float start_x, float start_y, float start_z,
        float end_x, float end_y, float end_z,
        float r, float g, float b, float a,
        float duration, LupineBool depth_test);

    /** Editor-only debug box (wireframe or filled). */
    void (LUPINE_EXT_CALL *debug_draw_box)(
        float center_x, float center_y, float center_z,
        float size_x, float size_y, float size_z,
        float r, float g, float b, float a,
        LupineBool wireframe, float duration, LupineBool depth_test);

    /** Editor-only debug sphere (wireframe or filled). */
    void (LUPINE_EXT_CALL *debug_draw_sphere)(
        float center_x, float center_y, float center_z,
        float radius,
        float r, float g, float b, float a,
        LupineBool wireframe, int32_t segments, float duration, LupineBool depth_test);

    /** Editor-only debug circle in the plane defined by a normal. */
    void (LUPINE_EXT_CALL *debug_draw_circle)(
        float center_x, float center_y, float center_z,
        float normal_x, float normal_y, float normal_z,
        float radius,
        float r, float g, float b, float a,
        int32_t segments, float duration, LupineBool depth_test);

    /** Editor-only debug world-space text. */
    void (LUPINE_EXT_CALL *debug_draw_text)(
        float pos_x, float pos_y, float pos_z,
        const char* text,
        float r, float g, float b, float a,
        float size, float duration);

    /** Editor-only debug 2D rectangle (centre + size, in the owner's 2D space).
     *  ABI v3. */
    void (LUPINE_EXT_CALL *debug_draw_rect_2d)(
        float center_x, float center_y,
        float size_x, float size_y,
        float r, float g, float b, float a);
} LupineHostInterface;

/* ============================================================================
 * Component class vtable — function pointers the plugin provides to the host
 *
 * Every slot receives the plugin instance (`self_instance`) and the host handle
 * for the owning component (`self`) so the plugin can call back through the host
 * interface. A slot whose corresponding LupineHookFlags bit is not set in the
 * class descriptor is never called and may be left NULL. `create`/`destroy` and
 * `define_properties` are always required.
 * ============================================================================ */

typedef struct LupineComponentVTable {
    /** Construct the plugin's C++ object for one component instance. */
    LupineInstance (LUPINE_EXT_CALL *create)(LupineComponentHandle self);
    /** Destroy a plugin instance previously returned by create. */
    void (LUPINE_EXT_CALL *destroy)(LupineInstance self_instance);

    /** Declare exported properties via host->define_property(self, ...). Required. */
    void (LUPINE_EXT_CALL *define_properties)(LupineInstance self_instance, LupineComponentHandle self);

    /* Lifecycle hooks (mirror lupine::core::Component virtuals). */
    void (LUPINE_EXT_CALL *on_awake)(LupineInstance self_instance, LupineComponentHandle self);
    void (LUPINE_EXT_CALL *on_ready)(LupineInstance self_instance, LupineComponentHandle self);
    void (LUPINE_EXT_CALL *on_update)(LupineInstance self_instance, LupineComponentHandle self, float delta_time);
    void (LUPINE_EXT_CALL *on_process)(LupineInstance self_instance, LupineComponentHandle self, float delta_time);
    void (LUPINE_EXT_CALL *on_physics_process)(LupineInstance self_instance, LupineComponentHandle self, float delta_time);
    void (LUPINE_EXT_CALL *on_input)(LupineInstance self_instance, LupineComponentHandle self, float delta_time);
    void (LUPINE_EXT_CALL *on_render)(LupineInstance self_instance, LupineComponentHandle self);
    void (LUPINE_EXT_CALL *on_late_update)(LupineInstance self_instance, LupineComponentHandle self, float delta_time);
    void (LUPINE_EXT_CALL *on_enter_tree)(LupineInstance self_instance, LupineComponentHandle self);
    void (LUPINE_EXT_CALL *on_exit_tree)(LupineInstance self_instance, LupineComponentHandle self);
    void (LUPINE_EXT_CALL *on_destroy)(LupineInstance self_instance, LupineComponentHandle self);

    /** Discrete input event (payload is the InputEvent JSON). */
    void (LUPINE_EXT_CALL *on_input_event)(LupineInstance self_instance, LupineComponentHandle self, const char* event_json);
    /** A property was edited (name + new JSON value). */
    void (LUPINE_EXT_CALL *on_property_changed)(LupineInstance self_instance, LupineComponentHandle self,
                                                const char* name, const char* value_json);
    /**
     * Generic control method dispatch (e.g. invoked from scripts via CallMethod).
     * @param args_json JSON array of arguments, or NULL.
     * @return a string allocated via host->string_dup (so the host both owns and
     *         frees it), or NULL when the method returns nothing. The SDK does this
     *         duplication for the author automatically.
     */
    char* (LUPINE_EXT_CALL *call_method)(LupineInstance self_instance, LupineComponentHandle self,
                                         const char* method, const char* args_json);

    /* ---- Rendering (ABI v2, append-only) ---- */

    /**
     * Record runtime draw geometry. Called during the render gather stage when
     * the class advertises LUPINE_HOOK_DRAW. Use `ctx` with the host's
     * render_draw_* functions; the handle is valid only for this call. May be
     * NULL if the class does not draw.
     */
    void (LUPINE_EXT_CALL *build_draw_commands)(LupineInstance self_instance, LupineComponentHandle self,
                                                LupineRenderContextHandle ctx);

    /**
     * Report this component's world-space axis-aligned bounding box for frustum
     * culling and editor selection. Writes the minimum corner to out_min_xyz[3]
     * and the maximum corner to out_max_xyz[3].
     * @return nonzero if bounds were written; zero to let the host fall back to a
     *         node-transform bounding box. May be NULL (treated as returning zero).
     */
    LupineBool (LUPINE_EXT_CALL *get_render_bounds)(LupineInstance self_instance, LupineComponentHandle self,
                                                    float* out_min_xyz, float* out_max_xyz);
} LupineComponentVTable;

/* ============================================================================
 * Component class descriptor — what the plugin registers
 * ============================================================================ */

typedef struct LupineComponentClassDesc {
    uint32_t struct_size;          /**< sizeof(LupineComponentClassDesc). */
    const char* class_name;        /**< Unique registered type name (e.g. "OrbitMover"). */
    const char* base_class_name;   /**< Base component type name, or NULL/"" for Component. */
    const char* display_category;  /**< Editor subcategory hint (e.g. "Native/Gameplay"), or NULL. */
    uint32_t hook_flags;           /**< Bitwise-OR of LupineHookFlags this class implements. */
    LupineComponentVTable vtable;  /**< Per-instance function table (copied by host). */
} LupineComponentClassDesc;

/* ============================================================================
 * Required plugin exports
 *
 * A native extension binary MUST export `lupine_extension_init`. The host
 * resolves it by name, checks the ABI version, then calls it once so the plugin
 * can register its classes. `lupine_extension_deinit` and
 * `lupine_extension_abi_version` are optional but recommended.
 * ============================================================================ */

/**
 * Entry point. Called once when the host loads the extension.
 * @param host_abi_version the host's LUPINE_EXTENSION_ABI_VERSION.
 * @param host the host interface table (valid until deinit).
 * @param ctx registration context to pass to host->register_component_class.
 * @param out_extension optional out-param for the plugin's top-level state,
 *        handed back to lupine_extension_deinit. May be left untouched.
 * @return nonzero on success; zero aborts the load.
 */
typedef LupineBool (LUPINE_EXT_CALL *LupineExtensionInitFn)(
    uint32_t host_abi_version,
    const LupineHostInterface* host,
    LupineRegistrationContext ctx,
    LupineExtensionHandle* out_extension);

/** Optional teardown, called when the host unloads the extension. */
typedef void (LUPINE_EXT_CALL *LupineExtensionDeinitFn)(LupineExtensionHandle extension);

/** Optional ABI probe; if exported, the host prefers it over a hardcoded check. */
typedef uint32_t (LUPINE_EXT_CALL *LupineExtensionAbiVersionFn)(void);

/** Canonical exported symbol names the host looks up. */
#define LUPINE_EXTENSION_INIT_SYMBOL        "lupine_extension_init"
#define LUPINE_EXTENSION_DEINIT_SYMBOL      "lupine_extension_deinit"
#define LUPINE_EXTENSION_ABI_VERSION_SYMBOL "lupine_extension_abi_version"

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LUPINE_EXTENSION_INTERFACE_H */
