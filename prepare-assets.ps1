$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$sourcePath = Join-Path $PSScriptRoot 'source'
$dataPath = Join-Path $sourcePath 'locate data'
Add-Type -AssemblyName System.Drawing

function Assert-Point($point, $width, $height, $label) {
    if ($null -eq $point -or $null -eq $point.x -or $null -eq $point.y -or
        $point.x -isnot [ValueType] -or $point.y -isnot [ValueType] -or
        $point.x -ne [math]::Floor($point.x) -or $point.y -ne [math]::Floor($point.y) -or
        $point.x -lt 0 -or $point.y -lt 0 -or $point.x -ge $width -or $point.y -ge $height) {
        throw "Invalid or missing point: $label"
    }
}

function Find-Asset($name) {
    if ([IO.Path]::GetFileName($name) -ne $name) { throw "Asset name must not contain a path: $name" }
    $matches = @(Get-ChildItem -LiteralPath $sourcePath -Recurse -File | Where-Object {
        $_.DirectoryName -ne $dataPath -and $_.Name -ieq $name
    })
    if ($matches.Count -ne 1) { throw "Expected one asset named '$name', found $($matches.Count)." }
    return $matches[0]
}

$backgroundSourcePath = Join-Path $dataPath 'Skea-Idle_1.points.json'
$smoothedBackgroundPath = Join-Path $dataPath 'Skea-Idle_1.smoothed.points.json'
$backgroundSource = Get-Content -LiteralPath $backgroundSourcePath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($backgroundSource.frames.Count -ne $backgroundSource.source.frameCount -or $backgroundSource.frames.Count -lt 5) {
    throw 'Idle background points JSON has an invalid frame count.'
}
for ($i = 0; $i -lt $backgroundSource.frames.Count; $i++) {
    Assert-Point $backgroundSource.frames[$i] $backgroundSource.source.width $backgroundSource.source.height "idle frame $i"
}
$smoothedBackground = $backgroundSource | ConvertTo-Json -Depth 10 | ConvertFrom-Json
$weights = @(1, 4, 6, 4, 1)
$weightTotal = 16.0
for ($i = 0; $i -lt $backgroundSource.frames.Count; $i++) {
    $x = 0.0
    $y = 0.0
    for ($offset = -2; $offset -le 2; $offset++) {
        $frame = $backgroundSource.frames[($i + $offset + $backgroundSource.frames.Count) % $backgroundSource.frames.Count]
        $weight = $weights[$offset + 2]
        $x += $frame.x * $weight
        $y += $frame.y * $weight
    }
    $smoothedBackground.frames[$i].x = [math]::Round($x / $weightTotal, 4)
    $smoothedBackground.frames[$i].y = [math]::Round($y / $weightTotal, 4)
}
$smoothedBackground | Add-Member -NotePropertyName smoothing -NotePropertyValue ([pscustomobject][ordered]@{
    source = 'Skea-Idle_1.points.json'
    method = 'circular-weighted-average'
    weights = $weights
})
$smoothedBackground | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $smoothedBackgroundPath -Encoding UTF8

$documents = @(Get-ChildItem -LiteralPath $dataPath -File -Filter '*.json' | ForEach-Object {
    [pscustomobject]@{ File = $_; Data = Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 | ConvertFrom-Json }
})
$touchDocs = @($documents | Where-Object { $_.File.Name -ieq 'Skea-Idle_touch.json' })
$touchWarpDocs = @($documents | Where-Object { $_.File.Name -ieq 'Skea-Touch_Idle_head.points.json' })
$backgroundDocs = @($documents | Where-Object { $_.File.Name -ieq 'Skea-Idle_1.smoothed.points.json' })
$vectorDocs = @($documents | Where-Object { $_.Data.version -eq 1 -and $_.Data.type -eq 'skea-hand-vectors' })
if ($backgroundDocs.Count -ne 1 -or $touchDocs.Count -ne 1 -or $touchWarpDocs.Count -ne 1 -or $vectorDocs.Count -ne 1) {
    throw 'locate data must contain exactly one background points JSON, one touch JSON, one touch-head points JSON and one skea-hand-vectors JSON.'
}
$background = $backgroundDocs[0].Data
$touch = $touchDocs[0].Data
$touchWarp = $touchWarpDocs[0].Data
$vectors = $vectorDocs[0].Data

