/**
  \file App.h

 */
#ifndef App_h
#define App_h

#include <G3D/G3DAll.h>

#ifdef G3D_WINDOWS
 // Compile RtAudio with windows direct sound support
#   define __WINDOWS_DS__
#endif
#include "RtAudio.h"
#include "chuck_fft.h"

/** Global array where we dump raw audio data from RtAudio */
Array<float> g_currentAudioBuffer;

class App : public GApp {
protected:
    RtAudio m_rtAudio;
    /** While prototyping, I wanted many modes that worked in completely different ways */
    G3D_DECLARE_ENUM_CLASS(VisualizationMode, 
        SNDPEEK_ALIKE, 
        PARTICLES, 
        SHADERTOY,
        EYE,
        TWO_EYES);
    VisualizationMode m_visualizationMode;

    // Parallel for eyeModel.glsl
    // ANGULAR_WAVEFORM_SPIRAL_FREQUENCY_HISTORY is baller
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
    

    /** Simple settings for our eye shader */
    struct EyeSettings {
        /** Mode */
        EyeMode mode;

        /** Width of the pupil in the eye */
        float pupilWidth;
        /** How fast do we rotate angular values? */
        float angleOffsetTimeMultiplier;
        /** If true, pupil radius varies with RMS */
        bool useRootMeanSquarePupil;
        EyeSettings() : pupilWidth(0.15), angleOffsetTimeMultiplier(1.0), useRootMeanSquarePupil(true),
            mode(EyeMode::ANGULAR_WAVEFORM_SPIRAL_FREQUENCY_HISTORY) {}
        
        /** Randomize settings */
        void randomize() {
            mode = EyeMode(Random::common().integer(0, EyeMode::COUNT - 1));
            pupilWidth = Random::common().uniform() * 0.5f;
            angleOffsetTimeMultiplier = (Random::common().uniform() > 0.8f) ? 0.0f : Random::common().uniform() * 2.0f;
            useRootMeanSquarePupil = (Random::common().uniform() > 0.2f);
        }
    };
    EyeSettings m_eyeSettings;
    EyeSettings m_secondaryEyeSettings;


    /** EWMA of RMS */
    float m_smoothedRootMeanSquare;
    
    
    /** A separate framebuffer to render the eye texture to, just in case we wnat to re-use it */
    shared_ptr<Framebuffer> m_eyeFramebuffer;


    /** Just a multiplier to stretch out the sndpeek-like visualizations to cover the whole screen */
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

    /** 3 different rates of exponentially-weighted moving averages of frequencies */
    EWMAFrequency m_fastMovingAverage;
    EWMAFrequency m_slowMovingAverage;
    EWMAFrequency m_glacialMovingAverage;


    /** Settings for RtAudio. We never need to change the defaults */
    struct AudioSettings {
      int numChannels;
      int sampleRate;
      RtAudioFormat rtAudioFormat;
      
      AudioSettings() :
          numChannels(1),
          sampleRate(48000),
          rtAudioFormat(RTAUDIO_FLOAT32) {}
    } m_audioSettings;

    /** How many slices of time to save */
    int m_maxSavedTimeSlices;

    /** CPU storage of raw samples */
    Array<float> m_cpuRawAudioData;
    /** CPU storage of fft samples */
    Array<complex> m_cpuFrequencyAudioData;

    /** GPU storage of raw samples */
    shared_ptr<Texture> m_rawAudioTexture;
    /** GPU storage of fft samples */
    shared_ptr<Texture> m_frequencyAudioTexture;

    /** For a dropdown for choosing shaders in shadertoy mode, this was only used during prototyping */
    Array<String>   m_shadertoyShaders;
    int             m_shadertoyShaderIndex;

    /** Set all our audio textures on \param Args */
    void setAudioShaderArgs(Args& args);

    /** Does what it says on the tin. Corresponds to the upper half of sndpeek */
    void drawLineGraphFromRawSamples(RenderDevice* rd);

    /** Does what it says on the tin. Corresponds to the lower half of sndpeek */
    void drawLineGraphFromFrequencyMagnitude(RenderDevice* rd);

    /** Do all of the interaction with RtAudio that we need to to set up realtime audio capture */
    void initializeAudio();

    /** Draw a single eye using our special eye shader configured with the options passed in as parameters */
    void drawEye(RenderDevice* rd, const Rect2D& rect, const EyeSettings& settings);

    /** Called from onInit */
    void makeGUI();

    /** Called once a frame to get the latest audio data and compute statistics such as RMS */
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
