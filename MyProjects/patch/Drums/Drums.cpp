#include "daisy_patch.h"
#include "daisysp.h"
#include <string>


using namespace daisy;
using namespace daisysp;
using namespace std;

// Hardware
DaisyPatch hw;

// drums
AnalogBassDrum bassDrum;

// Persistent filtered Value for smooth delay time changes.
float ctrl1,smooth_time,sampleRate,f0,fForDrive,volume;
int encoder;
float attack_fm_amount,self_fm_amount,drive,cvIn4;

void UpdateControls();

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{

    UpdateControls();
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = bassDrum.Process()*volume;
    }
    

}

int main(void)
{
    // Init everything.
    hw.Init();
    hw.seed.StartLog(false);
    sampleRate = hw.AudioSampleRate();
    bassDrum.Init(sampleRate);
    
    //briefly display the module name
    std::string str  = "Drums";
    char *      cstr = &str[0];
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();
    hw.DelayMs(1000);
        
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
    }
}

void UpdateControls()
{
    hw.ProcessAllControls();

    //knobs
    auto maxFreq = sampleRate/16;
    auto newFreq = hw.GetKnobValue(DaisyPatch::CTRL_2)*(maxFreq);
    
    if(abs(f0 - newFreq) > 1)
    {
        f0 = newFreq;
        fForDrive = newFreq/(maxFreq);
    }
        

    attack_fm_amount = std::min(hw.GetKnobValue(DaisyPatch::CTRL_1)*4.0f ,1.0f);
    
    self_fm_amount = std::max(hw.GetKnobValue(DaisyPatch::CTRL_1)*4.0f - 1.0f ,0.0f);
    
    //drive = std::max(hw.GetKnobValue(DaisyPatch::CTRL_1)*2.0f - 1.0f, 0.0f)*std::max(1.0f-16.f*fForDrive, 0.0f);
    drive = hw.GetKnobValue(DaisyPatch::CTRL_3);

    volume = std::min(hw.GetKnobValue(DaisyPatch::CTRL_4)*8.f ,8.f);

    bassDrum.SetAttackFmAmount(attack_fm_amount);
    bassDrum.SetSelfFmAmount(self_fm_amount);
    bassDrum.SetDecay(drive);
    //bassDrum.SetAccent(drive);

    bassDrum.SetFreq(f0);

    if(hw.gate_input[0].Trig())
    {
        bassDrum.Trig();
    }

    encoder += abs(hw.encoder.Increment());
}
