#version 330 core

uniform mat4 ciModelView;
uniform mat4 ciProjectionMatrix;
uniform mat3 ciNormalMatrix;

in vec4 ciPosition;
in vec3 ciNormal;
in vec4 ciColor;

in mat4 iModelMatrix;
in vec3 iColor;

// Fragment 
out vec4 vColor;
out vec3 vFragPos;
out vec3 vFragNormal;

// Light & Depth Map
out vec4 vDepthMapCoord;
uniform mat4 uLightViewProjMatrix;

void main()
{
	// kLightPosition position in eye space (relative to camera)
	vColor = vec4(iColor, 0.0f);

	// Fragment postion world space
	vFragPos = vec3(iModelMatrix * ciPosition);
	vFragNormal = mat3(transpose(inverse(iModelMatrix))) * ciNormal;

	// lightViewMatrix Frag
	vDepthMapCoord = (uLightViewProjMatrix * iModelMatrix) * ciPosition;

	gl_Position = ciProjectionMatrix * ciModelView * vec4(vFragPos, 1.0f);
}
