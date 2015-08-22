### Generic native Skirmish AI config
#

set(mySourceDirRel         "src/") # Common values are "" or "src"
set(additionalSources      ${CUtils_SRC})
#set(additionalCompileFlags "-Isrc/lib/ -Isrc/circuit/ -O1 -DDEBUG -DDEBUG_VIS")
set(additionalCompileFlags "-Isrc/lib/ -Isrc/circuit/ -DDEBUG_VIS")
set(additionalLibraries    ${Cpp_AIWRAPPER_TARGET})


if    (BUILD_Cpp_AIWRAPPER)
	include_directories(BEFORE ${Cpp_AIWRAPPER_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR})
	ConfigureNativeSkirmishAI(mySourceDirRel additionalSources additionalCompileFlags additionalLibraries)
else  (BUILD_Cpp_AIWRAPPER)
	message ("warning: (New) C++ Circuit AI will not be built! (missing Cpp Wrapper)")
endif (BUILD_Cpp_AIWRAPPER)
