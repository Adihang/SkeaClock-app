#pragma once

IMFMediaEngine* alarmValidationEngine = nullptr;
ULONGLONG alarmValidationStart = 0;
bool alarmValidationCustom = false;
std::wstring alarmValidationPath;
bool alarmValidationActive = false;

enum class AlarmValidationResult { Valid, FileError, MediaUnavailable, PlaybackUnavailable };

AlarmValidationResult AlarmMediaFailure(HRESULT error) {
    if (error == MF_E_PLATFORM_NOT_INITIALIZED || error == REGDB_E_CLASSNOTREG ||
        error == CLASS_E_CLASSNOTAVAILABLE || error == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND) ||
        error == HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND) || error == CO_E_DLLNOTFOUND)
        return AlarmValidationResult::MediaUnavailable;
    if (error == MF_E_NO_AUDIO_PLAYBACK_DEVICE || error == MF_E_AUDIO_PLAYBACK_DEVICE_IN_USE ||
        error == MF_E_AUDIO_PLAYBACK_DEVICE_INVALIDATED || error == MF_E_AUDIO_SERVICE_NOT_RUNNING ||
        error == E_OUTOFMEMORY)
        return AlarmValidationResult::PlaybackUnavailable;
    return AlarmValidationResult::FileError;
}

#include "clock-alarm-scan.h"

void CancelAlarmSoundValidation() {
    ReleaseAlarmMediaEngine(alarmValidationEngine);
    alarmValidationActive = false;
    if (alarmAudioScan) alarmAudioScan->cancelled = true;
}

void FinishAlarmSoundValidation(HWND window, AlarmValidationResult result) {
    CancelAlarmSoundValidation();
    if (result == AlarmValidationResult::Valid) return;
    if (result == AlarmValidationResult::FileError && alarmValidationCustom &&
        useCustomAlarmSound && customAlarmSoundPath == alarmValidationPath) {
        useCustomAlarmSound = false;
        customAlarmSoundPath.clear();
        SaveSoundSettings(settingsPath);
    }
    const wchar_t* warning = result == AlarmValidationResult::MediaUnavailable
        ? L"윈도우 미디어 팩이 없습니다. 정상적으로 동작하지 않을 수 있습니다."
        : L"선택한 알람 파일 실행 도중 오류가 발생했습니다";
    MessageBoxW(window, warning, L"Skea clock", MB_OK | MB_ICONWARNING);
}

void BeginAlarmSoundValidation(HWND window) {
    CancelAlarmSoundValidation();
    alarmValidationActive = true;
    alarmValidationCustom = useCustomAlarmSound;
    alarmValidationPath = useCustomAlarmSound ? customAlarmSoundPath : soundDirectory + L"base_alert.wav";
    // Check the system first: an unavailable engine must not erase the user's selection.
    HRESULT result = CreateAlarmMediaEngine(&alarmValidationEngine);
    if (FAILED(result)) {
        const auto failure = AlarmMediaFailure(result);
        FinishAlarmSoundValidation(window, failure == AlarmValidationResult::FileError
            ? AlarmValidationResult::PlaybackUnavailable : failure);
        return;
    }
    if (!SoundFileExists(alarmValidationPath) || !AlarmMediaFileSupported(alarmValidationPath)) {
        FinishAlarmSoundValidation(window, AlarmValidationResult::FileError);
        return;
    }
    // Use a separate, muted engine so validation never stops a real alarm.
    result = alarmValidationEngine->SetMuted(TRUE);
    if (SUCCEEDED(result)) result = LoadAlarmMediaSource(alarmValidationEngine, alarmValidationPath);
    if (FAILED(result)) {
        FinishAlarmSoundValidation(window, AlarmMediaFailure(result));
        return;
    }
    alarmValidationStart = GetTickCount64();
}

void CheckAlarmSoundValidation(HWND window) {
    if (alarmAudioScan && alarmAudioScan->complete.load(std::memory_order_acquire)) {
        const auto job = std::move(alarmAudioScan);
        if (!job->cancelled && alarmValidationActive) {
            FinishAlarmSoundValidation(window, job->result);
            return;
        }
    }
    if (!alarmValidationActive) return;
    if (!alarmValidationEngine) {
        if (!StartAlarmAudioScan(alarmValidationPath) ||
            (alarmAudioScan && GetTickCount64() - alarmAudioScan->progress.load() >= 15000))
            FinishAlarmSoundValidation(window, AlarmValidationResult::PlaybackUnavailable);
        return;
    }
    IMFMediaError* error = nullptr;
    alarmValidationEngine->GetError(&error);
    if (error) {
        const HRESULT code = error->GetExtendedErrorCode();
        error->Release();
        FinishAlarmSoundValidation(window, AlarmMediaFailure(code));
        return;
    }
    if (alarmValidationEngine->GetReadyState() >= MF_MEDIA_ENGINE_READY_HAVE_METADATA) {
        if (!alarmValidationEngine->HasAudio()) {
            FinishAlarmSoundValidation(window, AlarmValidationResult::FileError);
            return;
        }
        if (alarmValidationEngine->GetCurrentTime() > 0 &&
            (!alarmValidationEngine->IsPaused() || alarmValidationEngine->IsEnded())) {
            ReleaseAlarmMediaEngine(alarmValidationEngine);
            if (!StartAlarmAudioScan(alarmValidationPath))
                FinishAlarmSoundValidation(window, AlarmValidationResult::PlaybackUnavailable);
            return;
        }
    }
    // A timeout alone does not prove that a file is broken; retain the selection.
    if (GetTickCount64() - alarmValidationStart >= 15000)
        FinishAlarmSoundValidation(window, AlarmValidationResult::PlaybackUnavailable);
}
