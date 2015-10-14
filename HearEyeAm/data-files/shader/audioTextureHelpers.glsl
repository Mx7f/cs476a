#ifndef audioTextureHelpers_glsl
#define audioTextureHelpers_glsl
#include <Texture/Texture.glsl>
uniform_Texture(sampler2D, frequencyAudio_);
uniform_Texture(sampler2D, rawAudio_);
uniform_Texture(sampler2D, fastEWMAfreq_);
uniform_Texture(sampler2D, slowEWMAfreq_);
uniform_Texture(sampler2D, glacialEWMAfreq_);


// A bunch of helper methods for sampling from the audio textures and perhaps doing a transform on the data
float sampleRawAudio(float coord, int time) {
    return textureLod(rawAudio_buffer, vec2(coord, (rawAudio_size.y - time - 0.5)*rawAudio_invSize.y), 0).x;
}

vec2 sampleFrequencyAudio(float coord, float time) {
    return textureLod(frequencyAudio_buffer, vec2(coord, (frequencyAudio_size.y - time - 0.5)*frequencyAudio_invSize.y), 0).xy;
}

float sampleFrequencyMagnitudeAudio(float coord, float time) {
    return length(sampleFrequencyAudio(coord, time));
}

float log10(float x) {
    return log(x) / log(10.0);
}

float sampleFrequencyDbAudio(sampler2D s, float coord) {
    return 20.0*log10(textureLod(s, vec2(coord, 0.5), 0).x*frequencyAudio_invSize.x);
}

//https://groups.google.com/forum/#!topic/comp.dsp/cZsS1ftN5oI
float sampleFrequencyDbAudio(float coord, float time) {
    return 20.0*log10(sampleFrequencyMagnitudeAudio(coord, time)*frequencyAudio_invSize.x);
}

float sampleFrequencyDbAudioOverNFrames(float coord, float scale, int n) {
    float sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += sampleFrequencyDbAudio(coord, i);
    }
    return sum / float(n);
}

//https://www.youtube.com/watch?v=R5rkg8mTRBI
float sampleFrequencyRescaledDbAudio(float coord, float scale, int time) {
    return (sampleFrequencyDbAudio(coord, time) + scale) / scale;
}

float sampleExactFrequencyRescaledDbAudio(int coord, float scale, int time) {
    vec2 frequency = texelFetch(frequencyAudio_buffer, ivec2(coord, frequencyAudio_size.y - time - 1), 0).xy;
    return (20.0*log10(length(frequency)*frequencyAudio_invSize.x) + scale) / scale;
}

// Slow, only use for prototyping
float sampleAverageFreqRescaledDbOverNFrames(float coord, float scale, int n) {
    float sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += sampleFrequencyRescaledDbAudio(coord, scale, i);
    }
    return sum / float(n);
}

// Slow, only use for prototyping
float sampleAverageFreqMagnitudeOverNFrames(float coord, int n) {
      float sum = 0.0;
      for (int i = 0; i < n; ++i) {
      	  sum += sampleFrequencyMagnitudeAudio(coord, i);
      }
      return sum / float(n);     
}

#endif