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
    settings.dataDir = FileSystem::currentDirectory();
    settings.screenshotDirectory = "../journal/";

    settings.renderer.deferredShading = false;
    settings.renderer.orderIndependentTransparency = true;


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

    m_shadertoyShaderIndex = 0;
    m_shadertoyShaders.append("sunShader.pix", "cubescape.pix", "fractalLand.pix");

    m_maxSavedTimeSlices = 512;
    m_waveformWidth = 7.9f;
    initializeAudio();
    m_rawAudioTexture = Texture::createEmpty("Raw Audio Texture", g_currentAudioBuffer.size(), 1, ImageFormat::R32F());

    m_frequencyAudioTexture = Texture::createEmpty("Frequency Audio Texture", g_currentAudioBuffer.size()/2, 1, ImageFormat::RG32F());

    makeGUI();
    loadScene("Visualizer");
}

void App::makeGUI() {
    // Initialize the developer HUD (using the existing scene)
    createDeveloperHUD();
    debugWindow->setVisible(true);
    developerWindow->videoRecordDialog->setEnabled(true);
    debugPane->beginRow(); {
        debugPane->addEnumClassRadioButtons<VisualizationMode>("Mode", &m_visualizationMode);
    } debugPane->endRow();
    GuiDropDownList* list = debugPane->addDropDownList("Shadertoy Shader", m_shadertoyShaders, &m_shadertoyShaderIndex);
    list->setCaptionWidth(100);
    debugPane->pack();


    debugWindow->pack();
    debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
    developerWindow->cameraControlWindow->setVisible(false);
    showRenderingStats = false;
    developerWindow->cameraControlWindow->moveTo(Point2(developerWindow->cameraControlWindow->rect().x0(), 0));


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
    Color3 color = Color3::green() * 1.1f;;
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

void App::drawLineGraphFromFrequencyMagnitude(RenderDevice* rd) {
    Color3 color = Color3::blue() * 1.3f;
    float yOffset = -1.6f;
    Args args;
    args.setUniform("color", color);
    args.setUniform("waveformWidth", m_waveformWidth);
    args.setUniform("yOffset", yOffset);
    setAudioShaderArgs(args);
    args.setPrimitiveType(PrimitiveType::LINE_STRIP);
    int numTimeSlices = m_frequencyAudioTexture->height() / 4;
    args.setUniform("numTimeSlices", float(numTimeSlices));

    args.setNumIndices(m_frequencyAudioTexture->width());
    args.setNumInstances(numTimeSlices);
    LAUNCH_SHADER("visualizeFrequencyMagnitude.*", args);
}

void App::updateAudioData() {
    int sampleCount = g_currentAudioBuffer.size();
    int freqCount = sampleCount / 2;
    m_cpuRawAudioData.appendPOD(g_currentAudioBuffer);

    int numStoredTimeSlices = m_cpuRawAudioData.size() / sampleCount;
    shared_ptr<CPUPixelTransferBuffer> ptb = CPUPixelTransferBuffer::fromData(sampleCount, numStoredTimeSlices, ImageFormat::R32F(), m_cpuRawAudioData.getCArray());
    m_rawAudioTexture->resize(sampleCount, numStoredTimeSlices);
    m_rawAudioTexture->update(ptb);

    Array<complex> frequency;
    frequency.resize(freqCount);
    float* currentRawAudioDataPtr = m_cpuRawAudioData.getCArray() + (m_cpuRawAudioData.size() - sampleCount);
    memcpy(frequency.getCArray(), currentRawAudioDataPtr, sizeof(float)*sampleCount);
    rfft((float*)frequency.getCArray(), frequency.size(), FFT_FORWARD);
    m_cpuFrequencyAudioData.appendPOD(frequency);

    shared_ptr<CPUPixelTransferBuffer> freqPTB = CPUPixelTransferBuffer::fromData(freqCount, numStoredTimeSlices, ImageFormat::RG32F(), m_cpuFrequencyAudioData.getCArray());

    m_frequencyAudioTexture->resize(freqCount, numStoredTimeSlices);
    m_frequencyAudioTexture->update(freqPTB);

    if (numStoredTimeSlices == m_maxSavedTimeSlices) {
        int newTotalSampleCount = sampleCount*(numStoredTimeSlices - 1);
        int newTotalFrequencyCount = (sampleCount / 2)*(numStoredTimeSlices - 1);
        for (int i = 0; i < newTotalSampleCount; ++i) {
            m_cpuRawAudioData[i] = m_cpuRawAudioData[i + sampleCount];
        }
        m_cpuRawAudioData.resize(newTotalSampleCount);
        for (int i = 0; i < newTotalFrequencyCount; ++i) {
            m_cpuFrequencyAudioData[i] = m_cpuFrequencyAudioData[i + freqCount];
        }
        m_cpuFrequencyAudioData.resize(newTotalFrequencyCount);
    }
}

void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& allSurfaces) {

    if (!scene()) {
        return;
    }


    debugAssertGLOk();
    GApp::swapBuffers();
    debugAssertGLOk();
    rd->clear();
    debugAssertGLOk();
    FilmSettings filmSettings = activeCamera()->filmSettings();


    switch (m_visualizationMode) {
    case VisualizationMode::SNDPEEK_ALIKE:
        filmSettings.setAntialiasingEnabled(false);
        rd->pushState(m_framebuffer); {
            rd->setColorClearValue(Color3::black());
            rd->clear();
            rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());

            drawLineGraphFromRawSamples(rd);
            drawLineGraphFromFrequencyMagnitude(rd);
            //	drawLineGraph(Color3::blue(), -1.2f, m_frequencyAudioTexture);
        } rd->popState();
        break;
    case VisualizationMode::PARTICLES:
        m_gbuffer->setSpecification(m_gbufferSpecification);
        m_gbuffer->resize(m_framebuffer->width(), m_framebuffer->height());
        m_gbuffer->prepare(rd, activeCamera(), 0, -(float)previousSimTimeStep(), m_settings.depthGuardBandThickness, m_settings.colorGuardBandThickness);

        m_renderer->render(rd, m_framebuffer, m_depthPeelFramebuffer, scene()->lightingEnvironment(), m_gbuffer, allSurfaces);

        break;
    case VisualizationMode::SHADERTOY:
        filmSettings.setBloomStrength(0.0f);
        rd->push2D(m_framebuffer); {
            rd->setColorClearValue(Color3::black());
            rd->clear();
            
            Args args;
            args.setUniform("iResolution", rd->viewport().wh());
            args.setUniform("iGlobalTime", scene()->time());
            
            setAudioShaderArgs(args);
            args.setRect(rd->viewport());
            
            const shared_ptr<G3D::Shader> theShader = G3D::Shader::getShaderFromPattern(m_shadertoyShaders[m_shadertoyShaderIndex]);
	        G3D::RenderDevice::current->apply(theShader, args);
        } rd->pop2D();
        
        break;
    }
	    


    //    Draw::axes(CoordinateFrame(Vector3(0, 0, 0)), rd);
    debugAssertGLOk();
    
    m_film->exposeAndRender(rd, filmSettings, m_framebuffer->texture(0));
    // Call to make the GApp show the output of debugDraw
    drawDebugShapes();
    debugAssertGLOk();
}

