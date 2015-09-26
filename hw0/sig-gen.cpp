#include "RtAudio.h"
#include <cstdio>
#include <string.h>
#include <math.h>
#include <iostream>
#include <cstdlib>
#include <functional>
#include <random>
using namespace std;


/** Our datatype for samples */
typedef double Sample;

/** Corresponding format for RtAudio */
#define RTAUDIO_FORMAT RTAUDIO_FLOAT64

/** In Hz */
#define SAMPLE_RATE 44100

#define NUM_CHANNELS 2
/** tau/2 */
#define PI 3.14159265358979323846


/** Frequency in Hz */
Sample g_frequency = 440;

/** Width of initial part of waveform, in fraction of period */
Sample g_width = 0.0;

/** If this is true, multiply the generated signal with the mic input */
bool g_useInput = false;

/** Count of the number of sample we have written to so far */
long long g_currentSampleCount = 0;

std::function<Sample()> g_sampleGenerator;

//-----------------------------------------------------------------------------
// name: callme()
// desc: audio callback
//-----------------------------------------------------------------------------
int audioCallback( void * outputBuffer, void * inputBuffer, unsigned int numFrames,
            double streamTime, RtAudioStreamStatus status, void * data ) {
  // cast!
  Sample * buffer   = (Sample *)outputBuffer;
  Sample * inBuffer = (Sample *)inputBuffer;
  // fill
  for( int i = 0; i < numFrames; i++ ) {
      // generate signal, using the std::function g_sampleGenerator, which is chose based on input arguments to main()
      Sample generatedSample = g_sampleGenerator();
 
     // CPU Branch prediction means this won't actually be that expensive of an op
      if (g_useInput) {
	// Multiply the generated signal by the mic input
	generatedSample *= inBuffer[i*NUM_CHANNELS];
      }
      buffer[i*NUM_CHANNELS] = generatedSample;
        
      // copy into other channels
      for( int j = 1; j < NUM_CHANNELS; j++ ) {
	buffer[i*NUM_CHANNELS+j] = buffer[i*NUM_CHANNELS];
      }
            
      // increment sample number
      g_currentSampleCount += 1;
  }
  
  return 0;
}

/** Clip to [-1,1] */
Sample clipSample(Sample s) {
  return max(min(s,1.0),-1.0);
}

/** The period of the signal given in samples. Uses the global g_frequency */
double periodInSamples() {
  return SAMPLE_RATE / g_frequency;
}


