#ifndef audioTextureHelpers_glsl
#define audioTextureHelpers_glsl
#include <Texture/Texture.glsl>
uniform_Texture(sampler2D, frequencyAudio_);
uniform_Texture(sampler2D, rawAudio_);

float sampleRawAudio(float coord, int time) {
    return texture(rawAudio_buffer, vec2(coord, (rawAudio_size.y - time - 0.5)*rawAudio_invSize.y)).x;
}

vec2 sampleFrequencyAudio(float coord, int time) {
    return texture(frequencyAudio_buffer, vec2(coord, (frequencyAudio_size.y - time - 0.5)*frequencyAudio_invSize.y)).xy;
}

float sampleFrequencyMagnitudeAudio(float coord, int time) {
    return length(sampleFrequencyAudio(coord, time));
}

float sampleAverageFreqMagnitudeOverNFrames(float coord, int n) {
      float sum = 0.0;
      for (int i = 0; i < n; ++i) {
      	  sum += sampleFrequencyMagnitudeAudio(coord, i);
      }
      return sum / float(n);     
}

#endif