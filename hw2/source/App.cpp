/** \file App.cpp */
#include "App.h"

// Tells C++ to invoke command-line main() function even on OS X and Win32.
G3D_START_AT_MAIN();

int main(int argc, const char* argv[]) {
    (void)argc; (void)argv;
    GApp::Settings settings(argc, argv);
    
    // Change the window and other startup parameters by modifying the
    // settings class.  For example:
    settings.window.width       = 1280; 
    settings.window.height      = 720;
    settings.window.asynchronous = false;


    return App(settings).run();
}


App::App(const GApp::Settings& settings) : GApp(settings) {
    renderDevice->setColorClearValue(Color3::white());
}

void App::onCleanup() {
  m_rtAudio.stopStream();
  if( m_rtAudio.isStreamOpen() )
    m_rtAudio.closeStream();
}

int audioCallback( void * outputBuffer, void * inputBuffer, unsigned int numFrames,
            double streamTime, RtAudioStreamStatus status, void * data ) {
  float * output = (float *)outputBuffer;
  float * input  = (float *)inputBuffer;
  size_t numBytes = numFrames * sizeof(float);
  memset(output, 0, numBytes);
  // Copy input buffer, handle race condition by not caring about it.
  memcpy(g_currentAudioBuffer.getCArray(), input, numBytes);
  return 0;
}



void App::initializeAudio() {

  unsigned int bufferByteCount = 0;
  unsigned int bufferFrameCount = 512;
    
  // Check for audio devices
  if( m_rtAudio.getDeviceCount() < 1 ) {
    // None :(
    debugPrintf("No audio devices found!\n");
    exit( 1 );
  }
  RtAudio::DeviceInfo info;

  unsigned int devices = m_rtAudio.getDeviceCount();
  std::cout << "\nFound " << devices << " device(s) ...\n";

  for (unsigned int i=0; i<devices; i++) {
    info = m_rtAudio.getDeviceInfo(i);

    std::cout << "\nDevice Name = " << info.name << '\n';
    if ( info.probed == false )
      std::cout << "Probe Status = UNsuccessful\n";
    else {
      std::cout << "Probe Status = Successful\n";
      std::cout << "Output Channels = " << info.outputChannels << '\n';
      std::cout << "Input Channels = " << info.inputChannels << '\n';
      std::cout << "Duplex Channels = " << info.duplexChannels << '\n';
      if ( info.isDefaultOutput ) std::cout << "This is the default output device.\n";
      else std::cout << "This is NOT the default output device.\n";
      if ( info.isDefaultInput ) std::cout << "This is the default input device.\n";
      else std::cout << "This is NOT the default input device.\n";
      if ( info.nativeFormats == 0 )
	std::cout << "No natively supported data formats(?)!";
      else {
	std::cout << "Natively supported data formats:\n";
        if ( info.nativeFormats & RTAUDIO_SINT8 )
	  std::cout << "  8-bit int\n";
        if ( info.nativeFormats & RTAUDIO_SINT16 )
	  std::cout << "  16-bit int\n";
        if ( info.nativeFormats & RTAUDIO_SINT24 )
	  std::cout << "  24-bit int\n";
        if ( info.nativeFormats & RTAUDIO_SINT32 )
	  std::cout << "  32-bit int\n";
        if ( info.nativeFormats & RTAUDIO_FLOAT32 )
	  std::cout << "  32-bit float\n";
        if ( info.nativeFormats & RTAUDIO_FLOAT64 )
	  std::cout << "  64-bit float\n";
      }
      if ( info.sampleRates.size() < 1 )
	std::cout << "No supported sample rates found!";
      else {
	std::cout << "Supported sample rates = ";
        for (unsigned int j=0; j<info.sampleRates.size(); j++)
	  std::cout << info.sampleRates[j] << " ";
      }
      std::cout << std::endl;
    }
  }

  // Let RtAudio print messages to stderr.
  m_rtAudio.showWarnings( true );

  // Set input and output parameters
  RtAudio::StreamParameters iParams, oParams;
  iParams.deviceId = m_rtAudio.getDefaultInputDevice();
  iParams.nChannels = m_audioSettings.numChannels;
  iParams.firstChannel = 0;
  oParams.deviceId = m_rtAudio.getDefaultOutputDevice();
  oParams.nChannels = m_audioSettings.numChannels;
  oParams.firstChannel = 0;
    
  // Create stream options
  RtAudio::StreamOptions options;

  g_currentAudioBuffer.resize(bufferFrameCount);
  try {
    // Open a stream
    m_rtAudio.openStream( &oParams, &iParams, m_audioSettings.rtAudioFormat, m_audioSettings.sampleRate, &bufferFrameCount, &audioCallback, (void *)&bufferByteCount, &options );
  } catch( RtAudioError& e ) {
    // Failed to open stream
    std::cout << e.getMessage() << std::endl;
    exit( 1 );
  }
  g_currentAudioBuffer.resize(bufferFrameCount);
  m_rtAudio.startStream();

}

void App::onInit() {
    GApp::onInit();

    m_maxSavedTimeSlices = 512;
    m_waveformWidth = 8.0f;
    initializeAudio();
    m_rawAudioTexture = Texture::createEmpty("Raw Audio Texture", g_currentAudioBuffer.size(), 1, ImageFormat::R32F());

    m_frequencyAudioTexture = Texture::createEmpty("Frequency Audio Texture", g_currentAudioBuffer.size()/2, 1, ImageFormat::RG32F());


    // Turn on the developer HUD
    createDeveloperHUD();
    debugWindow->setVisible(true);
    developerWindow->setVisible(true);
    developerWindow->cameraControlWindow->setVisible(false);
    showRenderingStats = false;

}


