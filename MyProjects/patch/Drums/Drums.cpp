#include "daisy_patch.h"
#include "daisysp.h"
#include <string>
#include <algorithm>


using namespace daisy;
using namespace daisysp;
using namespace std;

// Hardware
DaisyPatch hw;

// drums
AnalogBassDrum bassDrum;
Overdrive overDrive;

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
        out[0][i] = bassDrum.Process()*volume;//overDrive.Process(bassDrum.Process())*volume;
    }
    

}

int main(void)
{
    // Init everything.
    hw.Init();
    hw.seed.StartLog(false);
    sampleRate = hw.AudioSampleRate();
    bassDrum.Init(sampleRate);
    overDrive.Init();
    
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

// template<typename _Tp>
// constexpr const _Tp&
// clampMy(const _Tp& __val, const _Tp& __lo, const _Tp& __hi)
// {
//     __glibcxx_assert(!(__hi < __lo));
//     return (__val < __lo) ? __lo : (__hi < __val) ? __hi : __val;
// }
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
        

    attack_fm_amount =hw.GetKnobValue(DaisyPatch::CTRL_1);// std::min(hw.GetKnobValue(DaisyPatch::CTRL_1)*4.0f ,1.0f);
    bassDrum.SetAttackFmAmount(attack_fm_amount);

    self_fm_amount = hw.GetKnobValue(DaisyPatch::CTRL_2);// std::max(hw.GetKnobValue(DaisyPatch::CTRL_2)*4.0f - 1.0f ,0.0f);
    bassDrum.SetSelfFmAmount(self_fm_amount);
    //drive = std::max(hw.GetKnobValue(DaisyPatch::CTRL_1)*2.0f - 1.0f, 0.0f)*std::max(1.0f-16.f*fForDrive, 0.0f);
    auto decay = hw.GetKnobValue(DaisyPatch::CTRL_3);
    bassDrum.SetDecay(decay);
    
    auto accent = hw.GetKnobValue(DaisyPatch::CTRL_4);
    bassDrum.SetAccent(accent);
    
    
    //bassDrum.SetAccent(drive);
    
    bassDrum.SetFreq(f0);

    if(hw.gate_input[0].Trig())
    {
        bassDrum.Trig();
    }

    // drive = std::max(hw.GetKnobValue(DaisyPatch::CTRL_3)-0.5f,0.3f);
    // overDrive.SetDrive(drive);
    //volume = std::min(hw.GetKnobValue(DaisyPatch::CTRL_4)*8.f ,8.f);
    encoder += hw.encoder.Increment();
    volume = DSY_CLAMP(encoder,0,10);

}

  
