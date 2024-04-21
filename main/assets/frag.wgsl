struct VertexOut 
{
    @builtin(position) vPos: vec4<f32>,
    @location(0) vNormal: vec3<f32>,
    @location(1) vUv: vec2<f32>
}

struct Common 
{
    proj: mat4x4f,
    view: mat4x4f,
    vp: mat4x4f,

    lightDirection: vec3<f32>,
    lightColor: vec3<f32>,

    time: f32
}

@group(0) @binding(0) var<uniform> u_common: Common;
@group(1) @binding(0) var u_albedo: texture_2d<f32>;
@group(1) @binding(1) var u_albedoSampler: sampler;

@fragment
fn main(in: VertexOut) -> @location(0) vec4<f32> {
    let normal = normalize(in.vNormal);

    let shading = u_common.lightColor * max(0.0, dot(u_common.lightDirection, in.vNormal));
    let albedo = textureSample(u_albedo, u_albedoSampler, in.vUv).rgb;
    let color = albedo * (shading + vec3<f32>(0.2));

    return vec4<f32>(pow(color, vec3f(2.2)), 1.0);
}

