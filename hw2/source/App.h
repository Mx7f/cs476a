/**
  @file App.h

  The G3D 9.0 default starter app is configured for OpenGL 3.0 and relatively recent
  GPUs.  To support older GPUs you may want to disable the framebuffer and film
  classes and use G3D::Sky to handle the skybox.
 */
#ifndef App_h
#define App_h

#include <G3D/G3DAll.h>

// Compile RtAudio with windows direct sound support
#ifdef G3D_WINDOWS
#   define __WINDOWS_DS__
#endif
#include "RtAudio.h"

Array<float> g_currentAudioBuffer;

class App : public GApp {
protected:
    RtAudio m_rtAudio;
    float m_waveformWidth;


    struct AudioSettings {
      int numChannels;
      int sampleRate;
      RtAudioFormat rtAudioFormat;
      
      AudioSettings() :
        numChannels(1),
	  sampleRate(48000),
	rtAudioFormat(RTAUDIO_FLOAT32)
      {}
    } m_audioSettings;

    shared_ptr<Texture> m_rawAudioTexture;

    shared_ptr<Texture> m_frequencyAudioTexture;

    void initializeAudio();
public:

    App(const GApp::Settings& settings = GApp::Settings());

    virtual void onInit() override;
    virtual void onGraphics3D(RenderDevice* rd, Array< shared_ptr<Surface> >& surface) override;
    virtual void onGraphics2D(RenderDevice* rd, Array< shared_ptr<Surface2D> >& surface2D) override;

    virtual bool onEvent(const GEvent& e) override;
    virtual void onCleanup() override;

};

#endif
