#version 430

vec2 vertices[3] = vec2[](
    {0, -1.0},
    {-0.866, -0.5},
    {0.866, -0.5},
);

int main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
}
