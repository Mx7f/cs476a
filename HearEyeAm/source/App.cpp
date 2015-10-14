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
    settings.window.resizable = true;
    settings.window.caption = "HearEyeAm";
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

    printf("Welcome to HearEyeAm.\nUse ',' and '.' to toggle the different eye modes.\nUse 'r' to randomize the eye settings.\nUse 'e' to switch between monocular and binocular visualiztion. :)\n");

    m_shadertoyShaderIndex = 0;
    m_shadertoyShaders.append("sunShader.pix", "cubescape.pix", "fractalLand.pix", "hex.pix", "playground.pix");

    m_maxSavedTimeSlices = 512;
    m_waveformWidth = 7.9f;
    initializeAudio();
    m_rawAudioTexture = Texture::createEmpty("Raw Audio Texture", g_currentAudioBuffer.size(), 1, ImageFormat::R32F());

    m_frequencyAudioTexture = Texture::createEmpty("Frequency Audio Texture", g_currentAudioBuffer.size()/2, 1, ImageFormat::RG32F());

    m_visualizationMode = VisualizationMode::EYE;

    m_eyeFramebuffer = Framebuffer::create(Texture::createEmpty("Eye Texture", window()->height(), window()->height(), ImageFormat::RGBA16F()));
    
    m_secondaryEyeSettings.randomize();

    m_smoothedRootMeanSquare = 0.0f;

    makeGUI();
    loadScene("Visualizer");
}

