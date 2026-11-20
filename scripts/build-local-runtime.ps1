[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$lock=Get-Content (Join-Path $PSScriptRoot 'native-dependencies.lock.json') -Raw | ConvertFrom-Json
$asset=$lock.assets | Where-Object name -eq 'llama-b12284-source.zip'
if(!$asset){throw 'Pinned llama source is missing from dependency lock.'}
$cache=Join-Path $root '.gpaa\dependencies'
if($env:GPAA_NATIVE_DEPENDENCY_CACHE){$cache=$env:GPAA_NATIVE_DEPENDENCY_CACHE}
$archive=Join-Path $cache $asset.name
if((Get-FileHash -LiteralPath $archive).Hash -ine $asset.sha256){throw 'llama source integrity failed.'}
$output=Join-Path $root 'plugin\Source\ThirdParty\Llama'
$manifestPath=Join-Path $output 'build-manifest.json'
$flags=@('-DBUILD_SHARED_LIBS=OFF','-DGGML_NATIVE=OFF','-DGGML_AVX=ON','-DGGML_AVX2=ON','-DGGML_BMI2=OFF',
 '-DGGML_OPENMP=OFF','-DGGML_BACKEND_DL=OFF','-DLLAMA_BUILD_COMMON=OFF','-DLLAMA_BUILD_TESTS=OFF',
 '-DLLAMA_BUILD_TOOLS=OFF','-DLLAMA_BUILD_EXAMPLES=OFF','-DLLAMA_BUILD_SERVER=OFF','-DLLAMA_BUILD_APP=OFF',
 '-DLLAMA_OPENSSL=OFF','-DLLAMA_SUBPROCESS=OFF','-DGGML_CCACHE=OFF')
$identity=$asset.sha256+'|'+($flags -join '|')
if(Test-Path -LiteralPath $manifestPath) {
 $old=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
 $valid=$old.identity -eq $identity
 foreach($file in $old.files){$path=Join-Path $output $file.path;if(!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ine $file.sha256){$valid=$false}}
 if($valid){Write-Host 'Pinned local inference static libraries verified.';return}
}
$buildRoot=Join-Path $root '.gpaa\local-runtime-build'
$source=Join-Path $buildRoot 'llama.cpp-b12284'
if(!(Test-Path (Join-Path $source 'CMakeLists.txt'))){Expand-Archive -LiteralPath $archive -DestinationPath $buildRoot -Force}
$build=Join-Path $buildRoot 'build'
$cmake=Get-Command cmake -ErrorAction SilentlyContinue
$cmakePath=if($cmake){$cmake.Source}else{'C:\Program Files\CMake\bin\cmake.exe'}
if(!(Test-Path -LiteralPath $cmakePath)){throw 'CMake is required on the build machine (not on user machines).'}
& $cmakePath -S $source -B $build -G 'Visual Studio 17 2022' -A x64 @flags
if($LASTEXITCODE -ne 0){throw 'Could not configure local inference runtime.'}
& $cmakePath --build $build --config Release --target llama --parallel 10
if($LASTEXITCODE -ne 0){throw 'Could not build local inference runtime.'}
New-Item -ItemType Directory -Force -Path (Join-Path $output 'Win64') | Out-Null
$files=@()
foreach($name in @('llama','ggml','ggml-base','ggml-cpu')) {
 $found=@(Get-ChildItem -LiteralPath $build -Recurse -File -Filter "$name.lib" | Where-Object FullName -match '[\\/]Release[\\/]')
 if($found.Count -ne 1){throw "Expected exactly one $name static library."}
 $relative="Win64/$name.lib"
 Copy-Item -LiteralPath $found[0].FullName -Destination (Join-Path $output $relative) -Force
 $files+=@{path=$relative;sha256=(Get-FileHash -LiteralPath (Join-Path $output $relative)).Hash}
}
@{identity=$identity;sourceSha256=$asset.sha256;tag='b12284';cpu='AVX2';flags=$flags;files=$files} |
 ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Write-Host 'Local inference runtime built without helper executables, OpenMP or shared ggml libraries.'
