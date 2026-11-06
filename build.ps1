# Builds Sonic Heroes: Modern Video and High FPS as one Reloaded-II mod.
# VideoSettings is the entry DLL; it loads the bundled FPS DLL (FrameInterpolation).
# -Test also builds and runs the isolated checks for both parts.
param([switch]$Test)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$video = Join-Path $root 'VideoSettings'
$fps = Join-Path $root 'FrameInterpolation'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio was not found.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC x86 build tools were not found.' }
$vars = & $env:ComSpec /d /c "call `"$(Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat')`" x86 >nul && set"
foreach ($line in $vars) { if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') } }
$cl = Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x86\cl.exe'

$manifest = Get-Content -LiteralPath (Join-Path $root 'ModConfig.json') -Raw | ConvertFrom-Json
$dist = Join-Path $root "dist\$($manifest.ModId)"
$build = Join-Path $root 'build'
if (Test-Path -LiteralPath $dist) { Remove-Item -LiteralPath $dist -Recurse -Force }
New-Item -ItemType Directory -Force $dist, $build, (Join-Path $dist 'assets') | Out-Null

$flags = '/nologo', '/std:c++17', '/O2', '/MT', '/W4', '/WX', '/EHsc'
& $cl @flags /LD /DMODERN_VIDEO "$video\src\mod.cpp" "/Fo:$build\video.obj" /link "/DEF:$video\src\mod.def" "/OUT:$dist\$($manifest.ModNativeDll32)" "/IMPLIB:$build\ModernVideo.lib" /MACHINE:X86 user32.lib gdi32.lib bcrypt.lib gdiplus.lib
if ($LASTEXITCODE) { throw 'Video build failed' }
& $cl @flags /LD /DMODERN_VIDEO "$fps\src\mod.cpp" "/Fo:$build\fps.obj" /link "/DEF:$fps\src\mod.def" "/OUT:$dist\ModernVideo.Fps.dll" "/IMPLIB:$build\ModernVideo.Fps.lib" /MACHINE:X86 user32.lib bcrypt.lib winmm.lib
if ($LASTEXITCODE) { throw 'FPS build failed' }
Remove-Item -Path "$dist\*.exp", "$dist\*.lib" -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $root 'ModConfig.json') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $video 'assets\video-title.png') -Destination (Join-Path $dist 'assets') -Force

if ($Test) {
    # Each suite runs from its own folder: it writes to build\ there, and the
    # Video suite reads menu art extracted from your own copy of the game
    # (VideoSettings\research\assets-*), which is not part of this repository.
    $failed = @()
    foreach ($part in @(@{ Dir = $video; Libs = 'user32.lib', 'gdi32.lib', 'bcrypt.lib', 'gdiplus.lib' }, @{ Dir = $fps; Libs = 'user32.lib', 'bcrypt.lib', 'winmm.lib' })) {
        Push-Location $part.Dir
        try {
            New-Item -ItemType Directory -Force 'build' | Out-Null
            & $cl @flags 'tests\verify.cpp' '/Fo:build\verify.obj' /link '/OUT:build\verify.exe' /MACHINE:X86 @($part.Libs)
            if ($LASTEXITCODE) { throw "Test build failed in $($part.Dir)" }
            & '.\build\verify.exe'
            if ($LASTEXITCODE) { $failed += Split-Path $part.Dir -Leaf }
        } finally { Pop-Location }
    }
    if ($failed) { throw "Checks failed: $($failed -join ', ')" }
}
Get-ChildItem -Recurse -File $dist | ForEach-Object { '{0}  {1}' -f $_.FullName.Substring($dist.Length + 1), $_.Length }
