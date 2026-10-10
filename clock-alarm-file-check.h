#pragma once

std::uint32_t AlarmBigEndian32(const BYTE* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
        (static_cast<std::uint32_t>(bytes[1]) << 16) |
        (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

bool AlarmFileIsTruncated(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false; // The decoder reports open errors.
    LARGE_INTEGER size = {};
    BYTE header[64] = {};
    DWORD read = 0;
    bool truncated = false;
    if (GetFileSizeEx(file, &size) && ReadFile(file, header, sizeof(header), &read, nullptr) && read >= 12) {
        const ULONGLONG length = static_cast<ULONGLONG>(size.QuadPart);
        if (!std::memcmp(header, "RIFF", 4) && !std::memcmp(header + 8, "WAVE", 4)) {
            // WAV decoders may accept EOF even when a declared data chunk was cut short.
            const ULONGLONG end = static_cast<ULONGLONG>(WaveValue32(header + 4)) + 8;
            truncated = end > length;
            for (ULONGLONG offset = 12; !truncated && offset + 8 <= std::min(end, length);) {
                LARGE_INTEGER position; position.QuadPart = static_cast<LONGLONG>(offset);
                if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN) ||
                    !ReadFile(file, header, 8, &read, nullptr) || read != 8) break;
                const ULONGLONG bytes = WaveValue32(header + 4);
                truncated = bytes > length - offset - 8;
                offset += 8 + bytes + (bytes & 1);
            }
        } else {
            ULONGLONG offset = 0;
            if (!std::memcmp(header, "ID3", 3) && !(header[6] & 128) && !(header[7] & 128) &&
                !(header[8] & 128) && !(header[9] & 128)) {
                offset = 10ULL + (header[6] << 21) + (header[7] << 14) + (header[8] << 7) + header[9];
                if (header[3] == 4 && (header[5] & 16)) offset += 10; // ID3v2.4 footer.
                LARGE_INTEGER position; position.QuadPart = static_cast<LONGLONG>(offset);
                if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN) ||
                    !ReadFile(file, header, sizeof(header), &read, nullptr)) read = 0;
            }
            if (read >= 64 && offset < length && header[0] == 255 && (header[1] & 224) == 224 &&
                (header[1] & 6) == 2 && (header[1] & 24) != 8) {
                // Read the first MPEG Layer III frame's Xing/Info or VBRI byte count.
                const bool mpeg1 = (header[1] & 24) == 24, mono = (header[3] & 192) == 192;
                const size_t tag = 4 + (mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17));
                std::uint32_t declared = 0;
                if (!std::memcmp(header + tag, "Xing", 4) || !std::memcmp(header + tag, "Info", 4)) {
                    const auto flags = AlarmBigEndian32(header + tag + 4);
                    if (flags & 2) declared = AlarmBigEndian32(header + tag + 8 + ((flags & 1) ? 4 : 0));
                } else if (!std::memcmp(header + 36, "VBRI", 4) && header[40] == 0 && header[41] == 1) {
                    declared = AlarmBigEndian32(header + 46);
                }
                truncated = declared > length - offset;
            }
        }
    }
    CloseHandle(file);
    return truncated;
}
