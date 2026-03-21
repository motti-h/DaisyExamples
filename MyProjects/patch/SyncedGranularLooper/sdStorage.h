#pragma once

#include "daisy_patch.h"
#include "ff.h"
#include "sys/fatfs.h"
#include "util/wav_format.h"
#include <cstring>
#include <cstdio>

namespace storage {

// Maximum number of recording slots
static const int kMaxSlots = 5;
// Chunk size for streaming reads/writes (in floats, 8KB per chunk)
static const size_t kTransferChunkSamples = 2048;

enum class SDStatus {
    Idle,
    Saving,
    Loading,
    Success,
    ErrorNoCard,
    ErrorMount,
    ErrorFileOpen,
    ErrorFileWrite,
    ErrorFileRead,
    ErrorNotWav,
};

class SDStorage {
  public:

    /// Call once after hw.Init(). Returns true if SD card is mounted.
    bool Init() {
        if (_mounted) return true; // already initialized

        // Init SDMMC peripheral
        daisy::SdmmcHandler::Config sd_cfg;
        sd_cfg.Defaults();
        _sdmmc.Init(sd_cfg);

        // Init FatFS interface
        _fsi.Init(daisy::FatFSInterface::Config::MEDIA_SD);

        // Mount the filesystem at root
        FRESULT res = f_mount(&_fsi.GetSDFileSystem(), "/", 1);
        if (res != FR_OK) {
            _lastFR = res;
            _status = SDStatus::ErrorMount;
            _mounted = false;
            return false;
        }

        _mounted = true;
        _status  = SDStatus::Idle;
        return true;
    }

    /// Save the buffer contents as a mono 32-bit float WAV file.
    bool SaveToSlot(const float* buffer, size_t num_samples,
                    int slot, uint32_t sample_rate = 48000) {
        if (!_mounted) { _status = SDStatus::ErrorNoCard; return false; }
        if (slot < 0 || slot >= kMaxSlots) { _status = SDStatus::ErrorFileOpen; return false; }

        _status = SDStatus::Saving;

        // Build filename at root: e.g. "rec_00.wav"
        char path[20];
        snprintf(path, sizeof(path), "rec_%02d.wav", slot);

        FRESULT res = f_open(&_fil, path, FA_CREATE_ALWAYS | FA_WRITE);
        if (res != FR_OK) { _lastFR = res; _status = SDStatus::ErrorFileOpen; return false; }

        // --- Write WAV header ---
        daisy::WAV_FormatTypeDef header;
        uint32_t data_size = num_samples * sizeof(float);
        BuildWavHeader(header, 1, sample_rate, 32, data_size);

        UINT bw;
        // Copy header into bounce buffer (AXI SRAM, DMA-accessible)
        memcpy(_bounce, &header, sizeof(header));
        res = f_write(&_fil, _bounce, sizeof(header), &bw);
        if (res != FR_OK || bw != sizeof(header)) {
            f_close(&_fil);
            _lastFR = res;
            _status = SDStatus::ErrorFileWrite;
            return false;
        }

        // --- Write sample data in chunks via bounce buffer ---
        size_t samples_written = 0;
        while (samples_written < num_samples) {
            size_t chunk = num_samples - samples_written;
            if (chunk > kTransferChunkSamples) chunk = kTransferChunkSamples;

            // Copy from SDRAM to AXI SRAM bounce buffer
            memcpy(_bounce, &buffer[samples_written], chunk * sizeof(float));

            res = f_write(&_fil, _bounce,
                          chunk * sizeof(float), &bw);
            if (res != FR_OK) {
                f_close(&_fil);
                _lastFR = res;
                _status = SDStatus::ErrorFileWrite;
                return false;
            }
            samples_written += chunk;
        }

        f_sync(&_fil);
        f_close(&_fil);
        _status = SDStatus::Success;
        return true;
    }

