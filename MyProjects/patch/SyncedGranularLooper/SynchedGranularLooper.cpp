#include "daisy_patch.h"
#include "daisysp.h"
#include "samplerPlayer.h"
#include "sdStorage.h"
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

// SD card storage
static storage::SDStorage sdStorage;
static bool sdInitialized = false;
static uint32_t sdStatusDisplayTime = 0;
static const uint32_t kStatusDisplayMs = 2000;
static bool showSdStatus = false;

// Two-layer UI
enum class UIScreen { ModeSelect, Play, Load, Save };
static UIScreen uiScreen = UIScreen::Play;
static int modeSelectCursor = 0;   // 0=Play, 1=Load, 2=Save
static int slotCursor = 0;         // 0..4 = slots, 5 = Exit
static const int kNumSlots = 5;
static const int kExitItem = kNumSlots; // index 5 = Exit

// Encoder event accumulators (written in audio callback, read in main loop)
static volatile int encoderAccum = 0;
static volatile bool encoderPressed = false;

// Structure to hold the dot positions
struct Dot {
    int x;
    int y;
};
Dot sparklingDots[10];
DaisyPatch hw;
Parameter loopStart, loopLength, baseSpeed;
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
    baseSpeed.Init(hw.controls[3], 0.5f, 2.0f, Parameter::EXPONENTIAL); // Knob 4 for base speed
    // Note: We read control[2] directly for CV pitch control
    samplerPlayer.Init(buffer, kBufferLengthSamples);

    // Initialize SD card (retry a few times — card needs time after power-on)
    std::string str;
    char* cstr;
    hw.DelayMs(500); // let SD card power up
    for (int attempt = 0; attempt < 3 && !sdInitialized; attempt++) {
        sdInitialized = sdStorage.Init();
        if (!sdInitialized) hw.DelayMs(250);
    }
    if (sdInitialized) {
        str = "SD Card Ready";
    } else {
        sdStorage.UpdateErrorString();
        str = sdStorage.GetStatusString();
    }
    cstr = &str[0];
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

        // --- UI controls (processed outside audio callback) ---
        if (sdInitialized) {
            // Read and reset accumulated encoder events
            int inc = encoderAccum;
            encoderAccum = 0;
            bool pressed = encoderPressed;
            encoderPressed = false;

            switch (uiScreen) {
            case UIScreen::ModeSelect:
                // Turn = cycle Play/Load/Save
                if (inc != 0) {
                    modeSelectCursor += inc;
                    if (modeSelectCursor < 0) modeSelectCursor = 2;
                    if (modeSelectCursor > 2) modeSelectCursor = 0;
                }
                // Press = enter selected mode
                if (pressed) {
                    if (modeSelectCursor == 0)      uiScreen = UIScreen::Play;
                    else if (modeSelectCursor == 1) { uiScreen = UIScreen::Load; slotCursor = 0; }
                    else                            { uiScreen = UIScreen::Save; slotCursor = 0; }
                }
                break;

            case UIScreen::Play:
                // Turn = exit back to mode select
                if (inc != 0) {
                    if (recordOn) {
                        recordOn = false;
                        samplerPlayer.SetRecording(false);
                        // After stopping recording, check if buffer has nonzero data
                        bool nonzero = false;
                        for (size_t i = 0; i < kBufferLengthSamples; i += 128) {
                            if (fabsf(buffer[i]) > 1e-5f) { nonzero = true; break; }
                        }
                        hw.display.Fill(false);
                        hw.display.SetCursor(0, 30);
                        hw.display.WriteString(const_cast<char*>(nonzero ? "REC OK" : "REC ZERO"), Font_7x10, true);
                        hw.display.Update();
                        hw.DelayMs(1000);
                    }
                    uiScreen = UIScreen::ModeSelect;
                }
                // Press = toggle recording
                if (pressed) {
                    if (!recordOn) {
                        // Starting recording: clear buffer first
                        samplerPlayer.ClearBuffer();
                        // Show input sample value for debug
                        float input_val = 0.0f;
                        // Try to get a recent input sample (from buffer[0] if possible)
                        // But buffer[0] is the audio buffer, not input. So show a message.
                        hw.display.Fill(false);
                        hw.display.SetCursor(0, 10);
                        hw.display.WriteString(const_cast<char*>("REC STARTED"), Font_6x8, true);
                        hw.display.SetCursor(0, 24);
                        hw.display.WriteString(const_cast<char*>("Check input wiring!"), Font_6x8, true);
                        hw.display.Update();
                        hw.DelayMs(1000);
                    }
                    recordOn = !recordOn;
                    samplerPlayer.SetRecording(recordOn);
                    if (!recordOn) {
                        // Just stopped recording: check buffer
                        bool nonzero = false;
                        float maxval = 0.0f;
                        for (size_t i = 0; i < kBufferLengthSamples; i += 128) {
                            float v = fabsf(buffer[i]);
                            if (v > 1e-5f) nonzero = true;
                            if (v > maxval) maxval = v;
                        }
                        hw.display.Fill(false);
                        hw.display.SetCursor(0, 10);
                        char msg[32];
                        snprintf(msg, sizeof(msg), "%s L:%lu", nonzero ? "REC OK" : "REC ZERO", (unsigned long)samplerPlayer.GetLoopLength());
                        hw.display.WriteString(msg, Font_6x8, true);
                        hw.display.SetCursor(0, 24);
                        snprintf(msg, sizeof(msg), "B0:%.3f M:%.3f", buffer[0], maxval);
                        hw.display.WriteString(msg, Font_6x8, true);
                        hw.display.Update();
                        hw.DelayMs(2000);
                    }
                }
                break;

            case UIScreen::Load:
                // Turn = cycle slots 0..4 + Exit
                if (inc != 0) {
                    slotCursor += inc;
                    if (slotCursor < 0) slotCursor = kExitItem;
                    if (slotCursor > kExitItem) slotCursor = 0;
                }
                // Press = load selected slot or exit
                if (pressed) {
                    if (slotCursor == kExitItem) {
                        uiScreen = UIScreen::ModeSelect;
                    } else if (sdStorage.SlotExists(slotCursor)) {
                        size_t loaded = 0;
                        hw.StopAudio();
                        sdStorage.LoadFromSlot(buffer, kBufferLengthSamples,
                                               slotCursor, loaded);
                        hw.StartAudio(AudioCallback);
                        if (loaded > 0) samplerPlayer.SetLoaded();
                        sdStorage.UpdateErrorString();
                        showSdStatus = true;
                        sdStatusDisplayTime = currentTime;
                        uiScreen = UIScreen::Play;
                    }
                }
                break;

            case UIScreen::Save:
                // Turn = cycle slots 0..4 + Exit
                if (inc != 0) {
                    slotCursor += inc;
                    if (slotCursor < 0) slotCursor = kExitItem;
                    if (slotCursor > kExitItem) slotCursor = 0;
                }
                // Press = save to selected slot or exit
                if (pressed) {
                    if (slotCursor == kExitItem) {
                        uiScreen = UIScreen::ModeSelect;
                    } else {
                        hw.StopAudio();
                        sdStorage.SaveToSlot(buffer, kBufferLengthSamples,
                                             slotCursor, kSampleRate);
                        hw.StartAudio(AudioCallback);
                        sdStorage.UpdateErrorString();
                        showSdStatus = true;
                        sdStatusDisplayTime = currentTime;
                        uiScreen = UIScreen::Play;
                    }
                }
                break;
            }

            // Auto-hide status after timeout
            if (showSdStatus
                && (currentTime - sdStatusDisplayTime > kStatusDisplayMs)) {
                showSdStatus = false;
            }
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
    baseSpeed.Process();
    hw.controls[2].Process(); // Process control 3 directly for CV

    auto loop_start = loopStart.Value();
    auto loop_length = loopLength.Value();
    auto base_speed_value = baseSpeed.Value(); // 0.5x to 2.0x from knob 4
    
    // Convert base speed to octaves: 0.5x = -1 octave, 1.0x = 0 octaves, 2.0x = +1 octave
    float base_octaves = log2f(base_speed_value);
    
    float controlValue = hw.controls[2].Value(); // 0 to 1 (nominally 0V to 5V)
    
    // Two-stage calibration: accurate first octave, then adjust higher octaves
    const float FIRST_OCTAVE_THRESHOLD = 0.2f; // First 20% of range = first octave
    const float HIGH_OCTAVE_SCALE = 0.96f;     // Adjust this for higher octaves (try 0.9-1.05)
    
    float cv_octaves;
    
    if (controlValue <= FIRST_OCTAVE_THRESHOLD) {
        // First octave: use direct linear mapping (already accurate)
        cv_octaves = controlValue * 5.0f;
    } else {
        // Higher octaves: apply calibration scaling
        // Keep first octave as-is, then scale the rest
        float firstOctave = FIRST_OCTAVE_THRESHOLD * 5.0f; // = 1.0 octave
        float remainingValue = controlValue - FIRST_OCTAVE_THRESHOLD;
        float remainingOctaves = (remainingValue / (1.0f - FIRST_OCTAVE_THRESHOLD)) * 4.0f; // Remaining 4 octaves
        cv_octaves = firstOctave + (remainingOctaves * HIGH_OCTAVE_SCALE);
    }
    
    // Add base octaves to CV octaves (this preserves V/oct tracking!)
    float total_octaves = base_octaves + cv_octaves;
    
    // Apply exponential conversion: speed = 2^octaves
    float final_speed = powf(2.0f, total_octaves);

    samplerPlayer.SetLoop(loop_start, loop_length);
    samplerPlayer.SetPlaybackSpeed(final_speed);

    samplerPlayer.SetRecording(recordOn);

    // Capture encoder events for main loop consumption
    encoderAccum += hw.encoder.Increment();
    if (hw.encoder.RisingEdge()) encoderPressed = true;
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

    // Display base speed multiplier
    hw.display.SetCursor(0, 10);
    int baseSpeedInt = (int)(baseSpeed.Value() * 100);
    std::string baseStr = "BS:" + std::to_string(baseSpeedInt);
    char* baseCstr = &baseStr[0];
    hw.display.WriteString(baseCstr, Font_6x8, true);
    
    // Display final playback speed
    hw.display.SetCursor(70, 0);
    float controlValue = hw.controls[2].Value();
    float octaves;
    const float FIRST_OCTAVE_THRESHOLD = 0.2f;
    const float HIGH_OCTAVE_SCALE = 0.96f;
    
    if (controlValue <= FIRST_OCTAVE_THRESHOLD) {
        octaves = controlValue * 5.0f;
    } else {
        float firstOctave = FIRST_OCTAVE_THRESHOLD * 5.0f;
        float remainingValue = controlValue - FIRST_OCTAVE_THRESHOLD;
        float remainingOctaves = (remainingValue / (1.0f - FIRST_OCTAVE_THRESHOLD)) * 4.0f;
        octaves = firstOctave + (remainingOctaves * HIGH_OCTAVE_SCALE);
    }
    
    float cv_speed = powf(2.0f, octaves);
    float final_speed = baseSpeed.Value() * cv_speed;
    int speedInt = (int)(final_speed * 100);
    std::string speedStr = std::to_string(speedInt);
    char* speedCstr = &speedStr[0];
    hw.display.WriteString(speedCstr, Font_6x8, true);

    // --- UI overlay ---
    if (sdInitialized) {
        switch (uiScreen) {
        case UIScreen::ModeSelect: {
            // Full-screen mode select menu
            hw.display.Fill(false);
            hw.display.SetCursor(20, 0);
            hw.display.WriteString(const_cast<char*>("Select Mode"), Font_7x10, true);
            const char* modes[] = {"Play", "Load", "Save"};
            for (int i = 0; i < 3; i++) {
                hw.display.SetCursor(20, 18 + i * 14);
                char line[20];
                snprintf(line, sizeof(line), "%s %s", (i == modeSelectCursor) ? ">" : " ", modes[i]);
                hw.display.WriteString(line, Font_7x10, true);
            }
            break;
        }
        case UIScreen::Play:
            // Show mode label + recording hint
            hw.display.SetCursor(80, 0);
            hw.display.WriteString(const_cast<char*>(recordOn ? "[REC]" : "Play"), Font_6x8, true);
            hw.display.SetCursor(0, 56);
            hw.display.WriteString(const_cast<char*>(recordOn ? "Press:Stop  Turn:Exit" : "Press:Rec   Turn:Exit"), Font_6x8, true);
            if (showSdStatus) {
                hw.display.SetCursor(0, 46);
                hw.display.WriteString(const_cast<char*>(sdStorage.GetStatusString()), Font_6x8, true);
            }
            break;

        case UIScreen::Load:
        case UIScreen::Save: {
            // Slot selection overlay on bottom half
            bool isLoad = (uiScreen == UIScreen::Load);
            hw.display.SetCursor(0, 30);
            hw.display.WriteString(const_cast<char*>(isLoad ? "-- Load --" : "-- Save --"), Font_6x8, true);
            // Draw slot items + Exit
            for (int s = 0; s <= kExitItem; s++) {
                int yPos = 40 + (s % 3) * 8;
                int xPos = (s < 3) ? 0 : 64;
                hw.display.SetCursor(xPos, yPos);
                char item[16];
                if (s < kNumSlots) {
                    bool exists = sdStorage.SlotExists(s);
                    snprintf(item, sizeof(item), "%s%d%s",
                             (s == slotCursor) ? ">" : " ",
                             s + 1,
                             exists ? "*" : " ");
                } else {
                    snprintf(item, sizeof(item), "%sExit",
                             (s == slotCursor) ? ">" : " ");
                }
                hw.display.WriteString(item, Font_6x8, true);
            }
            break;
        }
        }
    }

    hw.display.Update();
}