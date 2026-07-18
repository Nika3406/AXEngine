#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform vec4 uColor=vec4(1,0.2,0.05,0.75); uniform float uFrame=0.0;
void main(){ float edge=smoothstep(0.0,0.08,vUV.x)*smoothstep(1.0,0.88,vUV.x); FragColor=vec4(uColor.rgb*uColor.a*edge,uColor.a*edge); }
