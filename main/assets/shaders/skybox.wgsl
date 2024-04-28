struct VertexIn 
{
    @location(0) aPos: vec3<f32>,
}

struct VertexOut 
{
    @builtin(position) vPos: vec4<f32>,
    @location(0) vUv: vec3<f32>,
}

struct Common 
{
    proj: mat4x4f,
    view: mat4x4f,
    vp: mat4x4f,

    lightDirection: vec3<f32>,
    time: f32,

    lightColor: vec3<f32>,
    normalMapStrength: f32,

    cameraPosition: vec3<f32>,
}

struct Instance 
{
    model: mat4x4f,
    exposure: f32
}

@group(0) @binding(0) var<uniform> u_common: Common;
@group(1) @binding(0) var<uniform> u_instance: Instance;
@group(1) @binding(1) var cubemapSampler: sampler;
@group(1) @binding(2) var skyboxMap: texture_cube<f32>;


@vertex
fn vs_main(input: VertexIn) -> VertexOut
{
    var out: VertexOut;
    let mvp = u_common.vp * u_instance.model;
    out.vPos = (mvp * vec4<f32>(input.aPos, 1.0)).xyzw;
    out.vUv = input.aPos;

    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4<f32>
{
    var color = pow(textureSample(skyboxMap, cubemapSampler, in.vUv).rgb, vec3<f32>(2.2));
    color = vec3<f32>(1.0) - exp(-color * u_instance.exposure);

    return vec4<f32>(color, 1.0);
}