void App::onUserInput(UserInput* ui) {
    GApp::onUserInput(ui);
    (void)ui;
    // Add key handling here based on the keys currently held or
    // ones that changed in the last frame.
}

void App::onGraphics2D(RenderDevice* rd, Array<Surface2D::Ref>& posed2D) {
    // Render 2D objects like Widgets.  These do not receive tone mapping or gamma correction
    Surface2D::sortAndRender(rd, posed2D);
}

void App::onSimulation(RealTime rdt, SimTime sdt, SimTime idt) {
    GApp::onSimulation(rdt, sdt, idt);
    updateAudioData();
    if (m_visualizationMode == VisualizationMode::PARTICLES) {
        int sampleCount = g_currentAudioBuffer.size();
        int freqCount = sampleCount / 2;
        shared_ptr<ParticleSystem> ps = scene()->typedEntity<ParticleSystem>("pulsar");
        shared_ptr<ParticleSystemModel> model = dynamic_pointer_cast<ParticleSystemModel>(ps->model());
        ParticleSystemModel::Emitter::Specification spec = model->emitterArray()[0]->specification();
        shared_ptr<ParticleMaterial> pm = ParticleMaterial::create(spec.material);
        Random& rng = Random::common();
        for (int i = m_cpuFrequencyAudioData.size() - freqCount; i < m_cpuFrequencyAudioData.size(); ++i) {

            ParticleSystem::Particle particle;
            particle.emitterIndex = 0;
            particle.angle = 0.0f;
            particle.radius = fabs(rng.gaussian(spec.radiusMean, sqrt(spec.radiusVariance)));

            if (spec.coverageFadeInTime == 0.0f) {
                particle.coverage = 1.0f;
            } else {
                particle.coverage = 0.0f;
            }

            particle.userdataFloat = 0.0f;
            particle.mass = spec.particleMassDensity * (4.0f / 3.0f) * pif() * particle.radius * particle.radius * particle.radius;

            complex c = m_cpuFrequencyAudioData[i];
            particle.velocity = Vector3(c.re, c.im, 0.0f) * 10.f;
            
            SimTime absoluteTime = scene()->time();
            particle.spawnTime = absoluteTime;
            particle.expireTime = absoluteTime + fabs(rng.gaussian(spec.particleLifetimeMean, spec.particleLifetimeVariance));

            particle.dragCoefficient = spec.dragCoefficient;
            particle.material = pm;

            particle.userdataInt = 0;

            if (ps->particlesAreInWorldSpace()) {
                // Transform to world space
                particle.position = ps->frame().pointToWorldSpace(particle.position);
                particle.velocity = ps->frame().vectorToWorldSpace(particle.velocity);
            }
            if (particle.velocity.length() > 0.1f) {
                ps->addParticle(particle);
            }
        }


    }


    // Example GUI dynamic layout code.  Resize the debugWindow to fill
    // the screen horizontally.
    debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
}