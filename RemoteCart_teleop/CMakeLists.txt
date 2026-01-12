cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "/home/calebe/micro_ros_ws/src/pico-sdk")
set(PICO_BOARD pico_w)

# 1. Defina o caminho para a pasta 'libmicroros'
set(MICROROS_LIB_DIR "/home/calebe/micro_ros_ws/src/micro_ros_raspberrypi_pico_sdk/libmicroros")

# 2. Prepara o SDK
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# 3. Define o projeto e CARREGA O TOOLCHAIN PRINCIPAL
project(RemoteCart_teleop C CXX ASM)

# 4. Finaliza a configuração do SDK para este projeto
pico_sdk_init()

# 5. Crie seu executável
add_executable(pico_car_main
    main.c
    picow_udp_transports.c
)

# 6. Diga ao CMake onde encontrar os HEADERS (.h) do micro-ROS
#    (Dentro de .../libmicroros/include)
target_include_directories(pico_car_main PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    "${MICROROS_LIB_DIR}/include" 
)

# 7. REMOVEMOS o comando link_directories()
#    Não precisamos mais dele.

# 8. Linke TODAS as bibliotecas necessárias
target_link_libraries(pico_car_main
    # Bibliotecas do Pico W
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_pwm
    hardware_gpio
    
    # --- A GRANDE MUDANÇA ---
    # Fornecemos o caminho completo e absoluto para o arquivo .a
    # Isso elimina qualquer ambiguidade de busca do linker.
    "${MICROROS_LIB_DIR}/libmicroros.a"
    
    # A biblioteca de matemática (necessária para o micro-ROS)
    m
)

# 9. Configurações de stdio e .uf2
pico_enable_stdio_usb(pico_car_main 1)
pico_enable_stdio_uart(pico_car_main 0)
pico_add_uf2_output(pico_car_main)