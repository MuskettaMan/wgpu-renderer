fn acesToneMap(hdr: vec3<f32>) -> vec3<f32>
{
    let m1 = mat3x3(
        0.59719, 0.07600, 0.02840,
        0.35458, 0.90834, 0.13383,
        0.04823, 0.01566, 0.83777,
    );
    let m2 = mat3x3(
        1.60475, -0.10208, -0.00327,
        -0.53108,  1.10813, -0.07276,
        -0.07367, -0.00605,  1.07602,
    );

    let v = m1 * hdr;
    let a = v * (v + 0.0245786) - 0.000090537;
    let b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return clamp(m2 * (a / b), vec3(0.0), vec3(1.0));
}

struct VertexOutput
{
    @location(0) uv: vec2<f32>,
    @builtin(position) clipPosition: vec4<f32>,
}

@vertex 
fn vs_main(@builtin(vertex_index) vi: u32) -> VertexOutput
{
    var out: VertexOutput;

    out.uv = vec2<f32>(
        f32((vi << 1u) & 2u),
        f32(vi & 2u),
    );
    out.clipPosition = vec4<f32>(out.uv * 2.0 - 1.0, 0.0, 1.0);
    out.uv.y = 1.0 - out.uv.y;

    return out;
}

@group(0) @binding(0) var hdrImage: texture_2d<f32>;
@group(0) @binding(1) var hdrSampler: sampler;

@fragment
fn fs_main(vs: VertexOutput) -> @location(0) vec4<f32>
{
    let hdr = textureSample(hdrImage, hdrSampler, vs.uv);
    let sdr = acesToneMap(hdr.rgb);
    return vec4(sdr, hdr.a);
}