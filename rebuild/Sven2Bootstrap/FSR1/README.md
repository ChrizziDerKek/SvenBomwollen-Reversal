# AMD FidelityFX Super Resolution 1

`ffx_a.h` and `ffx_fsr1.h` are unmodified upstream files from
[GPUOpen-Effects/FidelityFX-FSR](https://github.com/GPUOpen-Effects/FidelityFX-FSR),
pinned to commit `a21ffb8f6c13233ba336352bdff293894c706575`.
Their MIT license/copyright notices are retained in the files.
[SOURCE.json](SOURCE.json) records SHA-256 checksums and provenance.

The rebuild uses FP32 HLSL EASU and RCAS on D3D11 Shader Model 5.0.
Integration glue is `Sven2Bootstrap/graphics/Fsr1.hlsl`. The algorithms run in
separate pixel passes; the EASU intermediate is RGBA16F. Source/output images
use display-encoded UNORM color without an extra sRGB transfer.
Shader files are copied next to each executable by `FsrShaders.targets`.
