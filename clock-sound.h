#pragma once
#include <commdlg.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfmediaengine.h>
#include <mferror.h>
#include <mmsystem.h>
#include <shlwapi.h>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <new>
#include <random>
#include <vector>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winmm.lib")

constexpr int EffectVolumeFirst = 300;
constexpr int AlarmVolumeFirst = 320;
constexpr int AlarmFileChoose = 340;
constexpr int AlarmSoundDefault = 341;
constexpr int AlarmSoundCustom = 342;

int effectSoundVolume = 80;
int alarmSoundVolume = 80;
bool useCustomAlarmSound = false;
std::wstring customAlarmSoundPath;
std::wstring soundDirectory;
std::vector<BYTE> currentSoundData;
IMFMediaEngine* alarmMediaEngine = nullptr;
bool alarmMediaFoundationStarted = false;
bool alarmComInitialized = false;
HMODULE alarmMediaLibrary = nullptr;
decltype(&MFStartup) alarmMFStartup = nullptr;
decltype(&MFShutdown) alarmMFShutdown = nullptr;
decltype(&MFCreateAttributes) alarmMFCreateAttributes = nullptr;

class AlarmMediaNotify final : public IMFMediaEngineNotify {
    volatile LONG references = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** result) override {
        if (!result) return E_POINTER;
        if (id == __uuidof(IUnknown) || id == __uuidof(IMFMediaEngineNotify)) {
            *result = static_cast<IMFMediaEngineNotify*>(this);
            AddRef();
            return S_OK;
        }
        *result = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&references); }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = InterlockedDecrement(&references);
        if (!remaining) delete this;
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE EventNotify(DWORD, DWORD_PTR, DWORD) override { return S_OK; }
};

int NormalizeSoundVolume(int value) {
    value = std::clamp(value, 0, 100);
    return ((value + 5) / 10) * 10;
}

bool SoundFileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool AlarmMediaFileSupported(const std::wstring& path) {
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    const wchar_t* extension = path.c_str() + dot;
    const wchar_t* supported[] = {
        L".wav", L".mp3", L".m4a", L".aac", L".flac", L".wma",
        L".mp4", L".m4v", L".mov", L".wmv"
    };
    for (const wchar_t* item : supported)
        if (_wcsicmp(extension, item) == 0) return true;
    return false;
}

std::wstring SelectedAlarmSoundPath() {
    if (useCustomAlarmSound && SoundFileExists(customAlarmSoundPath) &&
        AlarmMediaFileSupported(customAlarmSoundPath)) return customAlarmSoundPath;
    return soundDirectory + L"base_alert.wav";
}

void SaveSoundSettings(const std::wstring& path) {
    WritePrivateProfileStringW(L"sound", L"effectVolume",
        std::to_wstring(effectSoundVolume).c_str(), path.c_str());
    WritePrivateProfileStringW(L"sound", L"alarmVolume",
        std::to_wstring(alarmSoundVolume).c_str(), path.c_str());
    WritePrivateProfileStringW(L"sound", L"alarmFile",
        customAlarmSoundPath.c_str(), path.c_str());
    WritePrivateProfileStringW(L"sound", L"useCustomAlarm",
        useCustomAlarmSound ? L"1" : L"0", path.c_str());
}

void LoadSoundSettings(const std::wstring& path) {
    effectSoundVolume = NormalizeSoundVolume(GetPrivateProfileIntW(
        L"sound", L"effectVolume", 80, path.c_str()));
    alarmSoundVolume = NormalizeSoundVolume(GetPrivateProfileIntW(
        L"sound", L"alarmVolume", 80, path.c_str()));
    wchar_t file[32768] = {};
    GetPrivateProfileStringW(L"sound", L"alarmFile", L"", file,
        static_cast<DWORD>(std::size(file)), path.c_str());
    customAlarmSoundPath = file;
    useCustomAlarmSound = GetPrivateProfileIntW(
        L"sound", L"useCustomAlarm", 0, path.c_str()) != 0;
}

