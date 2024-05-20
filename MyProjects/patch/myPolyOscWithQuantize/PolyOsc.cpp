#include "daisysp.h"
#include "daisy_patch.h"
#include <string>

using namespace daisy;
using namespace daisysp;

DaisyPatch patch;

Oscillator osc[3];

std::string waveNames[5];

int waveform;
int final_wave;

float testval;

std::vector<float> notesVector;

std::vector<float> sika_voltages_one_octave = {0.000f, 0.087f, 0.252f, 0.418f, 0.586f, 0.671f, 0.839f, 1.000f, 1.087f, 1.252f, 1.418f, 1.586f, 1.671f, 1.839f, 2.000f};
std::vector<float> huseyni_voltages_one_octave = { 0.0, 0.125, 0.25, 0.41666, 0.583, 0.7083, 0.833, 1.0, 1.125, 1.25, 1.41666, 1.583, 1.7083, 1.833, 2.0 };

std::vector<std::vector<float>> scales_one_octave = { huseyni_voltages_one_octave/*, huzam_voltages_one_octave, rast_voltages_one_octave */};
float closestNote=0;

void UpdateControls();

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    UpdateControls();
    for(size_t i = 0; i < size; i++)
    {
        float mix = 0;
        //Process and output the three oscillators
        for(size_t chn = 0; chn < 3; chn++)
        {
            float sig = osc[chn].Process();
            mix += sig * .25f;
            out[chn][i] = sig;
        }

        //output the mixed oscillators
        out[3][i] = mix;
    }
}

void SetupOsc(float samplerate)
{
    for(int i = 0; i < 3; i++)
    {
        osc[i].Init(samplerate);
        osc[i].SetAmp(.7);
    }
}

void SetupWaveNames()
{
    waveNames[0] = "sine";
    waveNames[1] = "triangle";
    waveNames[2] = "saw";
    waveNames[3] = "ramp";
    waveNames[4] = "square";
}

void UpdateOled();

int main(void)
{
    notesVector = scales_one_octave.at(0);

    float samplerate;
    patch.Init(); // Initialize hardware (daisy seed, and patch)
    samplerate = patch.AudioSampleRate();

    waveform   = 0;
    final_wave = Oscillator::WAVE_POLYBLEP_TRI;

    SetupOsc(samplerate);
    SetupWaveNames();

    testval = 0.f;

    patch.StartAdc();
    patch.StartAudio(AudioCallback);
    while(1)
    {
        UpdateOled();
    }
}

void UpdateOled()
{
    patch.display.Fill(false);

    patch.display.SetCursor(0, 0);
    std::string str  = "PolyOsc";
    char*       cstr = &str[0];
    patch.display.WriteString(cstr, Font_7x10, true);

    str = "waveform:";
    patch.display.SetCursor(0, 30);
    patch.display.WriteString(cstr, Font_7x10, true);

    patch.display.SetCursor(70, 30);
    cstr = &waveNames[waveform][0];
    patch.display.WriteString(cstr, Font_7x10, true);

    patch.display.Update();
}

void UpdateControls()
{
    patch.ProcessDigitalControls();
    patch.ProcessAnalogControls();

    float minDiff;

    


    //knobs
    float ctrl[4];
    for(int i = 0; i < 4; i++)
    {
        ctrl[i] = patch.GetKnobValue((DaisyPatch::Ctrl)i);
    }

    for(int i = 0; i < 3; i++)
    {
        // ctrl[i] += ctrl[3];
        // ctrl[i] = ctrl[i];           //voltage
        
        // Quantize to semitones
        closestNote = 0;
        minDiff = std::abs(ctrl[i]);
        for (auto note: notesVector) 
        {
        float diff = std::abs(note - ctrl[i]);
        if (diff < minDiff) 
            {
                minDiff = diff;
                closestNote = note;  
            }
        }
        closestNote += ctrl[3]*5;
        ctrl[i] = powf(2.f, closestNote) * 55; //Hz
    }

    testval = patch.GetKnobValue((DaisyPatch::Ctrl)2) * 5.f;

    //encoder
    waveform += patch.encoder.Increment();
    waveform = (waveform % final_wave + final_wave) % final_wave;

    //Adjust oscillators based on inputs
    for(int i = 0; i < 3; i++)
    {
        osc[i].SetFreq(ctrl[i]);
        osc[i].SetWaveform((uint8_t)waveform);
    }
}