function Get-Alignment($targetName) {
    $matches = @($documents | Where-Object {
        $_.Data.version -eq 1 -and $_.Data.gifA.fileName -ieq $background.source.name -and
        $_.Data.gifB.fileName -ieq $targetName -and $null -ne $_.Data.gifB.topLeftRelativeToGifA
    })
    if ($matches.Count -ne 1) { throw "Expected one alignment JSON for '$targetName', found $($matches.Count)." }
    $alignment = $matches[0].Data
    $offset = $alignment.gifB.topLeftRelativeToGifA
    if ($alignment.coordinateSystem.origin -ne 'GIF A top-left' -or $alignment.coordinateSystem.unit -ne 'pixel' -or
        $alignment.coordinateSystem.positiveX -ne 'right' -or $alignment.coordinateSystem.positiveY -ne 'down' -or
        $alignment.gifA.width -ne $background.source.width -or $alignment.gifA.height -ne $background.source.height -or
        $alignment.gifA.topLeft.x -ne 0 -or $alignment.gifA.topLeft.y -ne 0 -or
        $offset.x -isnot [ValueType] -or $offset.y -isnot [ValueType] -or
        $offset.x -ne [math]::Floor($offset.x) -or $offset.y -ne [math]::Floor($offset.y)) {
        throw "Invalid alignment JSON for '$targetName'."
    }
    $asset = Find-Asset $targetName
    $image = [System.Drawing.Image]::FromFile($asset.FullName)
    try {
        if ($image.Width -ne $alignment.gifB.width -or $image.Height -ne $alignment.gifB.height) {
            throw "GIF dimensions do not match the alignment JSON: $targetName"
        }
    } finally { $image.Dispose() }
    return [pscustomobject]@{ Data = $alignment; Asset = $asset }
}

function Get-LayerAlignment($baseName, $targetName) {
    $matches = @($documents | Where-Object {
        $_.Data.version -eq 1 -and $_.Data.gifA.fileName -ieq $baseName -and
        $_.Data.gifB.fileName -ieq $targetName -and $null -ne $_.Data.gifB.topLeftRelativeToGifA
    })
    if ($matches.Count -ne 1) { throw "Expected one alignment JSON for '$baseName' and '$targetName', found $($matches.Count)." }
    $alignment = $matches[0].Data
    $offset = $alignment.gifB.topLeftRelativeToGifA
    if ($alignment.coordinateSystem.origin -ne 'GIF A top-left' -or $alignment.coordinateSystem.unit -ne 'pixel' -or
        $alignment.coordinateSystem.positiveX -ne 'right' -or $alignment.coordinateSystem.positiveY -ne 'down' -or
        $alignment.gifA.topLeft.x -ne 0 -or $alignment.gifA.topLeft.y -ne 0 -or
        $offset.x -isnot [ValueType] -or $offset.y -isnot [ValueType] -or
        $offset.x -ne [math]::Floor($offset.x) -or $offset.y -ne [math]::Floor($offset.y)) {
        throw "Invalid layer alignment JSON for '$targetName'."
    }
    $baseAsset = Find-Asset $baseName
    $targetAsset = Find-Asset $targetName
    $baseImage = [System.Drawing.Image]::FromFile($baseAsset.FullName)
    $targetImage = [System.Drawing.Image]::FromFile($targetAsset.FullName)
    try {
        if ($baseImage.Width -ne $alignment.gifA.width -or $baseImage.Height -ne $alignment.gifA.height -or
            $targetImage.Width -ne $alignment.gifB.width -or $targetImage.Height -ne $alignment.gifB.height) {
            throw "GIF dimensions do not match the layer alignment JSON: $targetName"
        }
    } finally {
        $baseImage.Dispose()
        $targetImage.Dispose()
    }
    return [pscustomobject]@{ Data = $alignment; BaseAsset = $baseAsset; TargetAsset = $targetAsset }
}

