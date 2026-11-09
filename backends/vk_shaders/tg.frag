#version 450
layout( location = 0 ) in vec2 vUV;
layout( location = 1 ) in vec4 vCol;
layout( location = 2 ) in vec2 vPos;
layout( set = 0, binding = 0 ) uniform sampler2D uTex;
layout( location = 0 ) out vec4 oCol;
void main() { oCol = vCol * texture( uTex, vUV ); }