bool ChooseAlarmSoundFile(HWND owner) {
    wchar_t file[32768] = {};
    OPENFILENAMEW dialog = { sizeof(dialog) };
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = UiText().alarmFileFilter;
    dialog.lpstrFile = file;
    dialog.nMaxFile = static_cast<DWORD>(std::size(file));
    dialog.lpstrTitle = UiText().alarmFileTitle;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return false;
    if (!AlarmMediaFileSupported(file)) return false;
    customAlarmSoundPath = file;
    useCustomAlarmSound = true;
    return true;
}

void SetSoundDirectory(const std::wstring& sourceDirectory) {
    soundDirectory = sourceDirectory + L"sound\\";
}

std::uint16_t WaveValue16(const BYTE* data) {
    std::uint16_t value;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

std::uint32_t WaveValue32(const BYTE* data) {
    std::uint32_t value;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

bool FindPcm16Data(const std::vector<BYTE>& wave, size_t& dataOffset, size_t& dataSize) {
    if (wave.size() < 12 || std::memcmp(wave.data(), "RIFF", 4) != 0 ||
        std::memcmp(wave.data() + 8, "WAVE", 4) != 0) return false;
    bool pcm16 = false;
    dataOffset = dataSize = 0;
    for (size_t offset = 12; offset + 8 <= wave.size();) {
        const std::uint32_t chunkSize = WaveValue32(wave.data() + offset + 4);
        const size_t content = offset + 8;
        if (chunkSize > wave.size() - content) return false;
        if (std::memcmp(wave.data() + offset, "fmt ", 4) == 0 && chunkSize >= 16) {
            pcm16 = WaveValue16(wave.data() + content) == 1 &&
                WaveValue16(wave.data() + content + 14) == 16;
        } else if (std::memcmp(wave.data() + offset, "data", 4) == 0) {
            dataOffset = content;
            dataSize = chunkSize;
        }
        offset = content + chunkSize + (chunkSize & 1u);
    }
    return pcm16 && dataSize > 0 && dataSize % sizeof(std::int16_t) == 0;
}

bool LoadPcmWave(const std::wstring& path, int volume, std::vector<BYTE>& wave) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {};
    const bool validSize = GetFileSizeEx(file, &size) && size.QuadPart >= 12 &&
        size.QuadPart <= static_cast<LONGLONG>(MAXDWORD);
    if (!validSize) {
        CloseHandle(file);
        return false;
    }
    wave.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const bool loaded = ReadFile(file, wave.data(), static_cast<DWORD>(wave.size()), &read, nullptr) &&
        static_cast<size_t>(read) == wave.size();
    CloseHandle(file);
    size_t dataOffset = 0, dataSize = 0;
    if (!loaded || !FindPcm16Data(wave, dataOffset, dataSize)) {
        wave.clear();
        return false;
    }
    volume = NormalizeSoundVolume(volume);
    for (size_t offset = dataOffset; offset < dataOffset + dataSize; offset += sizeof(std::int16_t)) {
        std::int16_t sample;
        std::memcpy(&sample, wave.data() + offset, sizeof(sample));
        sample = static_cast<std::int16_t>(sample * volume / 100);
        std::memcpy(wave.data() + offset, &sample, sizeof(sample));
    }
    return true;
}

void StopSound() {
    PlaySoundW(nullptr, nullptr, 0);
    currentSoundData.clear();
}

void PlaySoundFile(const wchar_t* fileName) {
    StopSound();
    if (effectSoundVolume == 0) return;
    if (!LoadPcmWave(soundDirectory + fileName, effectSoundVolume, currentSoundData)) return;
    PlaySoundW(reinterpret_cast<LPCWSTR>(currentSoundData.data()), nullptr,
        SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

void PlayRandomIdleSound() {
    static std::mt19937 generator(std::random_device{}());
    static std::uniform_int_distribution<int> choice(0, 1);
    PlaySoundFile(choice(generator) ? L"idle1.wav" : L"idle2.wav");
}

bool InitializeAlarmSoundSystem() {
    if (alarmMediaFoundationStarted) return true;
    if (!alarmMediaLibrary) {
        alarmMediaLibrary = LoadLibraryExW(L"mfplat.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!alarmMediaLibrary) return false;
        alarmMFStartup = reinterpret_cast<decltype(alarmMFStartup)>(GetProcAddress(alarmMediaLibrary, "MFStartup"));
        alarmMFShutdown = reinterpret_cast<decltype(alarmMFShutdown)>(GetProcAddress(alarmMediaLibrary, "MFShutdown"));
        alarmMFCreateAttributes = reinterpret_cast<decltype(alarmMFCreateAttributes)>(GetProcAddress(alarmMediaLibrary, "MFCreateAttributes"));
    }
    if (!alarmMFStartup || !alarmMFShutdown || !alarmMFCreateAttributes) return false;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) return false;
    alarmComInitialized = SUCCEEDED(com);
    if (FAILED(alarmMFStartup(MF_VERSION, MFSTARTUP_FULL))) {
        if (alarmComInitialized) CoUninitialize();
        alarmComInitialized = false;
        return false;
    }
    alarmMediaFoundationStarted = true;
    return true;
}

void ReleaseAlarmMediaEngine(IMFMediaEngine*& engine) {
    if (!engine) return;
    engine->Pause();
    engine->Shutdown();
    engine->Release();
    engine = nullptr;
}

void StopAlarmSound() { ReleaseAlarmMediaEngine(alarmMediaEngine); }

HRESULT CreateAlarmMediaEngine(IMFMediaEngine** engine) {
    if (!InitializeAlarmSoundSystem()) return MF_E_PLATFORM_NOT_INITIALIZED;
    AlarmMediaNotify* notify = new (std::nothrow) AlarmMediaNotify();
    IMFAttributes* attributes = nullptr;
    IMFMediaEngineClassFactory* factory = nullptr;
    HRESULT result = notify ? alarmMFCreateAttributes(&attributes, 1) : E_OUTOFMEMORY;
    if (SUCCEEDED(result)) result = attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, notify);
    if (SUCCEEDED(result)) result = CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) result = factory->CreateInstance(
        MF_MEDIA_ENGINE_AUDIOONLY, attributes, engine);
    if (factory) factory->Release();
    if (attributes) attributes->Release();
    if (notify) notify->Release();
    return result;
}

HRESULT LoadAlarmMediaSource(IMFMediaEngine* engine, const std::wstring& path) {
    wchar_t url[32768] = {};
    DWORD urlLength = static_cast<DWORD>(std::size(url));
    HRESULT result = UrlCreateFromPathW(path.c_str(), url, &urlLength, 0);
    if (FAILED(result)) return result;
    BSTR source = SysAllocString(url);
    if (!source) return E_OUTOFMEMORY;
    result = engine->SetAutoPlay(TRUE);
    if (SUCCEEDED(result)) result = engine->SetSource(source);
    SysFreeString(source);
    if (SUCCEEDED(result)) result = engine->Load();
    return result;
}

void StartAlarmSound() {
    StopAlarmSound();
    if (alarmSoundVolume == 0) return;
    const std::wstring path = SelectedAlarmSoundPath();
    if (!SoundFileExists(path) || !AlarmMediaFileSupported(path)) return;
    HRESULT result = CreateAlarmMediaEngine(&alarmMediaEngine);
    if (SUCCEEDED(result)) result = alarmMediaEngine->SetLoop(TRUE);
    if (SUCCEEDED(result)) result = alarmMediaEngine->SetVolume(alarmSoundVolume / 100.0);
    if (SUCCEEDED(result)) result = LoadAlarmMediaSource(alarmMediaEngine, path);
    if (FAILED(result)) StopAlarmSound();
}

void ShutdownSoundSystem() {
    StopAlarmSound();
    StopSound();
    if (alarmMediaFoundationStarted) alarmMFShutdown();
    if (alarmComInitialized) CoUninitialize();
    alarmMediaFoundationStarted = false;
    alarmComInitialized = false;
    if (alarmMediaLibrary) FreeLibrary(alarmMediaLibrary);
    alarmMediaLibrary = nullptr;
    alarmMFStartup = nullptr;
    alarmMFShutdown = nullptr;
    alarmMFCreateAttributes = nullptr;
}
