@group(0) @binding(0) var hdri: texture_2d<f32>;
@group(0) @binding(1) var hdriSampler: sampler;
@group(0) @binding(2) var<uniform> face: u32;

const PI: f32 = 3.14159265359;

fn uvToXYZ(face: u32, uv: vec2<f32>) -> vec3<f32>
{
	if(face == 0u)
    {
        return vec3<f32>(1.0, uv.y, -uv.x);
    }
	else if(face == 1u)
    {
        return vec3<f32>(-1.0, uv.y, uv.x);
    }
	else if(face == 2u)
    {
        return vec3<f32>(uv.x, -1.0, uv.y);
    }
	else if(face == 3u)
    {
        return vec3<f32>(uv.x, 1.0, -uv.y);
    }
	else if(face == 4u)
    {
        return vec3<f32>(uv.x, uv.y, 1.0);
    }
	else if(face == 5u)
    {
        return vec3<f32>(-uv.x, uv.y, -1.0);
    }

    return vec3<f32>();
}

fn dirToUV(dir: vec3<f32>) -> vec2<f32>
{
	return vec2<f32>(
		0.5 + 0.5 * atan2(dir.z, dir.x) / PI,
		1.0 - acos(dir.y) / PI);
}

fn panoramaToCubeMap(face: u32, texCoord: vec2<f32>) -> vec3<f32>
{
	let texCoordNew: vec2<f32> = texCoord * 2.0 - 1.0;
	let scan: vec3<f32> = uvToXYZ(face, texCoordNew); 
	let direction: vec3<f32> = normalize(scan);
	let src: vec2<f32> = dirToUV(direction);

	return textureSample(hdri, hdriSampler, src).rgb; //< get the color from the panorama
}

struct VertexOutput
{
    @builtin(position) vPos: vec4<f32>,
    @location(0) vUv: vec2<f32>,
}

@vertex
fn vs_main(@builtin(vertex_index) vi: u32) -> VertexOutput 
{
    var out: VertexOutput;

    out.vUv = vec2<f32>(
        f32((vi << 1u) & 2u),
        f32(vi & 2u),
    );
    out.vPos = vec4<f32>(out.vUv * 2.0 - 1.0, 0.0, 1.0);
    out.vUv.y = 1.0 - out.vUv.y;

    return out;
}

@fragment
fn fs_main(out: VertexOutput) -> @location(0) vec4<f32>
{
    return vec4<f32>(panoramaToCubeMap(face, out.vUv), 1.0);
}
