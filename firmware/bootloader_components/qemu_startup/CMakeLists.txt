if(IDF_TARGET STREQUAL "esp32s3")
    idf_component_register(SRCS "qemu_startup.c"
        INCLUDE_DIRS "../../main" PRIV_REQUIRES soc)
else()
    idf_component_register()
endif()
