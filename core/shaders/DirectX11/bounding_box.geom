// DirectX11 BoundingBox Geometry Shader

cbuffer PushConstants : register(b0)
{
    float4x4 u_ViewProjection;
    float4x4 u_Model;
    float4x4 u_NormalMatrix;
    float4 u_TintColor;
    int u_UseTexture;
    float u_AlphaCutoff;
    float _pad1;
    float _pad2;
    float4 u_UVRect;
    float4 u_TextureFlags;
    float4 u_MaterialParams1;
    float4 u_MaterialParams2;
    float4 u_CameraPosition;
    float4 u_AlbedoColor;
    float4 u_EmissiveColor;
    int u_ReceiveShadow;
    float _pad3;
    float _pad4;
    float _pad5;
    // --- Shader-specific uniforms (set via setUniform*) ---
    float4 u_BoxColor;
    float u_LineWidth;
    float2 u_ScreenSize;
    float4 u_BoxMin;
    float4 u_BoxMax;
};

struct GS_INPUT
{
    float4 position : SV_POSITION;
    float4 v_WorldPos : TEXCOORD0;
    float4 v_Color : TEXCOORD1;
};

struct GS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 v_Color : COLOR;
    float2 v_LineCoord : TEXCOORD0;
};

[maxvertexcount(4)]
void main(line GS_INPUT input[2], inout TriangleStream<GS_OUTPUT> outputStream)
{
    GS_OUTPUT output;

    // Get clip-space positions from the two line endpoints
    float4 p0 = input[0].position;
    float4 p1 = input[1].position;
    // Convert to NDC
    float2 ndc0 = p0.xy / p0.w;
    float2 ndc1 = p1.xy / p1.w;
    // Calculate line direction in screen space
    float2 lineDir = ndc1 - ndc0;
    float lineLength = length(lineDir);
    // Avoid division by zero for degenerate lines
    if (lineLength < 0.0001) {
    return;
    }
    lineDir = lineDir / lineLength;
    // Calculate perpendicular direction
    float2 perpDir = float2(-lineDir.y, lineDir.x);
    // Calculate offset in NDC space (accounting for aspect ratio)
    float2 aspect = float2(1.0, u_ScreenSize.x / u_ScreenSize.y);
    float2 offset = perpDir * (u_LineWidth / u_ScreenSize.y) * aspect;
    // Extend line slightly to avoid gaps at corners
    float2 extension = lineDir * (u_LineWidth * 0.5 / u_ScreenSize.y) * aspect;
    // Vertex 0: Start point, negative offset
    output.position = float4((ndc0 - extension - offset) * p0.w, p0.z, p0.w);
    output.v_Color = input[0].v_Color;
    output.v_LineCoord = float2(0.0, -1.0);
    outputStream.Append(output);
    // Vertex 1: Start point, positive offset
    output.position = float4((ndc0 - extension + offset) * p0.w, p0.z, p0.w);
    output.v_Color = input[0].v_Color;
    output.v_LineCoord = float2(0.0, 1.0);
    outputStream.Append(output);
    // Vertex 2: End point, negative offset
    output.position = float4((ndc1 + extension - offset) * p1.w, p1.z, p1.w);
    output.v_Color = input[1].v_Color;
    output.v_LineCoord = float2(1.0, -1.0);
    outputStream.Append(output);
    // Vertex 3: End point, positive offset
    output.position = float4((ndc1 + extension + offset) * p1.w, p1.z, p1.w);
    output.v_Color = input[1].v_Color;
    output.v_LineCoord = float2(1.0, 1.0);
    outputStream.Append(output);
    outputStream.RestartStrip();
}