void App::makeGUI() {
    // Initialize the developer HUD (using the existing scene)
    createDeveloperHUD();
    debugWindow->setVisible(false);
    developerWindow->videoRecordDialog->setEnabled(true);
    debugPane->beginRow(); {
        debugPane->addEnumClassRadioButtons<VisualizationMode>("Mode", &m_visualizationMode);
    } debugPane->endRow();
    debugPane->beginRow(); {
        debugPane->addEnumClassRadioButtons<EyeMode>("Eye Mode", &m_eyeSettings.mode);
    } debugPane->endRow();
    debugPane->beginRow(); {
        debugPane->addCheckBox("Use RMS", &m_eyeSettings.useRootMeanSquarePupil);
        debugPane->addNumberBox("Pupil Size", &m_eyeSettings.pupilWidth, "", GuiTheme::LINEAR_SLIDER, 0.0f, 0.4f);
        debugPane->addNumberBox("Time Mult.", &m_eyeSettings.angleOffsetTimeMultiplier, "", GuiTheme::LINEAR_SLIDER, 0.0f, 4.0f);
    } debugPane->endRow();
    GuiDropDownList* list = debugPane->addDropDownList("Shadertoy Shader", m_shadertoyShaders, &m_shadertoyShaderIndex);
    list->setCaptionWidth(100);
    debugPane->pack();


    debugWindow->pack();
    debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
    developerWindow->cameraControlWindow->setVisible(false);
    developerWindow->sceneEditorWindow->setVisible(false);
    developerWindow->setVisible(false);
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
    m_fastMovingAverage.gpuData->setShaderArgs(args, "fastEWMAfreq_", Sampler::video());
    m_slowMovingAverage.gpuData->setShaderArgs(args, "slowEWMAfreq_", Sampler::video());
    m_glacialMovingAverage.gpuData->setShaderArgs(args, "glacialEWMAfreq_", Sampler::video());
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

    float sumSquare = 0.0f;
    for (int i = m_cpuRawAudioData.size() - sampleCount; i < m_cpuRawAudioData.size(); ++i) {
        sumSquare += square(m_cpuRawAudioData[i]);
    }
    float rms = sqrt(sumSquare / sampleCount);
    m_smoothedRootMeanSquare = lerp(rms, m_smoothedRootMeanSquare, 0.95f);

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

    Array<float> frequencyMagnitude;
    for (complex c : frequency) {
        frequencyMagnitude.append(cmp_abs(c));
    }
    if (isNull(m_fastMovingAverage.gpuData)) {
        m_fastMovingAverage.init(0.6, frequencyMagnitude, "Fast Freq EWMA");
        m_slowMovingAverage.init(0.85, frequencyMagnitude, "Slow Freq EWMA");
        m_glacialMovingAverage.init(0.95, frequencyMagnitude, "Glacial Freq EWMA");
    } else {
        m_fastMovingAverage.update(frequencyMagnitude);
        m_slowMovingAverage.update(frequencyMagnitude);
        m_glacialMovingAverage.update(frequencyMagnitude);
    }

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

void App::drawEye(RenderDevice* rd, const Rect2D& rect, const EyeSettings& settings) {
    Args args;
    args.setUniform("iResolution", rect.wh());
    args.setUniform("iGlobalTime", scene()->time());

    float adjustedRMS = m_smoothedRootMeanSquare * (1 - settings.pupilWidth) + settings.pupilWidth;
    args.setUniform("pupilWidth", settings.useRootMeanSquarePupil ? adjustedRMS : settings.pupilWidth);
    args.setUniform("angleOffsetTimeMultiplier", settings.angleOffsetTimeMultiplier);
    args.setMacro("MODE", settings.mode);
    setAudioShaderArgs(args);
    args.setRect(rect);

    LAUNCH_SHADER("eye.pix", args);
}

void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& allSurfaces) {

    if (!scene() || m_cpuRawAudioData.size() == 0) {
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
    case VisualizationMode::EYE:
        rd->push2D(m_eyeFramebuffer); {
            rd->setColorClearValue(Color3::black());
            rd->clear();
            drawEye(rd, rd->viewport(), m_eyeSettings);
            
        } rd->pop2D();
        rd->push2D(m_framebuffer); {
            rd->setColorClearValue(Color3::black());
            rd->clear();
        } rd->pop2D();
        /** Copy into the relevant framebuffer*/
        Texture::copy(m_eyeFramebuffer->texture(0),
            m_framebuffer->texture(0),
            0, 0, 1.0f,
            Vector2int16(-(m_framebuffer->width() - m_eyeFramebuffer->width()) / 2, 0),
            CubeFace::POS_X, CubeFace::POS_X, rd, false);
        
        break;
    case VisualizationMode::TWO_EYES:
        rd->push2D(m_framebuffer); {
            rd->setColorClearValue(Color3::black());
            rd->clear();
            float s = rd->viewport().width() / 2;
            float y = (rd->viewport().height() - s) / 2;
            Rect2D left = Rect2D::xywh(0, y, s, s);
            drawEye(rd, left, m_eyeSettings);
            Rect2D right = Rect2D::xywh(rd->viewport().width()/2, y, s, s);
            drawEye(rd, right, m_secondaryEyeSettings);
        } rd->pop2D();
        break;
    }
	    
    debugAssertGLOk();
    
    m_film->exposeAndRender(rd, filmSettings, m_framebuffer->texture(0));
    // Call to make the GApp show the output of debugDraw
    drawDebugShapes();
    debugAssertGLOk();
}

void App::onUserInput(UserInput* ui) {
    GApp::onUserInput(ui);
    if (ui->keyPressed(GKey::PERIOD)) {
        m_eyeSettings.mode = EyeMode((m_eyeSettings.mode + 1) % EyeMode::COUNT);
    }
    if (ui->keyPressed(GKey::COMMA)) {
        m_eyeSettings.mode = EyeMode((m_eyeSettings.mode + EyeMode::COUNT - 1) % EyeMode::COUNT);
    }
    if (ui->keyPressed(GKey('r'))) {
        m_eyeSettings.randomize();
        m_secondaryEyeSettings.randomize();
    }
    if (ui->keyPressed(GKey('e'))) {
        m_visualizationMode = (m_visualizationMode == VisualizationMode::EYE) ? 
            VisualizationMode::TWO_EYES : 
            VisualizationMode::EYE;
    }

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
    /* Prototype debug code for particle systems, not used in final product */
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
