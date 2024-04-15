#include "daisy_patch.h"
#include "daisysp.h"
#include "Deley.h"
#include <string>

using namespace daisy;
using namespace daisysp;
using namespace effects;

#define DAC_MAX 4095.f

// // Setup pins
// static const int record_pin      = D(S30);
// static const int loop_start_pin  = A(S31);
// static const int loop_length_pin = A(S32);
// static const int pitch_pin       = A(S33);
//daisySP
Wavefolder waveFolder;
bool isSideChained;

// Allocate buffer in SDRAM 
static const uint32_t kBufferLengthSec = 5;
static const uint32_t kSampleRate = 48000;
static const size_t kBufferLenghtSamples = kBufferLengthSec * kSampleRate;

//static float DSY_SDRAM_BSS buffer[kBufferLenghtSamples];

//static effects::Deley playDeley;
static DSY_SDRAM_BSS daisysp::DelayLine<float,kBufferLenghtSamples> delayLine;
// Hardware
DaisyPatch hw;

// Parameters
Parameter delayLength, feedback, interpolationCoef,waveFolderGain,dryWetParam;
float targetDelaySamples=0.0f,currentDelaySamples=0.0f,targetDelay=0.0f,dryWet=0.0f;

// state
static std::vector<std::string> uiStates = {"first", "second"};
static int uiState;
void UpdateControls();

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{

    
    float signal;
    float dry_in, dry_sidechain;
    UpdateControls();
    // Scales input by 2 and then the output by 0.5
    // This is because there are 6dB of headroom on the daisy
    // and currently no way to tell where '0dB' is supposed to be

    for (size_t i = 0; i < size; i++) {
        //wavefolder section
        //dry_in        = in[0][i] * 2.0f;
        //dry_sidechain = in[1][i] * 2.0f;
        signal = in[0][i] * 2.0f;
        signal = waveFolder.Process(signal)*0.5;

        // auto o = playDeley.Process(in[1][i], 0, delay);
        // out[0][i] = o[0];
        // out[1][i] = o[1];
        // auto del = delayLine.ReadHermite(delayLast*kSampleRate);
        //delay section
        auto coeff = interpolationCoef.Value();
        if(coeff==0) coeff = 1;
        fonepole(currentDelaySamples, targetDelaySamples, 1.0f / coeff); 
        delayLine.SetDelay(currentDelaySamples);
        auto del = delayLine.Read()*feedback.Value();
        //signal *= 0.5;
        auto delWrite = signal + del;
        delayLine.Write(delWrite);
        out[0][i] = signal*(100-dryWet)/100 + del*dryWet/100;
        out[1][i] = signal*(100-dryWet)/100 + del*dryWet/100;
    
    //out[0][i] = out[1][i] = pitch_shifter.Process(looper_out);
    }
    
    

}

int main(void)
{
    // Init everything.
    hw.Init();
    delayLine.Init();
    waveFolder.Init();
    hw.seed.StartLog(false);

    delayLength.Init(hw.controls[0], 0, 1, Parameter::LINEAR);
    feedback.Init(hw.controls[1], 0, 1.5, Parameter::EXPONENTIAL);
    interpolationCoef.Init(hw.controls[2], 20, (float)kBufferLenghtSamples, Parameter::EXPONENTIAL);
    waveFolderGain.Init(hw.controls[3], 0, 200, Parameter::EXPONENTIAL);
    dryWetParam.Init(hw.controls[3], 0, 100, Parameter::LINEAR);
    //briefly display the module name
    std::string str  = "effects Player";
    char *      cstr = &str[0];
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();
    hw.DelayMs(1000);
    
    // Start the ADC and Audio Peripherals on the Hardware
    hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.StartAdc();
    hw.StartAudio(AudioCallback);


    //float sample_rate = hw.seed.AudioSampleRate();

    // Setup looper
    //playDeley.Init(buffer, kBufferLenghtSamples);
    

    for(;;)
    {

    System::Delay(10);
    

    hw.display.Fill(false);
    if(uiState==0)
    {
    
        std::string str = "delay length: " + std::to_string(static_cast<uint32_t>(targetDelaySamples));
        char *      cstr = &str[0];
        hw.display.SetCursor(0, 0);
        hw.display.WriteString(cstr,Font_6x8,true);

        str = "feedback: " + std::to_string(static_cast<uint32_t>(100*feedback.Value()));
        cstr = &str[0];
        hw.display.SetCursor(0, 12);
        hw.display.WriteString(cstr,Font_6x8,true);

        str = "coef: " + std::to_string(static_cast<uint32_t>(interpolationCoef.Value()));
        cstr = &str[0];
        hw.display.SetCursor(0, 24);
        hw.display.WriteString(cstr,Font_6x8,true);

        str = "wave folder gain: " + std::to_string(static_cast<uint32_t>(waveFolderGain.Value()));
        cstr = &str[0];
        hw.display.SetCursor(0, 36);
        hw.display.WriteString(cstr,Font_6x8,true);
    }  
    else
    {
      std::string str2 = "dry/wet: " + std::to_string(static_cast<uint32_t>(dryWet));
      char *      cstr2 = &str2[0];
      hw.display.SetCursor(0, 0);
      hw.display.WriteString(cstr2,Font_6x8,true);
    }

    hw.display.Update();
    }
}

void UpdateControls()
{
    hw.ProcessAllControls();

    delayLength.Process();
    feedback.Process();
    interpolationCoef.Process();
    waveFolderGain.Process();
    dryWetParam.Process();

    if(hw.encoder.RisingEdge())
      uiState += 1;
    uiState %= 2;
    
    if(uiState==0)
    {
      auto delay = delayLength.Value();
      if(abs(delay - targetDelay)>0.01) 
      {
        targetDelay = delay;
        targetDelaySamples = targetDelay*kBufferLenghtSamples; 
      }
      waveFolder.SetGain(waveFolderGain.Value());
    }
    else
    {
        dryWet = dryWetParam.Value();
    }
    

}