    /// Load a WAV file from a slot into the buffer.
    bool LoadFromSlot(float* buffer, size_t max_samples,
                      int slot, size_t& samples_read) {
        samples_read = 0;
        if (!_mounted) { _status = SDStatus::ErrorNoCard; return false; }
        if (slot < 0 || slot >= kMaxSlots) { _status = SDStatus::ErrorFileOpen; return false; }

        _status = SDStatus::Loading;

        char path[20];
        snprintf(path, sizeof(path), "rec_%02d.wav", slot);

        FRESULT res = f_open(&_fil, path, FA_OPEN_EXISTING | FA_READ);
        if (res != FR_OK) { _lastFR = res; _status = SDStatus::ErrorFileOpen; return false; }

        // --- Read & validate WAV header via bounce buffer ---
        daisy::WAV_FormatTypeDef header;
        UINT br;
        res = f_read(&_fil, _bounce, sizeof(header), &br);
        if (res != FR_OK || br != sizeof(header)) {
            f_close(&_fil);
            _lastFR = res;
            _status = SDStatus::ErrorFileRead;
            return false;
        }
        memcpy(&header, _bounce, sizeof(header));

        // Validate RIFF / WAVE markers
        if (header.ChunkId != 0x46464952 || header.FileFormat != 0x45564157) {
            f_close(&_fil);
            _status = SDStatus::ErrorNotWav;
            return false;
        }

        bool is_float = (header.AudioFormat == 0x0003);
        bool is_pcm16 = (header.AudioFormat == 0x0001 && header.BitPerSample == 16);
        uint16_t channels = header.NbrChannels;

        size_t total_samples = header.SubCHunk2Size / (header.BitPerSample / 8) / channels;
        size_t to_read = (total_samples < max_samples) ? total_samples : max_samples;

        // --- Read sample data in chunks via SRAM bounce buffer ---
        if (is_float) {
            size_t done = 0;
            while (done < to_read) {
                size_t chunk = to_read - done;
                if (chunk > kTransferChunkSamples) chunk = kTransferChunkSamples;

                if (channels == 1) {
                    res = f_read(&_fil, _bounce,
                                 chunk * sizeof(float), &br);
                    if (res == FR_OK)
                        memcpy(&buffer[done], _bounce, chunk * sizeof(float));
                } else {
                    float* stereo_buf = reinterpret_cast<float*>(_bounce);
                    size_t stereo_chunk = chunk * 2;
                    res = f_read(&_fil, stereo_buf,
                                 stereo_chunk * sizeof(float), &br);
                    for (size_t i = 0; i < chunk; i++) {
                        buffer[done + i] = stereo_buf[i * 2];
                    }
                }
                if (res != FR_OK) {
                    f_close(&_fil);
                    _lastFR = res;
                    _status = SDStatus::ErrorFileRead;
                    return false;
                }
                done += chunk;
            }
        } else if (is_pcm16) {
            size_t done = 0;
            while (done < to_read) {
                size_t chunk = to_read - done;
                if (chunk > kTransferChunkSamples) chunk = kTransferChunkSamples;

                int16_t* pcm_buf = reinterpret_cast<int16_t*>(_bounce);
                size_t read_count = chunk * channels;
                res = f_read(&_fil, pcm_buf,
                             read_count * sizeof(int16_t), &br);
                if (res != FR_OK) {
                    f_close(&_fil);
                    _lastFR = res;
                    _status = SDStatus::ErrorFileRead;
                    return false;
                }
                for (size_t i = 0; i < chunk; i++) {
                    buffer[done + i] = static_cast<float>(pcm_buf[i * channels])
                                       / 32768.0f;
                }
                done += chunk;
            }
        } else {
            f_close(&_fil);
            _status = SDStatus::ErrorNotWav;
            return false;
        }

        if (to_read < max_samples) {
            memset(&buffer[to_read], 0, (max_samples - to_read) * sizeof(float));
        }

        samples_read = to_read;
        f_close(&_fil);
        _status = SDStatus::Success;
        return true;
    }

