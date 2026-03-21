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
            // When starting a new recording, set playhead to rec_head
            _play_head = _rec_head;
            _play_head_float = static_cast<float>(_rec_head);
        }
        _rec_env_pos_inc = is_rec_on ? 1 : -1;
    }

    void SetLoop(const float loop_start, const float loop_length) {
      size_t new_loop_start = static_cast<size_t>(loop_start * (_buffer_length - 1));
      size_t new_loop_length = std::max(kMinLoopLength, static_cast<size_t>(loop_length * _buffer_length));

      // Jitter filter: ignore tiny changes (< 50 samples)
      size_t start_diff = (new_loop_start > _loop_start)
                              ? (new_loop_start - _loop_start)
                              : (_loop_start - new_loop_start);
      size_t length_diff = (new_loop_length > _loop_length)
                               ? (new_loop_length - _loop_length)
                               : (_loop_length - new_loop_length);

      // Apply loop start immediately
      if (start_diff >= kJitterFilterSamples) {
        _jump_pending_start = new_loop_start;
        _jump_pending_start_valid = true;
        if (_jump_state == JumpState::None && _jump_cooldown == 0) {
          _jump_state = JumpState::FadeOut;
          _jump_fade_pos = 0;
        }
      }

      // Apply loop length immediately
      if (length_diff >= kJitterFilterSamples || _loop_length == 0) {
        _loop_length = new_loop_length;
      }

      // Ensure playhead stays in range
      if (_loop_length > 0 && _play_head >= _loop_length) {
        _play_head = 0;
        _play_head_float = 0.0f;
        _jump_fade_pos = 0;
      }

      _is_loop_set = true;
    }

    void SetPlaybackSpeed(float speed) {
      _playback_speed = speed;
    }
  
    float Process(float in, bool startOver) {
      // Handle the startOver request
      if (startOver) {
        _jump_pending_start_valid = false;
        _jump_state = JumpState::FadeOut;
        _jump_fade_pos = 0;
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

      // Playback with smooth fades
      float attenuation = 1.0f;
      float output = 0.0f;

      if (_play_head < kFadeLength) {
        attenuation = static_cast<float>(_play_head) / kFadeLength;
      } else if (_play_head >= _loop_length - kFadeLength) {
        attenuation = static_cast<float>(_loop_length - _play_head) / kFadeLength;
      }

      size_t play_pos = (_loop_start + _play_head) % _buffer_length;
      size_t next_pos = (_loop_start + _play_head + 1) % _buffer_length;
      float frac = _play_head_float - static_cast<float>(_play_head);

      output = (_buffer[play_pos] * (1.0f - frac) + _buffer[next_pos] * frac) * attenuation;

      if (_jump_state == JumpState::FadeOut) {
        float mix = 1.0f - (static_cast<float>(_jump_fade_pos)
                            / static_cast<float>(kJumpFadeOutSamples));
        if (mix < 0.0f) mix = 0.0f;
        output *= mix;
        _jump_fade_pos++;
        if (_jump_fade_pos >= kJumpFadeOutSamples) {
          if (_jump_pending_start_valid) {
            _loop_start = _jump_pending_start;
          }
          _play_head = 0;
          _play_head_float = 0.0f;
          _jump_pending_start_valid = false;
          _jump_state = JumpState::FadeIn;
          _jump_fade_pos = 0;
          _jump_cooldown = kJumpCooldownSamples;
        }
      } else if (_jump_state == JumpState::FadeIn) {
        float mix = static_cast<float>(_jump_fade_pos)
                    / static_cast<float>(kJumpFadeInSamples);
        if (mix > 1.0f) mix = 1.0f;
        output *= mix;
        _jump_fade_pos++;
        if (_jump_fade_pos >= kJumpFadeInSamples) {
          _jump_state = JumpState::None;
        }
      }

      if (_jump_cooldown > 0) {
        _jump_cooldown--;
      }

      // Advance playhead with speed multiplier
      _play_head_float += _playback_speed;
      _play_head = static_cast<size_t>(_play_head_float);
      
      if (_play_head >= _loop_length) {
        // Reset the playhead to start a new loop
        _play_head = 0;
        _play_head_float = 0.0f;
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

    void ClearBuffer() {
        memset(_buffer, 0, sizeof(float) * _buffer_length);
        // Don't set _is_empty = true here.
        // The buffer data is zeroed (no old audio bleeds through),
        // but playback should keep running so new recorded audio
        // is heard as soon as the playhead crosses the rec_head.
    }

    /// Call after externally loading data into the buffer (e.g. from SD card)
    /// so the player knows it has valid audio to play.
    void SetLoaded() {
        _is_empty = false;
        _play_head = 0;
        _play_head_float = 0.0f;
    }


  private:
    static const size_t kFadeLength = 200;
    static const size_t kMinLoopLength = 2 * kFadeLength;
    static const size_t kJitterFilterSamples = 400;
    static const size_t kJumpFadeOutSamples = 128;
    static const size_t kJumpFadeInSamples = 128;
    static const size_t kJumpCooldownSamples = 2000;

    enum class JumpState { None, FadeOut, FadeIn };

    float* _buffer;

    size_t _buffer_length       = 0;
    size_t _loop_length         = 0;
    size_t _loop_start          = 0;

    size_t _play_head = 0;
    float _play_head_float = 0.0f;  // Float playhead for smooth speed control
    size_t _rec_head  = 0;

    size_t _rec_env_pos      = 0;
    int32_t _rec_env_pos_inc = 0;
    bool _is_empty  = true;
    bool _is_loop_set = false;

    JumpState _jump_state = JumpState::None;
    size_t _jump_fade_pos = 0;
    size_t _jump_pending_start = 0;
    bool _jump_pending_start_valid = false;
    size_t _jump_cooldown = 0;
    
    float _playback_speed = 1.0f;  // Playback speed multiplier (0.5x to 2.0x)
};

};