# Build
This README is for internal reading only.
Do not export this together with other installables, or it may confuse the user. 

## Unix
1) Install ECAL 5.11.3 (download from https://eclipse-ecal.github.io/ecal/_download_archive/download_archive.html#download-archive).
2) Build and install CapNProto 1.0.2 follow instructions at https://capnproto.org/install.html.
3) Clone vk_sdk repository.
4) Enter the following commands.
```
cd vk_sdk
mkdir build
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release ..
make package
```

## Windows
Before following any instructions below, ensure you have Visual Studio C++ Build Tools installed.

1) Install and setup `vcpkg` by following the instructions at https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-cmd.
2) Install ECAL 5.11.3 (download from https://eclipse-ecal.github.io/ecal/_download_archive/download_archive.html#download-archive).
3) Build and install CapnProto 1.0.2 (follow instructions for Windows at https://capnproto.org/install.html#supported-compilers).
    - Note: Use `cmake -DCMAKE_INSTALL_PREFIX=C:\capnproto` instead of the original instruction to specify install directory.
4) Clone ÌXWebSocket at https://github.com/machinezone/IXWebSocket.
    - Run the following commands:
    ```
    cd IXWebSocket
    mkdir build
    cd build
    cmake -DBUILD_SHARED_LIBS=ON -DUSE_TLS=OFF -DUSE_ZLIB=OFF -DCMAKE_INSTALL_PREFIX="C:\IXWebSocket" ..
    cmake --build . --config Release
    ```
    - Note: Need to manually copy `.dll` file to vk_sdk install folder later.
5) Install protobuf using `vcpkg install protobuf:x64-windows` (Required for ECAL).
6) Run the following commands:
```
cd vk_sdk
mkdir build
cd build
cmake -DVK_SDK_STANDALONE=YES -DCapnProto_DIR=C:\capnproto\lib\cmake\CapnProto -DIXWebSocket_DIR=C:\IXWebSocket\lib\cmake\ixwebsocket -DCMAKE_TOOLCHAIN_FILE="C:\Users\User\Desktop\vcpkg\scripts\buildsystems\vcpkg.cmake"  -DCMAKE_INSTALL_PREFIX="C:\vk_sdk" ..
cmake --build . --config Release
cmake --install . --config Release
```
You should find a `build/vk_sdk` directory containing all the files to be distributed to the customer. Simply zip that folder up.

