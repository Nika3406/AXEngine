$input v_normal

#include <bgfx_shader.sh>

uniform vec4 u_color;

void main()
{
	// Simple lighting calculation
	vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
	float NdotL = max(dot(normalize(v_normal), lightDir), 0.0);
	float ambient = 0.3;
	float lighting = ambient + (1.0 - ambient) * NdotL;
	
	gl_FragColor = vec4(u_color.rgb * lighting, u_color.a);
}