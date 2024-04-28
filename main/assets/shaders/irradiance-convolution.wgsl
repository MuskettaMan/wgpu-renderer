@group(0) @binding(0) var skyboxTexture: texture_cube<f32>;
@group(0) @binding(1) var cubemapSampler: sampler;
@group(0) @binding(2) var<uniform> face: u32;

const PI: f32 = 3.14159265359;

fn uvToXYZ(face: u32, uv: vec2<f32>) -> vec3<f32>
{
	if(face == 0u)
    {
        return vec3<f32>(-1.0, uv.y, -uv.x);
    }
	else if(face == 1u)
    {
        return vec3<f32>(1.0, uv.y, uv.x);
    }
	else if(face == 2u)
    {
        return vec3<f32>(uv.x, 1.0, uv.y);
    }
	else if(face == 3u)
    {
        return vec3<f32>(uv.x, -1.0, -uv.y);
    }
	else if(face == 4u)
    {
        return vec3<f32>(uv.x, uv.y, -1.0);
    }
	else if(face == 5u)
    {
        return vec3<f32>(-uv.x, uv.y, 1.0);
    }

    return vec3<f32>();
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
    //out.vUv.y = 1.0 - out.vUv.y;

    return out;
}

@fragment
fn fs_main(out: VertexOutput) -> @location(0) vec4<f32>
{
    let texCoordNew: vec2<f32> = out.vUv * 2.0 - 1.0;
	let scan: vec3<f32> = uvToXYZ(face, texCoordNew); 
    let normal = normalize(scan);
    var irradiance = vec3<f32>(0.0);

    var up = vec3<f32>(0.0, 1.0, 0.0); 
    let right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    let sampleDelta = 0.025;
    var nrSamples = 0;
    for(var phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(var theta = 0.0; theta < 0.5 * PI; theta += 0.1)
        {
            let tangentSample = vec3<f32>(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            let sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += textureSample(skyboxTexture, cubemapSampler, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / f32(nrSamples));

    return vec4<f32>(irradiance, 1.0);
}
