option(CT_HARDENED "Should the standard library be hardened" OFF)
option(CT_SANITIZED "Should the build be sanitized" OFF)
option(CT_THREAD_SANITIZED "Should the build be thread-sanitized" OFF)

function(ct_configure_compiler)

    set(is_gcc OFF)
    set(is_clang OFF)
    set(is_msvc OFF)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(is_gcc ON)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(is_clang ON)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(is_msvc ON)
    endif()

    set(compiler_options "")
    set(compiler_definitions "")
    set(linker_options "")

    if(is_clang)
        list(APPEND compiler_options -stdlib=libc++)
        list(APPEND linker_options -stdlib=libc++)
        message(STATUS "Using libc++ as a standard library")
    endif()

    if(CT_HARDENED)
        if(is_gcc)
            list(APPEND compiler_definitions _GLIBCXX_DEBUG)
            message(STATUS "Enabled debug mode for libstdc++")
        elseif(is_clang)
            list(APPEND compiler_definitions _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)
            message(STATUS "Enabled hardening mode for libc++")
        else()
            # TODO: https://github.com/microsoft/STL/wiki/STL-Hardening
            message(STATUS "Hardening is not supported for CXX compiler: '${CMAKE_CXX_COMPILER_ID}'")
        endif()
    endif()

    if(CT_SANITIZED)
        if(is_gcc OR is_clang)
            list(APPEND compiler_options -fsanitize=undefined,address)
            list(APPEND linker_options -fsanitize=undefined,address)
            list(APPEND compiler_options
                -fno-sanitize-recover=all
                -fno-optimize-sibling-calls
                -fno-omit-frame-pointer
            )
            message(STATUS "Enabled UBSan and ASan")
        elseif(is_msvc)
            list(APPEND compiler_options /fsanitize=address)
            message(STATUS "Enabled ASan")
        else()
            message(WARNING "Sanitized builds are not supported for CXX compiler: '${CMAKE_CXX_COMPILER_ID}'")
        endif()
    endif()

    if(CT_THREAD_SANITIZED)
        if(is_gcc OR is_clang)
            list(APPEND compiler_options -fsanitize=thread)
            list(APPEND linker_options -fsanitize=thread)
            list(APPEND compiler_options
                -fno-sanitize-recover=all
                -fno-optimize-sibling-calls
                -fno-omit-frame-pointer
            )
            message(STATUS "Enabled TSan")
        else()
            message(WARNING "ThreadSanitized builds are not supported for CXX compiler: '${CMAKE_CXX_COMPILER_ID}'")
        endif()
    endif()

    message(STATUS "Setting global compiler options: ${compiler_options}")
    message(STATUS "Setting global compiler definitions: ${compiler_definitions}")
    message(STATUS "Setting global linker options: ${linker_options}")

    add_compile_options(${compiler_options})
    add_compile_definitions(${compiler_definitions})
    add_link_options(${linker_options})

endfunction()
