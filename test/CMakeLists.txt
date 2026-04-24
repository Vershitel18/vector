add_executable(tests)

# Add sources
target_sources(tests
    PUBLIC FILE_SET HEADERS FILES
        "element.h"
        "fault-injection.h"
        "ordered-element.h"
    PRIVATE
        "element.cpp"
        "fault-injection.cpp"
        "tests.cpp"
)

# Setup warnings
ct_set_compiler_warnings(tests)

# Link with solution
target_link_libraries(tests PRIVATE solution)

# Link with dependencies
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
