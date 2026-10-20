param(
    [string]$QtRoot = "$PSScriptRoot/../.deps/Qt/6.5.3/mingw_64",
    [string]$CompilerRoot = "$PSScriptRoot/../.deps/Qt/Tools/mingw1310_64",
    [string]$BoostRoot = "$PSScriptRoot/../.deps/boost_1_84_0",
    [string]$OpenSSLRoot = "$PSScriptRoot/../.deps/Qt/Tools/OpenSSLv3/Win_x64",
    [int]$Jobs = 4
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot/..").Path
$cmake = "$root/.build-tools/cmake/data/bin/cmake.exe"
$ninja = "$root/.build-tools/bin/ninja.exe"
foreach ($path in @($QtRoot, $CompilerRoot, $BoostRoot, $OpenSSLRoot, $cmake, $ninja)) {
    if (!(Test-Path -LiteralPath $path)) { throw "Missing build dependency: $path" }
}
$QtRoot = (Resolve-Path $QtRoot).Path.Replace('\', '/')
$BoostRoot = (Resolve-Path $BoostRoot).Path.Replace('\', '/')
$OpenSSLRoot = (Resolve-Path $OpenSSLRoot).Path.Replace('\', '/')
$env:PATH = "$CompilerRoot/bin;$QtRoot/bin;$env:PATH"

function Invoke-CMake {
    & $cmake @args
    if ($LASTEXITCODE -ne 0) { throw "CMake failed with exit code $LASTEXITCODE" }
}

Invoke-CMake -S "$root/3rd/libtorrent-rasterbar" -B "$root/build/torrent" -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_INSTALL_PREFIX=$root/.deps/torrent" "-DBOOST_ROOT=$BoostRoot" `
    -DCMAKE_POLICY_DEFAULT_CMP0167=OLD -DBUILD_SHARED_LIBS=OFF `
    -Dstatic_runtime=ON -Dbuild_tests=OFF -Ddeprecated-functions=OFF `
    "-DOPENSSL_INCLUDE_DIR=$OpenSSLRoot/include" `
    "-DLIB_EAY=$OpenSSLRoot/lib/libcrypto.lib" "-DSSL_EAY=$OpenSSLRoot/lib/libssl.lib"
Invoke-CMake --build "$root/build/torrent" --parallel $Jobs
Invoke-CMake --install "$root/build/torrent"

Invoke-CMake -S $root -B "$root/build/application" -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_INSTALL_PREFIX=$root/dist/DragonDownloadManager" `
    "-DBoost_ROOT=$BoostRoot" "-DOpenSSL_ROOT_DIR=$OpenSSLRoot" `
    "-DLibtorrentRasterbar_ROOT=$root/.deps/torrent" `
    -DCMAKE_POLICY_DEFAULT_CMP0167=OLD -DBUILD_TESTS=OFF
Invoke-CMake --build "$root/build/application" --parallel $Jobs
Invoke-CMake --install "$root/build/application"
Copy-Item "$CompilerRoot/bin/libgcc_s_seh-1.dll", "$CompilerRoot/bin/libstdc++-6.dll", "$CompilerRoot/bin/libwinpthread-1.dll" "$root/dist/DragonDownloadManager"
Write-Output "Windows application: $root/dist/DragonDownloadManager/DragonDownloadManager.exe"
