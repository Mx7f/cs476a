#include "RtAudio.h"
#include <cstdio>
#include <string.h>
#include <math.h>
#include <iostream>
#include <cstdlib>
#include <functional>
using namespace std;


/** Our datatype for samples */
typedef double Sample;

/** Corresponding format for RtAudio */
#define RTAUDIO_FORMAT RTAUDIO_FLOAT64

/** In Hz */
#define SAMPLE_RATE 44100

#define NUM_CHANNELS 2

/** tau/2 */
#ifdef PI
#  undef PI
#  define PI 3.14159265358979323846
#endif

/** Frequency in Hz */
Sample g_frequency = 440;

/** Count of the number of sample we have written to so far */
Sample g_currentSampleCount = 0;

std::function<Sample()> g_sampleGenerator;

//-----------------------------------------------------------------------------
// name: callme()
// desc: audio callback
//-----------------------------------------------------------------------------
int audioCallback( void * outputBuffer, void * inputBuffer, unsigned int numFrames,
            double streamTime, RtAudioStreamStatus status, void * data )
{

  // cast!
  Sample * buffy = (Sample *)outputBuffer;
    
  // fill
  for( int i = 0; i < numFrames; i++ )
    {
      // generate signal
      buffy[i*NUM_CHANNELS] = g_sampleGenerator();
        
      // copy into other channels
      for( int j = 1; j < NUM_CHANNELS; j++ ) {
	buffy[i*NUM_CHANNELS+j] = buffy[i*NUM_CHANNELS];
      }
            
      // increment sample number
      g_currentSampleCount += 1.0;
    }
    
  return 0;
}




int main(int argc, char ** argv) {
  // Argument parsing
  std::string usage = "sig-gen [type] [frequency] [width]\n";
  usage += "[type]: --sine | --saw | --pulse | --noise | --impulse\n";
  usage += "[frequency]: (a number > 0, only applicable to some signal types)\n";
  usage += "[width]: pulse width (only applicable to some signal types)\n";

  g_sampleGenerator = [&](){
    return ::sin( 2 * PI * g_frequency * g_currentSampleCount / SAMPLE_RATE );
  };

  /*
  if (argc < 2) {
    printf("Error, no parameters specified.\nUsage:\n%s", usage.c_str());
    return 0;
  }
  std::string type = argv[1];
  */



  // Instantiate RtAudio object
  RtAudio rtAudio;

  unsigned int bufferByteCount = 0;
  unsigned int bufferFrameCount = 512;
    
  // Check for audio devices
  if( rtAudio.getDeviceCount() < 1 ) {
    // None :(
    cout << "No audio devices found!" << endl;
    exit( 1 );
  }

  // Let RtAudio print messages to stderr.
  rtAudio.showWarnings( true );

  // Set input and output parameters
  RtAudio::StreamParameters iParams, oParams;
  iParams.deviceId = rtAudio.getDefaultInputDevice();
  iParams.nChannels = NUM_CHANNELS;
  iParams.firstChannel = 0;
  oParams.deviceId = rtAudio.getDefaultOutputDevice();
  oParams.nChannels = NUM_CHANNELS;
  oParams.firstChannel = 0;
    
  // Create stream options
  RtAudio::StreamOptions options;

  try {
    // Open a stream
    rtAudio.openStream( &oParams, &iParams, RTAUDIO_FORMAT, SAMPLE_RATE, &bufferFrameCount, &audioCallback, (void *)&bufferByteCount, &options );
  } catch( RtAudioError& e ) {
    // Failed to open stream
    cout << e.getMessage() << endl;
    exit( 1 );
  }

  bufferByteCount = bufferFrameCount * NUM_CHANNELS * sizeof(Sample);
    

  // Main loop
  try {
    // Start the stream. RtAudio will make asynchronous calls to our callback function from now on
    rtAudio.startStream();

    // Process Input
    char input;
    std::cout << "running... press <enter> to quit (buffer frame count: " << bufferFrameCount << ")" << endl;
    std::cin.get(input);
        
    // We lied, you can press anything to quit...
    // Stop the stream.
    rtAudio.stopStream();
  } catch( RtAudioError& e ) {
    cout << e.getMessage() << endl;
    goto cleanup;
  }
    
 cleanup:
  // Close if open
  if( rtAudio.isStreamOpen() )
    rtAudio.closeStream();
    
  // done
  return 0;
}
