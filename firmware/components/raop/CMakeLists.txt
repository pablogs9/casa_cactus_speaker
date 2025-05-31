
idf_component_register(SRC_DIRS .
						INCLUDE_DIRS .
						PRIV_REQUIRES newlib freertos pthread mdns mbedtls esp_wifi espressif__esp_audio_codec
)
set_source_files_properties(raop.c
    PROPERTIES COMPILE_FLAGS
    -Wno-misleading-indentation
)