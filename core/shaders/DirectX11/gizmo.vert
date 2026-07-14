// DirectX11 Gizmo Vertex Shader

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
    float u_LineWidth;
    float2 u_ScreenSize;
    float u_GizmoScale;
    float4 u_AxisColorX;
    float4 u_AxisColorY;
    float4 u_AxisColorZ;
    int u_SelectedAxis;
};

struct VS_INPUT
{
    float3 a_Position : POSITION;
    float4 a_Color : COLOR;
    float a_AxisID : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 v_WorldPos : TEXCOORD0;
    float4 v_Color : TEXCOORD1;
    float v_AxisID : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // Scale gizmo
    float3 scaledPos = input.a_Position * u_GizmoScale;
    // Transform to world space
    float4 worldPos = mul(u_Model, float4(scaledPos, 1.0));
    output.v_WorldPos = worldPos;
    // Determine axis color (select from X/Y/Z based on axis ID)
    int axisID = int(input.a_AxisID);
    float4 axisColor = u_AxisColorX;
    if (axisID == 1) { axisColor = u_AxisColorY; }
    if (axisID == 2) { axisColor = u_AxisColorZ; }
    // Highlight selected axis in yellow
    if (axisID == u_SelectedAxis) {
    axisColor = float4(1.0, 1.0, 0.0, 1.0);
    }
    output.v_Color = axisColor * input.a_Color * u_TintColor;
    output.v_AxisID = input.a_AxisID;
    output.position = mul(u_ViewProjection, worldPos);

    return output;
}
