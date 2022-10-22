#!/bin/sh

ProgramName="steven.exe"

# User defines
UserDefines="-DDEBUG"

# Compiler
CXX="x86_64-w64-mingw32-g++"

CompilerFlags="-std=c++11"
CompilerFlags+=" -g"
#CompilerFlags+=" -fsanitize=address"
CompilerFlags+=" -fno-rtti -fno-exceptions"
CompilerFlags+=" -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-unused-value -Wno-unused-function"

# Source files
SourceFiles="code/*.cpp"

# MinGW
CompilerFlags+=" -lmingw32"

# SDL
CompilerFlags+=" -lSDL2main -lSDL2"

# Windows specific
CompilerFlags+=" -Wl,--subsystem,windows"


echo "Compiling..."
$CXX $UserDefines $SourceFiles -o $ProgramName $CompilerFlags

compiled=$?
if [ $compiled != 0 ]
then
	exit $compiled
fi

echo "Done!"

#x86_64-w64-mingw32-g++ -std=c++11 -g -fno-rtti -fno-exceptions -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-unused-value -Wno-unused-function -DDEBUG code/main.cpp -o steven2.exe -lmingw32 -lSDL2main -lSDL2 -Wl,--subsystem,windows
