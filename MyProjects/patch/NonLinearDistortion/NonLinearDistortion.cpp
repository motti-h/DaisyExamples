#include "daisy_patch.h"
#include "daisysp.h"
#include "AsymptoticDistoration.h"

using namespace daisy;
using namespace daisysp;
using namespace asymptotic;
DaisyPatch hw;
AsymptoticDistoration dist;
Parameter gainParam,volumeParam;
float gain,volume;

void updateControls();

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	hw.ProcessAllControls();
	updateControls();
	for (size_t i = 0; i < size; i++)
	{
		
		out[0][i] = dist.Process(in[0][i]);
		out[1][i] = in[0][i];//clean
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	gainParam.Init(hw.controls[0], 0, 60, Parameter::EXPONENTIAL);
	volumeParam.Init(hw.controls[1], 1, 2, Parameter::LOGARITHMIC);
	dist.SetGain(0.5);

	hw.StartAdc();
	hw.StartAudio(AudioCallback);
	while(1) {}
}

void updateControls()
{
	gainParam.Process();
	auto g = gainParam.Value();
	if(abs(g-gain)>0.01) gain = g;

	dist.SetGain(gain);

	volumeParam.Process();
	auto v = volumeParam.Value();
	if(abs(v-volume)>0.01) volume = v;

	dist.SetVolumeCompensation(volume);

}
