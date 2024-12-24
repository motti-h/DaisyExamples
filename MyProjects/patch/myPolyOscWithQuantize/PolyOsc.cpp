#include "daisysp.h"
#include "daisy_patch.h"
#include <string>

using namespace daisy;
using namespace daisysp;

#define DAC_MAX 4095.f
#define SCALE_CV 819.2 
DaisyPatch patch;

Oscillator osc[3];

std::string waveNames[5];

int waveform;
int final_wave;

float testval;
float tone = (1.f/6.f), equalDevision = (1.f/7.f);
std::vector<float> huseyni = { 0.0, 0.75f*tone, 1.5f*tone, 2.5f*tone, 3.5f*tone, 4.25f*tone, 5.f*tone, 1.0,1 + 0.75f*tone,1 + 1.5f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.25f*tone,1 + 5.f*tone, 2.0 };
std::vector<float> rast = { 0.0, 1.f*tone, 1.75f*tone, 2.5f*tone, 3.5f*tone, 4.5f*tone, 5.25f*tone, 1,1 + 1.f*tone,1 + 1.75f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.5f*tone,1 + 5.25f*tone, 2};
std::vector<float> hijaz = { 0.0, 0.5f*tone, 2.f*tone, 2.5f*tone, 3.5f*tone, 4.25f*tone, 5.f*tone, 1,1 + 0.5f*tone,1 + 2.f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.25f*tone,1 + 5.f*tone, 2};
std::vector<float> suznak = { 0.0, 1.f*tone, 1.75f*tone, 2.5f*tone, 3.5f*tone, 4.0f*tone, 5.5f*tone, 1.0,1 + 1.f*tone,1 + 1.75f*tone,1 + 2.5f*tone,1 + 3.5f*tone,1 + 4.0f*tone,1 + 5.5f*tone, 2.0 };
std::vector<float> huzam = { 0.0, 0.75f*tone, 1.75f*tone, 2.25f*tone, 3.75f*tone, 4.25f*tone, 5.25f*tone, 1.0,1 + 0.75f*tone,1 + 1.75f*tone,1 + 2.25f*tone,1 + 3.75f*tone,1 + 4.25f*tone,1 + 5.25f*tone, 2.0 };
std::vector<float> equal_Temprament = { 0.0, 1.f*equalDevision, 2.f*equalDevision, 3.f*equalDevision, 4.f*equalDevision, 5.f*equalDevision, 6.f*equalDevision, 1,1 + 1.f*equalDevision,1 + 2.f*equalDevision,1 + 3.f*equalDevision,1 + 4.f*equalDevision,1 + 5.f*equalDevision,1 + 6.f*equalDevision, 2};
std::vector<std::vector<float>> scales_one_octave = { equal_Temprament/*, huzam_voltages_one_octave, rast_voltages_one_octave */};
std::vector<std::vector<float>> scales{huseyni,rast,hijaz,suznak,huzam};
uint16_t scaleIndex = 0;
void CalculateClosestNote(float* cvIn, int* indexes);
uint16_t ConvertNoteToDacValue(float note);
//knobs
float cvInArr[2];

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
    float samplerate;
    patch.Init(); // Initialize hardware (daisy seed, and patch)
    //patch.seed.StartLog(true);
    samplerate = patch.AudioSampleRate();

    waveform   = 0;
    final_wave = Oscillator::WAVE_POLYBLEP_TRI;

    SetupOsc(samplerate);
    SetupWaveNames();

    testval = 0.f;
    DacHandle::Config dac_cfg;
    dac_cfg.bitdepth   = DacHandle::BitDepth::BITS_12;
    dac_cfg.buff_state = DacHandle::BufferState::ENABLED;
    dac_cfg.mode       = DacHandle::Mode::POLLING;
    dac_cfg.chn        = DacHandle::Channel::BOTH;
    patch.seed.dac.Init(dac_cfg);
    patch.StartAdc();
    patch.StartAudio(AudioCallback);
    while(1)
    {
        UpdateOled();
        cvInArr[0] = (patch.GetKnobValue(DaisyPatch::CTRL_1)+1.0)*2;
        cvInArr[1] = (patch.GetKnobValue(DaisyPatch::CTRL_1)+1.0)*2; 
        //patch.PrintLine("Print a float value: %d",  (int)(cvInArr[0]*100));
        // Quantize to semitones
        int indexes[2] = {0,0};
        CalculateClosestNote(cvInArr,indexes);    
        //frequencies[0] = powf(2.f, closestNotes[0]) * 55; //Hz
        uint16_t cvOut1 = ConvertNoteToDacValue(scales[scaleIndex][indexes[0]]);
        uint16_t cvOut2 = ConvertNoteToDacValue(scales[scaleIndex][indexes[1]]);
        patch.seed.dac.WriteValue(daisy::DacHandle::Channel::ONE, 4096/5);
        patch.seed.dac.WriteValue(daisy::DacHandle::Channel::TWO, 0);
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

    // float minDiff;

    // //knobs
    // float ctrl[4];
    // float frequencies[4];
    // for(int i = 0; i < 4; i++)
    // {
    //     ctrl[i] = patch.GetKnobValue((DaisyPatch::Ctrl)i);
    //     // patch.seed.PrintLine("Print a float value:%.3f", ctrl[i]);
    // }

    // for(int i = 0; i < 3; i++)
    // {
    //     // Quantize to semitones
    //     closestNote = 0;
    //     minDiff = std::abs(ctrl[i]);
    //     for (auto note: notesVector) 
    //     {
    //     float diff = std::abs(note - ctrl[i]);
    //     if (diff < minDiff) 
    //         {
    //             minDiff = diff;
    //             closestNote = note;  
    //         }
    //     }
    //     closestNote += ctrl[3];//*5;
    //     // closestNotes[i] = closestNote;
    //     frequencies[i] = powf(2.f, closestNote) * 55; //Hz
    // }

    //write the quantized cv to cv output
    //scale note by 1.083f ???
    
    // patch.seed.dac.WriteValue(DacHandle::Channel::ONE,static_cast<uint16_t>(SCALE_CV * 1.05f * closestNotes[0]));
    // patch.seed.dac.WriteValue(DacHandle::Channel::TWO,static_cast<uint16_t>(SCALE_CV * 1.05f * closestNotes[1]));
    // 
     


    //Adjust oscillators based on inputs
    // for(int i = 0; i < 3; i++)
    // {
    //     osc[i].SetFreq(frequencies[i]);
    //     osc[i].SetWaveform((uint8_t)waveform);
    // }


   
}

void CalculateClosestNote(float* cvIn, int* indexes){
    float minDiff1 = cvIn[0], minDiff2 = cvIn[1];

    for (auto index = 0; index < equal_Temprament.size(); ++index) 
    {
        float diff1 = std::abs(equal_Temprament[index] - cvIn[0]), diff2 = std::abs(equal_Temprament[index] - cvIn[1]);
        if (diff1 < minDiff1) 
            {
                minDiff1 = diff1;
                indexes[0] = index;
                //closestNotes[0] = note;  
            }

        if (diff2 < minDiff2) 
            {
                minDiff2 = diff2;
                indexes[1] = index;
                //closestNotes[1] = note;  
            }
    }
}


uint16_t ConvertNoteToDacValue(float note){
    return (note == 2.000f) ? (uint16_t)(note*1948+80) : (uint16_t)(note*2000);
}
