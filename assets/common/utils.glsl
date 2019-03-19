// ---------------------------------------------
// UV functions
// ---------------------------------------------

vec3 backToDepthRange(vec4 coord)
{
	vec3 projCoords = coord.xyz / coord.w;
	return (projCoords * 0.5f + 0.5f); // NDC[-1;1] -> Depth range [0;1]
}

// ---------------------------------------------
// Sub surface scattering functions
// ---------------------------------------------

float thickness(vec3 pos, vec3 normal, mat4 clipSpaceMatrix, sampler2D depthMap)
{
	vec4 shrinkedPos = vec4(pos - 0.05 * normal, 1.0f);
	vec3 depthMapPos = backToDepthRange(clipSpaceMatrix * shrinkedPos);
	float d1 = texture(depthMap, depthMapPos.xy).r;
	float d2 = depthMapPos.z;	
	return abs(d2 - d1);
}

vec3 T(float s, float t) // Optical depth
{
	return vec3(0.233,0.455,0.649 ) * exp(-s*s/0.0064*t) +
		   vec3(0.1,  0.366,0.344 ) * exp(-s*s/0.0484*t) +
		   vec3(0.118,0.198,0.0   ) * exp(-s*s/0.187*t) +
		   vec3(0.113,0.007,0.007 ) * exp(-s*s/0.567*t) +
		   vec3(0.358,0.004,0.0   ) * exp(-s*s/1.99*t) +
		   vec3(0.078,0.0,  0.0   ) * exp(-s*s/7.41*t);
}

// ---------------------------------------------
// HDR functions:
// ---------------------------------------------

float clampFloat(float f)
{
	//if(max > 1.0f)
	return clamp( f, 0.0f, 1.0f);
}