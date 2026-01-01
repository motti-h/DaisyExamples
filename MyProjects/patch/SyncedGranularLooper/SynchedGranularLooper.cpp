#include "daisy_patch.h"
#include "daisysp.h"
#include "samplerPlayer.h"
#include <string>
#include <cstdlib> // For rand() and srand()
#include <ctime>   // For time()
#include <cmath>   // For powf()

using namespace daisy;
using namespace daisysp;
using namespace sampler;

#define DAC_MAX 4095.f

static const float kKnobMax = 1023;
static const uint32_t kBufferLengthSec = 15;
static const uint32_t kSampleRate = 48000;
static const size_t kBufferLengthSamples = kBufferLengthSec * kSampleRate;
static float DSY_SDRAM_BSS buffer[kBufferLengthSamples]; // Single channel buffer

static sampler::SamplerPlayer samplerPlayer; // Single sampler player
// Structure to hold the dot positions
struct Dot {
    int x;
    int y;
};
Dot sparklingDots[10];
DaisyPatch hw;
Parameter loopStart, loopLength;
bool startOver = false;
bool recordOn = false; // Single recording state

void UpdateControls();
void updateDisplay();
void updateSparklingDots();
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    
    UpdateControls();

    for (size_t i = 0; i < size; ++i) {
        // Process single channel
        auto o = samplerPlayer.Process(in[0][i], startOver);
        out[0][i] = o; // Output to left channel
        out[1][i] = o; // Output to right channel (same signal)
    }
}

int main(void)
{
    hw.Init();
    hw.seed.StartLog(false);

    loopStart.Init(hw.controls[0], 0, 1, Parameter::LINEAR);
    loopLength.Init(hw.controls[1], 0, 1, Parameter::EXPONENTIAL);
    // Note: We don't use Parameter for playbackSpeed, we read control directly
    samplerPlayer.Init(buffer, kBufferLengthSamples);

    std::string str = "Granular Sampler";
    char* cstr = &str[0];
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();
    hw.DelayMs(1000);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    srand(static_cast<unsigned int>(time(nullptr)));

    uint32_t lastUpdateTime = 0;
    const uint32_t sparkleInterval = 100; // ms

    while (true) {
        uint32_t currentTime = hw.seed.system.GetNow();
        if (currentTime - lastUpdateTime >= sparkleInterval) {
            lastUpdateTime = currentTime;
            updateSparklingDots();
        }

        updateDisplay();
        System::Delay(10);
    }
}

void UpdateControls()
{
    hw.ProcessAllControls();
    
    if (hw.gate_input[0].Trig()) {
        startOver = true;
    } else {
        startOver = false;
    }

    loopStart.Process();
    loopLength.Process();
    hw.controls[2].Process(); // Process control 3 directly

    auto loop_start = loopStart.Value();
    auto loop_length = loopLength.Value();
    
    float controlValue = hw.controls[2].Value(); // 0 to 1 (nominally 0V to 5V)
    
    // Two-stage calibration: accurate first octave, then adjust higher octaves
    const float FIRST_OCTAVE_THRESHOLD = 0.2f; // First 20% of range = first octave
    const float HIGH_OCTAVE_SCALE = 0.96f;     // Adjust this for higher octaves (try 0.9-1.05)
    
    float octaves;
    
    if (controlValue <= FIRST_OCTAVE_THRESHOLD) {
        // First octave: use direct linear mapping (already accurate)
        octaves = controlValue * 5.0f;
    } else {
        // Higher octaves: apply calibration scaling
        // Keep first octave as-is, then scale the rest
        float firstOctave = FIRST_OCTAVE_THRESHOLD * 5.0f; // = 1.0 octave
        float remainingValue = controlValue - FIRST_OCTAVE_THRESHOLD;
        float remainingOctaves = (remainingValue / (1.0f - FIRST_OCTAVE_THRESHOLD)) * 4.0f; // Remaining 4 octaves
        octaves = firstOctave + (remainingOctaves * HIGH_OCTAVE_SCALE);
    }
    
    // Apply exponential volt/octave conversion: speed = 2^octaves
    float speed = powf(2.0f, octaves);

    samplerPlayer.SetLoop(loop_start, loop_length);
    samplerPlayer.SetPlaybackSpeed(speed);
    samplerPlayer.SetRecording(recordOn);

    if (hw.encoder.RisingEdge()) {
        recordOn = !recordOn; // Toggle recording
    }
}

void updateSparklingDots()
{
    for (int i = 0; i < 10; ++i) {
        sparklingDots[i].x = rand() % 128;
        sparklingDots[i].y = rand() % 64;
    }
}

void updateDisplay()
{
    hw.display.Fill(false);

    const int displayWidth = 128;
    const int displayHeight = 64;

    // Draw waveform
    for (int i = 0; i < displayWidth; i++)
    {
        size_t bufferIndex = i * (kBufferLengthSamples / displayWidth);
        if (bufferIndex < kBufferLengthSamples)
        {
            float sample = buffer[bufferIndex];
            float avg = 0;

            for (size_t x = 0; x < (kBufferLengthSamples / displayWidth); x++)
            {
                avg += abs(sample);
            }

            avg = (avg / (kBufferLengthSamples / displayWidth)) * (displayHeight / 2);

            int y = static_cast<int>((sample * displayHeight / 4) + displayHeight / 2);

            if (y >= 0 && y < displayHeight)
            {
                hw.display.DrawLine(i, displayHeight / 2 + avg, i, displayHeight / 2 - avg, true);
            }
        }
    }

    // Draw playhead
    size_t loopStartPos = samplerPlayer.GetLoopStartPosition();
    size_t playheadPosition = (loopStartPos + samplerPlayer.GetCurrentPosition()) % samplerPlayer.GetBufferLength();
    int playheadX = playheadPosition / (samplerPlayer.GetBufferLength() / displayWidth);

    if (playheadX < displayWidth)
    {
        hw.display.DrawLine(playheadX, 0, playheadX, displayHeight, true);
    }

    // Draw loop markers
    int loopStartX = (loopStartPos / (samplerPlayer.GetBufferLength() / displayWidth)) % displayWidth;
    int loopEndX = ((loopStartPos + samplerPlayer.GetLoopLength()) / (samplerPlayer.GetBufferLength() / displayWidth)) % displayWidth;

    if (loopStartX < displayWidth) {
        hw.display.DrawLine(loopStartX, 0, loopStartX, displayHeight, true);
    }

    if (loopEndX < displayWidth) {
        hw.display.DrawLine(loopEndX, 0, loopEndX, displayHeight, true);
    }

    // Display status
    hw.display.SetCursor(0, 0);
    std::string str = recordOn ? "Recording" : "Playing";
    char* cstr = &str[0];
    hw.display.WriteString(cstr, Font_6x8, true);

    // Display control value as integer (0-1000) for debugging
    hw.display.SetCursor(0, 10);
    float controlValue = hw.controls[2].Value();
    int ctrlInt = (int)(controlValue * 1000); // Convert to 0-1000 integer
    std::string ctrlStr = "CV:" + std::to_string(ctrlInt);
    char* ctrlCstr = &ctrlStr[0];
    hw.display.WriteString(ctrlCstr, Font_6x8, true);
    
    // Display playback speed
    hw.display.SetCursor(70, 0);
    float octaves = controlValue * 5.0f;
    float speed = powf(2.0f, octaves);
    int speedInt = (int)(speed * 100); // Convert to integer for display
    std::string speedStr = std::to_string(speedInt);
    char* speedCstr = &speedStr[0];
    hw.display.WriteString(speedCstr, Font_6x8, true);

    hw.display.Update();
}