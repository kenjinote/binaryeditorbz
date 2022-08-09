echo "running scripts in the build job"
echo %CI_BUILDS_DIR%
cd Bz
7z x WTL91_5321_Final.zip -o"WTL91_5321_Final"
dos2unix atlscrl.h.patch
cd WTL91_5321_Final
dos2unix include\atlscrl.h
patch -p1 -l --binary < ..\atlscrl.h.915321.patch
unix2dos include\atlscrl.h
cd ..
curl -O https://zlib.net/zlib1212.zip
7z x zlib1212.zip
cd zlib-1.2.12
md build
cd build
cmake -G "Visual Studio 16 2019" -A win32 -DBUILD_SHARED_LIBS=OFF -D CMAKE_C_FLAGS_RELEASE:STRING="/MT /Os /GF /GL" ..
cmake --build . --config Release
copy zconf.h ..
cd ..
cd ..
md build
cd build
cmake -G "Visual Studio 16 2019" -A win32 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DZLIB_LIBRARY=%CI_BUILDS_DIR%\Bz\zlib-1.2.12\build\Release\zlibstatic.lib -DZLIB_INCLUDE_DIR=%CI_BUILDS_DIR%\Bz\zlib-1.2.12 -DBUILD_SHARED_LIBS=OFF  -D CMAKE_CXX_FLAGS_RELEASE:STRING="/MT /Os /GF /GL /I%CI_BUILDS_DIR%\Bz\WTL91_5321_Final\Include" ..
cmake --build . --config Release
cd ..
cd zlib-1.2.12
md build64
cd build64
cmake -G "Visual Studio 16 2019" -A x64 -DBUILD_SHARED_LIBS=OFF -D CMAKE_C_FLAGS_RELEASE:STRING="/MT /Os /GF /GL" ..
cmake --build . --config Release
copy /Y zconf.h ..
cd ..
cd ..
md build64
cd build64
cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DZLIB_LIBRARY=%CI_BUILDS_DIR%\Bz\zlib-1.2.12\build64\Release\zlibstatic.lib -DZLIB_INCLUDE_DIR=%CI_BUILDS_DIR%\Bz\zlib-1.2.12 -DBUILD_SHARED_LIBS=OFF  -D CMAKE_CXX_FLAGS_RELEASE:STRING="/MT /Os /GF /GL /I%CI_BUILDS_DIR%\Bz\WTL91_5321_Final\Include" ..
cmake --build . --config Release
cd ..
cd nsis
makensis.exe /X"SetCompressor /SOLID lzma" /X"OutFile BzTest.exe" setupTest.nsi
