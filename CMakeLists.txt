### Generic native Skirmish AI config
#

set(CMAKE_CXX_STANDARD 20)

set(mySourceDirRel         "src/") # Common values are "" or "src"
set(additionalSources      ${CMAKE_SOURCE_DIR}/rts/System/StringUtil.cpp)
set(additionalSources      ${additionalSources} ${CMAKE_SOURCE_DIR}/rts/System/Misc/SpringTime.cpp)
if (WIN32)  # spring::condition_variable_any requirement
	set(additionalSources  ${additionalSources} ${CMAKE_SOURCE_DIR}/rts/System/Platform/Win/CriticalSection.cpp)
elseif (APPLE)
	set(additionalSources  ${additionalSources} ${CMAKE_SOURCE_DIR}/rts/System/Platform/Mac/Signal.cpp)
else ()
	set(additionalSources  ${additionalSources} ${CMAKE_SOURCE_DIR}/rts/System/Platform/Linux/Futex.cpp)
endif ()
if(MSVC AND NOT IS_ARM64_BUILD)
            enable_language(ASM_MASM)
            if(CMAKE_ASM_MASM_COMPILER_WORKS)
                            set(additionalSources ${additionalSources} ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/angelscript/source/as_callfunc_x64_msvc_asm.asm)
							set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/lib/angelscript/source/as_callfunc_x64_msvc_asm.asm PROPERTIES LANGUAGE ASM_MASM)
            else()
                            message(FATAL ERROR "MSVC x86_64 target requires a working assembler")
            endif()
endif()
set(additionalLibraries    ${Cpp_AIWRAPPER_TARGET} CUtils)
#set(additionalLibraries    ${Cpp_AIWRAPPER_TARGET} CUtils ${SDL2_LIBRARY})

if (MSVC)
	set(additionalCompileFlags "${additionalCompileFlags} -DLEMON_ONLY_TEMPLATES")
else ()
	#set(additionalCompileFlags "-Isrc/lib/ -Isrc/circuit/ -O1 -DDEBUG -DDEBUG_VIS -DDEBUG_LOG -Wall -Wextra -D_GLIBCXX_USE_CXX11_ABI=0")
	if (CIRCUIT_DEBUG)
		set(additionalCompileFlags "-DDEBUG_VIS -Wall")
	#	set(additionalCompileFlags "-DDEBUG_LOG -Wall -fsanitize=address -fno-omit-frame-pointer -fno-common -O1 -fno-strict-aliasing")
	else ()
		set(additionalCompileFlags "-Wall -fno-omit-frame-pointer -O1")
	endif ()
endif ()	
#set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -lasan")


if    (BUILD_Cpp_AIWRAPPER)
	include_directories(BEFORE
		${Cpp_AIWRAPPER_INCLUDE_DIRS}
		${SDL2_INCLUDE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src/lib/
		${CMAKE_CURRENT_SOURCE_DIR}/src/lib/angelscript/include/
		${CMAKE_CURRENT_SOURCE_DIR}/src/circuit/
	)
	configure_native_skirmish_ai(mySourceDirRel additionalSources additionalCompileFlags additionalLibraries)
else  (BUILD_Cpp_AIWRAPPER)
	message ("warning: (New) C++ Circuit AI will not be built! (missing Cpp Wrapper)")
endif (BUILD_Cpp_AIWRAPPER)
