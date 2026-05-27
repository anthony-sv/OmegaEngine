#version 460 core
in vec3 v_Color;
in vec2 v_TexCoord;

out vec4 o_Color;

uniform sampler2D u_Texture;

void main() {
    o_Color = texture(u_Texture, v_TexCoord) * vec4(v_Color, 1.0);
}