# -*- cmake -*-

INCLUDE(APR)
INCLUDE(LLMath)
INCLUDE(Tut)
INCLUDE(Cwdebug)

MACRO(ADD_BUILD_TEST_NO_COMMON name parent)
#   MESSAGE("${CMAKE_CURRENT_SOURCE_DIR}/tests/${name}_test.cpp")
    IF (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${name}_test.cpp")
        SET(no_common_libraries
            ${APRUTIL_LIBRARIES}
            ${APR_LIBRARIES}
            ${PTHREAD_LIBRARY}
            ${WINDOWS_LIBRARIES}
            )
        SET(no_common_source_files
            ${name}.cpp
            tests/${name}_test.cpp
            ${CMAKE_SOURCE_DIR}/test/test.cpp
            )
        ADD_BUILD_TEST_INTERNAL("${name}" "${parent}" "${no_common_libraries}" "${no_common_source_files}")
    ENDIF (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${name}_test.cpp")
ENDMACRO(ADD_BUILD_TEST_NO_COMMON name parent)


MACRO(ADD_BUILD_TEST name parent)
    # optional extra parameter: list of additional source files
    SET(more_source_files "${ARGN}")

#   MESSAGE("${CMAKE_CURRENT_SOURCE_DIR}/tests/${name}_test.cpp")
    IF (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${name}_test.cpp")

        SET(basic_libraries
            ${LLCOMMON_LIBRARIES}
            ${APRUTIL_LIBRARIES}
            ${APR_LIBRARIES}
            ${PTHREAD_LIBRARY}
            ${WINDOWS_LIBRARIES}
            )
        SET(basic_source_files
            ${name}.cpp
            tests/${name}_test.cpp
            ${CMAKE_SOURCE_DIR}/test/test.cpp
            ${CMAKE_SOURCE_DIR}/test/lltut.cpp
            ${more_source_files}
            )
        ADD_BUILD_TEST_INTERNAL("${name}" "${parent}" "${basic_libraries}" "${basic_source_files}")

    ENDIF (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${name}_test.cpp")
ENDMACRO(ADD_BUILD_TEST name parent)


MACRO(ADD_VIEWER_BUILD_TEST name parent)
    # This is just like the generic ADD_BUILD_TEST, but we implicitly
    # add the necessary precompiled header .cpp file (anyone else find that
    # oxymoronic?) because the MSVC build errors will NOT point you there.
    ADD_BUILD_TEST("${name}" "${parent}" llviewerprecompiledheaders.cpp)
ENDMACRO(ADD_VIEWER_BUILD_TEST name parent)


MACRO(ADD_SIMULATOR_BUILD_TEST name parent)
    ADD_BUILD_TEST("${name}" "${parent}" llsimprecompiledheaders.cpp)

    if (WINDOWS)
        SET_SOURCE_FILES_PROPERTIES(
            "tests/${name}_test.cpp"
            PROPERTIES
            COMPILE_FLAGS "/Yullsimprecompiledheaders.h"
        )
    endif (WINDOWS)
ENDMACRO(ADD_SIMULATOR_BUILD_TEST name parent)

MACRO(ADD_BUILD_TEST_INTERNAL name parent libraries source_files)
    # Optional additional parameter: pathname of Python wrapper script
    SET(wrapper "${ARGN}")
    #MESSAGE(STATUS "ADD_BUILD_TEST_INTERNAL ${name} libraries = \"${libraries}\"; source_files = \"${source_files}\"; wrapper = \"${wrapper}\"")

    SET(TEST_SOURCE_FILES ${source_files})
    SET(HEADER "${name}.h")
    set_source_files_properties(${HEADER}
                            PROPERTIES HEADER_FILE_ONLY TRUE)
    LIST(APPEND TEST_SOURCE_FILES ${HEADER})
    INCLUDE_DIRECTORIES("${LIBS_OPEN_DIR}/test")
    ADD_EXECUTABLE(${name}_test ${TEST_SOURCE_FILES})
    TARGET_LINK_LIBRARIES(${name}_test
        ${libraries}
        )

	SET(TEST_EXE $<TARGET_FILE:${name}_test>)
    SET(TEST_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}_test_ok.txt)

    IF ("${wrapper}" STREQUAL "")
      SET(TEST_CMD ${TEST_EXE} --touch=${TEST_OUTPUT} --sourcedir=${CMAKE_CURRENT_SOURCE_DIR})
    ELSE ("${wrapper}" STREQUAL "")
      SET(TEST_CMD ${PYTHON_EXECUTABLE} ${wrapper} ${TEST_EXE} --touch=${TEST_OUTPUT} --sourcedir=${CMAKE_CURRENT_SOURCE_DIR})
    ENDIF ("${wrapper}" STREQUAL "")

    #MESSAGE(STATUS "ADD_BUILD_TEST_INTERNAL ${name} test_cmd  = ${TEST_CMD}")
    #MESSAGE(STATUS "CMAKE_BINARY_DIR = \"${CMAKE_BINARY_DIR}\"")
    SET(LD_LIBRARY_PATH "${CMAKE_BINARY_DIR}/llcommon:/usr/lib:/usr/local/lib")
    IF (NOT "${ARCH_PREBUILT_DIRS}" STREQUAL "")
      SET(LD_LIBRARY_PATH "${ARCH_PREBUILT_DIRS}:${LD_LIBRARY_PATH}")
    ENDIF (NOT "${ARCH_PREBUILT_DIRS}" STREQUAL "")
    IF (NOT "$ENV{LD_LIBRARY_PATH}" STREQUAL "")
      SET(LD_LIBRARY_PATH "$ENV{LD_LIBRARY_PATH}:${LD_LIBRARY_PATH}")
    ENDIF (NOT "$ENV{LD_LIBRARY_PATH}" STREQUAL "")

    ADD_CUSTOM_TARGET(${name}_test_ok ALL DEPENDS ${TEST_OUTPUT})
    IF (${parent})
      ADD_DEPENDENCIES(${parent} ${name}_test_ok)
    ENDIF (${parent})

ENDMACRO(ADD_BUILD_TEST_INTERNAL name parent libraries source_files)


MACRO(ADD_COMM_BUILD_TEST name parent wrapper)
##  MESSAGE(STATUS "ADD_COMM_BUILD_TEST ${name} wrapper = ${wrapper}")
    # optional extra parameter: list of additional source files
    SET(more_source_files "${ARGN}")
##  MESSAGE(STATUS "ADD_COMM_BUILD_TEST ${name} more_source_files = ${more_source_files}")

    SET(libraries
        ${LLMESSAGE_LIBRARIES}
        ${LLMATH_LIBRARIES}
        ${LLVFS_LIBRARIES}
        ${LLCOMMON_LIBRARIES}
        ${APRUTIL_LIBRARIES}
        ${APR_LIBRARIES}
        ${PTHREAD_LIBRARY}
        ${WINDOWS_LIBRARIES}
        )
    SET(source_files
        ${name}.cpp
        tests/${name}_test.cpp
        ${CMAKE_SOURCE_DIR}/test/test.cpp
        ${CMAKE_SOURCE_DIR}/test/lltut.cpp
        ${more_source_files}
        )

    ADD_BUILD_TEST_INTERNAL("${name}" "${parent}" "${libraries}" "${source_files}" "${wrapper}")
ENDMACRO(ADD_COMM_BUILD_TEST name parent wrapper)

MACRO(ADD_VIEWER_COMM_BUILD_TEST name parent wrapper)
    # This is just like the generic ADD_COMM_BUILD_TEST, but we implicitly
    # add the necessary precompiled header .cpp file (anyone else find that
    # oxymoronic?) because the MSVC build errors will NOT point you there.
##  MESSAGE(STATUS "ADD_VIEWER_COMM_BUILD_TEST ${name} wrapper = ${wrapper}")
    ADD_COMM_BUILD_TEST("${name}" "${parent}" "${wrapper}" llviewerprecompiledheaders.cpp)
ENDMACRO(ADD_VIEWER_COMM_BUILD_TEST name parent wrapper)
MACRO(SET_TEST_PATH LISTVAR)
  IF(WINDOWS)
    # We typically build/package only Release variants of third-party
    # libraries, so append the Release staging dir in case the library being
    # sought doesn't have a debug variant.
    set(${LISTVAR} ${SHARED_LIB_STAGING_DIR}/${CMAKE_CFG_INTDIR} ${SHARED_LIB_STAGING_DIR}/Release)
  ELSEIF(DARWIN)
    # We typically build/package only Release variants of third-party
    # libraries, so append the Release staging dir in case the library being
    # sought doesn't have a debug variant.
    set(${LISTVAR} ${SHARED_LIB_STAGING_DIR}/${CMAKE_CFG_INTDIR}/Resources ${SHARED_LIB_STAGING_DIR}/Release/Resources /usr/lib)
  ELSE(WINDOWS)
    # Linux uses a single staging directory anyway.
    IF (STANDALONE)
      set(${LISTVAR} ${CMAKE_BINARY_DIR}/llcommon /usr/lib /usr/local/lib)
    ELSE (STANDALONE)
      set(${LISTVAR} ${SHARED_LIB_STAGING_DIR} /usr/lib)
    ENDIF (STANDALONE)
  ENDIF(WINDOWS)
ENDMACRO(SET_TEST_PATH)
