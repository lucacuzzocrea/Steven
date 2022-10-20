#!/bin/sh

ProgramName="steven"

# User defines
UserDefines="-DDEBUG"

# Compiler
CXX="g++"

CompilerFlags="-std=c++11"
CompilerFlags+=" -g"
#CompilerFlags+=" -fsanitize=address"
CompilerFlags+=" -fno-rtti -fno-exceptions"
CompilerFlags+=" -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-unused-value -Wno-unused-function"

# Source files
SourceFiles="code/*.cpp"

# SDL
CompilerFlags+=" -lSDL2"


echo "Compiling..."
$CXX $CompilerFlags $UserDefines $SourceFiles -o $ProgramName

compiled=$?
if [ $compiled != 0 ]
then
	exit $compiled
fi

echo "Done!"