bool App::onEvent(const GEvent& e) {
    if (GApp::onEvent(e)) {
        return true;
    }
    // If you need to track individual UI events, manage them here.
    // Return true if you want to prevent other parts of the system
    // from observing this specific event.
    //
    // For example,
    // if ((e.type == GEventType::GUI_ACTION) && (e.gui.control == m_button)) { ... return true;}
    // if ((e.type == GEventType::KEY_DOWN) && (e.key.keysym.sym == GKey::TAB)) { ... return true; }

    return false;
}

void App::setAudioShaderArgs(Args& args) {
   m_rawAudioTexture->setShaderArgs(args, "rawAudio_", Sampler::video());
   m_frequencyAudioTexture->setShaderArgs(args, "frequencyAudio_", Sampler::video());
}

void App::drawLineGraphFromRawSamples(RenderDevice* rd) {
  Color3 color = Color3::green();
  float yOffset = 1.2f;
  Args args;
  args.setUniform("color", color);
  args.setUniform("waveformWidth", m_waveformWidth);
  args.setUniform("yOffset", yOffset);
  setAudioShaderArgs(args);
  args.setPrimitiveType(PrimitiveType::LINE_STRIP);
  args.setNumIndices(m_rawAudioTexture->width());
  LAUNCH_SHADER("visualizeLines.*", args);
}


void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& surface3D) {
  int sampleCount = g_currentAudioBuffer.size();
  int freqCount = sampleCount/2;
  m_cpuRawAudioData.appendPOD(g_currentAudioBuffer);

  int numStoredTimeSlices = m_cpuRawAudioData.size()/sampleCount;  
  shared_ptr<CPUPixelTransferBuffer> ptb = CPUPixelTransferBuffer::fromData(sampleCount, numStoredTimeSlices, ImageFormat::R32F(), m_cpuRawAudioData.getCArray());
  m_rawAudioTexture->resize(sampleCount, numStoredTimeSlices);
  m_rawAudioTexture->update(ptb);

  Array<complex> frequency;
  frequency.resize(freqCount);
  float* currentRawAudioDataPtr = m_cpuRawAudioData.getCArray() + (m_cpuRawAudioData.size() - sampleCount);
  memcpy(frequency.getCArray(), currentRawAudioDataPtr, sizeof(float)*sampleCount);
  rfft( (float*)frequency.getCArray(), frequency.size(), FFT_FORWARD );
  m_cpuFrequencyAudioData.appendPOD(frequency);
  
  shared_ptr<CPUPixelTransferBuffer> freqPTB = CPUPixelTransferBuffer::fromData(freqCount, numStoredTimeSlices, ImageFormat::RG32F(), m_cpuFrequencyAudioData.getCArray());

    m_frequencyAudioTexture->resize(freqCount, numStoredTimeSlices);
    m_frequencyAudioTexture->update(freqPTB);
    
    if (numStoredTimeSlices == m_maxSavedTimeSlices) {
      int newTotalSampleCount = sampleCount*(numStoredTimeSlices-1);
      int newTotalFrequencyCount = (sampleCount/2)*(numStoredTimeSlices-1);
      for (int i = 0; i < newTotalSampleCount; ++i) {
	m_cpuRawAudioData[i] = m_cpuRawAudioData[i+sampleCount];
      }
      m_cpuRawAudioData.resize(newTotalSampleCount);
      for (int i = 0; i < newTotalFrequencyCount; ++i) {
	m_cpuFrequencyAudioData[i] = m_cpuFrequencyAudioData[i+freqCount];
      }
      m_cpuFrequencyAudioData.resize(newTotalFrequencyCount);
    }


    debugAssertGLOk();
    rd->swapBuffers();
    debugAssertGLOk();
    rd->clear();
    debugAssertGLOk();
    
    rd->push2D(); {
      Args args;
      args.setUniform("invScreenWidth", 1.0f / rd->viewport().width());
      m_rawAudioTexture->setShaderArgs(args, "rawAudio_", Sampler::video());
      args.setRect(rd->viewport());
      LAUNCH_SHADER("rawAudioVisualize.pix", args);
    } rd->pop2D();

    rd->pushState(m_framebuffer); {
      rd->setColorClearValue(Color3::black());
        rd->clear();
        rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());

	drawLineGraphFromRawSamples(rd);
	//	drawLineGraph(Color3::blue(), -1.2f, m_frequencyAudioTexture);
    } rd->popState();

    //    Draw::axes(CoordinateFrame(Vector3(0, 0, 0)), rd);
    debugAssertGLOk();
    activeCamera()->filmSettings().setAntialiasingEnabled(false);
    m_film->exposeAndRender(rd, activeCamera()->filmSettings(), m_framebuffer->texture(0));
    // Call to make the GApp show the output of debugDraw
    drawDebugShapes();
    debugAssertGLOk();
}


void App::onGraphics2D(RenderDevice* rd, Array<Surface2D::Ref>& posed2D) {
    // Render 2D objects like Widgets.  These do not receive tone mapping or gamma correction
    Surface2D::sortAndRender(rd, posed2D);
}
