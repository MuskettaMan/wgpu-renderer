struct VertexOut 
{
    @builtin(position) vPos: vec4<f32>,
    @location(0) vNormal: vec3<f32>,
    @location(1) vTangent: vec3<f32>,
    @location(2) vBitangent: vec3<f32>,
    @location(3) vUv: vec2<f32>,
    @location(4) vWorldPos: vec3<f32>,
}

struct PointLight 
{
    color: vec4<f32>,
    position: vec3<f32>,
    radius: f32,
}

struct Common 
{
    proj: mat4x4f,
    view: mat4x4f,
    vp: mat4x4f,

    lightDirection: vec3<f32>,
    time: f32,

    lightColor: vec3<f32>,
    normalMapStrength: f32, // TODO: Remove.

    pointLights: array<PointLight, 4>,

    cameraPosition: vec3<f32>,
}

struct Material 
{
    albedoFactor: vec3<f32>,
    metallicFactor: f32,
    roughnessFactor: f32,
    aoFactor: f32,
    normalFactor: f32,
    emmisiveFactor: f32,
}

@group(0) @binding(0) var<uniform> u_common: Common;

@group(1) @binding(0) var<uniform> u_material: Material;
@group(1) @binding(1) var u_sampler: sampler;

@group(1) @binding(2) var u_albedo: texture_2d<f32>;
@group(1) @binding(3) var u_normal: texture_2d<f32>;
@group(1) @binding(4) var u_metallic: texture_2d<f32>;
@group(1) @binding(5) var u_roughness: texture_2d<f32>;
@group(1) @binding(6) var u_ao: texture_2d<f32>;
@group(1) @binding(7) var u_emissive: texture_2d<f32>;

fn GetNormal(in: VertexOut) -> vec3<f32> 
{
    let normalSample = textureSample(u_normal, u_sampler, in.vUv).rgb;
    let localNormal = normalSample * 2.0 - 1.0;
    
    let localToWorld = mat3x3f(
        normalize(in.vTangent),
        normalize(in.vBitangent),
        normalize(in.vNormal)
    );
    let worldNormal = localToWorld * normalize(localNormal);
    return normalize(mix(normalize(in.vNormal), normalize(worldNormal), u_common.normalMapStrength));
}

const PI: f32 = 3.14159265359;

fn D_GGX(N: vec3<f32>, H: vec3<f32>, roughness: f32) -> f32
{
    let a = roughness * roughness;
    let a2 = a * a;
    let NoH = max(dot(N, H), 0.0);
    let NoH2 = NoH * NoH;

    let num = a2;
    var denom = NoH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;

    return num / denom;
}

fn G_SchlickGGX(NoV: f32, roughness: f32) -> f32
{
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;

    let num = NoV;
    let denom = NoV * (1.0 - k) + k;

    return num / denom;
}

fn G_Smith(N: vec3<f32>, V: vec3<f32>, L: vec3<f32>, roughness: f32) -> f32
{
    let NoV = max(dot(N, V), 0.0);
    let NoL = max(dot(N, L), 0.0);
    let ggx1 = G_SchlickGGX(NoL, roughness);
    let ggx2 = G_SchlickGGX(NoV, roughness);

    return ggx1 * ggx2;
}

fn F_Schlick(cosTheta: f32, f0: vec3<f32>) -> vec3<f32>
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

@fragment
fn main(in: VertexOut) -> @location(0) vec4<f32> {
    let albedoSample = pow(textureSample(u_albedo, u_sampler, in.vUv).rgb, vec3<f32>(2.2));
    let metallicSample = textureSample(u_metallic, u_sampler, in.vUv).bbb;
    let roughnessSample = textureSample(u_roughness, u_sampler, in.vUv).ggg;
    let aoSample = textureSample(u_ao, u_sampler, in.vUv);
    let emissiveSample = pow(textureSample(u_emissive, u_sampler, in.vUv).rgb, vec3<f32>(2.2));

    let albedo = albedoSample;
    let metallic = 1.0 - metallicSample.r;
    let roughness = roughnessSample.r;
    let ao = aoSample.r;
    let emissive = emissiveSample;

    let N = GetNormal(in);
    let V = normalize(u_common.cameraPosition - in.vWorldPos);
    let f0 = mix(vec3<f32>(0.04), albedo, metallic);

    var Lo = vec3<f32>(0.0);
    for(var i: i32 = 0; i < 4; i++) 
    {
        let L = normalize(u_common.pointLights[i].position - in.vWorldPos);
        let H = normalize(V + L);

        let HoV = max(dot(H, V), 0.0);
        let NoH = max(dot(N, H), 0.0);
        let NoL = max(dot(N, L), 0.0);

        let distance = length(u_common.pointLights[i].position - in.vWorldPos);
        let attenuation = 1.0 / (distance * distance);
        let radiance = u_common.pointLights[i].color.rgb * attenuation * u_common.pointLights[i].color.a;

        let D = D_GGX(N, H, roughness);
        let G = G_Smith(N, V, L, roughness);
        let F = F_Schlick(HoV, f0);

        let kS = F;
        var kD = (vec3<f32>(1.0) - kS);
        kD *= 1.0 - metallic;

        let DGF = D * G * F;
        let denominator = 4.0 * max(dot(N, V), 0.0) * NoL + 0.0001;
        let specular = DGF / denominator;


        Lo += (kD * albedo / PI + specular) * radiance * NoL;
    }

    let ambient = vec3<f32>(0.03) * albedo * ao;
    var color = ambient + Lo + emissive;

    // Gamma correct.
    //color = color / (color + vec3<f32>(1.0));
    //color = pow(color, vec3<f32>(1.0 / 2.2));

    //color = pow(color, vec3<f32>(2.2));

    return vec4<f32>(color, 1.0);
}

