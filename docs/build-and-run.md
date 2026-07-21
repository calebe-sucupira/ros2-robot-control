# Configuração e compilação

Os comandos devem ser executados a partir da raiz do repositório.

## Firmware ESP32

### Requisitos

- ESP-IDF instalado
- Ambiente do ESP-IDF ativado
- Componente micro-ROS disponível

### Compilação

```bash
cd firmware/esp32
./scripts/build.sh
```

As configurações de rede e do agente podem ser ajustadas com:

```bash
idf.py menuconfig
```

## Protótipo HTTP com ESP32

```bash
cd prototypes/esp32-http-control/remoteCart
idf.py fullclean build
```

As configurações do ponto de acesso podem ser alteradas com:

```bash
idf.py menuconfig
```

## Protótipo Pico W

Defina os caminhos locais:

```bash
export PICO_SDK_PATH="$HOME/micro_ros_ws/src/pico-sdk"
export MICROROS_LIB_DIR="$HOME/micro_ros_ws/src/micro_ros_raspberrypi_pico_sdk/libmicroros"
```

Configure o projeto:

```bash
cmake \
    -S prototypes/pico-microros-teleop \
    -B prototypes/pico-microros-teleop/build \
    -DREMOTE_CART_WIFI_SSID="YOUR_WIFI_SSID" \
    -DREMOTE_CART_WIFI_PASSWORD="YOUR_WIFI_PASSWORD" \
    -DREMOTE_CART_AGENT_IP="10.42.0.1" \
    -DREMOTE_CART_AGENT_PORT="8888"
```

Compile:

```bash
cmake \
    --build prototypes/pico-microros-teleop/build \
    --parallel
```

As credenciais são configuradas localmente e não precisam ser versionadas.

## micro-ROS Agent

Exemplo de agente UDP:

```bash
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
```

## Dashboard

Crie um ambiente Python:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r dashboard/requirements.txt
```

Execute:

```bash
uvicorn dashboard.app:app --reload
```

O dashboard também pode ser iniciado sem `rclpy`, mas indicará que o ROS 2 está indisponível.

## MATLAB e Simulink

Adicione o módulo ao caminho do MATLAB:

```matlab
addpath(genpath("control/matlab-simulink"))
```

Execute o exemplo de publicação em `/cmd_vel`:

```matlab
control_turtlebot
```

Os modelos principais estão em:

```text
control/matlab-simulink/Controle_Trajetoria/
```

## Gazebo

Os arquivos principais são:

```text
simulation/gazebo/carrinho.urdf
simulation/gazebo/meu_mundo.world
simulation/gazebo/spawn_carro.py
simulation/gazebo/ponte.py
```

O ambiente requer ROS 2, Gazebo e os plugins `gazebo_ros`.
