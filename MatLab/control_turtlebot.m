% =======================================================
%         SCRIPT DE CONTROLE DO TURTLEBOT3
% =======================================================

% --- Lógica Principal do Script (Vem primeiro) ---
clear
clc

disp('Inicializando o nó MATLAB no ROS 2...');
ros2node = ros2node("/matlab_turtlebot_controller");

disp('Criando publicador para o tópico /cmd_vel...');
velPub = ros2publisher(ros2node, '/cmd_vel', 'geometry_msgs/Twist');
velMsg = ros2message(velPub);

send_velocity = @(linear, angular) set_vel(velPub, velMsg, linear, angular);

% ----- Sequência de Movimentos -----

disp('Movendo para frente por 2 segundos...');
send_velocity(0.2, 0.0);
pause(2);

disp('Girando para a direita por 2 segundos...');
send_velocity(0.0, -0.5);
pause(2);

disp('Parando o robô...');
send_velocity(0.0, 0.0);

clear velPub velMsg ros2node;

disp('Sequência de movimentos completa!');


% --- Definições de Funções Auxiliares (SEMPRE NO FINAL) ---

function set_vel(pub, msg, lin, ang)
    msg.linear.x = lin;
    msg.angular.z = ang;
    send(pub, msg);
end