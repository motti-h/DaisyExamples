#include "daisysp.h"
#include "daisy_patch.h"
#include <string>

using namespace daisy;
using namespace daisysp;

DaisyPatch patch;
Oscillator osc;
Parameter  freqctrl, wavectrl, ampctrl, finectrl;
// Your ADC buffer, assumed to be filled with samples elsewhere
# define BUFFER_SIZE 512 
#define SAMPLE_RATE 48000 // 16 kHz sample rate
AudioHandle::InputBuffer adcBuffer;

float detectPitch();
void calculateYinDifference(float *difference, int bufferSize, int maxTau);
void cumulativeMeanNormalizedDifference(float *difference, int maxTau);
int absoluteThreshold(float *difference, int maxTau, float threshold) ;

float  freq,newFreq;
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    float  sig, amp;
    size_t wave;
    adcBuffer = in;
    patch.ProcessAnalogControls();
    newFreq = detectPitch();
    if(newFreq> 300) freq= newFreq;
    //patch.seed.PrintLine("%d." , (int)freq);
    for(size_t i = 0; i < size; i++)
    {
        // Read Knobs
        //freq = mtof(freqctrl.Process() + finectrl.Process());
        wave = wavectrl.Process();
        amp  = ampctrl.Process();
        // Set osc params
        osc.SetFreq(freq);
        osc.SetWaveform(wave);
        osc.SetAmp(amp);
        //.Process
        sig = osc.Process();
        // Assign Synthesized Waveform to all four outputs.
        for(size_t chn = 0; chn < 4; chn++)
        {
            out[chn][i] = sig;
        }
    }
}

int main(void)
{
    
    float samplerate;
    int   num_waves = Oscillator::WAVE_LAST - 1;
    patch.Init(); // Initialize hardware (daisy seed, and patch)
    patch.seed.StartLog(false);
    patch.SetAudioBlockSize(BUFFER_SIZE);
    patch.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
    samplerate = patch.AudioSampleRate();
    osc.Init(samplerate); // Init oscillator

    freqctrl.Init(
        patch.controls[patch.CTRL_1], 10.0, 110.0f, Parameter::LINEAR);
    finectrl.Init(patch.controls[patch.CTRL_2], 0.f, 7.f, Parameter::LINEAR);
    wavectrl.Init(
        patch.controls[patch.CTRL_3], 0.0, num_waves, Parameter::LINEAR);
    ampctrl.Init(patch.controls[patch.CTRL_4], 0.0, 0.5f, Parameter::LINEAR);

    //briefly display module name
    std::string str  = "VCO";
    char*       cstr = &str[0];
    patch.display.WriteString(cstr, Font_7x10, true);
    patch.display.Update();
    patch.DelayMs(1000);

    patch.StartAdc();
    patch.StartAudio(AudioCallback);
    while(1)
    {
        patch.DisplayControls(false);
    }
}



// Function to calculate the YIN difference
void calculateYinDifference(float *difference, int bufferSize, int maxTau) {
    for (int tau = 1; tau < maxTau; tau++) {
        for (int i = 0; i < bufferSize - tau; i++) {
            float delta = (adcBuffer[0][i] - adcBuffer[0][i+tau]);
            difference[tau] += delta * delta;
        }
    }
}

// Apply cumulative mean normalized difference function
void cumulativeMeanNormalizedDifference(float *difference, int maxTau) {
    float runningSum = 0;
    difference[0] = 1;
    for (int tau = 1; tau < maxTau; tau++) {
        runningSum += difference[tau];
        difference[tau] *= tau / runningSum;
    }
}

// Determine pitch period
int absoluteThreshold(float *difference, int maxTau, float threshold) {
    for (int tau = 2; tau < maxTau; tau++) {
        if (difference[tau] < threshold) {
            while (tau + 1 < maxTau && difference[tau + 1] < difference[tau]) {
                tau++;
            }
            return tau;
        }
    }
    return -1; // No pitch detected
}

// Pitch detection function
float detectPitch() {
    int maxTau = BUFFER_SIZE / 2;
    float difference[maxTau];
    for (int i = 0; i < maxTau; i++) { difference[i] = 0; }

    calculateYinDifference(difference, BUFFER_SIZE, maxTau);
    cumulativeMeanNormalizedDifference(difference, maxTau);

    int pitchPeriod = absoluteThreshold(difference, maxTau, 0.6); // 0.1 is a typical threshold
    if (pitchPeriod != -1) {
        return (float)SAMPLE_RATE / pitchPeriod;
    }
    return 2.0; // Error code for no pitch detected
}
