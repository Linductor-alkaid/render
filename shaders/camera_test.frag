#version 450 core

const int MAX_DIRECTIONAL = 4;
const int MAX_POINT = 8;
const int MAX_SPOT = 4;
const int MAX_AMBIENT = 4;

in vec3 vFragPos;
in vec3 vNormal;
in vec4 vColor;

out vec4 FragColor;

uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform vec3 uLightColor;

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

vec3 ApplyDirectional(int index, vec3 norm, vec3 viewDir) {
    vec3 direction = normalize(-uLighting.directionalDirections[index].xyz);
    vec3 color = uLighting.directionalColors[index].rgb * uLighting.directionalColors[index].a;

    float diff = max(dot(norm, direction), 0.0);
    float specStrength = 0.5;
    vec3 reflectDir = reflect(-direction, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0) * specStrength;

    return color * (diff + spec);
}

vec3 ApplyPoint(int index, vec3 norm, vec3 viewDir) {
    vec3 pos = uLighting.pointPositions[index].xyz;
    float range = uLighting.pointPositions[index].w;
    vec3 toLight = pos - vFragPos;
    float distance = length(toLight);
    vec3 direction = distance > 0.0 ? toLight / distance : vec3(0.0);

    vec3 attenuation = uLighting.pointAttenuation[index];
    float denom = attenuation.x + attenuation.y * distance + attenuation.z * distance * distance;
    float att = denom > 0.0001 ? 1.0 / denom : 1.0;
    if (range > 0.0) {
        att *= clamp(1.0 - distance / range, 0.0, 1.0);
    }

    vec3 color = uLighting.pointColors[index].rgb * uLighting.pointColors[index].a;
    float diff = max(dot(norm, direction), 0.0);
    vec3 reflectDir = reflect(-direction, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0) * 0.5;

    return (color * (diff + spec)) * att;
}

vec3 ApplySpot(int index, vec3 norm, vec3 viewDir) {
    vec3 pos = uLighting.spotPositions[index].xyz;
    float range = uLighting.spotPositions[index].w;
    vec3 toLight = pos - vFragPos;
    float distance = length(toLight);
    vec3 direction = distance > 0.0 ? toLight / distance : vec3(0.0);

    vec3 spotDir = normalize(uLighting.spotDirections[index].xyz);
    float outerCos = uLighting.spotDirections[index].w;
    float innerCos = uLighting.spotInnerCos[index];

    float theta = dot(-direction, spotDir);
    float epsilon = max(innerCos - outerCos, 1e-4);
    float intensity = clamp((theta - outerCos) / epsilon, 0.0, 1.0);
    if (theta <= outerCos || intensity <= 0.0) {
        return vec3(0.0);
    }

    vec3 attenuation = uLighting.spotAttenuation[index];
    float denom = attenuation.x + attenuation.y * distance + attenuation.z * distance * distance;
    float att = denom > 0.0001 ? 1.0 / denom : 1.0;
    if (range > 0.0) {
        att *= clamp(1.0 - distance / range, 0.0, 1.0);
    }
    att *= intensity;

    vec3 color = uLighting.spotColors[index].rgb * uLighting.spotColors[index].a;
    float diff = max(dot(norm, direction), 0.0);
    vec3 reflectDir = reflect(-direction, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0) * 0.5;

    return (color * (diff + spec)) * att;
}

vec3 ApplyLegacy(vec3 norm, vec3 viewDir) {
    vec3 lightDir = normalize(uLightPos - vFragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0) * 0.5;
    return uLightColor * (0.3 + diff + spec);
}

void main() {
    vec3 norm = normalize(vNormal);
    vec3 cameraPos = (uLighting.hasLights != 0) ? uLighting.cameraPosition : uViewPos;
    vec3 viewDir = normalize(cameraPos - vFragPos);

    vec3 ambient = vec3(0.3) * (uLighting.hasLights != 0 ? vec3(1.0) : uLightColor);
    vec3 lighting = ambient;

    if (uLighting.hasLights != 0) {
        for (int i = 0; i < uLighting.ambientCount; ++i) {
            vec3 ambientColor = uLighting.ambientColors[i].rgb * uLighting.ambientColors[i].a;
            lighting += ambientColor;
        }
        for (int i = 0; i < uLighting.directionalCount && i < MAX_DIRECTIONAL; ++i) {
            lighting += ApplyDirectional(i, norm, viewDir);
        }
        for (int i = 0; i < uLighting.pointCount && i < MAX_POINT; ++i) {
            lighting += ApplyPoint(i, norm, viewDir);
        }
        for (int i = 0; i < uLighting.spotCount && i < MAX_SPOT; ++i) {
            lighting += ApplySpot(i, norm, viewDir);
        }
    } else {
        lighting += ApplyLegacy(norm, viewDir);
    }

    FragColor = vec4(lighting * vColor.rgb, vColor.a);
}

