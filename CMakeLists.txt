### Generic native Skirmish AI config
#

set(mySourceDirRel         "src/") # Common values are "" or "src"
set(additionalSources      ${CMAKE_SOURCE_DIR}/rts/System/Util.cpp)
set(additionalLibraries    ${Cpp_AIWRAPPER_TARGET} CUtils)

#set(additionalCompileFlags "-Isrc/lib/ -Isrc/circuit/ -O1 -DDEBUG -DDEBUG_VIS -DDEBUG_LOG -Wall -Wextra -D_GLIBCXX_USE_CXX11_ABI=0")
# Spring 100.0 was compiled with old ABI
if ("${CMAKE_SYSTEM}" MATCHES "ARCH")
	set(additionalCompileFlags "-DDEBUG_VIS -Wall -D_GLIBCXX_USE_CXX11_ABI=1")
else ()
	set(additionalCompileFlags "-Wall -D_GLIBCXX_USE_CXX11_ABI=0")
endif ()

set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++")


if    (BUILD_Cpp_AIWRAPPER)
	include_directories(BEFORE
		${Cpp_AIWRAPPER_INCLUDE_DIRS}
		${SDL2_INCLUDE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src/lib/
		${CMAKE_CURRENT_SOURCE_DIR}/src/circuit/
	)
	ConfigureNativeSkirmishAI(mySourceDirRel additionalSources additionalCompileFlags additionalLibraries)
else  (BUILD_Cpp_AIWRAPPER)
	message ("warning: (New) C++ Circuit AI will not be built! (missing Cpp Wrapper)")
endif (BUILD_Cpp_AIWRAPPER)
