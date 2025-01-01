#include "daisy_patch.h"
#include "daisysp.h"
#include "samplerPlayer.h"
#include <string>
#include <cstdlib> // For rand() and srand()
#include <ctime>   // For time()


using namespace daisy;
using namespace daisysp;
using namespace sampler;

#define DAC_MAX 4095.f

// // Setup pins
// static const int record_pin      = D(S30);
// static const int loop_start_pin  = A(S31);
// static const int loop_length_pin = A(S32);
// static const int pitch_pin       = A(S33);

static const float kKnobMax = 1023;

// Allocate buffer in SDRAM 
static const uint32_t kBufferLengthSec = 5;
static const uint32_t kSampleRate = 48000;
static const size_t kBufferLenghtSamples = kBufferLengthSec * kSampleRate;
static float DSY_SDRAM_BSS buffer[kBufferLenghtSamples];

static sampler::SamplerPlayer samplerPlayer;
// Structure to hold the dot positions
struct Dot {
    int x;
    int y;
};
Dot sparklingDots[10];
// GranularPlayer granularPlayer;
// Hardware
DaisyPatch hw;
Parameter loopStart, loopLength, pitch, delayTimePS;
auto startOver = false;
bool recordOn = false;
void UpdateControls();  
void updateDisplay();
void updateSparklingDots();
void CalculateRMS(float* buffer, size_t bufferLength, float* rmsBuffer, size_t rmsBufferLength, size_t samplesPerChunk);
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    
    if(hw.gate_input[0].Trig() ){
      startOver = true;
    }else{
      startOver = false;
    }
    for (size_t i = 0; i < size; i++) {
    auto o = samplerPlayer.Process(in[0][i],startOver);
    out[0][i] = out[1][i] = o;
    }
    
    UpdateControls();

}

int main(void)
{
  // Init everything.
  hw.Init();
  hw.seed.StartLog(false);
  loopStart.Init(hw.controls[0], 0, 1, Parameter::LINEAR);
  loopLength.Init(hw.controls[1], 0, 1, Parameter::EXPONENTIAL);
  pitch.Init(hw.controls[2], 1, 24, Parameter::LINEAR);
  delayTimePS.Init(hw.controls[3], 1, 500, Parameter::LINEAR);
  //briefly display the module name
  std::string str  = "Sampler Player";
  char *      cstr = &str[0];
  hw.display.WriteString(cstr, Font_7x10, true);
  hw.display.Update();
  hw.DelayMs(1000);
  
  
  // Start the ADC and Audio Peripherals on the Hardware
  hw.StartAdc();
  hw.StartAudio(AudioCallback);
 
  float sample_rate = hw.seed.AudioSampleRate();

  // Setup looper
  samplerPlayer.Init(buffer, kBufferLenghtSamples); 
  
      uint32_t lastUpdateTime = 0;
    const uint32_t sparkleInterval = 100; // 100 milliseconds

    // Initialize random seed
    srand(static_cast<unsigned int>(time(nullptr)));

    for (;;)
    {
        // Get the current time
        uint32_t currentTime = hw.seed.system.GetNow();
        if (currentTime - lastUpdateTime >= sparkleInterval)
        {
            lastUpdateTime = currentTime; // Update last update time
            updateSparklingDots(); // Update random dot positions
        }

        updateDisplay();
        System::Delay(10);
    }

}

void UpdateControls()
{
    hw.ProcessAllControls();

    loopStart.Process();
    loopLength.Process();
    pitch.Process();
      // Set loop parameters
    auto loop_start = loopStart.Value(); //fmap(loopStart.Value() / kKnobMax, 0.f, 1.f);
    auto loop_length = loopLength.Value();//fmap(loopLength.Value() / kKnobMax, 0.f, 1.f, Mapping::EXP);
    
    if(hw.encoder.RisingEdge()) {
      recordOn = !recordOn;
    }

      samplerPlayer.SetLoop(loop_start, loop_length);
      samplerPlayer.SetRecording(recordOn);
}

