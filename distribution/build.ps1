$ErrorActionPreference = 'Stop'
$productVersion = '1.0.8'
$root = Split-Path $PSScriptRoot -Parent
$work = Join-Path $PSScriptRoot '.build'
$output = Join-Path $PSScriptRoot 'SkeaClock.exe'
# shortcut: deleting the previous EXE resets the revision; use a state file if numbering must survive deletion.
$revision = 0
if (Test-Path -LiteralPath $output -PathType Leaf) {
    $previousVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($output)
    if ($previousVersion.ProductVersion -eq $productVersion) {
        $revision = $previousVersion.FilePrivatePart + 1
    }
}
$fileVersion = "$productVersion.$revision"
$utf8 = New-Object Text.UTF8Encoding($true)
function Write-Utf8($path, $text) { [IO.File]::WriteAllText($path, $text, $utf8) }
function Replace-Once($text, $old, $new) {
    if ($text.IndexOf($old) -lt 0 -or $text.IndexOf($old) -ne $text.LastIndexOf($old)) {
        throw 'Source changed: review the standalone adapter before building.'
    }
    return $text.Replace($old, $new)
}
function Clear-Work {
    if (Test-Path -LiteralPath $work) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        if ($resolved -ne ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '.build')))) {
            throw 'Unexpected build directory.'
        }
        if ((Get-Item -LiteralPath $work).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'Build directory must not be a link.'
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
Clear-Work
New-Item -ItemType Directory -Path $work | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'skea-clock.cpp'), (Join-Path $root 'prepare-assets.ps1'), (Join-Path $root 'prepare-fxaa.py'), (Join-Path $root 'animation_chunks.py') -Destination $work
Get-ChildItem -LiteralPath $root -Filter '*.h' -File | Copy-Item -Destination $work
Copy-Item -LiteralPath (Join-Path $root 'languages') -Destination $work -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'standalone.h') -Destination $work

# Prepare coordinates in an isolated copy; the original source tree is read-only.
$source = Join-Path $root 'source'
Get-ChildItem -LiteralPath $source -Recurse -File | Where-Object {
    $_.Extension -in '.gif', '.png', '.json', '.ico'
} | ForEach-Object {
    $relative = $_.FullName.Substring($source.Length + 1)
    $target = Join-Path (Join-Path $work 'source') $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $target
}
& (Join-Path $work 'prepare-assets.ps1')
$layout = Join-Path $work 'source\locate data\clock-layout.ini'
$paths = @('locate data\clock-layout.ini') + @(Get-Content -LiteralPath $layout -Encoding Unicode | Where-Object {
    $_ -match '^file\d*='
} | ForEach-Object { $_.Substring($_.IndexOf('=') + 1) })
$sounds = @('idle1', 'idle2', 'smash', 'touch_start', 'touch_end', 'touch1', 'touch2', 'touch3',
    'pat_start', 'pat_end', 'pat1', 'pat2', 'pat3', 'blank1', 'answer', 'smile', 'alert', 'base_alert')
$paths += @($sounds | ForEach-Object { "sound\$_.wav" })
$rc = @('#pragma code_page(65001)') + @(Get-Content -LiteralPath (Join-Path $root 'skea-clock.rc') -Encoding UTF8)
$table = @('struct EmbeddedAsset { int id; const wchar_t* path; };', 'constexpr EmbeddedAsset EmbeddedAssets[] = {')
$id = 100
foreach ($relative in ($paths | Select-Object -Unique)) {
    if ([IO.Path]::IsPathRooted($relative) -or $relative.Split('\') -contains '..') { throw "Invalid asset path: $relative" }
    $input = if ($relative.StartsWith('sound\')) { Join-Path $source $relative } else { Join-Path $work "source\$relative" }
    if (!(Test-Path -LiteralPath $input -PathType Leaf)) { throw "Missing asset: $input" }
    $rc += "$id RCDATA `"$($input.Replace('\', '\\'))`""
    $table += "    { $id, L`"$($relative.Replace('\', '\\'))`" },"
    $id++
}
$table += '};'
Write-Utf8 (Join-Path $work 'assets.rc') ($rc -join "`r`n")
Write-Utf8 (Join-Path $work 'assets.h') ($table -join "`r`n")
$cpp = [IO.File]::ReadAllText((Join-Path $work 'skea-clock.cpp')).Replace("`r`n", "`n")
$cpp = Replace-Once $cpp '#include "clock-artwork.h"' "#include `"standalone.h`"`n#include `"clock-artwork.h`""
$old = @'
    wchar_t executable[MAX_PATH];
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    settingsPath = executable;
    settingsPath = settingsPath.substr(0, settingsPath.find_last_of(L"\\/") + 1) + L"skea-clock.ini";
'@
$new = @'
    std::wstring sourceDirectory;
    if (!PrepareStandalone(settingsPath, sourceDirectory)) {
        LoadLanguageSettings(settingsPath);
        MessageBoxW(nullptr, UiText().standaloneDataError, L"Skea clock", MB_OK | MB_ICONERROR);
        CloseHandle(mutex);
        return 1;
    }
'@
$cpp = Replace-Once $cpp ($old.Replace("`r`n", "`n")) ($new.Replace("`r`n", "`n"))
$old = @'
    std::wstring directory = settingsPath.substr(0, settingsPath.find_last_of(L"\\/") + 1);
    std::wstring sourceDirectory = directory + L"source\\";
    if (GetFileAttributesW((sourceDirectory + L"locate data\\clock-layout.ini").c_str()) == INVALID_FILE_ATTRIBUTES)
        sourceDirectory = directory + L"..\\source\\";
'@
$cpp = Replace-Once $cpp ($old.Replace("`r`n", "`n")) ''
$cpp = Replace-Once $cpp 'UiText().artworkLoadHelp' 'UiText().standaloneArtworkLoadHelp'
Write-Utf8 (Join-Path $work 'skea-clock.cpp') $cpp
Push-Location -LiteralPath $work
try {
    & rc.exe /nologo /fo assets.res assets.rc
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
    & cl.exe /nologo /std:c++17 /W4 /WX /O2 /MT /EHsc /utf-8 /D_WIN32_WINNT=0x0A00 skea-clock.cpp assets.res /Fo:clock.obj /Fe:SkeaClock.exe /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED
    if ($LASTEXITCODE -ne 0) { throw 'C++ compilation failed.' }
    & (Join-Path $PSScriptRoot 'update-metadata.ps1') -ExePath (Join-Path $work 'SkeaClock.exe') -ProductVersion $productVersion -FileVersion $fileVersion
    Copy-Item -LiteralPath (Join-Path $work 'SkeaClock.exe') -Destination $output -Force
} finally { Pop-Location }
Set-Location -LiteralPath $PSScriptRoot
Clear-Work
Get-Item -LiteralPath $output | Select-Object Name, Length
