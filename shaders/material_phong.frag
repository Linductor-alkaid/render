#version 450 core

const int MAX_DIRECTIONAL = 4;
const int MAX_POINT = 8;
const int MAX_SPOT = 4;
const int MAX_AMBIENT = 4;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 VertexColor;

out vec4 FragColor;

uniform vec4 uAmbientColor;
uniform vec4 uDiffuseColor;
uniform vec4 uSpecularColor;
uniform float uShininess;

uniform sampler2D diffuseMap;
uniform bool hasDiffuseMap;

uniform vec3 uLightPos;
uniform vec3 uViewPos;

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

vec3 EvaluateDirectionalLight(int index, vec3 norm, vec3 viewDir, vec3 baseDiffuse) {
    vec3 direction = normalize(-uLighting.directionalDirections[index].xyz);
    vec3 lightColor = uLighting.directionalColors[index].rgb * uLighting.directionalColors[index].a;

    float diff = max(dot(norm, direction), 0.0);
    vec3 diffuse = diff * baseDiffuse * lightColor;

    vec3 reflectDir = reflect(-direction, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
    vec3 specular = spec * uSpecularColor.rgb * lightColor;

    return diffuse + specular;
}

vec3 EvaluatePointLight(int index, vec3 norm, vec3 viewDir, vec3 baseDiffuse) {
    vec3 position = uLighting.pointPositions[index].xyz;
    float range = uLighting.pointPositions[index].w;
    vec3 lightVector = position - FragPos;
    float distance = length(lightVector);
    vec3 lightDir = distance > 0.0 ? lightVector / distance : vec3(0.0);

    vec3 attenuation = uLighting.pointAttenuation[index];
    float denom = attenuation.x + attenuation.y * distance + attenuation.z * distance * distance;
    float att = denom > 0.0001 ? 1.0 / denom : 1.0;
    if (range > 0.0) {
        att *= clamp(1.0 - distance / range, 0.0, 1.0);
    }

    vec3 lightColor = uLighting.pointColors[index].rgb * uLighting.pointColors[index].a;

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseDiffuse * lightColor;

    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
    vec3 specular = spec * uSpecularColor.rgb * lightColor;

    return (diffuse + specular) * att;
}

vec3 EvaluateSpotLight(int index, vec3 norm, vec3 viewDir, vec3 baseDiffuse) {
    vec3 position = uLighting.spotPositions[index].xyz;
    float range = uLighting.spotPositions[index].w;
    vec3 lightVector = position - FragPos;
    float distance = length(lightVector);
    vec3 lightDir = distance > 0.0 ? lightVector / distance : vec3(0.0);

    vec3 spotDirection = normalize(uLighting.spotDirections[index].xyz);
    float outerCos = uLighting.spotDirections[index].w;
    float innerCos = uLighting.spotInnerCos[index];

    float theta = dot(-lightDir, spotDirection);
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

    vec3 lightColor = uLighting.spotColors[index].rgb * uLighting.spotColors[index].a;

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseDiffuse * lightColor;

    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
    vec3 specular = spec * uSpecularColor.rgb * lightColor;

    return (diffuse + specular) * att;
}

vec3 EvaluateLegacyLight(vec3 norm, vec3 viewDir, vec3 baseDiffuse) {
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseDiffuse;

    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uShininess);
    vec3 specular = spec * uSpecularColor.rgb;

    return diffuse + specular;
}

void main() {
    vec3 norm = normalize(Normal);

    vec4 baseColor = uDiffuseColor;
    if (hasDiffuseMap) {
        vec4 texColor = texture(diffuseMap, TexCoord);
        baseColor = texColor * uDiffuseColor;
    }

    vec3 result = vec3(0.0);

    vec3 ambientBase = uAmbientColor.rgb * baseColor.rgb;
    result += ambientBase;

    vec3 cameraPos = (uLighting.hasLights != 0) ? uLighting.cameraPosition : uViewPos;
    vec3 viewDir = normalize(cameraPos - FragPos);

    if (uLighting.hasLights != 0) {
        for (int i = 0; i < uLighting.ambientCount; ++i) {
            vec3 ambientColor = uLighting.ambientColors[i].rgb * uLighting.ambientColors[i].a;
            result += ambientColor * baseColor.rgb;
        }

        for (int i = 0; i < uLighting.directionalCount && i < MAX_DIRECTIONAL; ++i) {
            result += EvaluateDirectionalLight(i, norm, viewDir, baseColor.rgb);
        }

        for (int i = 0; i < uLighting.pointCount && i < MAX_POINT; ++i) {
            result += EvaluatePointLight(i, norm, viewDir, baseColor.rgb);
        }

        for (int i = 0; i < uLighting.spotCount && i < MAX_SPOT; ++i) {
            result += EvaluateSpotLight(i, norm, viewDir, baseColor.rgb);
        }
    } else {
        result += EvaluateLegacyLight(norm, viewDir, baseColor.rgb);
    }

    FragColor = vec4(result, baseColor.a);
}

