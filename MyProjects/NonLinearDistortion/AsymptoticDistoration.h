#pragma once
#include <stdlib.h>
// using namespace daisy;
using namespace daisysp;

namespace asymptotic
{
    class AsymptoticDistoration
    {
    private:
        /* data */
        float _gain;
        float _targetGain;
        float _volume;
        float _targetVolume;
    public:
        AsymptoticDistoration(/* args */);
        ~AsymptoticDistoration();

        void SetGain(float gain);
        void SetVolumeCompensation(float volume);
        float Process(float in);
    };

    AsymptoticDistoration::AsymptoticDistoration(/* args */)
    {
    }

    AsymptoticDistoration::~AsymptoticDistoration()
    {
    }

    void AsymptoticDistoration::SetGain(float gain)
    {
        _targetGain = gain;
    }

    void AsymptoticDistoration::SetVolumeCompensation(float volume)
    {
        _targetVolume = volume;
    }

    float AsymptoticDistoration::Process(float in)
    {
        fonepole(_gain, _targetGain, 1.0f / 480);
        in*=_gain;
        fonepole(_volume, _targetVolume, 1.0f / 480);
        return (in/(abs(in)+1))*_volume;
    }
}