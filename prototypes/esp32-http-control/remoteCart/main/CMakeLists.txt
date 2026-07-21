file(GLOB_RECURSE srcs "remoteCart.c" "src/*.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "./inc")
