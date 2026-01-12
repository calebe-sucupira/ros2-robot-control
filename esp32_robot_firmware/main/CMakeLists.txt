idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES micro_ros_espidf_component
                             nvs_flash
                             esp_wifi
                             driver
                             esp_timer)