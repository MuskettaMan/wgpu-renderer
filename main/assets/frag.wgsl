struct VertexOut 
{
    @builtin(position) vPos: vec4<f32>,
    @location(0) vNormal: vec3<f32>,
    @location(1) vCol: vec3<f32>
}

@fragment
fn main(in: VertexOut) -> @location(0) vec4<f32> {
    
    return vec4<f32>(in.vCol, 1.0);
}

