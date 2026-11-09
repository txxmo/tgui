#version 450
// tg Vulkan renderer: UI-unit positions -> clip space (top-left origin, y down like Vulkan's)
layout( location = 0 ) in vec2 aPos;
layout( location = 1 ) in vec2 aUV;
layout( location = 2 ) in vec4 aCol;
layout( push_constant ) uniform PC { vec4 uProj; float uScale; float Time; vec2 Resolution; };
layout( location = 0 ) out vec2 vUV;
layout( location = 1 ) out vec4 vCol;
layout( location = 2 ) out vec2 vPos;
void main()
{
    vUV = aUV;
    vCol = aCol;
    vPos = aPos * uScale;
    gl_Position = vec4( aPos * uProj.xy + uProj.zw, 0.0, 1.0 );
}
