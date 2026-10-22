#pragma once
#include <chrono>
#include <future>
#include "clock-animation-chunk.h"

struct AnimationImage {
    std::vector<AnimationFrameRegion> regions;
    std::vector<std::wstring> paths;
    std::vector<std::unique_ptr<AnimationChunk>> chunks;
    std::vector<UINT> frameEnds;
    UINT width = 0, height = 0, firstFrame = 0, storedFrames = 0, chunkSize = 0, frame = 0;
    UINT pinnedChunk = UINT_MAX, requestedChunk = UINT_MAX, loadingChunk = UINT_MAX;
    bool active = false;
    // Destruction waits for the loader before any metadata it reads is released.
    std::future<std::unique_ptr<AnimationChunk>> loading;

    UINT GetWidth() const { return width; }
    UINT GetHeight() const { return height; }
    UINT GetFrameCount() const { return static_cast<UINT>(frameEnds.size()); }
    UINT ChunkIndex(UINT index) const { return (index - firstFrame) / chunkSize; }
    UINT ChunkFirst(UINT index) const { return firstFrame + index * chunkSize; }
    UINT NextChunk() const { return (ChunkIndex(frame) + 1) % static_cast<UINT>(chunks.size()); }
    PointF FrameOffset() const { return regions[frame].offset; }
    Bitmap* Frame() const {
        const UINT index = ChunkIndex(frame);
        return chunks[index] ? chunks[index]->views[frame - ChunkFirst(index)].get() : nullptr;
    }

    bool Load(const std::wstring& directory, const std::wstring& config, const wchar_t* section) {
        auto read = [&](const wchar_t* key) {
            return static_cast<int>(GetPrivateProfileIntW(section, key, -1, config.c_str()));
        };
        const int w = read(L"width"), h = read(L"height"), count = read(L"frameCount");
        const int first = read(L"firstFrame"), stored = read(L"sheetFrames"), size = read(L"chunkSize");
        const int chunkCount = read(L"chunks");
        if (w <= 0 || h <= 0 || count <= 0 || first < 0 || first >= count || size <= 0 ||
            stored <= 0 || stored > count - first || chunkCount != (stored - 1) / size + 1) return false;
        width = static_cast<UINT>(w);
        height = static_cast<UINT>(h);
        firstFrame = frame = static_cast<UINT>(first);
        storedFrames = static_cast<UINT>(stored);
        chunkSize = static_cast<UINT>(size);
        regions.resize(count);
        chunks.resize(chunkCount);
        for (int i = 0; i < chunkCount; ++i) {
            wchar_t file[MAX_PATH] = {};
            const std::wstring key = i ? L"file" + std::to_wstring(i) : L"file";
            GetPrivateProfileStringW(section, key.c_str(), L"", file, MAX_PATH, config.c_str());
            if (!file[0]) return false;
            paths.push_back(directory + file);
            if (GetFileAttributesW(paths.back().c_str()) == INVALID_FILE_ATTRIBUTES) return false;
        }
        UINT duration = 0;
        for (int i = 0; i < count; ++i) {
            const int delay = read((L"delay" + std::to_wstring(i)).c_str());
            if (delay < 10 || delay > 600000 || duration > MAXUINT - static_cast<UINT>(delay)) return false;
            duration += static_cast<UINT>(delay);
            frameEnds.push_back(duration);
        }
        for (int i = first; i < first + stored; ++i) {
            wchar_t value[128] = {};
            const std::wstring key = L"rect" + std::to_wstring(i);
            GetPrivateProfileStringW(section, key.c_str(), L"", value, 128, config.c_str());
            Rect& bounds = regions[i].bounds;
            int x = -1, y = -1;
            if (swscanf_s(value, L"%d,%d,%d,%d,%d,%d", &bounds.X, &bounds.Y,
                &bounds.Width, &bounds.Height, &x, &y) != 6 || bounds.X < 0 || bounds.Y < 0 ||
                bounds.Width <= 0 || bounds.Height <= 0 || bounds.Width > w || bounds.Height > h ||
                x < 0 || y < 0 || x > w - bounds.Width || y > h - bounds.Height) return false;
            regions[i].offset = PointF(static_cast<float>(x), static_cast<float>(y));
        }
        return true;
    }

    std::unique_ptr<AnimationChunk> Decode(UINT index) const {
        auto chunk = std::make_unique<AnimationChunk>();
        const UINT first = ChunkFirst(index);
        const UINT count = std::min(chunkSize, firstFrame + storedFrames - first);
        if (!chunk->Load(paths[index], regions, first, count)) return nullptr;
        return chunk;
    }

    bool Preload(UINT entryFrame = UINT_MAX) {
        chunks[0] = Decode(0);
        if (!chunks[0]) return false;
        if (entryFrame != UINT_MAX) {
            if (entryFrame < firstFrame || entryFrame - firstFrame >= storedFrames) return false;
            pinnedChunk = ChunkIndex(entryFrame);
            if (pinnedChunk) {
                chunks[pinnedChunk] = Decode(pinnedChunk);
                if (!chunks[pinnedChunk]) return false;
            }
        }
        return true;
    }

    bool Needed(UINT index) const {
        return index == 0 || index == pinnedChunk || index == requestedChunk ||
            (active && (index == ChunkIndex(frame) || index == NextChunk()));
    }

    void Collect() {
        if (!loading.valid() || loading.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return;
        auto decoded = loading.get();
        if (Needed(loadingChunk)) chunks[loadingChunk] = std::move(decoded);
        loadingChunk = UINT_MAX;
    }

    void Request(UINT index) {
        if (chunks[index] || loading.valid()) return;
        loadingChunk = index;
        loading = std::async(std::launch::async, [this, index] { return Decode(index); });
    }

    void Trim() {
        for (UINT i = 1; i < chunks.size(); ++i) if (!Needed(i)) chunks[i].reset();
    }

    void SetActive(bool value) {
        active = value;
        if (!active) requestedChunk = UINT_MAX;
        Collect();
        Trim();
        if (active) Request(requestedChunk != UINT_MAX && !chunks[requestedChunk] ? requestedChunk : NextChunk());
    }

    bool Ready(UINT index) {
        if (index < firstFrame || index - firstFrame >= storedFrames) return false;
        requestedChunk = ChunkIndex(index);
        Collect();
        if (!chunks[requestedChunk]) {
            Request(requestedChunk);
            return false;
        }
        return true;
    }

    Status SelectFrame(UINT index) {
        if (!Ready(index)) return GenericError;
        frame = index;
        Trim();
        if (active) Request(NextChunk());
        return Ok;
    }
};
