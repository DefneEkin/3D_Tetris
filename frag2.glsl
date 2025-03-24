#version 330 core

vec3 I = vec3(1, 1, 1);          // point light intensity
vec3 Iamb = vec3(0.1, 0.1, 0.1); // ambient light intensity
//vec3 kd = vec3(0.86, 0.11, 0.31);     // diffuse reflectance coefficient
vec3 ka = vec3(0.1, 0.1, 0.1);   // ambient reflectance coefficient
vec3 ks = vec3(0.5, 0.5, 0.5);   // specular reflectance coefficient

uniform vec3 lightPos;
uniform vec3 eyePos;
uniform vec3 kd;

in vec4 fragWorldPos;
flat in vec3 fragWorldNor;

out vec4 fragColor;

void main(void)
{
	// Compute lighting. We assume lightPos and eyePos are in world
	// coordinates. fragWorldPos and fragWorldNor are the interpolated
	// coordinates by the rasterizer.

	vec3 L = normalize(lightPos - vec3(fragWorldPos));
	vec3 V = normalize(eyePos - vec3(fragWorldPos));
	vec3 H = normalize(L + V);
	vec3 N = normalize(fragWorldNor);

	float NdotL = dot(N, L); // for diffuse component
	float NdotH = dot(N, H); // for specular component

	vec3 diffuseColor = I * kd * max(0, NdotL);
	vec3 specularColor = I * ks * pow(max(0, NdotH), 100);
	vec3 ambientColor = Iamb * ka;

	//fragColor = vec4(diffuseColor + specularColor + ambientColor, 1);
	fragColor = vec4(1, 1, 1, 1);
}