$smashAlignment = Get-Alignment 'Skea-Smash_End_1.gif'
$angryAlignment = Get-Alignment 'Skea-Angry_6.gif'
$panicAlignment = Get-Alignment 'Skea-Panic_3.gif'
$blankAlignment = Get-Alignment 'Skea-Blank_1.gif'
$touchHeadAlignment = Get-LayerAlignment 'Skea-Touch_Idle_back.gif' 'Skea-Touch_Idle_head.gif'
$touchFrontAlignment = Get-LayerAlignment 'Skea-Touch_Idle_back.gif' 'Skea-Touch_Idle_front.gif'
$touchEndHeadAlignment = Get-LayerAlignment 'Skea-Touch_End.gif' 'Skea-Touch_End_head.gif'
$touchEndFrontAlignment = Get-LayerAlignment 'Skea-Touch_End.gif' 'Skea-Touch_End_front_no_hair.gif'
$touchEndFrontAsset = Find-Asset 'Skea-Touch_front.gif'
$touchEndFrontImage = [System.Drawing.Image]::FromFile($touchEndFrontAsset.FullName)
try {
    if ($touchEndFrontImage.Width -ne $touchEndFrontAlignment.Data.gifB.width -or
        $touchEndFrontImage.Height -ne $touchEndFrontAlignment.Data.gifB.height) {
        throw 'Touch_End full front dimensions do not match the no-hair front alignment.'
    }
} finally { $touchEndFrontImage.Dispose() }
$touchEndBackAsset = Find-Asset 'Skea-Touch_End_back.gif'
$touchEndBackImage = [System.Drawing.Image]::FromFile($touchEndBackAsset.FullName)
try {
    if ($touchEndBackImage.Width -ne $touchEndHeadAlignment.Data.gifA.width -or
        $touchEndBackImage.Height -ne $touchEndHeadAlignment.Data.gifA.height) {
        throw 'Touch_End back dimensions do not match its alignment JSON.'
    }
    $touchEndBackWidth = $touchEndBackImage.Width
    $touchEndBackHeight = $touchEndBackImage.Height
} finally { $touchEndBackImage.Dispose() }
$angryAltAsset = Find-Asset 'Skea-Angry_6_2.gif'
$angryAltImage = [System.Drawing.Image]::FromFile($angryAltAsset.FullName)
try {
    $angryAltWidth = $angryAltImage.Width
    $angryAltHeight = $angryAltImage.Height
} finally { $angryAltImage.Dispose() }

if ($background.coordinateSystem.origin -ne 'top-left' -or $background.coordinateSystem.unit -ne 'pixel' -or
    $background.coordinateSystem.frameIndexBase -ne 0) { throw 'Invalid GIF coordinate format.' }
$gifFile = Find-Asset $background.source.name
$gif = [System.Drawing.Image]::FromFile($gifFile.FullName)
try {
    $frameCount = $gif.GetFrameCount([System.Drawing.Imaging.FrameDimension]::Time)
    if ($gif.Width -ne $background.source.width -or $gif.Height -ne $background.source.height -or
        $frameCount -ne $background.source.frameCount -or $background.frames.Count -ne $frameCount) {
        throw 'GIF dimensions or frame count do not match the coordinate JSON.'
    }
    $delayBytes = $gif.GetPropertyItem(0x5100).Value
    $lines = @('; Generated from source\locate data JSON files. Do not edit this file.', '[background]',
        "file=$($gifFile.FullName.Substring($sourcePath.Length + 1))", "width=$($gif.Width)",
        "height=$($gif.Height)", "frameCount=$frameCount")
    for ($i = 0; $i -lt $frameCount; $i++) {
        $frame = $background.frames[$i]
        if ($frame.index -ne $i) { throw "GIF frame index mismatch at $i." }
        if ($frame.x -isnot [ValueType] -or $frame.y -isnot [ValueType] -or
            [double]::IsNaN([double]$frame.x) -or [double]::IsNaN([double]$frame.y) -or
            [double]::IsInfinity([double]$frame.x) -or [double]::IsInfinity([double]$frame.y) -or
            $frame.x -lt 0 -or $frame.y -lt 0 -or $frame.x -ge $gif.Width -or $frame.y -ge $gif.Height) {
            throw "Invalid smoothed point: GIF frame $i"
        }
        $actualDuration = [BitConverter]::ToUInt32($delayBytes, $i * 4) * 10
        if ($frame.durationMs -ne $actualDuration) { throw "GIF frame duration mismatch at $i." }
        $x = [Convert]::ToString([double]$frame.x, [Globalization.CultureInfo]::InvariantCulture)
        $y = [Convert]::ToString([double]$frame.y, [Globalization.CultureInfo]::InvariantCulture)
        $lines += "frame$i=$x,$y"
    }
} finally { $gif.Dispose() }

if ($touch.coordinateSystem.origin -ne 'top-left' -or $touch.coordinateSystem.unit -ne 'pixel' -or
    $touch.coordinateSystem.frameIndexBase -ne 0) { throw 'Invalid touch coordinate format.' }
