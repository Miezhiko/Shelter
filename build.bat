if not exist "build" mkdir "build"
cd "build"

cmake -DCMAKE_BUILD_TYPE=Release ..
"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe" shelter.sln /p:configuration=release /p:platform=x64
