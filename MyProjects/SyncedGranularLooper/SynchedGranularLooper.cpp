#include "daisy_patch.h"
#include "daisysp.h"
#include <string>

using namespace daisy;
using namespace daisysp;

#define NUM_VOICES 32
#define MAX_DELAY ((size_t)(10.0f * 48000.0f))
#define DAC_MAX 4095.f

// Hardware
DaisyPatch hw;
int encoder=0;
float ctrl1;
// Persistent filtered Value for smooth delay time changes.
float smooth_time;
std::vector<float> notesVector;

std::vector<float> sika_voltages_one_octave = {0.000f, 0.087f, 0.252f, 0.418f, 0.586f, 0.671f, 0.839f, 1.000f};
std::vector<float> huseyni_voltages_one_octave = { 0.0, 0.125, 0.25, 0.41666, 0.583, 0.7083, 0.833, 1.0 };
std::vector<float> huzam_voltages_one_octave = { 0.0, 3/24.f, 7/24.f, 9/24.f, 15/24.f, 17/24.f, 21/24.f, 1.0 };
std::vector<float> rast_voltages_one_octave = { 0.0, 9/53.f, 17/53.f, 22/53.f, 29/53.f, 38/53.f, 46/53.f, 1.0 };
std::vector<std::vector<float>> scales_one_octave = { huseyni_voltages_one_octave, huzam_voltages_one_octave, rast_voltages_one_octave };
std::vector<std::string> scales_names = { "Huseyni", "Huzam", "Rast" };
Parameter cvIn1;
float valOut=0;
float closestNote=0;

void UpdateControls();

float find_closest_sika_note(float input_voltage) {
    // Determine the octave based on the input voltage
    int octave = static_cast<int>(input_voltage);
    float voltage_in_octave = input_voltage - octave;

    // Generate the Sika voltages for the corresponding octave
    std::vector<float> voltages;
    for (float v : huseyni_voltages_one_octave) {
        voltages.push_back(voltage_in_octave + v);
    }

    // Find the closest voltage within the octave
    float min_diff = std::abs(voltage_in_octave - voltages[0]);
    int closest_index = 0;

    for (int i = 1; i < voltages.size(); i++) {
        float diff = std::abs(voltage_in_octave - voltages[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest_index = i;
        }
    }

    return voltages[closest_index];
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    float minDiff;

    UpdateControls();
    
    // Quantize to semitones
    closestNote = 0;
    minDiff = std::abs(ctrl1);
    for (auto note: notesVector) {
        float diff = std::abs(note - ctrl1);
        if (diff < minDiff) {
            minDiff = diff;
            closestNote = note;  
        }
    }
    
    hw.seed.dac.WriteValue(DacHandle::Channel::ONE,static_cast<uint16_t>((DAC_MAX/5.f) * 1.083f * closestNote));
}

int main(void)
{
    // Init everything.
    hw.Init();
    hw.seed.StartLog(false);
    cvIn1.Init(hw.controls[0], .3, 1, Parameter::LINEAR);
    //briefly display the module name
    std::string str  = "Quantizer1";
    char *      cstr = &str[0];
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();
    hw.DelayMs(1000);

    //prepare notes values vector
    //float noteValue = 0;
    // while(noteValue<=1)
    // {
    //     notesArray.push_back(noteValue);
    //     noteValue = noteValue + 1/(12.f*5);
    // }
    
    for(auto i = 0 ; i <= 4; ++i)
    {
        for(auto e: huseyni_voltages_one_octave)
        {
            notesVector.push_back((e/5.f)+i/5.f);
        }
    }
        
    // Start the ADC and Audio Peripherals on the Hardware
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    for(;;)
    {

    System::Delay(10);

    hw.display.Fill(false);
    std::string str = "value: " + std::to_string(static_cast<uint32_t>(100*ctrl1));
    char *      cstr = &str[0];
    hw.display.SetCursor(0, 0);
    hw.display.WriteString(cstr,Font_6x8,true);
    
    std::string str2 = "closest note: " + std::to_string(static_cast<uint32_t>((DAC_MAX/5.f)*closestNote));
    char *      cstr2 = &str2[0];
    hw.display.SetCursor(0, 9);
    hw.display.WriteString(cstr2,Font_6x8,true);

    
    notesVector = scales_one_octave.at(encoder % scales_one_octave.size());

    std::string str3 = "maqam: " + scales_names.at(encoder % scales_one_octave.size());
    char *      cstr3 = &str3[0];
    hw.display.SetCursor(0, 18);
    hw.display.WriteString(cstr3,Font_6x8,true);
    hw.display.Update();
    }
}

void UpdateControls()
{
    hw.ProcessAllControls();

    //knobs
    ctrl1 = hw.GetKnobValue(DaisyPatch::CTRL_1);
    encoder += abs(hw.encoder.Increment());
}
