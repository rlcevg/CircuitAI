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

# angelscript native calling convention for arm64 needs a hand-written assembly
# stub (CallARM64 et al.). The C/C++ source glob never picks up the .S file, so
# add it explicitly. macOS/Mach-O needs the xcode variant (underscore-mangled
# symbols) regardless of compiler; the generic glob handles the .cpp half.
if (APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
	enable_language(ASM)
	set(additionalSources  ${additionalSources} ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/angelscript/source/as_callfunc_arm64_xcode.S)
endif ()

set(additionalLibraries    ${Cpp_AIWRAPPER_TARGET} CUtils)  # can't use Tracy::TracyClient in dll, requires -fPIC

if (MSVC)
	set(additionalCompileFlags "${additionalCompileFlags} -DLEMON_ONLY_TEMPLATES")
else ()
	#set(additionalCompileFlags "-Isrc/lib/ -Isrc/circuit/ -O1 -DDEBUG -DDEBUG_VIS -DDEBUG_LOG -Wall -Wextra -D_GLIBCXX_USE_CXX11_ABI=0")
	if    (CIRCUIT_DEBUG)
		if (WIN32)
			set(additionalLibraries ${additionalLibraries} ${SDL2_LIBRARY})
		endif ()
		set(additionalCompileFlags "-DDEBUG_VIS -Wall")
		# set(additionalCompileFlags "-DDEBUG_VIS -Wall -fsanitize=address -fno-omit-frame-pointer -fno-common -O1 -fno-strict-aliasing")
	else  (CIRCUIT_DEBUG)
		set(additionalCompileFlags "-Wall")
		# set(additionalCompileFlags "-Wall -fno-omit-frame-pointer -O1")
	endif (CIRCUIT_DEBUG)
endif ()

# set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -lasan")


if    (CIRCUIT_AS_JIT)
	if (WIN32)
		set(additionalSources  ${additionalSources} platform/angelscript/jit/virtual_asm_windows.cpp)
	elseif (APPLE)
		set(additionalSources  ${additionalSources} platform/angelscript/jit/virtual_asm_linux.cpp)
	else ()
		set(additionalSources  ${additionalSources} platform/angelscript/jit/virtual_asm_linux.cpp)
	endif ()
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(additionalSources  ${additionalSources} platform/angelscript/jit/virtual_asm_x64.cpp)
	else ()
		set(additionalSources  ${additionalSources} platform/angelscript/jit/virtual_asm_x86.cpp)
	endif ()
	set(additionalSources      ${additionalSources} platform/angelscript/jit/as_jit.cpp)
	set(additionalCompileFlags "${additionalCompileFlags} -DCIRCUIT_AS_JIT")
endif (CIRCUIT_AS_JIT)


if    (BUILD_Cpp_AIWRAPPER)
	include_directories(BEFORE
		${Cpp_AIWRAPPER_INCLUDE_DIRS}
		${SDL2_INCLUDE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src/lib/
		${CMAKE_CURRENT_SOURCE_DIR}/src/lib/angelscript/include/
		${CMAKE_CURRENT_SOURCE_DIR}/src/circuit/
	)
	get_target_property(TRACY_INCLUDE_DIRS Tracy::TracyClient INCLUDE_DIRECTORIES)
	include_directories(BEFORE ${TRACY_INCLUDE_DIRS})
	if (CIRCUIT_PROFILING)
		set(additionalCompileFlags "${additionalCompileFlags} -fno-omit-frame-pointer -DCIRCUIT_PROFILING")
		if (NOT TRACY_ENABLE)
			get_target_property(TRACY_SOURCES Tracy::TracyClient SOURCES)
			set(additionalSources ${additionalSources} ${TRACY_SOURCES})
			set(additionalCompileFlags "${additionalCompileFlags} -DTRACY_ENABLE -DTRACY_ON_DEMAND -DTRACY_CALLSTACK=10 -DTRACY_NO_CALLSTACK_INLINES")
			set(additionalCompileFlags "${additionalCompileFlags} -DTRACY_NO_BROADCAST -DTRACY_NO_FRAME_IMAGE -DTRACY_NO_SYSTEM_TRACING")  # -DTRACY_NO_SAMPLING
		endif ()
	endif ()
	configure_native_skirmish_ai(mySourceDirRel additionalSources additionalCompileFlags additionalLibraries)
else  (BUILD_Cpp_AIWRAPPER)
	message ("warning: (New) C++ Circuit AI will not be built! (missing Cpp Wrapper)")
endif (BUILD_Cpp_AIWRAPPER)
