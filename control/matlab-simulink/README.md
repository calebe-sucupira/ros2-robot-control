# Controle MATLAB e Simulink

Este módulo reúne os controladores e modelos utilizados na integração do carrinho com MATLAB, Simulink, ROS 2 e Gazebo.

## Conteúdo

- `control_turtlebot.m`: publicação de uma sequência de comandos no tópico `/cmd_vel`.
- `Controle_Trajetoria/`: funções e modelos de acompanhamento de trajetória.
- `Controllers.slx`: controladores utilizados pelos modelos principais.
- `simulinkCart_ROS_GAZEBO-pratica5.slx`: integração com ROS 2 e Gazebo.
- `simulinkCart_.slx`: modelo de simulação do carrinho.
- `dados.m`: definição dos parâmetros do modelo dinâmico.

## Arquivos gerados

O diretório `Controle_Trajetoria/+bus_conv_fcns` contém conversores de mensagens gerados pelo Simulink e pelo ROS Toolbox. Esses arquivos são mantidos porque os modelos podem depender deles.

## Componentes externos

`Controle_Trajetoria/sfun3d.m` e `Controle_Trajetoria/m3dscope_new.mdl` fornecem a visualização tridimensional utilizada nos modelos. O arquivo `sfun3d.m` identifica a MathWorks como detentora dos direitos autorais.

## Requisitos

- MATLAB
- Simulink
- Control System Toolbox
- ROS Toolbox
- ROS 2 configurado no ambiente de simulação
