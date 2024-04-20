struct VertexIn 
{
    @location(0) aPos: vec3<f32>,
    @location(1) aNormal: vec3<f32>,
    @location(2) aUv: vec2<f32>
}

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

struct Instance 
{
    model: mat4x4f
}

@group(0) @binding(0) var<uniform> u_common: Common;
@group(0) @binding(1) var<uniform> u_instance: Instance;

@vertex
fn main(input: VertexIn) -> VertexOut {
    var output: VertexOut;
    
    var pos = input.aPos;
    //pos.y += sin(u_common.time);
    let mvp = u_common.vp * u_instance.model;
    
    output.vPos = mvp * vec4<f32>(pos, 1.0);
    output.vNormal = (u_instance.model * vec4(input.aNormal, 0.0)).xyz;
    output.vUv = input.aUv;

    return output;
}

