"""
Lupine Engine New Shader Dialog
Dialog for creating new .lsh (Lupine Shader) files from templates
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QLabel,
                             QLineEdit, QPushButton, QFileDialog, QComboBox,
                             QFormLayout, QMessageBox)
from PyQt6.QtCore import Qt
from pathlib import Path
import os


class NewShaderDialog(QDialog):
    """Dialog for creating a new .lsh shader file from a template"""

    def __init__(self, parent=None, default_location=None):
        super().__init__(parent)
        self.setWindowTitle("Create New Shader")
        self.setModal(True)
        self.setMinimumWidth(500)

        self._default_location = default_location or ""
        self._setup_ui()

    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout()
        layout.setSpacing(15)

        # Title
        title_label = QLabel("Create New Shader")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold;")
        layout.addWidget(title_label)

        # Form layout for inputs
        form_layout = QFormLayout()
        form_layout.setSpacing(10)

        # Shader name
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("my_shader")
        self.name_edit.textChanged.connect(self._validate_inputs)
        form_layout.addRow("Shader Name:", self.name_edit)

        # Description
        self.description_edit = QLineEdit()
        self.description_edit.setPlaceholderText("A short description of this shader")
        form_layout.addRow("Description:", self.description_edit)

        # Template selector
        self.template_combo = QComboBox()
        self.template_combo.addItems([
            "Empty",
            "Color Rect (2D UI)",
            "Shape 2D",
            "Unlit",
            "Lit (Standard3D)",
            "PBR",
            "Sprite 2D",
            "Custom"
        ])
        self.template_combo.currentTextChanged.connect(self._on_template_changed)
        form_layout.addRow("Template:", self.template_combo)

        # Template description label
        self.template_desc_label = QLabel("")
        self.template_desc_label.setWordWrap(True)
        self.template_desc_label.setStyleSheet("font-style: italic;")
        form_layout.addRow("", self.template_desc_label)
        self._on_template_changed(self.template_combo.currentText())

        # Save location
        location_layout = QHBoxLayout()
        self.location_edit = QLineEdit()
        self.location_edit.setPlaceholderText("Select save location...")
        if self._default_location:
            shaders_dir = str(Path(self._default_location) / "shaders")
            self.location_edit.setText(shaders_dir)
        self.location_edit.textChanged.connect(self._validate_inputs)
        location_layout.addWidget(self.location_edit)

        browse_button = QPushButton("Browse...")
        browse_button.setProperty("secondary", True)
        browse_button.clicked.connect(self._browse_location)
        browse_button.setFixedWidth(80)
        location_layout.addWidget(browse_button)

        form_layout.addRow("Location:", location_layout)

        layout.addLayout(form_layout)

        # Full path preview
        self.path_preview = QLabel()
        self.path_preview.setWordWrap(True)
        self.path_preview.setStyleSheet("font-style: italic;")
        layout.addWidget(self.path_preview)

        # Spacer
        layout.addStretch()

        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()

        cancel_button = QPushButton("Cancel")
        cancel_button.setProperty("secondary", True)
        cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(cancel_button)

        self.create_button = QPushButton("Create Shader")
        self.create_button.setProperty("success", True)
        self.create_button.clicked.connect(self._create_shader)
        self.create_button.setEnabled(False)
        button_layout.addWidget(self.create_button)

        layout.addLayout(button_layout)

        self.setLayout(layout)

    def _on_template_changed(self, template_name):
        """Update description when template selection changes"""
        descriptions = {
            "Empty": "Minimal shader with empty vertex and fragment stages.",
            "Color Rect (2D UI)": "2D UI shader for ColorRect/Image2D/Panel with rounded-corner SDF "
                                  "and exported parameters (glow color/strength) you can edit per-instance.",
            "Shape 2D": "2D shape shader for Shape2D. SDFs circle/square/polygon (fill + border) from "
                        "the engine-provided shape uniforms, with an exported brightness parameter.",
            "Unlit": "Basic unlit shader with texture sampling and tint color.",
            "Lit (Standard3D)": "Simple directional lighting with diffuse shading and texture.",
            "PBR": "Physically based rendering with albedo, normal, metallic, and roughness maps.",
            "Sprite 2D": "2D sprite rendering with texture and tint color.",
            "Custom": "Blank template with all sections ready for custom code.",
        }
        self.template_desc_label.setText(descriptions.get(template_name, ""))

    def _browse_location(self):
        """Open file dialog to select save location"""
        current = self.location_edit.text()
        if not current:
            current = str(Path.home() / "Documents")

        folder = QFileDialog.getExistingDirectory(
            self,
            "Select Shader Save Location",
            current,
            QFileDialog.Option.ShowDirsOnly
        )

        if folder:
            self.location_edit.setText(folder)

    def _validate_inputs(self):
        """Validate inputs and update path preview"""
        name = self.name_edit.text().strip()
        location = self.location_edit.text().strip()

        valid = bool(name and location)

        if name and location:
            # Ensure .lsh extension
            filename = name if name.endswith(".lsh") else name + ".lsh"
            full_path = Path(location) / filename
            self.path_preview.setText(f"Shader will be created at: {full_path}")

            if full_path.exists():
                self.path_preview.setText(
                    f"Warning: File already exists: {full_path}"
                )
                self.path_preview.setStyleSheet("color: orange; font-style: italic;")
            else:
                self.path_preview.setStyleSheet("font-style: italic;")
        else:
            self.path_preview.setText("")

        self.create_button.setEnabled(valid)

    def _create_shader(self):
        """Validate and accept the dialog"""
        name = self.name_edit.text().strip()
        location = self.location_edit.text().strip()

        if not name:
            QMessageBox.warning(self, "Invalid Name", "Please enter a shader name.")
            return

        # Check for invalid characters
        invalid_chars = ['<', '>', ':', '"', '/', '\\', '|', '?', '*']
        for char in invalid_chars:
            if char in name:
                QMessageBox.warning(
                    self,
                    "Invalid Shader Name",
                    "Shader name contains invalid characters.\n"
                    "Avoid using: < > : \" / \\ | ? *"
                )
                return

        if not location:
            QMessageBox.warning(self, "No Location", "Please select a save location.")
            return

        # Check if location exists, offer to create
        location_path = Path(location)
        if not location_path.exists():
            reply = QMessageBox.question(
                self,
                "Create Directory",
                f"The directory '{location}' does not exist.\n"
                "Do you want to create it?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if reply != QMessageBox.StandardButton.Yes:
                return

        # Check if file already exists
        filename = name if name.endswith(".lsh") else name + ".lsh"
        full_path = location_path / filename
        if full_path.exists():
            reply = QMessageBox.question(
                self,
                "File Exists",
                f"A shader file named '{filename}' already exists.\n"
                "Do you want to overwrite it?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if reply != QMessageBox.StandardButton.Yes:
                return

        self.accept()

    def get_shader_name(self) -> str:
        """Returns the entered shader name"""
        name = self.name_edit.text().strip()
        # Strip .lsh if user typed it
        if name.endswith(".lsh"):
            name = name[:-4]
        return name

    def get_template(self) -> str:
        """Returns the selected template name"""
        return self.template_combo.currentText()

    def get_save_path(self) -> str:
        """Returns the full path for the new shader file"""
        name = self.name_edit.text().strip()
        location = self.location_edit.text().strip()
        filename = name if name.endswith(".lsh") else name + ".lsh"
        return str(Path(location) / filename)

    def get_shader_source(self) -> str:
        """Returns the generated .lsh source for the selected template"""
        template = self.get_template()
        shader_name = self.get_shader_name()
        description = self.description_edit.text().strip() or f"{shader_name} shader"
        return self._generate_template(template, shader_name, description)

    def _generate_template(self, template_name: str, shader_name: str, description: str) -> str:
        """Generate .lsh source code for the selected template"""
        if template_name == "Empty":
            return self._template_empty(shader_name, description)
        elif template_name == "Color Rect (2D UI)":
            return self._template_colorrect(shader_name, description)
        elif template_name == "Shape 2D":
            return self._template_shape2d(shader_name, description)
        elif template_name == "Unlit":
            return self._template_unlit(shader_name, description)
        elif template_name == "Lit (Standard3D)":
            return self._template_lit(shader_name, description)
        elif template_name == "PBR":
            return self._template_pbr(shader_name, description)
        elif template_name == "Sprite 2D":
            return self._template_sprite2d(shader_name, description)
        elif template_name == "Custom":
            return self._template_custom(shader_name, description)
        else:
            return self._template_empty(shader_name, description)

    @staticmethod
    def _template_empty(name: str, description: str) -> str:
        return f'''#shader "{name}"
#description "{description}"

#begin properties
    uniform mat4 u_ViewProjection;
    uniform mat4 u_Model;
#end properties

#begin vertex
    #input vec3 a_Position : POSITION 0

    void main() {{
        VERTEX_OUTPUT = MUL(u_ViewProjection, MUL(u_Model, vec4(a_Position, 1.0)));
    }}
#end vertex

#begin fragment
    #output vec4 FragColor

    void main() {{
        FragColor = vec4(1.0, 0.0, 1.0, 1.0);
    }}
#end fragment
'''

    @staticmethod
    def _template_colorrect(name: str, description: str) -> str:
        return f'''#shader "{name}"
#description "{description}"

// 2D UI shader for ColorRect. The engine fills the system uniforms (u_ViewProjection,
// u_Model, u_TintColor, u_CornerRadius, u_Size, u_UVRect, u_UseTexture, u_Texture).
// Any extra uniforms below are exported as editable parameters in the inspector.
// Optional @annotations (quoted) give nicer editing metadata:
//   @display "Label"   @range "min,max"   @color "true"

#begin properties
    uniform mat4 u_ViewProjection;
    uniform mat4 u_Model;
    uniform vec4 u_TintColor = vec4(1.0);
    uniform bool u_UseTexture = false;
    uniform vec4 u_CornerRadius = vec4(0.0);
    uniform vec2 u_Size = vec2(1.0);
    uniform vec4 u_UVRect = vec4(0.0, 0.0, 1.0, 1.0);
    uniform sampler2D u_Texture;

    // ----- Exported parameters (edit these per ColorRect in the inspector) -----
    uniform vec4 u_GlowColor = vec4(1.0, 0.6, 0.2, 1.0); @display "Glow Color" @color "true"
    uniform float u_GlowStrength = 0.5; @display "Glow Strength" @range "0.0,4.0"
#end properties

#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec3 a_Normal : NORMAL 1
    #input vec2 a_TexCoord : TEXCOORD0 2
    #input vec4 a_Color : COLOR 3

    #output vec2 v_TexCoord
    #output vec4 v_Color
    #output vec2 v_LocalPos

    void main() {{
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_LocalPos = a_Position.xy;
        VERTEX_OUTPUT = MUL(u_ViewProjection, MUL(u_Model, vec4(a_Position, 1.0)));
    }}
#end vertex

#begin fragment
    #output vec4 FragColor

    float sdRoundedBox(vec2 p, vec2 size, vec4 radius) {{
        vec2 pos = p * size;
        vec2 halfSize = size * 0.5;

        float r;
        if (pos.x > 0.0) {{
            r = (pos.y > 0.0) ? radius.z : radius.y;
        }} else {{
            r = (pos.y > 0.0) ? radius.w : radius.x;
        }}

        r = min(r, min(halfSize.x, halfSize.y));

        vec2 q = abs(pos) - halfSize + r;
        return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
    }}

    void main() {{
        float dist = sdRoundedBox(v_LocalPos, u_Size, u_CornerRadius);

        float edgeSoftness = 1.5 / length(u_Size);
        float alpha = 1.0 - smoothstep(-edgeSoftness, edgeSoftness, dist);

        vec4 color = v_Color * u_TintColor;
        if (u_UseTexture) {{
            vec2 uvMin = u_UVRect.xy;
            vec2 uvMax = u_UVRect.zw;
            vec2 remappedUV = uvMin + (v_TexCoord * (uvMax - uvMin));
            color *= SAMPLE(u_Texture, remappedUV);
        }}

        // Exported-parameter effect: brighten edges toward the glow color.
        float edgeGlow = smoothstep(-edgeSoftness, edgeSoftness * 8.0, dist + edgeSoftness * 4.0);
        color.rgb = mix(color.rgb, u_GlowColor.rgb, edgeGlow * u_GlowStrength * u_GlowColor.a);

        color.a *= alpha;

        if (color.a < 0.01) {{
            DISCARD;
        }}

        FragColor = color;
    }}
#end fragment
'''

    @staticmethod
    def _template_shape2d(name: str, description: str) -> str:
        return f'''#shader "{name}"
#description "{description}"

// 2D shape shader for Shape2D. The engine fills the system uniforms and the shape description
// (u_ShapeType, u_ShapeParams, u_BorderColor, u_BorderParams). The shader performs the SDF for
// the fill and border. Extra uniforms below are exported as editable parameters in the inspector.
//   u_ShapeType:   0=circle, 1=square, 2=triangle, 3=pentagon, 4=hexagon
//   u_ShapeParams: x=radius(px), y=sides, z=halfWidth(px), w=halfHeight(px)
//   u_BorderParams: x=borderWidth(px), y=borderEnabled, z=filled

#begin properties
    uniform mat4 u_ViewProjection;
    uniform mat4 u_Model;
    uniform vec4 u_TintColor = vec4(1.0);
    uniform vec2 u_Size = vec2(1.0);
    uniform float u_ShapeType = 0.0;
    uniform vec4 u_ShapeParams = vec4(50.0, 0.0, 50.0, 50.0);
    uniform vec4 u_BorderColor = vec4(0.0, 0.0, 0.0, 1.0);
    uniform vec4 u_BorderParams = vec4(0.0, 0.0, 1.0, 0.0);

    // ----- Exported parameters -----
    uniform float u_Brightness = 1.0; @display "Brightness" @range "0.0,3.0"
#end properties

#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec3 a_Normal : NORMAL 1
    #input vec2 a_TexCoord : TEXCOORD0 2
    #input vec4 a_Color : COLOR 3

    #output vec2 v_TexCoord
    #output vec4 v_Color
    #output vec2 v_LocalPos

    void main() {{
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_LocalPos = a_Position.xy;
        VERTEX_OUTPUT = MUL(u_ViewProjection, MUL(u_Model, vec4(a_Position, 1.0)));
    }}
#end vertex

#begin fragment
    #output vec4 FragColor

    #define PI 3.14159265359

    float sdBox(vec2 p, vec2 b) {{
        vec2 q = abs(p) - b;
        return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
    }}

    float sdNgon(vec2 p, float r, float n) {{
        float an = PI / n;
        float he = r * cos(an);
        float a = atan(p.y, p.x);
        a = abs(mod(a + an, 2.0 * an) - an);
        return length(p) * cos(a) - he;
    }}

    void main() {{
        vec2 pos = v_LocalPos * u_Size;

        float radius = u_ShapeParams.x;
        float sides = max(3.0, u_ShapeParams.y);
        vec2 halfSize = u_ShapeParams.zw;

        float borderWidth = u_BorderParams.x;
        float borderEnabled = u_BorderParams.y;
        float filled = u_BorderParams.z;

        float dist;
        if (u_ShapeType > 0.5 && u_ShapeType < 1.5) {{
            dist = sdBox(pos, halfSize);
        }} else if (u_ShapeType >= 1.5) {{
            dist = sdNgon(pos, radius, sides);
        }} else {{
            dist = length(pos) - radius;
        }}

        float aa = 1.5;
        float fillMask = 1.0 - smoothstep(-aa, aa, dist);
        float outerMask = 1.0 - smoothstep(borderWidth - aa, borderWidth + aa, dist);

        vec4 fillCol = v_Color * u_TintColor;
        fillCol.rgb *= u_Brightness;

        vec4 col = vec4(0.0);

        bool hasBorder = (borderEnabled > 0.5 && borderWidth > 0.001);
        if (hasBorder) {{
            col = u_BorderColor;
            col.a *= outerMask;
            if (filled < 0.5) {{
                col.a *= (1.0 - fillMask);
            }}
        }}

        if (filled > 0.5) {{
            vec4 fill = fillCol;
            fill.a *= fillMask;
            col.rgb = mix(col.rgb, fill.rgb, fill.a);
            col.a = max(col.a, fill.a);
        }}

        if (col.a < 0.01) {{
            DISCARD;
        }}

        FragColor = col;
    }}
#end fragment
'''

    @staticmethod
    def _template_unlit(name: str, description: str) -> str:
        return f'''#shader "{name}"
#description "{description}"

#begin properties
    uniform mat4 u_ViewProjection;
    uniform mat4 u_Model;
    uniform vec4 u_TintColor = vec4(1.0);
    uniform int u_UseTexture = 0;
    uniform sampler2D u_Texture;
#end properties

#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec2 a_TexCoord : TEXCOORD0 1
    #input vec4 a_Color : COLOR 2

    #output vec2 v_TexCoord
    #output vec4 v_Color

    void main() {{
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        VERTEX_OUTPUT = MUL(u_ViewProjection, MUL(u_Model, vec4(a_Position, 1.0)));
    }}
#end vertex

#begin fragment
    #output vec4 FragColor

    void main() {{
        vec4 color = v_Color * u_TintColor;
        if (u_UseTexture != 0) {{
            color *= SAMPLE(u_Texture, v_TexCoord);
        }}
        if (color.a < 0.01) {{
            DISCARD;
        }}
        FragColor = color;
    }}
#end fragment
'''

    @staticmethod
    def _template_lit(name: str, description: str) -> str:
        return f'''#shader "{name}"
#description "{description}"

#begin properties
    uniform mat4 u_ViewProjection;
    uniform mat4 u_Model;
    uniform mat4 u_NormalMatrix;
    uniform vec4 u_TintColor = vec4(1.0);
    uniform int u_UseTexture = 0;
    uniform float u_AlphaCutoff = 0.0;
    uniform sampler2D u_Texture;
#end properties

#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec3 a_Normal : NORMAL 1
    #input vec2 a_TexCoord : TEXCOORD0 2
    #input vec4 a_Color : COLOR 3

    #output vec3 v_Position
    #output vec3 v_Normal
    #output vec2 v_TexCoord
    #output vec4 v_Color

    void main() {{
        vec4 worldPos = MUL(u_Model, vec4(a_Position, 1.0));
        v_Position = worldPos.xyz;
        v_Normal = MUL(MAT3(u_NormalMatrix), a_Normal);
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        VERTEX_OUTPUT = MUL(u_ViewProjection, worldPos);
    }}
#end vertex

#begin fragment
    #output vec4 FragColor

    void main() {{
        vec3 normal = normalize(v_Normal);
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 lighting = vec3(0.3) + vec3(0.7) * diff;

        vec4 baseColor = v_Color * u_TintColor;
        if (u_UseTexture != 0) {{
            baseColor *= SAMPLE(u_Texture, v_TexCoord);
        }}
        if (u_AlphaCutoff > 0.0 && baseColor.a < u_AlphaCutoff) {{
            DISCARD;
        }}
        FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
    }}
#end fragment
'''

    @staticmethod
    def _template_pbr(name: str, description: str) -> str:
        return f'''#shader "{name}"
#description "{description}"

#begin properties
    uniform mat4 u_ViewProjection;
    uniform mat4 u_View;
    uniform mat4 u_Model;
    uniform mat4 u_NormalMatrix;
    uniform vec3 u_CameraPosition = vec3(0.0);
    uniform vec4 u_TintColor = vec4(1.0);
    uniform vec4 u_AlbedoColor = vec4(1.0);
    uniform vec4 u_MaterialParams1 = vec4(0.0, 0.5, 1.0, 0.0);
    uniform vec4 u_TextureFlags = vec4(0.0);
    uniform sampler2D u_AlbedoTexture;
    uniform sampler2D u_NormalTexture;
#end properties

#begin shared
    const float PI = 3.14159265359;

    vec3 fresnelSchlick(float cosTheta, vec3 F0) {{
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }}

    float distributionGGX(vec3 N, vec3 H, float roughness) {{
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH * NdotH;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;
        return a2 / denom;
    }}

    float geometrySchlickGGX(float NdotV, float roughness) {{
        float r = (roughness + 1.0);
        float k = (r * r) / 8.0;
        return NdotV / (NdotV * (1.0 - k) + k);
    }}

    float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {{
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
    }}
#end shared

#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec3 a_Normal : NORMAL 1
    #input vec2 a_TexCoord : TEXCOORD0 2
    #input vec4 a_Color : COLOR 3

    #output vec3 v_WorldPos
    #output vec3 v_Normal
    #output vec2 v_TexCoord
    #output vec4 v_Color
    #output vec3 v_ViewDir

    void main() {{
        vec4 worldPos = MUL(u_Model, vec4(a_Position, 1.0));
        v_WorldPos = worldPos.xyz;
        v_Normal = MUL(MAT3(u_NormalMatrix), a_Normal);
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        v_ViewDir = normalize(u_CameraPosition - worldPos.xyz);
        VERTEX_OUTPUT = MUL(u_ViewProjection, worldPos);
    }}
#end vertex

#begin fragment
    #output vec4 FragColor

    void main() {{
        float metallic = u_MaterialParams1.x;
        float roughness = clamp(u_MaterialParams1.y, 0.04, 1.0);

        vec4 albedoSample = v_Color * u_TintColor * u_AlbedoColor;
        if (u_TextureFlags.x != 0.0) {{
            albedoSample *= SAMPLE(u_AlbedoTexture, v_TexCoord);
        }}
        vec3 albedo = albedoSample.rgb;

        vec3 N = normalize(v_Normal);
        if (u_TextureFlags.z != 0.0) {{
            vec3 tangentNormal = SAMPLE(u_NormalTexture, v_TexCoord).rgb * 2.0 - 1.0;
            vec3 Q1 = dFdx(v_WorldPos);
            vec3 Q2 = dFdy(v_WorldPos);
            vec2 st1 = dFdx(v_TexCoord);
            vec2 st2 = dFdy(v_TexCoord);
            vec3 T = normalize(Q1 * st2.y - Q2 * st1.y);
            vec3 B = normalize(cross(N, T));
            mat3 TBN = mat3(T, B, N);
            N = normalize(TBN * tangentNormal);
        }}

        vec3 V = normalize(v_ViewDir);
        vec3 F0 = mix(vec3(0.04), albedo, metallic);

        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
        vec3 L = lightDir;
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        float D = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 specular = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 Lo = (kD * albedo / PI + specular) * vec3(1.0) * NdotL;

        vec3 ambient = vec3(0.03) * albedo;
        vec3 color = ambient + Lo;

        // Tonemap and gamma
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0 / 2.2));

        FragColor = vec4(color, albedoSample.a);
    }}
#end fragment
'''

    @staticmethod
    def _template_sprite2d(name: str, description: str) -> str:
        return f'''#shader "{name}"
#description "{description}"

#begin properties
    uniform mat4 u_ViewProjection;
    uniform mat4 u_Model;
    uniform vec4 u_TintColor = vec4(1.0);
    uniform int u_UseTexture = 0;
    uniform sampler2D u_Texture;
#end properties

#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec2 a_TexCoord : TEXCOORD0 1
    #input vec4 a_Color : COLOR 2

    #output vec2 v_TexCoord
    #output vec4 v_Color

    void main() {{
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        VERTEX_OUTPUT = MUL(u_ViewProjection, MUL(u_Model, vec4(a_Position, 1.0)));
    }}
#end vertex

#begin fragment
    #output vec4 FragColor

    void main() {{
        vec4 color = v_Color * u_TintColor;
        if (u_UseTexture != 0) {{
            color *= SAMPLE(u_Texture, v_TexCoord);
        }}
        FragColor = color;
    }}
#end fragment
'''

    @staticmethod
    def _template_custom(name: str, description: str) -> str:
        return f'''#shader "{name}"
#description "{description}"

#begin properties
    uniform mat4 u_ViewProjection;
    uniform mat4 u_Model;
    uniform vec4 u_TintColor = vec4(1.0);
    // Add your custom uniforms here
#end properties

#begin shared
    // Shared functions available to both vertex and fragment stages
#end shared

#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec3 a_Normal : NORMAL 1
    #input vec2 a_TexCoord : TEXCOORD0 2
    #input vec4 a_Color : COLOR 3

    #output vec3 v_Normal
    #output vec2 v_TexCoord
    #output vec4 v_Color

    void main() {{
        v_Normal = a_Normal;
        v_TexCoord = a_TexCoord;
        v_Color = a_Color;
        VERTEX_OUTPUT = MUL(u_ViewProjection, MUL(u_Model, vec4(a_Position, 1.0)));
    }}
#end vertex

#begin fragment
    #output vec4 FragColor

    void main() {{
        // Write your custom fragment shader here
        FragColor = v_Color * u_TintColor;
    }}
#end fragment
'''
