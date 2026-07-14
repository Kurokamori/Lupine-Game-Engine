# 16 — Component Property Reference

Every serialized property of every built-in component, extracted from each `DefineProperties()` in `core/src/components/<Name>.cpp` (base nodes in `core/src/Node.cpp`, cameras in `core/src/CameraNodes.cpp`). Use these names/types in `.scene` JSON (`02`), `node:set("name", value)`, and inspector edits. Type = `PropertyValueType` (`07`); `Enum` serializes as an integer index. Colors are `{r,g,b,a}` 0–1; vectors `{x,y[,z[,w]]}`.

UI components also inherit all **UIControl** props (listed once below); only component-specific props are repeated per control. Containers inherit **Container** (which inherits UIControl).

---

## Base nodes

### Node
| property | type | default |
|---|---|---|
| name | String | "Node" |
| active | Bool | true |
| visible | Bool | true |
| unique_name_in_owner | Bool | false |

### Node2D (adds to Node)
| property | type | default |
|---|---|---|
| position | Vec2 | (0,0) |
| rotation | Float | 0 |
| scale | Vec2 | (1,1) |
| z_index | Int | 0 |

### Node3D (adds to Node)
| property | type | default |
|---|---|---|
| position | Vec3 | (0,0,0) |
| rotation | Quat | (0,0,0,1) |
| scale | Vec3 | (1,1,1) |

---

## 2D rendering

### Sprite2D
| property | type | default |
|---|---|---|
| texturePath | String | "" |
| modulate | Color | white |
| uvRect | Vec4 | (0,0,1,1) |
| size | Vec2 | (0,0) |
| centered | Bool | true |
| offset | Vec2 | (0,0) |
| flipH / flipV | Bool | false |
| alphaCutoff | Float | 0 |
| spriteSheetEnabled | Bool | false |
| spriteSize | Vec2 | (0,0) |
| hframes / vframes | Int | 0 |
| currentFrame | Int | 0 |

### AnimatedSprite2D
| property | type | default |
|---|---|---|
| animationFilePath | String | "" |
| animationName | String | "" |
| playing | Bool | false |
| loop | Bool | true |
| autoPlay | Bool | false |
| offset | Vec2 | (0,0) |
| flipH / flipV | Bool | false |
| modulate | Color | white |
| pixelSnap | Bool | false |

### Image2D
| property | type | default |
|---|---|---|
| texturePath | String | "" |
| materialOverride | String | "" |
| shaderParameters | String | "" |
| color | Color | white |
| preserveAspect | Bool | true |
| aspectMode | Enum | 0 (Fit) |
| flipH / flipV | Bool | false |
| blendMode | Enum | 0 (Alpha) |
| stretchMode | Enum | 0 (Stretch / KeepCentered / NineSlice) |
| nineSliceMargins | Vec4 | (8,8,8,8) |
| nineSliceAxisH / nineSliceAxisV | Enum | 0 (Stretch / Tile) |
| nineSliceDrawCenter | Bool | true |
| cornerRadius | Vec4 | (0,0,0,0) |
| cornerRadiusLinked | Bool | true |
| borderEnabled | Bool | false |
| borderColor | Color | black |
| borderWidth | Vec4 | (1,1,1,1) |
| borderWidthLinked | Bool | true |
| mouseBehaviour | Enum | 0 (Ignore) |

### Light2D
| property | type | default |
|---|---|---|
| lightEnabled | Bool | true |
| color | Color | (1,1,0.9,1) |
| intensity | Float | 1 |
| range | Float | 100 |
| falloff | Float | 2 |
| blendMode | Enum | 0 (SoftLight) |
| shadowEnabled | Bool | false |
| shadowSampleCount | Int | 96 |
| layer / sortingOrder | Int | 0 |

When `shadowEnabled` is true the light is rendered as a visibility-clipped mesh
(round body sampled at `shadowSampleCount`, plus crisp edges cast from every
`LightOccluder2D` within `range`), so occluded areas receive no light. When
false the legacy radial-gradient light is drawn (no shadows, unchanged look).

### LightOccluder2D
Occluder polygon/polyline that blocks shadows for any shadow-enabled `Light2D`.
Author the points in the owning Node2D's local space; they are transformed by the
node's position/rotation/scale at shadow time.

| property | type | default |
|---|---|---|
| occluderEnabled | Bool | true |
| closed | Bool | true |
| cullMode | Enum | 0 (Disabled) |
| polygon | FloatArray | [] (flat x,y pairs) |

`cullMode`: `Disabled` casts from both sides of every edge; `Clockwise` /
`CounterClockwise` cast only from edges with the matching winding as seen from the
light, so a closed polygon can shadow its outline without self-occluding.

### Line2D
| property | type | default |
|---|---|---|
| pointsData | String | "[]" |
| strokeColor | Color | white |
| strokeWidth | Float | 2 |
| closedLoop | Bool | false |
| capStyle | Enum | 0 (Butt) |
| joinStyle | Enum | 0 (Miter) |
| antiAliasing | Bool | true |
| smoothness | Int | 8 |
| bezierSegments | Int | 16 |
| layer / sortingOrder | Int | 0 |
| uiSpace | Bool | true |

### Shape2D
| property | type | default |
|---|---|---|
| shapeType | Enum | 0 (Circle) |
| filled | Bool | true |
| color | Color | white |
| radius | Float | 50 |
| width / height | Float | 100 |
| borderEnabled | Bool | false |
| borderColor | Color | black |
| borderWidth | Float | 2 |
| circleSegments | Int | 32 |
| layer / sortingOrder | Int | 0 |
| uiSpace | Bool | true |
| materialOverride / shaderParameters | String | "" |

### Particles2D
| property | type | default |
|---|---|---|
| emitting | Bool | true |
| amount | Int | 32 |
| oneShot | Bool | false |
| explosiveness | Float | 0 |
| speedScale | Float | 1 |
| preprocess | Float | 0 |
| localSpace | Bool | false |
| lifetime | Float | 1 |
| lifetimeRandomness | Float | 0 |
| emissionShape | Enum | 0 (Point) |
| emissionRadius | Float | 0 |
| emissionExtents | Vec2 | (0,0) |
| direction | Vec2 | (0,-1) |
| spread | Float | 45 |
| initialVelocityMin | Float | 0 |
| initialVelocityMax | Float | 100 |
| gravity | Vec2 | (0,200) |
| linearDamping | Float | 0 |
| initialAngleMin/Max | Float | 0 |
| angularVelocityMin/Max | Float | 0 |
| particleSize | Vec2 | (8,8) |
| scaleMin / scaleMax / scaleEnd | Float | 1 |
| texturePath | String | "" |
| colorStart | Color | white |
| colorEnd | Color | (1,1,1,0) |
| modulate | Color | white |
| blendMode | Enum | 0 (Alpha) |
| particleShape | Enum | 0 (Square) |
| colorGradient / scaleCurve | String | "" |