$touchFile = Find-Asset $touch.source.name
$touchGif = [System.Drawing.Image]::FromFile($touchFile.FullName)
try {
    $touchFrameCount = $touchGif.GetFrameCount([System.Drawing.Imaging.FrameDimension]::Time)
    if ($touchGif.Width -ne $touch.source.width -or $touchGif.Height -ne $touch.source.height -or
        $touchFrameCount -ne $touch.source.frameCount -or $touch.frames.Count -ne $touchFrameCount) {
        throw 'Touch GIF dimensions or frame count do not match the coordinate JSON.'
    }
    $touchDelayBytes = $touchGif.GetPropertyItem(0x5100).Value
    for ($i = 0; $i -lt $touchFrameCount; $i++) {
        if ($touch.frames[$i].index -ne $i -or
            $touch.frames[$i].durationMs -ne [BitConverter]::ToUInt32($touchDelayBytes, $i * 4) * 10) {
            throw "Touch GIF frame mismatch at $i."
        }
    }
} finally { $touchGif.Dispose() }
$markedTouchFrames = @($touch.frames | Where-Object { $null -ne $_.x -and $null -ne $_.y })
if ($markedTouchFrames.Count -ne 1) { throw 'Touch JSON must contain exactly one marked point.' }
$touchPoint = $markedTouchFrames[0]
Assert-Point $touchPoint $touch.source.width $touch.source.height 'cheek touch'
$lines += @('', '[touch]', "file=$($touchFile.FullName.Substring($sourcePath.Length + 1))",
    "width=$($touch.source.width)", "height=$($touch.source.height)",
    "frame=$($touchPoint.index)", "x=$($touchPoint.x)", "y=$($touchPoint.y)")

if ($touchWarp.coordinateSystem.origin -ne 'top-left' -or $touchWarp.coordinateSystem.unit -ne 'pixel' -or
    $touchWarp.coordinateSystem.frameIndexBase -ne 0 -or
    $touchWarp.source.name -ine $touchHeadAlignment.TargetAsset.Name) { throw 'Invalid touch-head coordinate format.' }
$touchHeadGif = [System.Drawing.Image]::FromFile($touchHeadAlignment.TargetAsset.FullName)
try {
    $touchWarpFrameCount = $touchHeadGif.GetFrameCount([System.Drawing.Imaging.FrameDimension]::Time)
    if ($touchHeadGif.Width -ne $touchWarp.source.width -or $touchHeadGif.Height -ne $touchWarp.source.height -or
        $touchWarpFrameCount -ne $touchWarp.source.frameCount -or $touchWarp.frames.Count -ne $touchWarpFrameCount) {
        throw 'Touch-head GIF dimensions or frame count do not match its points JSON.'
    }
    $touchHeadDelayBytes = $touchHeadGif.GetPropertyItem(0x5100).Value
    for ($i = 0; $i -lt $touchWarpFrameCount; $i++) {
        $frame = $touchWarp.frames[$i]
        if ($frame.index -ne $i -or
            $frame.durationMs -ne [BitConverter]::ToUInt32($touchHeadDelayBytes, $i * 4) * 10) {
            throw "Touch-head GIF frame mismatch at $i."
        }
        Assert-Point $frame $touchHeadGif.Width $touchHeadGif.Height "touch-head frame $i"
    }
} finally { $touchHeadGif.Dispose() }

$touchBackImage = [System.Drawing.Image]::FromFile($touchHeadAlignment.BaseAsset.FullName)
try {
    $lines += @('', '[touch_back]', "file=$($touchHeadAlignment.BaseAsset.FullName.Substring($sourcePath.Length + 1))",
        "width=$($touchBackImage.Width)", "height=$($touchBackImage.Height)")
} finally { $touchBackImage.Dispose() }
foreach ($item in @(
    [pscustomobject]@{ Section = 'touch_head'; Alignment = $touchHeadAlignment }
    [pscustomobject]@{ Section = 'touch_front'; Alignment = $touchFrontAlignment }
)) {
    $alignment = $item.Alignment
    $offset = $alignment.Data.gifB.topLeftRelativeToGifA
    $lines += @('', "[$($item.Section)]", "file=$($alignment.TargetAsset.FullName.Substring($sourcePath.Length + 1))",
        "width=$($alignment.Data.gifB.width)", "height=$($alignment.Data.gifB.height)",
        "offsetX=$($offset.x)", "offsetY=$($offset.y)")
    if ($item.Section -eq 'touch_head') {
        $lines += @("frameCount=$touchWarpFrameCount", 'radius=16')
        for ($i = 0; $i -lt $touchWarpFrameCount; $i++) {
            $lines += "frame$i=$($touchWarp.frames[$i].x),$($touchWarp.frames[$i].y)"
        }
    }
}
$lines += @('', '[touch_end]', "file=$($touchEndHeadAlignment.BaseAsset.FullName.Substring($sourcePath.Length + 1))",
    "width=$touchEndBackWidth", "height=$touchEndBackHeight")
