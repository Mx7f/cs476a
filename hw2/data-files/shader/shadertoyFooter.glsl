void main() {
    vec2 fragCoord = vec2(gl_FragCoord.x, iResolution.y - gl_FragCoord.y - 1);
    mainImage(result, fragCoord);
}