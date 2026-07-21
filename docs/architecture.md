# Arquitetura

## Fluxo principal

O sistema recebe comandos de velocidade por ROS 2 e os converte em referências para as rodas do carrinho.

```text
Teleoperação ou controlador
            |
            | /cmd_vel
            v
      ROS 2 / micro-ROS
            |
     +------+------+
     |             |
     v             v
Firmware ESP32   Simulação
     |             |
     v             v
Motores e       Estado do
encoders        modelo
     |             |
     +------+------+
            |
            | odometria
            v
Dashboard e MATLAB/Simulink
```

## Firmware ESP32

A implementação principal está dividida nos seguintes módulos:

- `app_main`: inicialização da aplicação;
- `wifi_manager`: conexão Wi-Fi;
- `udp_transport`: transporte personalizado;
- `microros_node`: entidades e executor do micro-ROS;
- `motor_control`: acionamento dos motores;
- `encoder`: contagem de pulsos;
- `odometry`: estimativa da pose;
- `robot_control`: comandos, controle PI e segurança;
- `robot_config`: configurações compartilhadas.

Os comandos recebidos em `/cmd_vel` são enviados para a lógica de controle por uma fila, reduzindo o acoplamento entre comunicação e controle.

## Controle diferencial

A velocidade linear e a velocidade angular são convertidas em referências para as rodas esquerda e direita.

```text
velocidade_esquerda = linear - angular
velocidade_direita  = linear + angular
```

Quando os valores ultrapassam a faixa suportada, são normalizados proporcionalmente.

## Segurança

Os módulos embarcados possuem timeout de comando. Caso uma nova referência não seja recebida dentro do intervalo configurado, os motores são parados.

O firmware principal também utiliza:

- regiões críticas para acesso aos encoders;
- proteção dos dados de odometria;
- anti-windup no controlador PI;
- normalização de ângulo;
- parada em falhas de inicialização.

## Comunicação

### Firmware ESP32

Utiliza um transporte UDP personalizado para conectar o cliente micro-ROS ao agente executado no computador.

### Protótipo HTTP

O ESP32 funciona como ponto de acesso e servidor web. A interface envia comandos por requisições HTTP `POST`.

### Pico W

O Pico W utiliza a pilha lwIP e um transporte UDP personalizado para micro-ROS.

## Simulação

O módulo Gazebo contém:

- descrição URDF;
- mundo de simulação;
- script de criação do modelo;
- ponte entre odometria ROS 2 e estado do modelo.

## Processamento físico

O dashboard mantém amostras sincronizadas e calcula grandezas derivadas a partir da telemetria recebida.

A camada de processamento físico foi separada da integração ROS 2 e da interface web.
