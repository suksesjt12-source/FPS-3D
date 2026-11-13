#version 300 es
precision highp float;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUv;
in vec3 vTangent;
in vec3 vBitangent;
in vec4 vLightSpacePos;

uniform vec4 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uEmissive;

uniform sampler2D uAlbedoTex;
uniform sampler2D uNormalTex;
uniform sampler2D uShadowMap;

uniform vec3 uCamPos;
uniform vec3 uDirLightDir;
uniform vec3 uDirLightColor;
uniform vec3 uAmbientColor;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform int uShadowsEnabled;

uniform int uNumPointLights;
uniform vec3 uPointLightPos[8];
uniform vec3 uPointLightColor[8];
uniform float uPointLightRadius[8];

out vec4 fragColor;

const float PI = 3.14159265;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 0.0001);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.0001);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float computeShadow(vec4 lightSpacePos, vec3 N, vec3 L) {
    if (uShadowsEnabled == 0) return 1.0;
    vec3 proj = lightSpacePos.xyz / max(lightSpacePos.w, 0.0001);
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        return 1.0;
    }

    float bias = max(0.0035 * (1.0 - dot(N, L)), 0.0008);
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float d = texture(uShadowMap, proj.xy + vec2(float(x), float(y)) * texelSize).r;
            shadow += (proj.z - bias > d) ? 0.28 : 1.0;
        }
    }
    return shadow / 9.0;
}

vec3 evalLight(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);
    float NdotV = max(dot(N, V), 0.001);

    float D = distributionGGX(N, H, roughness);
    float G = geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 spec = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo / PI + spec) * radiance * NdotL;
}

void main() {
    vec4 texSample = texture(uAlbedoTex, vUv);
    vec3 albedo = pow(texSample.rgb, vec3(2.2)) * uBaseColor.rgb;
    float roughness = clamp(uRoughness, 0.08, 0.98);
    float metallic = clamp(uMetallic, 0.0, 1.0);

    vec3 tangentNorm = texture(uNormalTex, vUv).xyz * 2.0 - 1.0;
    mat3 TBN = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
    vec3 N = normalize(TBN * tangentNorm);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Ldir = normalize(-uDirLightDir);
    float shadow = computeShadow(vLightSpacePos, N, Ldir);
    vec3 Lo = evalLight(Ldir, uDirLightColor, N, V, albedo, metallic, roughness, F0) * shadow;

    for (int i = 0; i < 8; ++i) {
        if (i >= uNumPointLights) break;
        vec3 toLight = uPointLightPos[i] - vWorldPos;
        float dist = length(toLight);
        float radius = max(uPointLightRadius[i], 0.1);
        if (dist < radius) {
            float att = clamp(1.0 - (dist / radius), 0.0, 1.0);
            att = att * att;
            Lo += evalLight(toLight / max(dist, 0.001), uPointLightColor[i] * att, N, V, albedo, metallic, roughness, F0);
        }
    }

    float hemi = N.y * 0.5 + 0.5;
    vec3 skyEnv = mix(uAmbientColor * 0.85, uAmbientColor * 1.55 + vec3(0.08, 0.10, 0.14), hemi);
    vec3 envF = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 ambient = skyEnv * albedo * (1.0 - metallic * 0.35) + skyEnv * envF * mix(0.22, 0.78, 1.0 - roughness);
    vec3 color = ambient + Lo + uEmissive;

    // Exponential atmospheric fog
    float viewDist = length(uCamPos - vWorldPos);
    float fogFactor = exp(-pow(viewDist * uFogDensity, 1.6));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    color = mix(pow(uFogColor, vec3(2.2)), color, fogFactor);

    fragColor = vec4(color, texSample.a * uBaseColor.a);
}