int main(int argc, char ** argv) {
  // Argument parsing
  std::string usage = "sig-gen type [frequency] [width] [--input]\n";
  usage += "type: --sine | --saw | --pulse | --noise | --impulse\n";
  usage += "[frequency]: (a number > 0, required and used by all signal types except noise)\n";
  usage += "[width]: pulse width (only required and used by saw and pulse types)\n";
  usage += "[--input]: If present, multiplies generated signal with the mic\n";

  // Argument handling.
  if (argc < 2) {
    printf("Error, no parameters specified.\nUsage:\n%s", usage.c_str());
    return 0;
  }
  std::string type = argv[1];

  // Figure out which arguments we need based on type
  bool requiresFrequency = false;
  bool requiresWidth = false;
  bool requiresWidthInsideUnitInterval = false;
  if (type != "--noise") { // Noise needs no arguments, all else require at least frequency
    requiresFrequency = true;
    if ( (type != "--sine") &&
	 (type != "--impulse") ) { 
      requiresWidth = true;
      requiresWidthInsideUnitInterval = (type == "--pulse");
      if ((type != "--saw") &&
	  (type != "--pulse") ) {
	printf("Error, invalid type flag.\nUsage:\n%s", usage.c_str());
	exit(1);
      }
    }
  } 

  if (requiresFrequency) {
    // Get and validate frequency input 
    bool validFrequency = true;
    if (argc < 3) {
      validFrequency = false;
    } else {
      try {
	g_frequency = std::stod(argv[2]);
      } catch (...) {
	validFrequency = false;
      }
      if (g_frequency <= 0) {
	validFrequency = false;
      }
    }
    if( !validFrequency ) {
      printf("Error, if noise is not the type selected, must provide a frequency (positive number in Hz)\nUsage:\n%s", usage.c_str());
      exit(1);
    }
  }

  
  if (requiresWidth) {
    // Get and validate width input 
    bool validWidth = true;
    if (argc < 2) {
      validWidth = false;
    } else {
      try {
	g_width = std::stod(argv[3]);
      } catch (...) {
	validWidth = false;
      }
      if (g_width < 0 || g_width > 1) {
	validWidth = false;
      } 
    }
    if( !validWidth ) {
      printf("Error, if saw or pulse is the type selected, must provide a width (number in [0-1])\nUsage:\n%s", usage.c_str());
      exit(1);
    }
  }

  if (requiresWidthInsideUnitInterval) {
    if (g_width == 0.0 || g_width == 1.0) {
      printf("Error, pulse is the type selected, must provide a width (number in (0-1) (exclusive!))\nUsage:\n%s", usage.c_str());
      exit(1);
    }
  }

  int maxArgumentRead = 1;
  if (requiresFrequency) {
    maxArgumentRead += 1;
  }
  if (requiresWidth) {
    maxArgumentRead += 1;
  }

  if (argc > maxArgumentRead + 1) {
    std::string potentialInputFlag = argv[maxArgumentRead+1];
    if (potentialInputFlag == "--input") {
      g_useInput = true;
    } else {
       printf("Error: invalid final argument (must be --input or nothing at all).\nUsage:\n%s", usage.c_str());
       exit(1);
    }
  }

  std::default_random_engine generator;
  // Gaussian distribution, mean 0, stddev 0.5, for white gaussian noise
  std::normal_distribution<Sample> whiteNoiseDistribution(0.0,0.5);


  // Assign the sample generation function based on the arguments provided
  if (type == "--sine") {
    // Just like in class
    g_sampleGenerator = [&](){
      return ::sin( 2 * PI * double(g_currentSampleCount) * g_frequency / SAMPLE_RATE );
    };
  } else if (type == "--saw") {
    // Saw tooth waveform generation
    //             1 |    /\        /\
    //               |   /  \      /  \
    //             0 |  /    \    /    \
    //               | /      \  /      \
    //            -1 |/        \/        \/
    // ____________________________________
    // 1 period      |         |
    // g_width = 0.5 |    |

    //             1 |        /|        /|
    //               |      /  |      /  |
    //             0 |    /    |    /    |
    //               |  /      |  /      |
    //            -1 |/        |/        |/
    // ____________________________________
    // 1 period      |         |
    // g_width = 1.0 |         |

    //             1 |\        |\        |\
    //               |  \      |  \      |
    //             0 |    \    |    \    |
    //               |      \  |      \  |
    //            -1 |        \|        \|
    // ____________________________________
    // 1 period      |         |
    // g_width = 0.0 |
    g_sampleGenerator = [&](){
      double numPeriods = 0.0;
      // In [0,1], tells us how far along we are in the current period
      double positionInPeriod = modf(double(g_currentSampleCount)/periodInSamples(), &numPeriods);
      bool upward = positionInPeriod < g_width || g_width == 1.0;
      if (upward) {
	// Go to [0-1] in x coordinate, where 0 is beginning of upwards section, 1 is end
	Sample s = positionInPeriod / g_width;
	// Convert to y coordinate, which is just 2x-1
	return 2*s-1;
      } else {
	// Go to [0-1] in x coordinate, where 0 is beginning of downward section, 1 is end
	Sample s = (1.0-positionInPeriod) / (1.0-g_width);
	// Same conversion as other branch, but flipped along the x axis by negating
	return -(2*s-1);
      }
    };
  } else if (type == "--pulse") {
    //          1 |----------             ----------
    //            |         |             |        |
    //          0 |         |             |        |
    //            |         |             |        |
    //         -1 |         ---------------        -------------
    // _________________________________________________________
    // 1 period   |                       |
    // g_width    |         |
    g_sampleGenerator = [&](){
      double numPeriods = 0.0;
      // In [0,1], tells us how far along we are in the current period
      double positionInPeriod = modf(double(g_currentSampleCount)/periodInSamples(), &numPeriods);
      return (positionInPeriod <= g_width) ? 1.0 : -1.0;
    };
  } else if (type == "--noise") {
    g_sampleGenerator = [&](){
      // Gaussian random, with outliers clipped to [-1,1]
      return clipSample(whiteNoiseDistribution(generator));
    };
  } else if (type == "--impulse") {
    //          1 |         o        o
    //            |         |        |
    //          0 |         |        |
    //            |         |        |
    //         -1 |-----------------------
    // ___________________________________
    // 1 period   |         |              
    g_sampleGenerator = [&](){
      // periodInSamples() is the fundamental period T is samples. Only go to 1 every T samples, otherwise 0
      // Integer overflow won't happen until after heat-death of universe
      long long T = (long long)(periodInSamples());
      long long s = g_currentSampleCount;
      return ((s % T) == 0) ? 1.0 : 0.0;
    };
  } else {
    printf("Somehow passed argument validation with invalid arg %s!\nUsage:\n%s", type.c_str(), usage.c_str());
    exit(1);
  }

  // The following is almost identical to the example code, with some renaming

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
