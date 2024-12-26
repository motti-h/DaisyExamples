#include "daisy_patch.h"
#include "daisysp.h"
#include "samplerPlayer.h"
#include <string>

using namespace daisy;
using namespace daisysp;
using namespace sampler;

#define DAC_MAX 4095.f

// // Setup pins
// static const int record_pin      = D(S30);
// static const int loop_start_pin  = A(S31);
// static const int loop_length_pin = A(S32);
// static const int pitch_pin       = A(S33);

static const float kKnobMax = 1023;

// Allocate buffer in SDRAM 
static const uint32_t kBufferLengthSec = 5;
static const uint32_t kSampleRate = 48000;
static const size_t kBufferLenghtSamples = kBufferLengthSec * kSampleRate;
static float DSY_SDRAM_BSS buffer[kBufferLenghtSamples];

static sampler::SamplerPlayer samplerPlayer;

// Hardware
DaisyPatch hw;
Parameter loopStart, loopLength, pitch;
auto startOver = false;
void UpdateControls();

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    
    if(hw.gate_input[0].Trig() ){
      startOver = true;
    }else{
      startOver = false;
    }
    for (size_t i = 0; i < size; i++) {
    auto o = samplerPlayer.Process(in[1][i],startOver);
    out[0][i] = o[0];
    out[1][i] = o[1];
    
    //out[0][i] = out[1][i] = pitch_shifter.Process(looper_out);
    }
    
    UpdateControls();

}

int main(void)
{
    // Init everything.
    hw.Init();
    hw.seed.StartLog(false);
    loopStart.Init(hw.controls[0], 0, 1, Parameter::LINEAR);
    loopLength.Init(hw.controls[1], 0, 1, Parameter::EXPONENTIAL);
    pitch.Init(hw.controls[2], 0, 1, Parameter::LINEAR);
    //briefly display the module name
    std::string str  = "Sampler Player";
    char *      cstr = &str[0];
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();
    hw.DelayMs(1000);
    
    // Start the ADC and Audio Peripherals on the Hardware
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

 
  float sample_rate = hw.seed.AudioSampleRate();

  // Setup looper
  samplerPlayer.Init(buffer, kBufferLenghtSamples);

    for(;;)
    {

    System::Delay(10);

    // hw.display.Fill(false);
    // std::string str = "value: " + std::to_string(static_cast<uint32_t>(100*cvIn1.Value()));
    // char *      cstr = &str[0];
    // hw.display.SetCursor(0, 0);
    // hw.display.WriteString(cstr,Font_6x8,true);

    hw.display.Update();
    }
}

void UpdateControls()
{
    hw.ProcessAllControls();

    loopStart.Process();
    loopLength.Process();
    pitch.Process();
      // Set loop parameters
    auto loop_start = loopStart.Value(); //fmap(loopStart.Value() / kKnobMax, 0.f, 1.f);
    auto loop_length = loopLength.Value();//fmap(loopLength.Value() / kKnobMax, 0.f, 1.f, Mapping::EXP);
    samplerPlayer.SetLoop(loop_start, loop_length);
    samplerPlayer.SetRecording(hw.encoder.Pressed());
    //knobs
    // ctrl1 = hw.GetKnobValue(DaisyPatch::CTRL_1);
    // encoder += abs(hw.encoder.Increment());
}
