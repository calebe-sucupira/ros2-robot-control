# Validações

## Firmware ESP32

O firmware principal foi compilado com ESP-IDF e gerou o binário:

```text
esp32_robot_firmware.bin
```

A compilação incluiu o componente micro-ROS com transporte personalizado.

## Protótipo HTTP

O protótipo foi submetido a uma compilação limpa:

```bash
idf.py fullclean build
```

O firmware, o HTML incorporado e o servidor HTTP foram compilados e vinculados corretamente.

## Pico W

O protótipo foi compilado com Pico SDK e micro-ROS.

Artefato principal:

```text
pico_car_teleop.elf
```

A compilação final foi concluída sem warnings no código do projeto.

## Dashboard

Foram verificados:

- importação da aplicação FastAPI;
- execução sem `rclpy`;
- processamento das grandezas físicas;
- dependências registradas em `requirements.txt`.

## Gazebo

Foram verificados:

- sintaxe dos scripts Python;
- estrutura XML do URDF;
- estrutura XML do mundo;
- integridade das alterações com `git diff --check`.

## MATLAB

Os scripts autorais foram analisados com o Code Analyzer:

```matlab
checkcode("arquivo.m", "-id")
```

Os arquivos analisados não apresentaram ocorrências.

Os conversores gerados pelo Simulink e os componentes externos de visualização não foram modificados.
