add_library(solution INTERFACE)

# Add sources
target_sources(solution
    PUBLIC FILE_SET HEADERS FILES
        "vector.h"
)
