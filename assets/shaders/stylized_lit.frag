#version 330 core
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;
uniform vec4 uBaseColor = vec4(1.0);
uniform vec3 uLightDir = normalize(vec3(-0.4, -1.0, -0.3));
uniform int uCel = 0;
uniform vec3 uEmission = vec3(0.0);
void main(){ float ndl=max(dot(normalize(vNormal), -uLightDir),0.0); if(uCel==1) ndl=floor(ndl*4.0)/3.0; FragColor=vec4(uBaseColor.rgb*(0.18+0.82*ndl)+uEmission,uBaseColor.a); }
