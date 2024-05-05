#include "daisy_patch.h"
#include "daisysp.h"
#include "looperTrigable.h"

using namespace daisy;
using namespace daisysp;

DaisyPatch patch;

#define kBuffSize 48000 * 60 // 60 seconds at 48kHz

// Loopers and the buffers they'll use
LooperTrigable      looper_l;
LooperTrigable      looper_r;

float DSY_SDRAM_BSS buffer_l[kBuffSize];
float DSY_SDRAM_BSS buffer_r[kBuffSize];

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // Process the controls
    patch.ProcessAllControls();

    // Set in and loop gain from CV_1 and CV_2 respectively
    float in_level   = patch.GetKnobValue(daisy::DaisyPatch::CTRL_2);
    float loop_level = patch.GetKnobValue(daisy::DaisyPatch::CTRL_3);

    //if you press the button, toggle the record state
    if(patch.encoder.RisingEdge())
    {
        looper_l.TrigRecord();
        looper_r.TrigRecord();
    }

    // if you hold the button longer than 1000 ms (1 sec), clear the loop
    if(patch.encoder.TimeHeldMs() >= 1000.f)
    {
        looper_l.Clear();
        looper_r.Clear();
    }

    if(patch.gate_input[0].Trig())
    {
        looper_l.StartOver();
        looper_r.StartOver();
    }

    // Set the led to 5V if the looper is recording
    //patch.WriteCvOut(2, 5.f * looper_l.Recording());

    // Process audio
    for(size_t i = 0; i < size; i++)
    {
        // store the inputs * the input gain factor
        float in_l = in[0][i] * in_level;
        float in_r = in[1][i] * in_level;

        // store signal = loop signal * loop gain + in * in_gain
        float sig_l = looper_l.Process(in_l) * loop_level;
        float sig_r = looper_r.Process(in_r) * loop_level;

        // send that signal to the outputs
        out[0][i] = in_l;
        out[1][i] = in_r;
        out[2][i] = sig_l;
        out[3][i] = sig_r;
    }
}

int main(void)
{
    // Initialize the hardware
    patch.Init();
    patch.StartAdc();
    // Init the loopers
    looper_l.Init(buffer_l, kBuffSize);
    looper_r.Init(buffer_r, kBuffSize);

    looper_l.SetMode(daisysp::LooperTrigable::Mode::REPLACE);
    looper_r.SetMode(daisysp::LooperTrigable::Mode::REPLACE);
    // // Init the button
    // patch.encoder.Init();

    // Start the audio callback
    patch.StartAudio(AudioCallback);


   
    // loop forever
    while(1) {}
}
