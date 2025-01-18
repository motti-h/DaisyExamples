#include "daisy_patch.h"
#include "daisysp.h"
#include "samplerPlayer.h"
#include <string>
#include <cstdlib> // For rand() and srand()
#include <ctime>   // For time()
#include <vector>

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

// Parameters for onset detection
float detectionThreshold = 0.01f; 
size_t minimumInterval = 480; // 10 ms
bool useOnsets = false;
std::vector<size_t> onsets[2];

enum SystemState {
    PlayRecord,
    SetParameters
};

enum PlayRecordMenu {
    channelSelect,
    recordPlay,
    exitPlayRecord
};

enum SetParametersMenu {
    thresh,
    interval,
    onset,
    exitSetParams
};

SystemState currentState = SystemState::PlayRecord;
PlayRecordMenu playRecordState = channelSelect;
SetParametersMenu setParametersState = thresh;

void UpdateControls();
void updateDisplay();
void updateSparklingDots();
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    UpdateControls();

    for (size_t i = 0; i < size; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            auto o = samplerPlayer[ch].Process(in[ch][i], startOver[ch]);
            out[ch][i] = o; // Output to the respective channel
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
    hw.display.WriteString(str.c_str(), Font_7x10, true);
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

void updateSparklingDots()
{
    for (int i = 0; i < 10; ++i) {
        sparklingDots[i].x = rand() % 128;
        sparklingDots[i].y = rand() % 64;
    }
}

void UpdateControls()
{
    hw.ProcessAllControls();

    // Navigation using the rising edge of the encoder
    if (hw.encoder.RisingEdge()) {
        if (currentState == PlayRecord) {
            // Toggle between PlayRecord options
            if (playRecordState == exitPlayRecord) {
                currentState = SetParameters; // Switch to SetParameters menu
                playRecordState = channelSelect; // Reset to initial state
            } else {
                playRecordState = static_cast<PlayRecordMenu>((playRecordState + 1) % 3); // Cycle through PlayRecord menu options
            }
        } 
        else if (currentState == SetParameters) {
            // Toggle between SetParameters options
            if (setParametersState == exitSetParams) {
                currentState = PlayRecord; // Go back to PlayRecord menu
                setParametersState = thresh; // Reset back to the first parameter option
            } else {
                setParametersState = static_cast<SetParametersMenu>((setParametersState + 1) % 4); // Cycle through SetParameters options
            }
        }
    }

    // Adjust parameters or options based on encoder increment
    if (currentState == PlayRecord) {
        switch (playRecordState) {
            case channelSelect:
                currentChannel += hw.encoder.Increment();
                currentChannel = (currentChannel + 2) % 2; // Wrap around if channel exceeds 1
                break;

            case recordPlay:
                if (hw.encoder.Increment() != 0) {
                    recordOn[currentChannel] = !recordOn[currentChannel]; // Toggle recording for the selected channel
                    if (!recordOn[currentChannel]) {
                        // Handle onset detection when recording ends
                        onsets[currentChannel] = samplerPlayer[currentChannel].DetectOnsets(detectionThreshold, minimumInterval);
                    }
                }
                break;

            case exitPlayRecord:
                // Optional confirmation message can be added here
                break;
        }

        // Handle each channel's looping and recording
        for (int ch = 0; ch < 2; ch++) {
            startOver[ch] = hw.gate_input[ch].Trig(); // Set startOver based on gate input status

            // Process loop start and length parameters
            loopStart[ch].Process();
            loopLength[ch].Process();

            float loop_start;
            if (useOnsets) {
                loop_start = onsets[currentChannel].empty() ? loopStart[currentChannel].Value() : onsets[currentChannel].front();
            } else {
                loop_start = loopStart[currentChannel].Value();
            }

            auto loop_length = loopLength[ch].Value();
            samplerPlayer[ch].SetLoop(loop_start, loop_length);
            samplerPlayer[ch].SetRecording(recordOn[ch]);
        }

    } else if (currentState == SetParameters) {
        // Adjust parameters based on selected state
        switch (setParametersState) {
            case thresh:
                if (hw.encoder.Increment() != 0) {
                    detectionThreshold += hw.encoder.Increment() * 0.01f; // Increment threshold
                    detectionThreshold = std::fmax(0.0f, std::fmin(detectionThreshold, 1.0f)); // Clamp to range
                }
                break;

            case interval:
                if (hw.encoder.Increment() != 0) {
                    minimumInterval += hw.encoder.Increment() * 10; // Change interval by 10 ms
                    minimumInterval = std::fmax(10, minimumInterval); // Ensure minimum value
                }
                break;

            case onset:
                if (hw.encoder.Increment() != 0) {
                    useOnsets = !useOnsets; // Toggle useOnsets
                }
                break;

            case exitSetParams:
                // Optionally handle exit confirmation or actions here
                break;
        }
    }
}



void updateDisplay()
{
    hw.display.Fill(false);

    // Display the current mode
    std::string stateString = (currentState == PlayRecord) ? "Playing" : "Set Params";
    hw.display.SetCursor(0, 0);
    hw.display.WriteString(stateString.c_str(), Font_6x8, true);

    if (currentState == PlayRecord) {
        // Show the current menu option
        std::string recordMenuString = "CH: " + std::to_string(currentChannel + 1) + 
                                        " | " + (recordOn[currentChannel] ? "Rec" : "Play");
        hw.display.SetCursor(0, 10);
        hw.display.WriteString(recordMenuString.c_str(), Font_6x8, true);
        hw.display.SetCursor(0, 20);
        switch (playRecordState) {
            case channelSelect:
                hw.display.WriteString("Select Channel", Font_6x8, true);
                break;
            case recordPlay:
                hw.display.WriteString("Toggle Record/Play", Font_6x8, true);
                break;
            case exitPlayRecord:
                hw.display.WriteString("Exit to Menu", Font_6x8, true);
                break;
        }
    } 
    else if (currentState == SetParameters) {
        // Show parameter adjustment
        std::string str;
        hw.display.SetCursor(0, 10);
        switch (setParametersState) {
            case thresh:
                str = "Threshold: " + std::to_string(detectionThreshold);
                hw.display.WriteString(str.c_str(), Font_6x8, true);
                break;
            case interval:
                str = "Min Interval: " + std::to_string(minimumInterval);
                hw.display.WriteString(str.c_str(), Font_6x8, true);
                break;
            case onset:
                str = "Min Interval: " + std::string(useOnsets ? "On" : "Off");
                hw.display.WriteString(str.c_str(), Font_6x8, true);
                break;
            case exitSetParams:
                str = "Exit to Menu";
                hw.display.WriteString(str.c_str(), Font_6x8, true);
                break;
        }
    }
    
    // Update the display
    hw.display.Update();
}