#version 330 core

in vec4 vColor;
in vec3 vFragPos;
in vec3 vFragNormal;

in vec4 vDepthMapCoord;
uniform sampler2D uDepthMap;
uniform mat4 uLightViewProjMatrix;

// Camera Data
uniform vec3 uEyePos;
uniform vec2 uNearFarPlane;

// Light Data
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uConstant;
uniform float uLinear;
uniform float uQuadratic;

// Shader Data
uniform float uStrength;
uniform bool uEnable;
uniform int uLightingModel;
uniform int uThickTechnique; // type of calculation of occluder thickness
uniform bool uThicknessEnable;
uniform bool uTransmitanceEnable;
uniform float uExtintionCoef;

out vec4 fragColor;

vec3 backToNDC(vec4 coord)
{
	vec3 projCoords = coord.xyz / coord.w;
	return (projCoords * 0.5f + 0.5f); // [-1;1] -> NDC [0;1]
}

float LinearizeDepth(float depth)
{
	float nearPlane = uNearFarPlane.x;
	float farPlane = uNearFarPlane.y;
    float z = depth * 2.0 - 1.0; // Back to NDC 
    return (2.0 * nearPlane * farPlane) / 
	       (nearPlane + farPlane - z * (nearPlane - farPlane));	
}

float distance(vec3 pos, vec3 normal, sampler2D depthMap)
{
	vec4 shrinkedPos = vec4(pos - 0.005 * normal, 1.0f);
	vec3 depthMapPos = backToNDC(uLightViewProjMatrix * shrinkedPos);
	float d1 = texture(depthMap, depthMapPos.xy).r;
	float d2 = depthMapPos.z;
	return abs(d1 - d2);
}

float distance2(vec4 pos)
{
	vec3 depthMapPos = backToNDC(pos);
	return depthMapPos.z;
}

vec3 T(float s, float t) //Profile -> depends of material color now it's red
{
	return vec3(0.233, 0.455, 0.649) * exp(-s*s/0.0064*t) +
		   vec3(0.1,   0.366, 0.344) * exp(-s*s/0.0484*t) +
		   vec3(0.118, 0.198, 0.0) * exp(-s*s/0.187*t)  +
		   vec3(0.113, 0.007, 0.007) * exp(-s*s/0.567*t)  +
		   vec3(0.358, 0.004, 0.0)   * exp(-s*s/1.99*t)   +
		   vec3(0.078, 0.0,   0.0)   * exp(-s*s/7.41*t);
}

void main()
{
	
	// Vector calculation 
	vec3 N = normalize(vFragNormal);
	vec3 L = normalize(uLightPos - vFragPos); // Light direction
	vec3 eyeDir = normalize(uEyePos - vFragPos); // Eye direction

	// Light attenuation
	float d = length(uLightPos - vFragPos); // distance to frag from light
    float fLightAttenuation = 1.0f / (uConstant + uLinear * d + uQuadratic * (d * d));

	// Ambient
	float ambientFactor = 0.01f;
	vec3 vAmbient = ambientFactor * vColor.rgb;

	// Diffuse color
	float diffuseFactor = max(dot(N, L),0.0f);

	// Specular color
	vec3 reflectDir = reflect(-L, N);  
	float specFactor = pow(max(dot(eyeDir, reflectDir), 0.0), 16);
	// ---------------------------------------------
	//  Blin-Phong
	// ---------------------------------------------

		vec3 blinPhong = ((ambientFactor + diffuseFactor + specFactor) * vColor.rgb) * fLightAttenuation;

	// ---------------------------------------------
	//  End
	// ---------------------------------------------

	// ---------------------------------------------
	//  Distance a light ray travels inside an occluder
	// ---------------------------------------------
		
		float s = 0;

		if(uThickTechnique == 0)
			s =  distance(vFragPos, N, uDepthMap) / uStrength;
		else if(uThickTechnique == 1)
			s =  abs(1 - distance2(vDepthMapCoord) -  (1.0f / uStrength));
		else if(uThickTechnique == 2)
			s = texture(uDepthMap, (vDepthMapCoord.xy / vDepthMapCoord.w)*0.5f + 0.5f).r / uStrength;
	
	// ---------------------------------------------
	// End
	// ---------------------------------------------

	// ---------------------------------------------
	// Beer-Lambertian + Jorge Jimenez Translucency
	// ---------------------------------------------

		float E = max(0.3f + dot(-vFragNormal, L), 0.0f); // irradiance
		// Transmission coefficient
		vec3 transmittance = T(s,uExtintionCoef) * uLightColor * fLightAttenuation * vColor.rgb * E;
		
		// Final lighting model
		vec3 modelA = transmittance + blinPhong;

	// ---------------------------------------------
	// End
	// ---------------------------------------------

	// ---------------------------------------------
	// Beer-Lambertian + Ben's translucency model
	// ---------------------------------------------
	
		// Rim
		float rimIntensity = 0.1;
		float rimFactor = 1.0f - max(dot(eyeDir, N), 0.0f); 
		vec3 rim = vec3(smoothstep(0.6, 1.0, rimFactor))*rimIntensity;

		// Translucent
		float contrIntensity = 0.25f;
		float contribution = 0.75f - contrIntensity * dot(-L, eyeDir);
		float tIntensity = 5.0f;
		vec3 translucent = contribution * T(s,uExtintionCoef) * (vColor.rgb/ tIntensity );

		// Final lighting model
		vec3 modelB = blinPhong + (rim + translucent) * fLightAttenuation;

	// ---------------------------------------------
	// End
	// ---------------------------------------------

	if(uLightingModel == 0)
		fragColor = vec4(modelA,1.0f);
	else if(uLightingModel == 1)
		fragColor = vec4(modelB,1.0f);
	else if(uLightingModel == 2)
		fragColor = vec4(blinPhong,1.0f);

	if(uThicknessEnable) fragColor = vec4(vec3(s),1.0f);
	if(uTransmitanceEnable) fragColor = vec4(T(s,uExtintionCoef), 1.0f);
}
