#pragma once
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <atomic>
#include <memory>
#include "clock-alarm-file-check.h"

struct AlarmAudioScan {
    std::wstring path;
    std::atomic<bool> cancelled{ false }, complete{ false };
    std::atomic<ULONGLONG> progress{ GetTickCount64() };
    AlarmValidationResult result = AlarmValidationResult::PlaybackUnavailable;
};
std::shared_ptr<AlarmAudioScan> alarmAudioScan;

HRESULT DecodeAlarmAudio(AlarmAudioScan& job,
    decltype(&MFCreateSourceReaderFromURL) createReader, decltype(&MFCreateMediaType) createType) {
    using Microsoft::WRL::ComPtr;
    constexpr DWORD audioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
    if (AlarmFileIsTruncated(job.path)) return MF_E_INVALID_FILE_FORMAT;
    ComPtr<IMFSourceReader> reader;
    HRESULT result = createReader(job.path.c_str(), nullptr, &reader);
    if (FAILED(result)) return result;
    job.progress = GetTickCount64();
    ComPtr<IMFMediaType> video;
    const bool hasVideo = SUCCEEDED(reader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &video));
    PROPVARIANT duration = {};
    ULONGLONG expected = 0;
    if (!hasVideo && SUCCEEDED(reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE),
        MF_PD_DURATION, &duration)) && duration.vt == VT_UI8) expected = duration.uhVal.QuadPart;
    PropVariantClear(&duration);
    result = reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    if (SUCCEEDED(result)) result = reader->SetStreamSelection(audioStream, TRUE);
    ComPtr<IMFMediaType> pcm;
    if (SUCCEEDED(result)) result = createType(&pcm);
    if (SUCCEEDED(result)) result = pcm->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (SUCCEEDED(result)) result = pcm->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (SUCCEEDED(result)) result = reader->SetCurrentMediaType(audioStream, nullptr, pcm.Get());
    if (FAILED(result)) return result;
    bool decodedAudio = false;
    LONGLONG decodedEnd = 0;
    while (!job.cancelled) {
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        ComPtr<IMFSample> sample;
        result = reader->ReadSample(audioStream, 0, nullptr, &flags, &timestamp, &sample);
        if (FAILED(result)) return result;
        if (flags & MF_SOURCE_READERF_ERROR) return MF_E_INVALID_FILE_FORMAT;
        if (sample) {
            DWORD bytes = 0;
            result = sample->GetTotalLength(&bytes);
            if (FAILED(result)) return result;
            if (bytes) {
                decodedAudio = true;
                LONGLONG sampleDuration = 0;
                sample->GetSampleDuration(&sampleDuration);
                decodedEnd = std::max(decodedEnd, timestamp + std::max(0LL, sampleDuration));
            }
        }
        job.progress = GetTickCount64();
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            if (!decodedAudio) return MF_E_INVALID_FILE_FORMAT;
            // Container duration can include video-only tails; compare audio-only files.
            const ULONGLONG tolerance = std::max(5000000ULL, expected / 100);
            if (expected > tolerance && static_cast<ULONGLONG>(decodedEnd) < expected - tolerance)
                return MF_E_INVALID_FILE_FORMAT;
            return S_OK;
        }
    }
    return E_ABORT;
}

void CALLBACK ScanAlarmAudio(PTP_CALLBACK_INSTANCE instance, void* context) {
    std::unique_ptr<std::shared_ptr<AlarmAudioScan>> holder(static_cast<std::shared_ptr<AlarmAudioScan>*>(context));
    const auto job = *holder;
    CallbackMayRunLong(instance);
    const bool background = SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN) != FALSE;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // Own these references so closing the clock never unloads an active scan's runtime.
    HMODULE platform = LoadLibraryExW(L"mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE readwrite = LoadLibraryExW(L"mfreadwrite.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    const auto startup = reinterpret_cast<decltype(&MFStartup)>(platform ? GetProcAddress(platform, "MFStartup") : nullptr);
    const auto shutdown = reinterpret_cast<decltype(&MFShutdown)>(platform ? GetProcAddress(platform, "MFShutdown") : nullptr);
    const auto createType = reinterpret_cast<decltype(&MFCreateMediaType)>(platform ? GetProcAddress(platform, "MFCreateMediaType") : nullptr);
    const auto createReader = reinterpret_cast<decltype(&MFCreateSourceReaderFromURL)>(
        readwrite ? GetProcAddress(readwrite, "MFCreateSourceReaderFromURL") : nullptr);
    bool started = false;
    try {
        if (FAILED(com)) job->result = AlarmValidationResult::PlaybackUnavailable;
        else if (!startup || !shutdown || !createType || !createReader || FAILED(startup(MF_VERSION, MFSTARTUP_FULL)))
            job->result = AlarmValidationResult::MediaUnavailable;
        else {
            started = true;
            const HRESULT result = job->cancelled ? E_ABORT : DecodeAlarmAudio(*job, createReader, createType);
            job->result = SUCCEEDED(result) ? AlarmValidationResult::Valid : AlarmMediaFailure(result);
        }
    } catch (...) { job->result = AlarmValidationResult::PlaybackUnavailable; }
    if (started) shutdown();
    if (readwrite) FreeLibrary(readwrite);
    if (platform) FreeLibrary(platform);
    if (SUCCEEDED(com)) CoUninitialize();
    if (background) SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
    job->complete.store(true, std::memory_order_release);
}

bool StartAlarmAudioScan(const std::wstring& path) {
    if (alarmAudioScan) return true; // A cancelled scan must finish before the next starts.
    auto job = std::make_shared<AlarmAudioScan>();
    job->path = path;
    auto holder = std::make_unique<std::shared_ptr<AlarmAudioScan>>(job);
    if (!TrySubmitThreadpoolCallback(ScanAlarmAudio, holder.get(), nullptr)) return false;
    holder.release();
    alarmAudioScan = job;
    return true;
}