    /// Check if a slot has a saved recording.
    bool SlotExists(int slot) const {
        if (!_mounted || slot < 0 || slot >= kMaxSlots) return false;
        char path[20];
        snprintf(path, sizeof(path), "rec_%02d.wav", slot);
        FILINFO fno;
        return (f_stat(path, &fno) == FR_OK);
    }

    /// Delete a recording from a slot.
    bool DeleteSlot(int slot) {
        if (!_mounted || slot < 0 || slot >= kMaxSlots) return false;
        char path[20];
        snprintf(path, sizeof(path), "rec_%02d.wav", slot);
        return (f_unlink(path) == FR_OK);
    }

    SDStatus GetStatus() const { return _status; }

    /// Get status string with error code for debugging.
    const char* GetStatusString() const {
        switch (_status) {
            case SDStatus::Idle:           return "SD Ready";
            case SDStatus::Saving:         return "Saving...";
            case SDStatus::Loading:        return "Loading...";
            case SDStatus::Success:        return "Done!";
            case SDStatus::ErrorNoCard:    return "No SD Card";
            case SDStatus::ErrorMount:     return _errBuf;
            case SDStatus::ErrorFileOpen:  return _errBuf;
            case SDStatus::ErrorFileWrite: return _errBuf;
            case SDStatus::ErrorFileRead:  return _errBuf;
            case SDStatus::ErrorNotWav:    return "Bad WAV";
        }
        return "Unknown";
    }

    /// Update error buffer with code (call after status changes)
    void UpdateErrorString() {
        const char* prefix = "";
        switch (_status) {
            case SDStatus::ErrorMount:     prefix = "Mnt"; break;
            case SDStatus::ErrorFileOpen:  prefix = "Open"; break;
            case SDStatus::ErrorFileWrite: prefix = "Wrt"; break;
            case SDStatus::ErrorFileRead:  prefix = "Rd"; break;
            default: return;
        }
        snprintf(_errBuf, sizeof(_errBuf), "%s Err:%d", prefix, (int)_lastFR);
    }

    bool IsMounted() const { return _mounted; }
    int GetMaxSlots() const { return kMaxSlots; }

  private:

    void BuildWavHeader(daisy::WAV_FormatTypeDef& h,
                        uint16_t channels, uint32_t sample_rate,
                        uint16_t bits_per_sample, uint32_t data_size) {
        h.ChunkId       = 0x46464952;
        h.FileSize      = data_size + sizeof(daisy::WAV_FormatTypeDef) - 8;
        h.FileFormat    = 0x45564157;
        h.SubChunk1ID   = 0x20746d66;
        h.SubChunk1Size = 16;
        h.AudioFormat   = 0x0003;
        h.NbrChannels   = channels;
        h.SampleRate    = sample_rate;
        h.ByteRate      = sample_rate * channels * (bits_per_sample / 8);
        h.BlockAlign    = channels * (bits_per_sample / 8);
        h.BitPerSample  = bits_per_sample;
        h.SubChunk2ID   = 0x61746164;
        h.SubCHunk2Size = data_size;
    }

    daisy::SdmmcHandler   _sdmmc;
    daisy::FatFSInterface _fsi;
    SDStatus              _status  = SDStatus::Idle;
    bool                  _mounted = false;
    FRESULT               _lastFR  = FR_OK;
    mutable char          _errBuf[16] = {0};

    // FIL must NOT be on the stack (DTCMRAM) — SDMMC IDMA can't access it.
    // As a class member of the global sdStorage object, it lives in .bss
    // (AXI SRAM 0x24000000), which IS DMA-accessible.
    FIL   _fil;

    // Bounce buffer to avoid passing SDRAM pointers to FatFS/DMA.
    // Lives in AXI SRAM (.bss) — DMA-accessible, with D-Cache managed
    // by libDaisy's sd_diskio.c automatically.
    float _bounce[kTransferChunkSamples] __attribute__((aligned(32)));
};

} // namespace storage
