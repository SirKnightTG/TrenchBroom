SET(TEST_SOURCE_DIR "${CMAKE_SOURCE_DIR}/test/src")
SET(BENCHMARK_SOURCE_DIR "${CMAKE_SOURCE_DIR}/benchmark/src")

FILE(GLOB_RECURSE TEST_SOURCE
    "${TEST_SOURCE_DIR}/*.h"
    "${TEST_SOURCE_DIR}/*.cpp"
)
FILE(GLOB_RECURSE BENCHMARK_SOURCE
    "${BENCHMARK_SOURCE_DIR}/*.h"
	"${BENCHMARK_SOURCE_DIR}/*.cpp"	
)

ADD_EXECUTABLE(TrenchBroom-Test ${TEST_SOURCE})
ADD_EXECUTABLE(TrenchBroom-Benchmark ${BENCHMARK_SOURCE})

IF(COMPILER_IS_GNU AND TB_ENABLE_ASAN)
	TARGET_LINK_LIBRARIES(TrenchBroom-Test asan)
	TARGET_LINK_LIBRARIES(TrenchBroom-Benchmark asan)
ENDIF()

ADD_TARGET_PROPERTY(TrenchBroom-Test INCLUDE_DIRECTORIES "${TEST_SOURCE_DIR}")
ADD_TARGET_PROPERTY(TrenchBroom-Benchmark INCLUDE_DIRECTORIES "${BENCHMARK_SOURCE_DIR}")

TARGET_LINK_LIBRARIES(TrenchBroom-Test common glew gtest gmock vecmath)
TARGET_LINK_LIBRARIES(TrenchBroom-Benchmark common glew gtest gmock vecmath)

SET_TARGET_PROPERTIES(TrenchBroom-Test PROPERTIES COMPILE_DEFINITIONS "GLEW_STATIC")
SET_TARGET_PROPERTIES(TrenchBroom-Benchmark PROPERTIES COMPILE_DEFINITIONS "GLEW_STATIC")

SET(RESOURCE_DEST_DIR "$<TARGET_FILE_DIR:TrenchBroom-Test>")
SET(BENCHMARK_RESOURCE_DEST_DIR "$<TARGET_FILE_DIR:TrenchBroom-Benchmark>")

IF(WIN32)
	SET(RESOURCE_DEST_DIR "${RESOURCE_DEST_DIR}/..")
	SET(BENCHMARK_RESOURCE_DEST_DIR "${BENCHMARK_RESOURCE_DEST_DIR}/..")
	
	# Copy some Windows-specific resources
	ADD_CUSTOM_COMMAND(TARGET TrenchBroom-Test POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${LIB_BIN_DIR}/win32" "${RESOURCE_DEST_DIR}"
	)
	ADD_CUSTOM_COMMAND(TARGET TrenchBroom-Benchmark POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${LIB_BIN_DIR}/win32" "${BENCHMARK_RESOURCE_DEST_DIR}"
	)
ENDIF()

# Copy some files used in unit tests
ADD_CUSTOM_COMMAND(TARGET TrenchBroom-Test POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/test/data" "${RESOURCE_DEST_DIR}/data"
)

ADD_CUSTOM_COMMAND(TARGET TrenchBroom-Test POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_DEST_DIR}/data/GameConfig"
)

# Prepare to collect all cfg files to copy them to the test data
FILE(GLOB_RECURSE GAME_CONFIG_FILES
    "${APP_DIR}/resources/games/*.cfg"
)

FOREACH(GAME_CONFIG_FILE ${GAME_CONFIG_FILES})
	ADD_CUSTOM_COMMAND(TARGET TrenchBroom-Test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${GAME_CONFIG_FILE}" "${RESOURCE_DEST_DIR}/data/GameConfig"
    )
ENDFOREACH(GAME_CONFIG_FILE)

# Prepare to collect all definition files to copy them to the test data
FILE(GLOB_RECURSE GAME_DEF_FILES
		"${APP_DIR}/resources/games/*.def"
        "${APP_DIR}/resources/games/*.fgd"
		)

FOREACH(GAME_CONFIG_FILE ${GAME_DEF_FILES})
	ADD_CUSTOM_COMMAND(TARGET TrenchBroom-Test POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy "${GAME_CONFIG_FILE}" "${RESOURCE_DEST_DIR}/data/GameConfig"
			)
ENDFOREACH(GAME_CONFIG_FILE)


SET_XCODE_ATTRIBUTES(TrenchBroom-Test)
SET_XCODE_ATTRIBUTES(TrenchBroom-Benchmark)

# cotire
set_target_properties(TrenchBroom-Test PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "Prefix.h")
cotire(TrenchBroom-Test)

set_target_properties(TrenchBroom-Benchmark PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "Prefix.h")
cotire(TrenchBroom-Benchmark)
