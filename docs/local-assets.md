# Local assets

Large runtime assets are intentionally not committed to Git:

- `models/**/*.onnx`
- `third_party/onnxruntime/`

The repository tracks the model metadata JSON files, but not the ONNX model weights.

## Required paths

Place the primary model here:

```text
models/dinov2_album_tagger_v3/dinov2_album_tagger_v3.onnx
```

Expected local size on this machine:

```text
1219938612 bytes
```

The older model is optional:

```text
models/smart_album_tags_v2/smart_album_tags_v2.onnx
```

Expected local size on this machine:

```text
198039026 bytes
```

For ONNX acceleration on Windows, place ONNX Runtime here:

```text
third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/
```

The CMake build enables ONNX support only when these files exist:

```text
third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/include/onnxruntime_cxx_api.h
third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/lib/onnxruntime.lib
```

The build copies these DLLs next to `LocalFileShare.exe`:

```text
third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/lib/onnxruntime.dll
third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/lib/onnxruntime_providers_cuda.dll
third_party/onnxruntime/onnxruntime-win-x64-gpu-1.23.2/lib/onnxruntime_providers_shared.dll
```

## Check another machine

After cloning the repo on another computer, run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/check-local-assets.ps1
```

If you copied the missing assets to a USB drive or another local folder with the same relative layout, run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/check-local-assets.ps1 -SourceRoot D:\local-file-share-assets -Copy
```

The app can still build without ONNX Runtime, but photo tagging will fall back to the null tagger unless an alternative Python tagger is configured.
