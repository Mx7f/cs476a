in vec2 g3d_TexCoord;
void main() {
    vec2 fragCoord = vec2(g3d_TexCoord.x, 1.0- g3d_TexCoord.y) * iResolution;
    mainImage(result, fragCoord);
}