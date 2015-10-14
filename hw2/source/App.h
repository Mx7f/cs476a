/**
  \file App.h

 */
#ifndef App_h
#define App_h

#include <G3D/G3DAll.h>

// Compile RtAudio with windows direct sound support
#ifdef G3D_WINDOWS
#   define __WINDOWS_DS__
#endif
#include "RtAudio.h"
#include "chuck_fft.h"

Array<float> g_currentAudioBuffer;

class App : public GApp {
protected:

    G3D_DECLARE_ENUM_CLASS(VisualizationMode, 
        SNDPEEK_ALIKE, 
        PARTICLES, 
        SHADERTOY,
        EYE);
    VisualizationMode m_visualizationMode;

    // Parallel for eyeModel.glsl
    G3D_DECLARE_ENUM_CLASS(EyeMode, 
			   RADIAL_FREQUENCY, 
			   RADIAL_WAVEFORM, 
			   RADIAL_FREQUENCY_HISTORY,
			   ANGULAR_FREQUENCY,
			   ANGULAR_WAVEFORM,
			   ANGULAR_WAVEFORM_SYMMETRY,
			   ANGULAR_WAVEFORM_RADIAL_FREQUENCY,
			   SPIRAL_FREQUENCY,
			   SPIRAL_FREQUENCY_HISTORY,
			   ANGULAR_WAVEFORM_SPIRAL_FREQUENCY_HISTORY,
			   COUNT);

    EyeMode m_eyeMode;

    struct EyeSettings {
        /** Width of the pupil in the eye */
        float pupilWidth;
        float angleOffsetTimeMultiplier;
        EyeSettings() : pupilWidth(0.15), angleOffsetTimeMultiplier(1.0) {}
    };
    EyeSettings m_eyeSettings;

    /** EWMA of RMS */
    float m_smoothedRootMeanSquare;
    bool m_useRootMeanSquarePupil;
    

    shared_ptr<Framebuffer> m_eyeFramebuffer;




    RtAudio m_rtAudio;
    float m_waveformWidth;

    /** Exponentially-weighted moving average of frequencies */
    struct EWMAFrequency {
        Array<float>        cpuData;
        shared_ptr<Texture> gpuData;
        // Update rule: freq_ewma = lerp(freq_current, freq_ewma, alpha)
        float               alpha;

        void init(float a, const Array<float>& newData, const String& name) {
            alpha = a;
            cpuData.appendPOD(newData);
            gpuData = Texture::createEmpty(name, newData.size(), 1, ImageFormat::R32F());
            upload();
        }

        void update(const Array<float>& newData) {
            alwaysAssertM(newData.size() == cpuData.size(), "Must have same size data for EWMAFrequency update");
            for (int i = 0; i < newData.size(); ++i) {
                cpuData[i] = lerp(newData[i], cpuData[i], alpha);
            }
            upload();
        }

        void upload() {
            debugAssert(notNull(gpuData));
            shared_ptr<CPUPixelTransferBuffer> ptb = CPUPixelTransferBuffer::fromData(cpuData.size(), 1, ImageFormat::R32F(), cpuData.getCArray());
            gpuData->update(ptb);
        }
    };

    EWMAFrequency m_fastMovingAverage;
    EWMAFrequency m_slowMovingAverage;
    EWMAFrequency m_glacialMovingAverage;



    struct AudioSettings {
      int numChannels;
      int sampleRate;
      RtAudioFormat rtAudioFormat;
      
      AudioSettings() :
          numChannels(1),
          sampleRate(48000),
          rtAudioFormat(RTAUDIO_FLOAT32) {}
    } m_audioSettings;

    int m_maxSavedTimeSlices;

    Array<float> m_cpuRawAudioData;
    Array<complex> m_cpuFrequencyAudioData;

    shared_ptr<Texture> m_rawAudioTexture;

    shared_ptr<Texture> m_frequencyAudioTexture;


    Array<String>   m_shadertoyShaders;
    int             m_shadertoyShaderIndex;


    void setAudioShaderArgs(Args& args);

    void drawLineGraphFromRawSamples(RenderDevice* rd);

    void drawLineGraphFromFrequencyMagnitude(RenderDevice* rd);

    void initializeAudio();

    /** Called from onInit */
    void makeGUI();

    /** Called once a frame to get the latest audio data and compute statistics */
    void updateAudioData();


public:

    App(const GApp::Settings& settings = GApp::Settings());

    virtual void onInit() override;
    virtual void onGraphics3D(RenderDevice* rd, Array< shared_ptr<Surface> >& surface) override;
    virtual void onGraphics2D(RenderDevice* rd, Array< shared_ptr<Surface2D> >& surface2D) override;

    virtual bool onEvent(const GEvent& e) override;
    virtual void onCleanup() override;

    virtual void onUserInput(UserInput* ui) override;

    virtual void onSimulation(RealTime rdt, SimTime sdt, SimTime idt) override;

};

#endif
