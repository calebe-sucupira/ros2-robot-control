# Robótica I — Carrinho móvel com ROS 2

Projeto de um carrinho robótico diferencial integrado a ESP32, Raspberry Pi Pico W, micro-ROS, ROS 2, Gazebo, MATLAB/Simulink e uma aplicação web para análise de telemetria.

O repositório reúne o firmware principal, protótipos embarcados, simulação, controle de trajetória e visualização de dados físicos.

## Recursos

- Controle diferencial de dois motores
- Comunicação ROS 2 por micro-ROS
- Transporte UDP personalizado
- Controle de motores por PWM
- Leitura de encoders
- Estimativa de odometria
- Controle PI com anti-windup
- Timeout de segurança
- Simulação no Gazebo
- Controle de trajetória no MATLAB/Simulink
- Dashboard web para análise física
- Protótipo de controle HTTP pelo ESP32

## Arquitetura geral

```text
Comandos de velocidade
        |
        v
ROS 2 / micro-ROS
        |
        +-- Firmware ESP32
        +-- Protótipo Pico W
        +-- Simulação Gazebo
        +-- MATLAB/Simulink

Odometria e telemetria
        |
        v
Dashboard de análise física
```

## Estrutura

```text
Robotica-I/
├── firmware/
│   └── esp32/
├── prototypes/
│   ├── esp32-http-control/
│   └── pico-microros-teleop/
├── simulation/
│   └── gazebo/
├── control/
│   └── matlab-simulink/
├── dashboard/
├── docs/
├── .gitignore
└── README.md
```

## Módulos

### Firmware ESP32

Implementação principal do carrinho utilizando ESP-IDF e micro-ROS.

O firmware possui módulos para:

- conexão Wi-Fi;
- transporte UDP;
- inicialização do nó micro-ROS;
- recepção de comandos em `/cmd_vel`;
- controle dos motores;
- leitura dos encoders;
- cálculo de odometria;
- controle PI;
- timeout de segurança.

Diretório:

```text
firmware/esp32/
```

### Controle HTTP com ESP32

Protótipo no qual o ESP32 cria um ponto de acesso Wi-Fi e disponibiliza uma interface web para controlar o carrinho.

A interface aceita comandos por toque contínuo ou teclado e envia requisições HTTP ao firmware.

Diretório:

```text
prototypes/esp32-http-control/
```

### Pico W com micro-ROS

Protótipo utilizando Raspberry Pi Pico W, transporte UDP, subscriber de `/cmd_vel` e controle PWM dos motores.

Diretório:

```text
prototypes/pico-microros-teleop/
```

### Gazebo

Modelo URDF, mundo de simulação e scripts de integração com ROS 2.

Diretório:

```text
simulation/gazebo/
```

### MATLAB e Simulink

Modelos e funções para controle, simulação e acompanhamento de trajetória.

Diretório:

```text
control/matlab-simulink/
```

### Dashboard

Aplicação FastAPI para exibição e análise da telemetria do carrinho.

Entre as grandezas processadas estão:

- posição;
- velocidade;
- aceleração;
- desaceleração;
- raio de curvatura;
- trajetória.

Diretório:

```text
dashboard/
```

## Documentação

- [Arquitetura](docs/architecture.md)
- [Configuração e compilação](docs/build-and-run.md)
- [Validações](docs/validation.md)

## Tecnologias

- C
- Python
- MATLAB
- Simulink
- ESP-IDF
- Raspberry Pi Pico SDK
- micro-ROS
- ROS 2
- Gazebo
- FastAPI
- Plotly

## Contexto

Projeto desenvolvido na disciplina de Robótica I, explorando firmware embarcado, comunicação distribuída, controle de movimento, simulação e análise física de um robô móvel.
