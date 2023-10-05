#file(GLOB_RECURSE MENJIN_SOURCES ./*.c)

idf_component_register(
        SRC_DIRS
        "."
        "app"
        "system"
#        "http"

        INCLUDE_DIRS
        "."
        "app"
        "system"
#        "http"
)