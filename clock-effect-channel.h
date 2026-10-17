#pragma once

enum class EffectSoundCategory { Start, End, Skea };

struct EffectSoundChannel {
    HWAVEOUT output = nullptr;
    WAVEHDR header = {};
    std::vector<BYTE> wave;

    bool Stop() {
        if (output) {
            if (waveOutReset(output) != MMSYSERR_NOERROR ||
                waveOutUnprepareHeader(output, &header, sizeof(header)) != MMSYSERR_NOERROR ||
                waveOutClose(output) != MMSYSERR_NOERROR) return false;
            output = nullptr;
        }
        header = {};
        wave.clear();
        return true;
    }

    void Play(const std::wstring& path, int volume) {
        if (!Stop() || volume == 0) return;
        if (!LoadPcmWave(path, volume, wave)) return;
        size_t offset = 0, size = 0;
        WAVEFORMATEX format = {};
        if (!FindPcm16Data(wave, offset, size, &format)) return;
        if (waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return;
        header.lpData = reinterpret_cast<LPSTR>(wave.data() + offset);
        header.dwBufferLength = static_cast<DWORD>(size);
        if (waveOutPrepareHeader(output, &header, sizeof(header)) != MMSYSERR_NOERROR ||
            waveOutWrite(output, &header, sizeof(header)) != MMSYSERR_NOERROR) Stop();
    }
};

EffectSoundChannel effectSoundChannels[3];
