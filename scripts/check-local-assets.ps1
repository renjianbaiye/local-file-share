param(
    [string]$SourceRoot,
    [switch]$Copy
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

$assets = @(
    @{
        Path = "models/dinov2_album_tagger_v3/dinov2_album_tagger_v3.onnx"
        Label = "Primary DINOv2 album tagger model"
        Bytes = 1219938612
        Required = $true
    },
    @{
        Path = "models/smart_album_tags_v2/smart_album_tags_v2.onnx"
        Label = "Legacy smart album tagger model"
        Bytes = 198039026
        Required = $false
    },
    @{
        Path = "third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/include/onnxruntime_cxx_api.h"
        Label = "ONNX Runtime C++ header"
        Required = $true
    },
    @{
        Path = "third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/lib/onnxruntime.lib"
        Label = "ONNX Runtime import library"
        Required = $true
    },
    @{
        Path = "third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/lib/onnxruntime.dll"
        Label = "ONNX Runtime DLL"
        Required = $true
    },
    @{
        Path = "third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/lib/onnxruntime_providers_cuda.dll"
        Label = "ONNX Runtime CUDA provider DLL"
        Required = $true
    },
    @{
        Path = "third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/lib/onnxruntime_providers_shared.dll"
        Label = "ONNX Runtime shared provider DLL"
        Required = $true
    }
)

if ($Copy -and [string]::IsNullOrWhiteSpace($SourceRoot)) {
    throw "Use -SourceRoot when passing -Copy."
}

$resolvedSourceRoot = $null
if (-not [string]::IsNullOrWhiteSpace($SourceRoot)) {
    $resolvedSourceRoot = Resolve-Path $SourceRoot
}

$missingRequired = 0

foreach ($asset in $assets) {
    $target = Join-Path $repoRoot $asset.Path

    if (-not (Test-Path -LiteralPath $target)) {
        if ($Copy) {
            $source = Join-Path $resolvedSourceRoot $asset.Path
            if (Test-Path -LiteralPath $source) {
                $targetDir = Split-Path -Parent $target
                New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
                Copy-Item -LiteralPath $source -Destination $target -Force
            }
        }
    }

    if (Test-Path -LiteralPath $target) {
        $file = Get-Item -LiteralPath $target
        $sizeNote = ""
        if ($asset.ContainsKey("Bytes") -and $file.Length -ne $asset.Bytes) {
            $sizeNote = " size differs: expected $($asset.Bytes), found $($file.Length)"
        }
        Write-Host "[ok]      $($asset.Path)$sizeNote"
    } else {
        $kind = if ($asset.Required) { "missing" } else { "optional" }
        Write-Host "[$kind] $($asset.Path) - $($asset.Label)"
        if ($asset.Required) {
            $missingRequired++
        }
    }
}

if ($missingRequired -gt 0) {
    Write-Host ""
    Write-Host "Missing $missingRequired required local asset(s)."
    Write-Host "If you have a copy from another machine, run:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File scripts/check-local-assets.ps1 -SourceRoot <path-to-backed-up-repo-or-assets> -Copy"
    exit 1
}

Write-Host ""
Write-Host "All required local assets are present."