### TileMap2D
| property | type | default |
|---|---|---|
| tileMapPath | String | "" |
| modulate | Color | white |
| showCollision | Bool | false |
| baseZIndex | Int | 0 |

### ParallaxBackground
`scrollScale` (1,1), `scrollBaseOffset` (0,0), `ignoreCameraScroll` false. Drives direct-child `ParallaxLayer`s each frame from the active Camera2D's effective position (measured relative to this node). Script: `set_scroll_offset` (manual scroll when `ignoreCameraScroll`), `set_scroll_scale` / `set_scroll_base_offset` / `set_ignore_camera_scroll`, `get_applied_scroll`, `update_layers`.

### ParallaxLayer
`motionScale` (0.5,0.5), `motionOffset` (0,0), `motionMirroring` (0,0). Attach to a direct child of a `ParallaxBackground`; the layer's authored local position is its "home". `motionScale` (1,1)=world-fixed/full scroll, (0,0)=screen-locked; smaller = further away. `motionMirroring` >0 per axis wraps the scroll for seamless tiling (needs a tiling sprite ≥ mirroring+viewport wide). Script: `set+get_motion_scale` / `_offset` / `_mirroring`, `get_home_position`, `capture_home_position`.

### GifPlayer
| property | type | default |
|---|---|---|
| gifPath | File (*.gif) | "" |
| autoPlay | Bool | true |
| playing | Bool | true |
| loopMode | Enum | 1 (Loop; OneShot,Loop,PingPong) |
| speed | Float | 1 |
| fpsOverride | Float | 0 (0 = honor each frame's own delay) |
| offset | Vec2 | (0,0) |
| size | Vec2 | (0,0) (0,0 = native pixel size) |
| flipH / flipV | Bool | false |
| modulate | Color | white |
| pixelSnap | Bool | false |

### VideoPlayer
| property | type | default |
|---|---|---|
| videoPath | File (mp4/m4v/mov/webm/mkv/avi/ogv/mpg/mpeg) | "" |
| autoPlay | Bool | true |
| playing | Bool | true |
| loop | Bool | false |
| speed | Float | 1 |
| audioEnabled | Bool | true |
| volume | Float | 1 |
| bus | String | "Master" |
| muted | Bool | false |
| offset | Vec2 | (0,0) |
| size | Vec2 | (0,0) (0,0 = native pixel size) |
| flipH / flipV | Bool | false |
| modulate | Color | white |

Requires `LUPINE_ENABLE_VIDEO` (ffmpeg); audio routes to the named `AudioManager` bus.

### Empty2D
| property | type | default |
|---|---|---|
| mode | Enum | 0 (Point; Point,Volume) |
| size | Vec2 | (100,100) |
| color | Color | (0.4, 0.9, 1.0, 0.9) |
| pointSize | Float [1,4096] | 16 (crosshair arm half-length) |

Editor-only marker for a `Node2D`; renders nothing at runtime. `Point` draws an axis-aligned crosshair at the node origin; `Volume` draws a rectangle of `size` transformed by the node's global transform (position/rotation/scale), resizable with the scale gizmo.

---

## 3D rendering

### Sprite3D
| property | type | default |
|---|---|---|
| texturePath | String | "" |
| modulate | Color | white |
| uvRect | Vec4 | (0,0,1,1) |
| flipH / flipV | Bool | false |
| size | Vec2 | (1,1) |
| pixelSize | Float | 0 |
| billboardMode | Enum | 1 (Enabled) |
| alphaCutoff | Float | 0 |
| doubleSided | Bool | true |
| castShadow / receiveShadow | Bool | true |
| spriteSheetEnabled | Bool | false |
| spriteSize | Vec2 | (0,0) |
| currentFrame | Int | 0 |

### AnimatedSprite3D
| property | type | default |
|---|---|---|
| animationFilePath / animationName | String | "" |
| playing | Bool | false |
| loop | Bool | true |
| autoPlay | Bool | false |
| offset | Vec3 | (0,0,0) |
| flipH / flipV | Bool | false |
| modulate | Color | white |
| billboard | Enum | 0 (Disabled) |
| doubleSided | Bool | true |
| castShadows / receiveShadows | Bool | true |

### StaticMesh3D
| property | type | default |
|---|---|---|
| modelPath | String | "" |
| castShadow / receiveShadow | Bool | true |
| doubleSided | Bool | false |
| materialSlotCount | Int | 0 |

Each material slot also carries `customVertShaderPath`, `customFragShaderPath`, and `customLshShaderPath` (a `.lsh` shader that takes precedence over the slot's `shaderType` when set), edited via the Material Slots inspector. Same for `SkeletalMesh3D`.

### SkeletalMesh3D
| property | type | default |
|---|---|---|
| modelPath | String | "" |
| defaultAnimation / currentAnimation | String | "" |
| playbackSpeed | Float | 1 |
| loop | Bool | true |
| autoPlay / playing | Bool | false |
| gpuSkinning | Bool | true |
| rootMotionEnabled | Bool | false |
| rootBoneName | String | "" |
| castShadow / receiveShadow | Bool | true |
| showSkeletonInEditor | Bool | false |

### PrimitiveMesh3D
| property | type | default |
|---|---|---|
| shape | Enum | 0 (Cube) |
| size | Float | 1 |
| height | Float | 2 |
| detail | Int | 32 |
| minorRadius | Float | 0.2 |
| color | Color | white |
| castShadow / receiveShadow | Bool | true |
| doubleSided | Bool | false |
| materialOverrideEnabled | Bool | false |
| shaderType | String | "PBR" |
| customLshShaderPath | File (`*.lsh`) | "" |
| albedoColor | Color | white |
| albedoTexture / metallicRoughnessTexture / normalTexture / emissiveTexture | Int | 0 |
| metallic | Float | 0 |
| roughness | Float | 0.5 |
| normalScale | Float | 1 |
| emissiveColor | Color | black |
| emissiveStrength | Float | 1 |
| shadowBands | Float | 3 |
| shadowThreshold | Float | 0.5 |
| shadowSoftness | Float | 0.02 |
| specularBands | Float | 2 |
| specularPower | Float | 32 |
| rimIntensity | Float | 0 |
| rimPower | Float | 3 |

### Particles3D
Same shape as Particles2D with 3D vectors: `emissionExtents` Vec3 (0,0,0), `direction` Vec3 (0,1,0), `spread` 20, `initialVelocityMax` 2, `gravity` Vec3 (0,-9.8,0), `particleSize` Vec2 (0.25,0.25), plus `billboardMode` Enum 1, `doubleSided` Bool true. Other fields identical to Particles2D.

### DirectionalLight3D
| property | type | default |
|---|---|---|
| color | Color | white |
| intensity | Float | 1 |
| negative | Bool | false |
| castShadows | Bool | true |
| shadowOpacity | Float | 1 |
| shadowBlur | Float | 1 |
| shadowBias | Float | 0.0005 |
| shadowResolution | Int | 2048 |
| shadowCascades | Int | 4 |

### OmniLight3D
| property | type | default |
|---|---|---|
| color | Color | white |
| intensity | Float | 1 |
| range | Float | 10 |
| attenuation | Float | 2 |
| negative | Bool | false |
| castShadows | Bool | false |
| shadowOpacity | Float | 1 |
| shadowBlur | Float | 1 |
| shadowBias | Float | 0.001 |
| shadowResolution | Int | 1024 |

### SpotLight3D
Same as OmniLight3D plus `innerConeAngle` Float 30, `outerConeAngle` Float 45; `castShadows` default true.

### WorldEnvironment
Large; grouped. Sky: `skyboxType` Enum 0(None), `skyboxColor` (0.5,0.7,1,1), `skyTopColor`/`skyHorizonColor`/`skyBottomColor`, `cubemapPosX..NegZ` (String ""), `panoramicTexture` String. Fog: `fogEnabled` false, `fogColor`, `fogDensity` 0.01, `fogStart` 10, `fogEnd` 100, `fogMode` Enum 0(Linear), `volumetricFogEnabled` false. Ambient: `ambientLightEnabled` true, `ambientLightColor` white, `ambientLightIntensity` 0.2. Post: `postProcessingEnabled` false, `tonemapMode` Enum 0, `exposure` 1, `whitePoint` 4, `postFlipY` Enum 0(Auto). Bloom: `bloomEnabled` false, `bloomThreshold` 1, `bloomSoftKnee` 0.5, `bloomIntensity` 0.6, `bloomIterations` 6. SSAO: `ssaoEnabled` false, `ssaoRadius` 0.5, `ssaoIntensity` 1, `ssaoBias` 0.025, `ssaoSamples` 24, `ssaoPower` 1.5. Color grading: `colorGradingEnabled` false, `contrast`/`saturation` 1, `brightness`/`temperature`/`tint` 0, `colorFilter` (1,1,1,0), `colorLift` (0,0,0,1), `colorGamma`/`colorGain` white. Vignette: `vignetteEnabled` false, `vignetteColor` black, `vignetteIntensity` 0.4, `vignetteSmoothness` 0.5, `vignetteRoundness` 1, `vignetteCenterX/Y` 0.5. Effects: `chromaticAberrationEnabled` false, `chromaticAberrationAmount` 0.004, `filmGrainEnabled` false, `filmGrainIntensity` 0.08, `filmGrainSize` 1, `overlayTexture` String, `overlayBlendMode` Enum 0(Normal), `overlayOpacity` 1.

### SubViewport
| property | type | default |
|---|---|---|
| renderWidth / renderHeight | Int | 512 |
| worldType | Enum | 0 (World2D) |
| transparentBackground | Bool | true |
| clearColor | Color | (0,0,0,1) |
| displayMode | Enum | 1 (Stretch) |
| displayWidth / displayHeight | Float | 512 |
| useUISpace | Bool | true |
| modulate | Color | white |
| flipVertical | Bool | false |
| inputMode | Enum | 1 (Isolated) |
| focusOnClick | Bool | true |

### Empty3D
| property | type | default |
|---|---|---|
| mode | Enum | 0 (Point; Point,Volume) |
| size | Vec3 | (1,1,1) |
| color | Color | (0.4, 0.9, 1.0, 0.9) |
| pointSize | Float [0.01,1000] | 0.5 (cross arm half-length) |

Editor-only marker for a `Node3D`; renders nothing at runtime. `Point` draws a three-axis cross at the node origin; `Volume` draws a wireframe cube of `size` transformed by the node's global transform (position/rotation/scale), resizable with the scale gizmo.

---

## Cameras (`core/src/CameraNodes.cpp`)

### Camera2D
`zoom` 1, `ortho_size` 1080, `aspect_ratio` 16/9, `is_active` true, `offset` (0,0), `follow_enabled` false, `follow_target` "", `follow_smoothing` true, `follow_speed` 5, `drag_horizontal_enabled`/`drag_vertical_enabled` false, `drag_left/right/top/bottom` 0.2, `limit_enabled` false, `limit_left/right/top/bottom` (±1e9), `effects_enabled` true, `effect_render_mode` Int 0(Separate).

Camera2D is a **Node2D**, so it also has the inherited `position` and `rotation` (radians) transform properties. `rotation` rotates the view (a positive value turns the scene the same direction); use the `zoom` property — not the node `scale` — to magnify (node `scale` is ignored by the camera).

### Camera3D
`projection_type` Int 0(Perspective), `fov` 60, `near_plane` 0.1, `far_plane` 1000, `ortho_size` 100, `is_active` true, `effects_enabled` true, `effect_render_mode` Int 0(Separate). Inherits the Node3D `position`/`rotation`/`scale` transform (the camera looks down its local −Z).

### CameraUI
`canvas_size` (1920,1080), `origin` (0.5,0.5), `position` (0,0), `rotation` 0 (radians, about the origin), `zoom` 1 (uniform canvas zoom about the origin: 2 = 2× in, 0.5 = out), `scale_factor` 1 (HiDPI), `pixel_perfect` false, `is_active` true, `offset` (0,0), `effects_enabled` true, `effect_render_mode` Int 0(Separate), plus the same `follow_*`, `drag_*` and `limit_*` set as Camera2D (screen-space). `rotation` rotates and `zoom` magnifies all UI drawn through this camera. **Note:** CameraUI is a plain Node — `rotation`/`zoom` here are its own properties (not a Node transform), and `zoom` is distinct from `scale_factor`, which only compensates for HiDPI.

---

## Camera effects (`core/src/components/CameraEffects.cpp`)

Stackable per-camera post-effect components. Add any number to a `Camera2D`/`Camera3D`/`CameraUI` node; they apply in attachment order after the WorldEnvironment post-process. The component **Enabled** flag toggles each one; the owning camera's `effects_enabled` and `effect_render_mode` (Separate/Composite) govern the whole stack (see *Cameras*). All parameters are plain registered properties — editable in the inspector and get/set-able from every scripting language.

### CameraEffectColorGrade
`contrast` 1, `saturation` 1, `brightness` 0, `temperature` 0, `tint` 0, `colorFilter` (1,1,1,0), `colorLift` (0,0,0,1), `colorGamma` white, `colorGain` white.

### CameraEffectTonemap
`exposure` 1, `mode` Enum 3(ACES: Linear/Reinhard/ReinhardExtended/ACES/Filmic/AGX), `whitePoint` 4.

### CameraEffectVignette
`vignetteColor` black, `intensity` 0.4, `smoothness` 0.5, `roundness` 1, `centerX` 0.5, `centerY` 0.5.

### CameraEffectFilmGrain
`intensity` 0.08, `size` 1.

### CameraEffectColorInvert
`strength` 1.

### CameraEffectPosterize
`levels` 8, `strength` 1.

### CameraEffectHueShift
`hueDegrees` 0 (−180..180), `saturation` 1, `value` 1.

### CameraEffectBlur
`mode` Enum 0(Box: Box/Directional), `radius` 4, `samples` Int 8, `direction` 0 (degrees, Directional mode). Box mode is a separable two-pass blur.

### CameraEffectGlow
`threshold` 0.7, `intensity` 1, `radius` 16, `samples` Int 32.

### CameraEffectOutline
`mode` Enum 0(Color: Color/Depth), `outlineColor` black, `thickness` 1, `threshold` 0.2. Depth mode requires a 3D/perspective camera; otherwise falls back to Color.

### CameraEffectPixelate
`pixelSize` 4.

### CameraEffectSharpen
`amount` 0.5.

### CameraEffectChromaticAberration
`amount` 0.004.

---

## UI

### UIControl (base for all UI controls)
| property | type | default |
|---|---|---|
| width / height | Float | (component-specific) |
| customMinSize / customMaxSize | Vec2 | (0,0) |
| layoutMode | Enum | 0 (Position) |
| anchorPreset | Enum | 0 (TopLeft) |
| anchorMin / anchorMax | Vec2 | (0,0) |
| offsetMin / offsetMax | Vec2 | (0,0) |
| growDirectionH / growDirectionV | Enum | 1 (End) |
| sizeFlagsHorizontal / sizeFlagsVertical | Int | 1 (Fill) — Fill / ShrinkBegin / ShrinkCenter / ShrinkEnd / Expand; named in READING order, so ShrinkBegin on Y is the TOP |
| sizeFlagsStretchRatio | Float | 1.0 |
| uiSpace / useUISpace | Bool | (component-specific) |
| theme | String | "" |
| themeTypeVariation | String | "" |
| focusMode | Enum | 2 (All) — None / Click / All |
| focusNeighborLeft / focusNeighborTop / focusNeighborRight / focusNeighborBottom | NodePath | "" |
| focusNext / focusPrevious | NodePath | "" |

`focusMode` governs whether a control participates in directional/tab focus traversal (set to `None` for decorative panels/labels; `Click` accepts click/programmatic focus but is skipped by traversal). `focusNeighbor*` / `focusNext` / `focusPrevious` are optional explicit overrides — when empty, neighbor traversal falls back to the nearest focusable control in that screen direction, and next/previous fall back to scene-tree order. See [04_scripting.md](04_scripting.md) for the scriptable focus methods.

Most controls also add `layer` (Int 0) and `sortingOrder` (Int 0).

### Button (component-specific; + UIControl)
Core: `buttonEnabled` true, `styleMode` Enum 0(Automatic), `scaleMode` Enum 0(Fixed), `text` "Button", `localizationKey`/`localizationTable` "", `fontPath` "", `fontSize` 16, `fontColor` white, `wordWrap` false, `textPadding` (10,5), `opacity` 1. Background: `backgroundColor` (0.3,0.3,0.3,1), `backgroundImagePath` "", `backgroundImageStretchMode` Enum 0, `nineSliceMargins` (8,8,8,8), `nineSliceAxisH/V` Enum 0, `nineSliceDrawCenter` true. Border: `borderEnabled` true, `borderWidth` (2,2,2,2), `borderWidthLinked` true, `borderColor` (0.5,0.5,0.5,1), `cornerRadius` (4,4,4,4), `cornerRadiusLinked` true. Per-state (`normal`/`hover`/`pressed`/`disabled`): `*Modulation` (Color), `*SoundPath` (String), `*TweenEnabled` (Bool), `*TweenScale` (Vec2), `*TweenRotation` (Float), `*TweenPosition` (Vec2), `*TweenDuration` (Float). Defaults: normal mod white, hover (1.2³,1), pressed (0.8³,1), disabled (0.5,0.5,0.5,0.5); hover/pressed tweens enabled (scale ±0.05, durations 0.2/0.1).

### Button3D (+ UIControl)
Button's full surface plus 3D: `width` 5, `height` 2, `billboardMode` Enum 1, `doubleSided` true, `castShadow`/`receiveShadow` true, `textPadding` (0.5,0.25), `borderWidth` (0.1×4), `cornerRadius` (0.2×4). State props as Button.

### Label
`text` "Label", `localizationKey`/`localizationTable` "", `fontPath` "", `fontSize` 16, `color` white, `horizontalAlign` Enum 0(Left), `verticalAlign` Enum 0(Top), `wordWrap` false, `multiline` true, `lineSpacing` 1, `autowrapMode` Enum 0(Off; Off,Arbitrary,Word,WordSmart), `overrunBehavior` Enum 0(None; None,TrimChar,TrimWord,EllipsisChar,EllipsisWord), `clipText` false, `tabSize` 4, `shadowOffset` (0,0), `shadowColor` transparent, `autoShrink` false, `minFontSize` 8, `outlineWidth` 0, `outlineColor` black, `centered` false, `offset` (0,0).

### RichTextLabel
`text` "", `localizationKey`/`localizationTable` "", `fontPath` "", `fontSize` Float, `color` Color, `wordWrap` Bool, `horizontalAlign` Enum, `lineSpacing` Float, `clipText` false, `tabSize` 4. (BBCode-style rich text.)

### LineEdit
`text`/`placeholder` "", `fontPath` "", `fontSize` 16, `editable` true, `secret` false, `maxLength` 0, `padding` 6, `fontColor` white, `placeholderColor` (0.6³,1), `backgroundColor` (0.12³,1), `borderColor` (0.4³,1), `borderWidth` 1, `cornerRadius` 4, `selectionColor` (0.2,0.4,0.8,0.6), `caretColor` white.

### TextEdit
Like LineEdit minus placeholder/secret/maxLength: `text` "", `fontPath` "", `fontSize` 16, `editable` true, `lineSpacing` 1, `padding` 6, `fontColor` white, `backgroundColor` (0.1³,1), `borderColor` (0.4³,1), `borderWidth` 1, `cornerRadius` 4, `selectionColor` (0.2,0.4,0.8,0.6), `caretColor` white.

### Checkbox
`boxSize` 20, `borderColor` (0.7³,1), `backgroundColor` (0.2³,1), `checkmarkColor` (0.3,0.7,1,1), `cornerRadius` 3, `borderWidth` 2, `useTextures` false, `uncheckedTexturePath`/`checkedTexturePath` "", `buttonEnabled` true, `isChecked` false, `groupName` "", `checkboxValue` 0, `text` "Checkbox", `fontPath` "", `fontSize` 16, `fontColor` white, `textOffset` 10, plus state modulations (`normal`/`hover`/`pressed`/`checked`/`checkedHover`/`disabled`) and sounds (`check`/`uncheck`/`hover`/`press`).

> When `useTextures` is on, the box draws `checkedTexturePath`/`uncheckedTexturePath` (tinted by the active state modulation) instead of the flat rounded rect; the flat border/background/checkmark are used as a fallback whenever the current state has no valid texture.

### RadioButton
`indicatorSize` 20, `outerCircleColor` (0.7³,1), `backgroundColor` (0.2³,1), `innerCircleColor` (0.3,0.7,1,1), `innerCircleScale` 0.5, `borderWidth` 2, `buttonEnabled` true, `isSelected` false, `groupName` "", `radioValue` 0, `text` "Radio Button", `fontPath` "", `fontSize` 16, `fontColor` white, `textOffset` 10, state modulations (`normal`/`hover`/`pressed`/`selected`/`selectedHover`/`disabled`), `selectSoundPath` "".

### ToggleButton
`buttonEnabled` true, `styleMode` Enum 0, `isToggled` false, `text` "Toggle", `fontPath` "", `fontSize` 16, `fontColor` white, `backgroundColor` (0.2³,1), `opacity` 1, border (`borderEnabled` true, `borderWidth` (2×4), `borderColor` (0.5³,1), `cornerRadius` (5×4)), state modulations + sounds + tweens (normal/hover/pressed) as Button.

### TextureButton
`buttonEnabled` true, `styleMode` Enum 0, `useStretch` true, `stretchMode` Enum 0(Stretch/KeepCentered/NineSlice), `nineSliceMargins` (8,8,8,8), `nineSliceAxisH/V` Enum 0(Stretch/Tile), `nineSliceDrawCenter` true, `isToggle` false, `isChecked` false, `clickMaskMode` Enum 0(Rect), `alphaThreshold` 0.1, `mouseButton` Enum 0(Left), `texturePath`/`normalTexturePath`/`hoverTexturePath`/`pressedTexturePath` "", `text` "", `localizationKey`/`localizationTable` "", `fontPath` "", `fontSize` 16, `fontColor` white, state modulations + sounds + tweens (normal/hover/pressed). Nine-slice margins are source-texture pixels. `localizationKey` overrides `text` and live-updates on locale change, as on Button/Label.

### TextureToggleButton
`buttonEnabled` true, `isToggled` false, `useNineSlice` false, `marginLeft/Right/Top/Bottom` 10, `nineSliceAxisH/V` Enum 0(Stretch/Tile), `nineSliceDrawCenter` true, `normalTexturePath`/`hoverTexturePath`/`pressedTexturePath`/`toggledTexturePath` "", state modulations (incl. `toggledModulation` (1.3³,1)) + sounds + tweens.

### Slider
`minValue` 0, `maxValue` 100, `value` 0, `step` 0, `orientation` Enum 0(Horizontal), `editable` true, `trackColor` (0.25³,1), `fillColor` (0.3,0.6,1,1), `handleColor` (0.9³,1), `trackThickness` 6, `handleRadius` 9.

### SpinBox
`value` 0, `minValue` 0, `maxValue` 100, `step` 1, `decimals` 0, `editable` true, `prefix`/`suffix` "", `dragSensitivity` 0.25, `fontPath` "", `fontSize` 16, `padding` 6, `fontColor` white, `backgroundColor` (0.12³,1), `buttonColor` (0.3³,1), `borderColor` (0.4³,1), `borderWidth` 1, `cornerRadius` 4.

### ProgressBar
`minValue` 0, `maxValue` 100, `value` 50, `step` 0, `smooth` false, `smoothSpeed` 5, `orientation` Enum 0, `fillDirection` Enum 0(LeftToRight), `backgroundTexturePath`/`fillTexturePath`/`borderTexturePath` "", `backgroundColor` (0.2³,1), `fillColor` (0,0.8,0,1), `borderColor` white, `cornerRadius` (0×4), `cornerRadiusLinked` true, `borderWidth` (0×4), `borderWidthLinked` true, `showValue` false, `valueFontPath` "", `valueFontSize` 14, `valueColor` white.

### Dropdown
`items` "", `selectedIndex` -1, `placeholder` "Select...", `itemHeight` 26, `padding` 8, `fontPath` "", `fontSize` 16, `fontColor` white, `backgroundColor` (0.2,0.2,0.22,1), `borderColor` (0.4³,1), `borderWidth` 1, `cornerRadius` 4, `listColor` (0.15,0.15,0.17,0.98), `hoverColor` (0.3,0.4,0.6,1).

### ItemList
`items` "", `selectedIndex` -1, `itemHeight` Float, `scrollSpeed` Float, `padding` Float, `fontPath` "", `fontSize` Float, `fontColor` Color, `backgroundColor` Color, `borderColor` Color, `borderWidth` Float, `selectionColor` Color, `hoverColor` Color.

### Tree
Like ItemList plus `indentWidth` Float: `items`, `selectedIndex` -1, `itemHeight`, `indentWidth`, `scrollSpeed`, `padding`, `fontPath`, `fontSize`, `fontColor`, `backgroundColor`, `borderColor`, `borderWidth`, `selectionColor`, `hoverColor`.

### PopupMenu
`items` "", `itemHeight` Float, `minWidth` Float, `fontPath` "", `fontSize` Float, `fontColor` Color, `backgroundColor` Color, `borderColor` Color, `hoverColor` Color.

### Panel
`stylePath` "", `backgroundColor` (0.8³,1), `opacity` 1, `backgroundImagePath` "", `backgroundImageStretchMode` Enum 0, `nineSliceMargins` (8×4), `nineSliceAxisH/V` Enum 0, `nineSliceDrawCenter` true, `borderEnabled` false, `borderWidth` (1×4), `borderWidthLinked` true, `borderColor` black, `cornerRadius` (0×4), `cornerRadiusLinked` true, `cornerDetail` 8, `antiAliasing` true, `antiAliasingSize` 1, `shadowEnabled` false, `shadowColor` (0,0,0,0.6), `shadowSize` 4, `shadowOffset` (0,0), `customShaderPath`/`materialOverride`/`shaderParameters` "", `mouseFilter` Enum 2(Ignore).

### NineSlicePanel
`texturePath` "", `modulate` white, `marginLeft/Right/Top/Bottom` 10, `nineSliceAxisH/V` Enum 0(Stretch), `nineSliceDrawCenter` true.

### ColorRect
`color` white, `blendMode` Enum 0(Alpha), `materialOverride`/`shaderParameters` "", `cornerRadius` (0×4), `cornerRadiusLinked` true, `borderEnabled` false, `borderColor` black, `borderWidth` (1×4), `borderWidthLinked` true, `mouseFilter` Enum 2(Ignore).

### StyleBox
A reusable style resource referenced by `stylePath` on controls like Panel. (Properties define background/border/corner styling; read `StyleBox.hpp`.)

---

## Layout containers

### Container (base; + UIControl)
`horizontalSizeMode`/`verticalSizeMode` Enum 0(Fixed; Fixed,FitChildren,Expand,Minimum — in any mode but Fixed, `width`/`height` are IGNORED), `padding` (0×4) — space inside, before the children, `paddingLinked` true, `margin` (0×4) — outer space a PARENT container reserves around this one, `marginLinked` true, `drawBackground` true, `backgroundColor` (0.2³,1), `opacity` 1, `borderEnabled` false, `borderWidth` (1×4), `borderWidthLinked` true, `borderColor` (0.5³,1), `cornerRadius` (0×4), `cornerRadiusLinked` true, `clipChildren` false — clips descendants for BOTH drawing and hit-testing, `separation` 0 — read by Horizontal/Vertical/Scroll only, `layer`/`sortingOrder` 0.

> The old `minSize` / `maxSize` properties were **removed**. They shadowed UIControl's `customMinSize` / `customMaxSize` — which is what the anchor solver actually reads — so the inspector showed two different fields with the same name and meaning, only one of which worked. `SetMinSize` / `SetMaxSize` survive as forwarders onto the `custom*` storage. **Scenes carrying non-default `minSize`/`maxSize` silently lose those values on load; re-author them as `customMinSize`/`customMaxSize`.**

### HorizontalContainer (+ Container)
`verticalAlignment` Enum 0(Top; Top,Center,Bottom,Fill) — cross-axis fallback for children with no UIControl. `horizontalAlignment` Enum 0(Begin; Begin,Center,End,Fill,SpaceBetween,SpaceAround,SpaceEvenly) — main-axis distribution of free space.

### VerticalContainer (+ Container)
`horizontalAlignment` Enum 0(Left; Left,Center,Right,Fill) — cross-axis fallback for children with no UIControl. `verticalAlignment` Enum 0(Begin; Begin,Center,End,Fill,SpaceBetween,SpaceAround,SpaceEvenly) — main-axis distribution of free space.

### GridContainer (+ Container)
`useRows` false, `rowCount` 2, `columnCount` 2, `cellSizingMode` Enum 0(Automatic), `fixedCellWidth`/`fixedCellHeight` 100, `homogeneousCells` true, `horizontalSpacing`/`verticalSpacing` 5, `flowDirection` Enum 0(LeftToRight), `sizeChildren` false.

### CenterContainer (+ Container)
`autoFitChild` false, `maintainAspectRatio` true, `stackChildren` false.

### PaddingContainer (+ Container)
`autoFitChildren` true, `maintainAspectRatio` true, `childAlignment` Enum 0(TopLeft).

### ScrollContainer (+ Container)
`hScrollEnabled`/`vScrollEnabled` true, `hScroll`/`vScroll` 0, `scrollSpeed` 30, `scrollBarWidth` 12, `hScrollBarVisibility`/`vScrollBarVisibility` Enum 0(Auto), `scrollBarColor` (0.6³,1), `scrollBarBackgroundColor` (0.2,0.2,0.2,0.5).

### TabContainer (+ Container)
`currentTab` 0, `tabHeight` 28, `fontPath` "", `fontSize` 16, `fontColor` white, `tabColor` (0.2³,1), `activeTabColor` (0.35,0.35,0.4,1), `tabBarColor` (0.12³,1).

### Stack (+ Container)
`defaultAlignment` Enum 4(Center; TopLeft…BottomRight), `sortByZIndex` true.

### Wrap (+ Container)
`spacingX`/`spacingY` Float, `lineAlignment` Enum (Left,Center,Right,Justify), `maxLines` Int (0 = unlimited; N yields exactly N lines), `wrapDirection` Enum (Horizontal,Vertical).

### SplitContainer (+ Container)
`orientation` Enum 0(Horizontal; Horizontal,Vertical), `splitOffset` 0 (pixels from the START edge — left, or **top**; 0 = centered), `splitterWidth` 6, `draggable` true, `splitterColor` (0.35,0.35,0.4,1). Signal: `dragged(offset)`. Uses the first two visible children; the offset is clamped so neither pane goes below its own minimum.

### AspectRatioContainer (+ Container)
`ratio` 1.0 (width ÷ height), `stretchMode` Enum 0(Fit; Fit,Cover,WidthControlsHeight,HeightControlsWidth), `horizontalAlignment`/`verticalAlignment` Enum 1(Center; Begin,Center,End,Fill — canvas-oriented, so Begin is the LOW edge: left on X, **bottom** on Y).

### LayoutSlot (+ Component)
Per-child attached properties read by `DockContainer` and `Stack`: `dockSide`, `alignment`, `zIndex`, `matchParent`, `ignoreLayout`. A child without a LayoutSlot falls back to its container's default.

### Spacer (+ UIControl)
`expand` Bool. (The old `minSize`/`maxSize` properties were removed: they shadowed UIControl's `customMinSize`/`customMaxSize`, which the anchor solver actually reads. `SetMinSize`/`SetMaxSize` survive as forwarders onto those.)

### YSort (+ Node)
`enabled` true, `sortAxis` Enum 0(Y; Y,X), `invert` false, `updateMode` Enum 0(EveryFrame; EveryFrame,OnTransformChange,Manual), `zOffset` Int 0, `affectOnlySpriteChildren` false.

---

## Physics 2D

### RigidBody2DComponent
`mass` 1, `gravityScale` 1, `linearDamping`/`angularDamping` 0, `fixedRotation` false, `bullet` false, `canSleep` true, `gravityEnabled` true.

### StaticBody2DComponent
`constantLinearVelocity` (0,0), `constantAngularVelocity` 0.

### KinematicBody2DComponent
`gravityEnabled` false, `gravityScale` 1.

### CollisionBody2DComponent
`shapeType` Enum 0, `size` (100,100), `radius` 50, `offset` (0,0), `density` 1, `friction` 0.3, `restitution` 0, `isSensor` false, `collisionLayers` Layers2D 1, `collisionMask` Layers2D 0xFFFFFFFF, `debugColor` (0,1,0,0.5).

### AreaTrigger2DComponent
`monitoring` true, `monitorable` true, `priority` 0, `shapeType` Enum 0, `size` (100,100), `radius` 50, `offset` (0,0), `collisionLayers` Layers2D 1, `collisionMask` Layers2D 0xFFFFFFFF.

### CharacterController2D
`gravity` -980, `maxFallSpeed` -1000, `groundDetectionDistance` 2, `wallDetectionDistance` 2, `maxSlopeAngle` 45, `snapToGround` true, `maxBounces` 4.

### RayCast2D
`targetPosition` (0,50), `excludeParent` true, `collisionMask` Layers2D 0xFFFFFFFF, `visibleInGame` false, `debugColor` (0,0.6,0.7,0.8), `debugColorHit` (1,0.3,0.2,0.9). Polls the physics world each frame; read results via `is_colliding` / `get_collider` / `get_collision_point` / `get_collision_normal`, or `force_raycast_update` to re-cast immediately.

### ShapeCast2D
`targetPosition` (0,50), `shapeRadius` 16, `excludeParent` true, `collisionMask` Layers2D 0xFFFFFFFF, `visibleInGame` false, `debugColor` (0,0.6,0.7,0.8), `debugColorHit` (1,0.3,0.2,0.9). Swept-circle (thick ray); same query API as RayCast2D plus `get_collision_fraction` / `set+get_shape_radius` and `force_shapecast_update`.

## Physics 3D

### RigidBody3DComponent
`mass` 1, `gravityScale` 1, `linearDamping`/`angularDamping` 0, `lockRotationX/Y/Z` false, `canSleep` true, `gravityEnabled` true, `useCCD` false.

### StaticBody3DComponent
`constantLinearVelocity` (0,0,0), `constantAngularVelocity` (0,0,0).

### KinematicBody3DComponent
`gravityEnabled` false, `gravityScale` 1.

### CollisionMesh3DComponent
`shapeType` Enum 0, `size` (1,1,1), `radius` 0.5, `height` 1, `planeNormal` (0,1,0), `planeDistance` 0, `planeWidth`/`planeLength` 10, `meshPath` "", `meshSolver` Enum 1, `offset` (0,0,0), `density` 1, `friction` 0.5, `restitution` 0, `isSensor` false, `collisionLayers` Layers3D 1, `debugColor` (0,1,0,0.5).

### AreaTrigger3DComponent
`monitoring` true, `monitorable` true, `priority` 0.

### CharacterController3D
`gravity` -9.8, `maxFallSpeed` -50, `groundDetectionDistance` 0.1, `wallDetectionDistance` 0.1, `maxSlopeAngle` 45, `stepHeight` 0.3, `snapToGround` true, `maxBounces` 4.

### RayCast3D
`targetPosition` (0,-1,0), `excludeParent` true, `collisionMask` all layers, `visibleInGame` false, `debugColor` (0,0.6,0.7,0.8), `debugColorHit` (1,0.3,0.2,0.9). A body is only hit when its collision layers intersect `collisionMask`. Polls the physics world each frame; read results via `is_colliding` / `get_collider` / `get_collision_point` / `get_collision_normal`, or `force_raycast_update` to re-cast immediately. Scripts can also `set+get_collision_mask`.

### ShapeCast3D
`targetPosition` (0,-1,0), `shapeRadius` 0.5, `excludeParent` true, `collisionMask` all layers, `visibleInGame` false, `debugColor` (0,0.6,0.7,0.8), `debugColorHit` (1,0.3,0.2,0.9). Swept-sphere (thick ray), masked like RayCast3D. Same query API as RayCast3D plus `get_collision_fraction` / `set+get_shape_radius` and `force_shapecast_update`.

---

## Navigation

### NavigationRegion2D
`outline` FloatArray [], `holes` Array [].

### NavigationAgent2D
`targetPosition` (0,0), `maxSpeed` 200, `pathDesiredDistance` 16, `targetDesiredDistance` 16, `autoMove` false, `radius` 16, `avoidanceEnabled` false, `neighborDistance` 200, `maxNeighbors` 10, `timeHorizon` 1, `timeHorizonObstacle` 0.5.

### NavigationObstacle2D
`radius` 32, `avoidanceEnabled` true, `carveNavMesh` false, `vertices` FloatArray [].

### NavigationRegion3D
Baking: `cellSize` 0.3, `cellHeight` 0.2, `agentHeight` 2, `agentRadius` 0.5, `agentMaxClimb` 0.4, `maxSlopeDegrees` 45, `minRegionArea` Int 8. Geometry: `geometrySource` Enum 0 (Children; Children,EntireScene), `includeCollision` true, `includeMeshes` false, `autoBake` true. Bounds: `useManualBounds` false, `boundsCenter` Vec3 (0,0,0), `boundsExtents` Vec3 (50,20,50). (Recast-style voxel baking from collision/mesh geometry.)

### NavigationAgent3D
Navigation: `targetPosition` Vec3 (0,0,0), `maxSpeed` 3.5, `pathDesiredDistance` 0.5, `targetDesiredDistance` 0.5, `autoMove` false. Avoidance: `radius` 0.5, `avoidanceEnabled` false, `neighborDistance` 5, `maxNeighbors` Int 10, `timeHorizon` 1, `timeHorizonObstacle` 0.5.

### NavigationObstacle3D
`radius` 1, `avoidanceEnabled` true, `height` 2, `carveNavMesh` false.

---

## Audio

### AudioPlayer
`audioAsset` File "", `autoplay` false, `loop` false, `volume` 1, `pitch` 1, `pan` 0, `bus` "Master", `is3D` false, `minDistance` 1, `maxDistance` 100, `rolloffFactor` 1.

### AudioListener
`active` true.

---

## Animation / tween / timer

### AnimationPlayer
`rootNode` NodePath "", `autoPlay` String "", `speed` 1, `defaultBlendTime` 0.

### AnimationTree
`graphPath` File "", `animationPlayer` NodePath "", `active` true, `rootNode` NodePath "".

### Tween
`duration` 1, `elapsed` 0, `easing` "linear", `loop` false, `autoRemove` false, `running` false.

### TweenSequence
`loops` 1, `autoRemove` false.

### Timer
`duration` 1, `elapsed` 0, `loop` false, `repeatCount` -1, `autoStart` false, `running` false, `ignoreTimeScale` false.

---

## Mesh / scatter / curves

### MultiMeshGeneric
`meshPath` File "", `materialOverride` File "", `customLshShaderPath` File (`*.lsh`) "" (instanced shader — declares per-instance inputs at locations 4-9), `castShadow` Enum 1, `receiveShadow` true, `baseRotation` (0,0,0), `baseScale` (1,1,1), `instanceCount` 0, `cullPerInstance` true, `maxDistance` 1000, `lodGroup` File "", `lod1MeshPath`/`lod2MeshPath`/`lod3MeshPath` File "", `lod1Distance` 30, `lod2Distance` 80, `lod3Distance` 200, `autoGenerateLods` false, `autoLodReduction` 0.5, `editableInEditor` true, `previewSingleInstance` false, `previewInEditor` true.

### ScatterMultiMesh (+ MultiMeshGeneric)
`scatterShape` Enum 0, `width`/`height`/`depth` 10, `radius` 5, `scatterCount` 100, `randomSeed` 12345, `useRandomSeed` true, `clumpingMode` Enum 0, `clumpFactor` 0.5, `clumpCount` 5, `clumpRadius` 2, `scaleMin` 0.8, `scaleMax` 1.2, `uniformScale` true, `rotationVariationX` 0, `rotationVariationY` 360, `rotationVariationZ` 0.

### CollisionScatterMultiMesh (+ MultiMeshGeneric)
`scatterLayers` Layers3D 1, `cutoutLayers` Layers3D 0, `distributionMode` Enum 0, `placementMode` Enum 0, `density` 1, `maxInstances` 1000, `gridSpacingX`/`gridSpacingZ` 1, `gridJitter` 0, `alignToNormal` true, `normalAlignmentStrength` 1, `surfaceOffset` 0, `minSlope` 0, `maxSlope` 45, `scanAreaSize` (20,50,20), `scanAreaOffset` (0,25,0).

### NodeScatter
`scatterMode` Enum 0, `scatterLayers` Layers3D 1.

### Curve2D
`pointsData` "[]", `closedLoop` false, `bezierSegments` 32, `debugDraw` true, `debugColor` (0.2,0.8,0.2,1), `debugLineWidth` 2.

### Path2D (+ Curve2D)
`speed` 100, `loop` false, `pingPong` false, `autoStart` false, `showStartEnd` true, `showDirection` true, `startColor` (0.2,1,0.2,1), `endColor` (1,0.2,0.2,1). Both `Curve2D` and `Path2D` are scriptable via `call` (`add_point`, `sample_curve`, `get_curve_length`, `start_following`, `set_progress`, `get_closest_progress`, …).

### Curve3D
`pointsData` "[]", `closedLoop` false, `bezierSegments` 32, `debugDraw` true, `debugColor` (0.2,0.8,0.2,1), `debugLineWidth` 2. 3D spline; points are local to the owning `Node3D`, debug lines + `*_world` queries transform through its global matrix.

### Path3D (+ Curve3D)
`speed` 5, `loop` false, `pingPong` false, `autoStart` false, `showStartEnd` true, `showDirection` true, `startColor` (0.2,1,0.2,1), `endColor` (1,0.2,0.2,1). Adds path following (progress/speed/loop/ping-pong) plus local- and world-space position/direction/closest-point queries. Per-point `tilt` (in `pointsData`) drives `sample_up_vector`/`sample_orientation`; direction arrows draw a blue up-tick showing banking.

### PathFollow3D
`pathNode` "" (NodePath; empty → parent node's curve), `progressRatio` 0 (0-1), `offset` 0, `hOffset` 0, `vOffset` 0, `rotationMode` Enum 1 (None/Forward/YawOnly), `flipForward` false, `speed` 0, `loop` true, `pingPong` false, `autoStart` true, `previewInEditor` true. Drives the owner `Node3D` transform along the resolved curve/path.

---

## Networking

### NetworkObject
`serverAuthoritative` true, `persistAcrossScenes` false.

### NetworkSpawner
`spawnableScenes` StringArray [], `autoSpawnOnReady` false.

### NetworkSynchronizer
`replicatedProperties` StringArray []. Entries are `"Type:prop"` / `"prop"`, optionally with a `"@hz"` send-rate cap (e.g. `"Health:current@2"`).

### NetworkTransform2D / NetworkTransform3D
`syncPosition` true, `syncRotation` true, `syncScale` false, `snapThreshold` 0, `compress` false (half-float position/scale + quantized rotation).

### NetworkController
`axes` StringArray [], `buttons` StringArray [], `autoSampleInput` true. Drives client-side prediction; pairs with an `on_network_simulate(input, dt)` script function.

### NetworkAnimator
`syncAnimationPlayer` true, `syncAnimationTree` false, `syncStateMachine` false, `stateLayers` 1, `timeResyncThreshold` 0.15, `targetPath` "", `treeParameters` StringArray [] (`"float:Name"` / `"int:Name"` / `"bool:Name"`).

### NetworkRigidBody2D / NetworkRigidBody3D
`syncVelocity` true, `snapThreshold` 0, `compress` false, `makeKinematicOnRemote` true.

---

## Scripting / misc

### ScriptComponent
`script_path` String "" (points at `res://…` `.lua`/`.rb`/`.py`). Any `@export` declared in the script becomes an additional typed property on the component (see `04_scripting.md`).

### CustomShaderParams
No `DefineProperties()` — utility that builds custom-shader material parameter blocks from JSON (see `12`).

### ParticleTextures
No `DefineProperties()` — utility providing built-in particle textures (square, circle).