// Function to generate new random sparkling dots
void updateSparklingDots()
{
    for (int i = 0; i < 10; i++) // Generate new positions for 10 sparkling dots
    {
        sparklingDots[i].x = rand() % 128; // Random X position within display width
        sparklingDots[i].y = rand() % 64;  // Random Y position within display height
    }
}
// void CalculateRMS(float* buffer, size_t bufferLength, float* rmsBuffer, size_t rmsBufferLength, size_t samplesPerChunk)
// {
//     for (size_t i = 0; i < rmsBufferLength; i++)
//     {
//         size_t startSample = i * samplesPerChunk;
//         if (startSample + samplesPerChunk <= bufferLength)
//         {
//             float sum = 0.0f;
//             // Compute the sum of squares for RMS calculation
//             for (size_t j = 0; j < samplesPerChunk; j++)
//             {
//                 float sample = buffer[startSample + j];
//                 sum += sample * sample;
//             }
//             // Calculate RMS value
//             rmsBuffer[i] = sqrtf(sum / samplesPerChunk);
//         }
//         else
//         {
//             rmsBuffer[i] = 0.0f; // If out of bounds, set RMS to 0
//         }
//     }
// }

// void updateDisplay()
// {
//     hw.display.Fill(false);
//     hw.display.SetCursor(0,0);
//     std::string str;
//     if(recordOn){
//       str="Recording: ON";
//     }else{
//       str="Recording: OFF";
//     }
//     char *      cstr = &str[0];
//     hw.display.WriteString(cstr, Font_7x10, true);
//     hw.display.Update();
// }

void updateDisplay()
{
    hw.display.Fill(false); // Clear the display

    // Constants for display dimensions
    const int displayWidth = 128; // Width of the display
    const int displayHeight = 64;  // Height of the display
    int prev_x = 0;
    int prev_y = displayHeight / 2; // Start drawing from the middle of the display

    // Draw the waveform
    for (int i = 0; i < displayWidth; i++)
    {
        size_t bufferIndex = i * (kBufferLenghtSamples / displayWidth); // Calculate buffer index
        if (bufferIndex < kBufferLenghtSamples) // Ensure index is valid
        {
            float sample = buffer[bufferIndex]; // Get the sample from the buffer

            // Normalize the sample assuming it ranges from -1 to 1
            int y = displayHeight / 2 - static_cast<int>(sample * (displayHeight / 2)); // Center the waveform display

            // Draw the line only if the y position is valid
            if (y >= 0 && y < displayHeight) 
            {
                if (i > 0) 
                {
                    hw.display.DrawLine(prev_x, prev_y, i, y, true); // Draw line to visualize waveform
                }
                prev_x = i;
                prev_y = y;
            }
        }
    }

    // Calculate the playhead position relative to loop start
    size_t loopStartPos = samplerPlayer.GetLoopStartPosition();
    size_t playheadPosition = (loopStartPos + samplerPlayer.GetCurrentPosition()) % samplerPlayer.GetBufferLength(); 
    int playheadX = playheadPosition / (samplerPlayer.GetBufferLength() / displayWidth); // Adjust for display scaling

    // Draw the playhead line
    if (playheadX < displayWidth) // Ensure playhead line remains within display width
    {
        hw.display.DrawLine(playheadX, 0, playheadX, displayHeight, true); // Draw playhead
    }

    // Draw loop start and end markers
    int loopStartX = loopStartPos / (samplerPlayer.GetBufferLength() / displayWidth);  
    int loopEndX = (loopStartPos + samplerPlayer.GetLoopLength()) / (samplerPlayer.GetBufferLength() / displayWidth);

    if (loopStartX < displayWidth) {
        hw.display.DrawLine(loopStartX, 0, loopStartX, displayHeight, 1); // Loop start marker
    }

    if (loopEndX < displayWidth) {
        hw.display.DrawLine(loopEndX, 0, loopEndX, displayHeight, 1); // Loop end marker
    }

    // Add random sparkling dots
    for (int i = 0; i < 10; i++) // Draw 10 random dots
    {
        int dotX = sparklingDots[i].x;
        int dotY = sparklingDots[i].y;
        hw.display.DrawPixel(dotX, dotY, true); // Draw the dot
    }

    // Display recording state
    hw.display.SetCursor(0, 0); // Set cursor position
    std::string str;
    if (recordOn)
    {
        str = "Record"; // Message when recording is ON
    }
    else
    {
        str = "Play"; // Message when recording is OFF
    }
    char* cstr = &str[0];
    hw.display.WriteString(cstr, Font_7x10, true); // Display the recording status

    // Update the display
    hw.display.Update();
}