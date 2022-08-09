choco install visualstudio2019buildtools -y --package-parameters "--add Microsoft.VisualStudio.Component.VC.ATL --add Microsoft.VisualStudio.Component.VC.ATLMFC --quiet --norestart" --force

dir "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.26.28801"
dir "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133"
dir "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\"
dir "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.16.11\atl\"

choco install -y --no-progress patch dos2unix cmake nsis
echo "running scripts in the build job"
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
cmake -G "Visual Studio 16 2019" -A win32 -DZLIB_LIBRARY=%cd%\..\zlib-1.2.12\build\Release\zlibstatic.lib -DZLIB_INCLUDE_DIR=%cd%\..\zlib-1.2.12 -DBUILD_SHARED_LIBS=OFF  -D CMAKE_CXX_FLAGS_RELEASE:STRING="/MT /Os /GF /GL /I%cd%\..\WTL91_5321_Final\Include /I\"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\atlmfc\include\"" ..
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
cmake -G "Visual Studio 16 2019" -A x64 -DZLIB_LIBRARY=%cd%\..\zlib-1.2.12\build64\Release\zlibstatic.lib -DZLIB_INCLUDE_DIR=%cd%\..\zlib-1.2.12 -DBUILD_SHARED_LIBS=OFF  -D CMAKE_CXX_FLAGS_RELEASE:STRING="/MT /Os /GF /GL /I%cd%\..\WTL91_5321_Final\Include /I\"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\atlmfc\include\"" ..
cmake --build . --config Release
cd ..
cd nsis
makensis.exe /X"SetCompressor /SOLID lzma" /X"OutFile BzTest.exe" setupTest.nsi
