#pragma once

constexpr int LanguageKorean = 110, LanguageJapanese = 111;

void LoadLanguageSettings(const std::wstring& path) {
    wchar_t language[16] = {};
    GetPrivateProfileStringW(L"clock", L"language", L"ko", language, 16, path.c_str());
    selectedLanguage = L"ko";
    for (const auto& pack : LanguagePacks) {
        if (wcscmp(language, pack.code) != 0) continue;
        selectedLanguage = pack.code;
        break;
    }
}

HMENU CreateLanguageMenu() {
    HMENU menu = CreatePopupMenu();
    int command = LanguageKorean;
    for (const auto& language : LanguagePacks) {
        AppendMenuW(menu, MF_STRING | (selectedLanguage == language.code ? MF_CHECKED : 0),
            command++, language.nativeName);
    }
    return menu;
}
