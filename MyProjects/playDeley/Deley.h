#pragma once
#include "daisysp.h"
namespace effects {

class Deley {
  public:
    void Init(float *buf, size_t length) {
      _buffer = buf;
      _buffer_length = length;
      // Reset buffer contents to zero
      memset(_buffer, 0, sizeof(float) * _buffer_length);
    }

    void SetRecording(bool is_rec_on) {
        //Initialize recording head position on start
        if (_rec_env_pos_inc <= 0 && is_rec_on) {
            _rec_head = (_play_head) % _buffer_length; 
            _is_empty = false;
        }
        // When record switch changes state it effectively
        // sets ramp to rising/falling, providing a
        // fade in/out in the beginning and at the end of 
        // the recorded region.
        _rec_env_pos_inc = is_rec_on ? 1 : -1;
    }
  
    std::vector<float> Process(float in, int channel, float delay) {
      // Calculate iterator position on the record level ramp.
      if (_rec_env_pos_inc > 0 && _rec_env_pos < kFadeLength
       || _rec_env_pos_inc < 0 && _rec_env_pos > 0) {
          _rec_env_pos += _rec_env_pos_inc;
      }
      // If we're in the middle of the ramp - record to the buffer.
      if (_rec_env_pos > 0) {
        // Calculate fade in/out
        float rec_attenuation = static_cast<float>(_rec_env_pos) / static_cast<float>(kFadeLength);
        _buffer[_rec_head] = in * rec_attenuation + _buffer[_rec_head] * (1.f - rec_attenuation);
        _rec_head ++;
        _rec_head %= _buffer_length;
      }
      
      if (_is_empty) {
        output[0]=output[1]=0;
        return output;
      }

      // Playback from the buffer
      float attenuation = 1;
      
      //Calculate fade in/out
      if (_play_head < kFadeLength) {
        attenuation = static_cast<float>(_play_head) / static_cast<float>(kFadeLength);
      }
      else if (_play_head >= _buffer_length - kFadeLength) {
        attenuation = static_cast<float>(_buffer_length - _play_head) / static_cast<float>(kFadeLength);
      }
      
      // Read from the buffer
      auto play_pos = _play_head % _buffer_length;
      output[0] = _buffer[play_pos] * attenuation;

      if(abs(delayLast-delay) > 0.01)
      {
        delayLast = delay;
        _attanuate_deley = 0;
      }
      if(_attanuate_deley <= kFadeLength)
      {
          attenuation = static_cast<float>(_attanuate_deley++) / static_cast<float>(kFadeLength);
      }else
      {
          attenuation = 1.0;
      }
        
      output[1] = _buffer[(play_pos + _buffer_length - static_cast<size_t>(delayLast*_buffer_length))%_buffer_length ]*attenuation;
      // Advance playhead
      _play_head ++;
      _play_head %= _buffer_length;
      return output;
    }

  private:
    static const size_t kFadeLength = 600;
    static const size_t kMinLoopLength = 2 * kFadeLength;

    float* _buffer;

    size_t _buffer_length       = 0;

    size_t _play_head = 0;
    size_t _rec_head  = 0;
    

    size_t _rec_env_pos      = 0;
    int32_t _rec_env_pos_inc = 0;
    bool _is_empty  = true;
    bool _is_loop_set = false;

    std::vector<float> output = {0,0};
    float _deley = 0.0,delayLast=0.0;
    size_t _attanuate_deley = 0;

    
};
};
