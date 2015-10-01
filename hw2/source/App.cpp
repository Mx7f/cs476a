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
    std::cout << "No audio devices found!" << std::endl;
    exit( 1 );
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

    initializeAudio();
    m_rawAudioTexture = Texture::createEmpty("Raw Audio Texture", g_currentAudioBuffer.size(), 1, ImageFormat::R32F());

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


void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& surface3D) {
  shared_ptr<CPUPixelTransferBuffer> ptb = CPUPixelTransferBuffer::fromData(g_currentAudioBuffer.size(), 1, ImageFormat::R32F(), g_currentAudioBuffer.getCArray());
  m_rawAudioTexture->update(ptb);

    debugAssertGLOk();
    rd->swapBuffers();
    debugAssertGLOk();
    rd->clear();
    debugAssertGLOk();
    
    rd->push2D(); {
      Args args;
      m_rawAudioTexture->setShaderArgs(args, "rawAudioTexture", Sampler::video());
      args.setRect(rd->viewport());
      LAUNCH_SHADER("rawAudioVisualize.pix", args);
    } rd->pop2D();

    //    Draw::axes(CoordinateFrame(Vector3(0, 0, 0)), rd);
    debugAssertGLOk();

    // Call to make the GApp show the output of debugDraw
    drawDebugShapes();
    debugAssertGLOk();
}


void App::onGraphics2D(RenderDevice* rd, Array<Surface2D::Ref>& posed2D) {
    // Render 2D objects like Widgets.  These do not receive tone mapping or gamma correction
    Surface2D::sortAndRender(rd, posed2D);
}