$lines += @('', '[touch_end_back]', "file=$($touchEndBackAsset.FullName.Substring($sourcePath.Length + 1))",
    "width=$touchEndBackWidth", "height=$touchEndBackHeight")
$touchEndHeadOffset = $touchEndHeadAlignment.Data.gifB.topLeftRelativeToGifA
$lines += @('', '[touch_end_head]',
    "file=$($touchEndHeadAlignment.TargetAsset.FullName.Substring($sourcePath.Length + 1))",
    "width=$($touchEndHeadAlignment.Data.gifB.width)", "height=$($touchEndHeadAlignment.Data.gifB.height)",
    "offsetX=$($touchEndHeadOffset.x)", "offsetY=$($touchEndHeadOffset.y)")
$touchEndFrontOffset = $touchEndFrontAlignment.Data.gifB.topLeftRelativeToGifA
$lines += @('', '[touch_end_front]', "file=$($touchEndFrontAsset.FullName.Substring($sourcePath.Length + 1))",
    "width=$($touchEndFrontAlignment.Data.gifB.width)", "height=$($touchEndFrontAlignment.Data.gifB.height)",
    "offsetX=$($touchEndFrontOffset.x)", "offsetY=$($touchEndFrontOffset.y)")

foreach ($item in @(
    [pscustomobject]@{ Section = 'smash'; Alignment = $smashAlignment }
    [pscustomobject]@{ Section = 'angry'; Alignment = $angryAlignment }
    [pscustomobject]@{ Section = 'panic'; Alignment = $panicAlignment }
    [pscustomobject]@{ Section = 'blank'; Alignment = $blankAlignment }
)) {
    $section = $item.Section
    $alignment = $item.Alignment
    $offset = $alignment.Data.gifB.topLeftRelativeToGifA
    $lines += @('', "[$section]", "file=$($alignment.Asset.FullName.Substring($sourcePath.Length + 1))",
        "width=$($alignment.Data.gifB.width)", "height=$($alignment.Data.gifB.height)",
        "offsetX=$($offset.x)", "offsetY=$($offset.y)")
}
$angryOffset = $angryAlignment.Data.gifB.topLeftRelativeToGifA
$lines += @('', '[angry_alt]', "file=$($angryAltAsset.FullName.Substring($sourcePath.Length + 1))",
    "width=$angryAltWidth", "height=$angryAltHeight", "offsetX=$($angryOffset.x)", "offsetY=$($angryOffset.y)")

if ($vectors.coordinateSystem.origin -ne 'top-left' -or $vectors.coordinateSystem.unit -ne 'pixel' -or
    $vectors.coordinateSystem.pointLocation -ne 'pixel-center' -or $vectors.coordinateSystem.xDirection -ne 'right' -or
    $vectors.coordinateSystem.yDirection -ne 'down') { throw 'Invalid hand coordinate format.' }
foreach ($role in @('hour', 'minute')) {
    $matches = @($vectors.hands | Where-Object { $_.role -eq $role })
    if ($matches.Count -ne 1) { throw "Expected one hand with role '$role'." }
    $hand = $matches[0]
    $imageFile = Find-Asset $hand.image.name
    $image = [System.Drawing.Image]::FromFile($imageFile.FullName)
    try {
        if ($image.Width -ne $hand.image.width -or $image.Height -ne $hand.image.height) { throw "Image dimensions do not match: $role" }
        Assert-Point $hand.center $image.Width $image.Height "$role center"
        Assert-Point $hand.tip $image.Width $image.Height "$role tip"
        if ($hand.center.x -eq $hand.tip.x -and $hand.center.y -eq $hand.tip.y) { throw "Zero-length hand vector: $role" }
        $lines += @('', "[$role]", "file=$($imageFile.FullName.Substring($sourcePath.Length + 1))",
            "width=$($image.Width)", "height=$($image.Height)", "centerX=$($hand.center.x)",
            "centerY=$($hand.center.y)", "tipX=$($hand.tip.x)", "tipY=$($hand.tip.y)")
    } finally { $image.Dispose() }
}
$configPath = Join-Path $dataPath 'clock-layout.ini'
$lines | Set-Content -LiteralPath $configPath -Encoding Unicode
Write-Output "Prepared assets from $($backgroundDocs[0].File.Name) and $($vectorDocs[0].File.Name)."
