#version 450 core

const int MAX_DIRECTIONAL = 4;
const int MAX_POINT = 8;
const int MAX_SPOT = 4;
const int MAX_AMBIENT = 4;

// 输入
in vec3 Normal;
in vec4 VertexColor;

// 输出
out vec4 FragColor;

// Uniforms
uniform vec4 uColor;
uniform vec3 uLightDir;

struct LightingData {
    int directionalCount;
    int pointCount;
    int spotCount;
    int ambientCount;
    int hasLights;
    vec3 cameraPosition;
    vec4 directionalDirections[MAX_DIRECTIONAL];
    vec4 directionalColors[MAX_DIRECTIONAL];
    vec4 pointPositions[MAX_POINT];
    vec4 pointColors[MAX_POINT];
    vec3 pointAttenuation[MAX_POINT];
    vec4 spotPositions[MAX_SPOT];
    vec4 spotColors[MAX_SPOT];
    vec4 spotDirections[MAX_SPOT];
    vec3 spotAttenuation[MAX_SPOT];
    float spotInnerCos[MAX_SPOT];
    vec4 ambientColors[MAX_AMBIENT];
    int culledDirectional;
    int culledPoint;
    int culledSpot;
    int culledAmbient;
};

uniform LightingData uLighting;

void main() {
    vec3 norm = normalize(Normal);
    vec4 baseColor = uColor * VertexColor;

    float ambient = 0.3;
    vec3 lighting = vec3(ambient);

    if (uLighting.hasLights != 0 && uLighting.directionalCount > 0) {
        vec3 lightDir = normalize(-uLighting.directionalDirections[0].xyz);
        vec3 lightColor = uLighting.directionalColors[0].rgb * uLighting.directionalColors[0].a;
        float diff = max(dot(norm, lightDir), 0.0);
        lighting += lightColor * diff;
    } else {
        vec3 lightDir = normalize(-uLightDir);
        float diff = max(dot(norm, lightDir), 0.0);
        lighting += diff;
    }

    FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}

