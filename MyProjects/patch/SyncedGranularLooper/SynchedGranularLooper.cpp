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

static const float kKnobMax = 1023;
static const uint32_t kBufferLengthSec = 15;
static const uint32_t kSampleRate = 48000;
static const size_t kBufferLengthSamples = kBufferLengthSec * kSampleRate;
static float DSY_SDRAM_BSS buffer[2][kBufferLengthSamples]; // Double buffered for two channels

static sampler::SamplerPlayer samplerPlayer[2]; // Two sampler players for two channels
// Structure to hold the dot positions
struct Dot {
    int x;
    int y;
};
Dot sparklingDots[10];
DaisyPatch hw;
Parameter loopStart[2], loopLength[2];
bool startOver[2] = {false, false};
bool recordOn[2] = {false, false}; // Separate recording state for each channel
int currentChannel = 0; // Channel selection state

void UpdateControls();
void updateDisplay();
void updateSparklingDots();
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    
    UpdateControls();

    for (size_t i = 0; i < size; ++i) {
        // Process each channel separately
        for (int ch = 0; ch < 2; ++ch) {
            auto o = samplerPlayer[ch].Process(in[ch][i], startOver[ch]);
            out[ch][i] = o; // Output to respective channel
        }
    }

    
}

int main(void)
{
    hw.Init();
    hw.seed.StartLog(false);

    for (int ch = 0; ch < 2; ++ch) {
        loopStart[ch].Init(hw.controls[ch * 2], 0, 1, Parameter::LINEAR);
        loopLength[ch].Init(hw.controls[ch * 2 + 1], 0, 1, Parameter::EXPONENTIAL);
        samplerPlayer[ch].Init(buffer[ch], kBufferLengthSamples);
    }

    std::string str = "Dual Sampler Player";
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
    


    currentChannel += hw.encoder.Increment();
    currentChannel %= 2; // Wrap around if channel exceeds 1
    if(currentChannel<0) currentChannel = 0;   
    for (int ch = 0; ch < 2; ++ch) {

        if (hw.gate_input[ch].Trig()) {
            startOver[ch] = true;
        } else {
            startOver[ch] = false;
        }

        loopStart[ch].Process();
        loopLength[ch].Process();

        auto loop_start = loopStart[ch].Value();
        auto loop_length = loopLength[ch].Value();

        samplerPlayer[ch].SetLoop(loop_start, loop_length);
        samplerPlayer[ch].SetRecording(recordOn[ch]);


        
    }

    if (hw.encoder.RisingEdge()) {
        recordOn[currentChannel] = !recordOn[currentChannel]; // Toggle recording for the selected channel
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
    const int halfHeight = displayHeight / 2;

    for (int ch = 0; ch < 2; ch++)
    {
        int yOffset = ch * halfHeight;

        for (int i = 0; i < displayWidth; i++)
        {
            size_t bufferIndex = i * (kBufferLengthSamples / displayWidth);
            if (bufferIndex < kBufferLengthSamples)
            {
                float sample = buffer[ch][bufferIndex];
                float avg = 0;

                for (size_t x = 0; x < (kBufferLengthSamples / displayWidth); x++)
                {
                    avg += abs(sample);
                }

                avg = (avg / (kBufferLengthSamples / displayWidth)) * (halfHeight);

                int y = static_cast<int>((sample * halfHeight / 2) + yOffset + halfHeight / 2);

                if (y >= 0 && y < displayHeight)
                {
                    hw.display.DrawLine(i, yOffset + halfHeight / 2 + avg, i, yOffset + halfHeight / 2 - avg, true);
                }
            }
        }

        size_t loopStartPos = samplerPlayer[ch].GetLoopStartPosition();
        size_t playheadPosition = (loopStartPos + samplerPlayer[ch].GetCurrentPosition()) % samplerPlayer[ch].GetBufferLength();
        int playheadX = playheadPosition / (samplerPlayer[ch].GetBufferLength() / displayWidth);

        if (playheadX < displayWidth)
        {
            hw.display.DrawLine(playheadX, yOffset, playheadX, yOffset + halfHeight, true);
        }

        int loopStartX = (loopStartPos / (samplerPlayer[ch].GetBufferLength() / displayWidth))%displayWidth;
        int loopEndX = ((loopStartPos + samplerPlayer[ch].GetLoopLength()) / (samplerPlayer[ch].GetBufferLength() / displayWidth))%displayWidth;

        if (loopStartX < displayWidth) {
            hw.display.DrawLine(loopStartX, yOffset, loopStartX, yOffset + halfHeight, true);
        }

        if (loopEndX < displayWidth) {
            hw.display.DrawLine(loopEndX, yOffset, loopEndX, yOffset + halfHeight, true);
        }

        hw.display.SetCursor(0, yOffset);
        std::string str = (recordOn[ch] ? "Record " : "Play ") + std::to_string(ch + 1);//+ 
        char* cstr = &str[0];
        hw.display.WriteString(cstr, Font_6x8, true);
    }
    hw.display.SetCursor(64, 0);
    std::string str = "CH " + std::to_string(currentChannel);
    char* cstr = &str[0];
    hw.display.WriteString(cstr, Font_6x8, true);

    hw.display.Update();
}