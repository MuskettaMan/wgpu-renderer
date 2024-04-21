struct VertexOut 
{
    @builtin(position) vPos: vec4<f32>,
    @location(0) vNormal: vec3<f32>,
    @location(1) vTangent: vec3<f32>,
    @location(2) vBitangent: vec3<f32>,
    @location(3) vUv: vec2<f32>
}

struct Common 
{
    proj: mat4x4f,
    view: mat4x4f,
    vp: mat4x4f,

    lightDirection: vec3<f32>,
    _padding: f32,
    lightColor: vec3<f32>,

    time: f32,
    normalMapStrength: f32,
}

@group(0) @binding(0) var<uniform> u_common: Common;
@group(1) @binding(0) var u_albedo: texture_2d<f32>;
@group(1) @binding(1) var u_normal: texture_2d<f32>;
@group(1) @binding(2) var u_sampler: sampler;

@fragment
fn main(in: VertexOut) -> @location(0) vec4<f32> {
    let normalSample = textureSample(u_normal, u_sampler, in.vUv).rgb;
    let localNormal = normalSample * 2.0 - 1.0;
    
    let localToWorld = mat3x3f(
        normalize(in.vTangent),
        normalize(in.vBitangent),
        normalize(in.vNormal)
    );
    let worldNormal = localToWorld * normalize(localNormal);
    let normal = mix(normalize(in.vNormal), normalize(worldNormal), u_common.normalMapStrength);

    let shading = u_common.lightColor * max(0.0, dot(u_common.lightDirection, normal));
    let albedo = textureSample(u_albedo, u_sampler, in.vUv).rgb;
    
    let color = albedo * (shading + vec3<f32>(0.2));

    return vec4<f32>(pow(color, vec3f(2.2)), 1.0);
}

