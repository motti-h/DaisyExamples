#pragma once

#include <vector>
#include <algorithm>
#include <cstring>

namespace sampler {

class SamplerPlayer {
  public:
    void Init(float* buf, size_t length) {
      _buffer = buf;
      _buffer_length = length;
      // Reset buffer contents to zero
      memset(_buffer, 0, sizeof(float) * _buffer_length);
    }

    void SetRecording(bool is_rec_on) {
        if (_rec_env_pos_inc <= 0 && is_rec_on) {
            _rec_head = _play_head % _buffer_length; 
            _is_empty = false;
        }
        _rec_env_pos_inc = is_rec_on ? 1 : -1;
    }

    void SetLoop(const float loop_start, const float loop_length) {
      // Set the start of the next loop
      _pending_loop_start = static_cast<size_t>(loop_start * (_buffer_length - 1));

      // Set the length of the next loop
      _pending_loop_length = std::max(kMinLoopLength, static_cast<size_t>(loop_length * _buffer_length));

      _is_loop_set = true;
    }
  
    float Process(float in, bool startOver) {
      // Handle the startOver request
      if (startOver) {
        _play_head = 0;
      }

      // Calculate iterator position on the record level ramp.
      if (_rec_env_pos_inc > 0 && _rec_env_pos < kFadeLength
       || _rec_env_pos_inc < 0 && _rec_env_pos > 0) {
          _rec_env_pos += _rec_env_pos_inc;
      }

      // If we're in the middle of the ramp - record to the buffer.
      if (_rec_env_pos > 0) {
        float rec_attenuation = static_cast<float>(_rec_env_pos) / static_cast<float>(kFadeLength);
        _buffer[_rec_head] = in * rec_attenuation + _buffer[_rec_head] * (1.f - rec_attenuation);
        _rec_head++;
        _rec_head %= _buffer_length;
      }
      
      if (_is_empty) {
        return 0;
      }

      // Smooth transition when loop start changes
      if (_play_head == 0) {
        _loop_start = _pending_loop_start;
        _loop_length = _pending_loop_length;
      }

      // Playback with smooth fades
      float attenuation = 1.0f;
      float output = 0.0f;

      if (_play_head < kFadeLength) {
        attenuation = static_cast<float>(_play_head) / kFadeLength;
      } else if (_play_head >= _loop_length - kFadeLength) {
        attenuation = static_cast<float>(_loop_length - _play_head) / kFadeLength;
      }

      // Read from the buffer
      size_t play_pos = (_loop_start + _play_head) % _buffer_length;  
      output = _buffer[play_pos] * attenuation;

      // Advance playhead
      _play_head++;
      if (_play_head >= _loop_length) {
        // Reset the playhead to start a new loop
        _play_head = 0;
      }

      return output;
    }

    // New methods for accessing playback and loop information
    size_t GetCurrentPosition() const {
        return _play_head;
    }

    size_t GetLoopStartPosition() const {
        return _loop_start;
    }

    size_t GetLoopEndPosition() const {
        return (_loop_start + _loop_length) % _buffer_length;
    }

    size_t GetLoopLength() const { // New method
        return _loop_length;
    }

    size_t GetBufferLength() const { // New method added
        return _buffer_length;
    }


  private:
    static const size_t kFadeLength = 10;
    static const size_t kMinLoopLength = 2 * kFadeLength;

    float* _buffer;

    size_t _buffer_length       = 0;
    size_t _loop_length         = 0;
    size_t _pending_loop_length = 0;
    size_t _loop_start          = 0;
    size_t _pending_loop_start  = 0;

    size_t _play_head = 0;
    size_t _rec_head  = 0;

    size_t _rec_env_pos      = 0;
    int32_t _rec_env_pos_inc = 0;
    bool _is_empty  = true;
    bool _is_loop_set = false;
};

};