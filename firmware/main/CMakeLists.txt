idf_component_register(SRCS "cactus_speaker.cpp"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "webpages/wifiprovisioning.html"
                    EMBED_FILES "songlists/beep.wav" "songlists/start_beep.wav" "songlists/volume_beep.wav")

# Remove some warnings
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
