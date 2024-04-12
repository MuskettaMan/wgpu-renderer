struct VertexIn 
{
    @location(0) aPos: vec3<f32>,
    @location(1) aCol: vec3<f32>
}

struct VertexOut 
{
    @location(0) vCol: vec3<f32>,
    @builtin(position) vPos: vec4<f32>
}

struct Common 
{
    proj: mat4x4f,
    view: mat4x4f,
    vp: mat4x4f,
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
    pos.y += sin(u_common.time);
    let mvp = u_common.vp * u_instance.model;
    
    output.vPos = mvp * vec4<f32>(pos, 1.0);
    output.vCol = input.aCol;
    return output;
}

