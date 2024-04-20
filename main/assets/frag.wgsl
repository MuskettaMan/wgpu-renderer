struct VertexOut 
{
    @builtin(position) vPos: vec4<f32>,
    @location(0) vNormal: vec3<f32>,
    @location(1) vCol: vec3<f32>
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

@fragment
fn main(in: VertexOut) -> @location(0) vec4<f32> {
    let normal = normalize(in.vNormal);

    let shading = max(0.0, dot(u_common.lightDirection, in.vNormal));
    let color = textureLoad(u_albedo, vec2i(in.vPos.xy), 0).rgb;//in.vCol * shading;

    return vec4<f32>(color, 1.0);
}

