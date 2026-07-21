function control_turtlebot()
node = ros2node("/matlab_turtlebot_controller");
publisher = ros2publisher(node, "/cmd_vel", "geometry_msgs/Twist");
message = ros2message(publisher);

cleanup = onCleanup( ...
    @() sendVelocity(publisher, message, 0.0, 0.0));

disp("Movendo para frente por 2 segundos...")
sendVelocity(publisher, message, 0.2, 0.0);
pause(2)

disp("Girando para a direita por 2 segundos...")
sendVelocity(publisher, message, 0.0, -0.5);
pause(2)

disp("Parando o robô...")
sendVelocity(publisher, message, 0.0, 0.0);

disp("Sequência de movimentos concluída.")
end

function sendVelocity(publisher, message, linearVelocity, angularVelocity)
message.linear.x = linearVelocity;
message.angular.z = angularVelocity;
send(publisher, message);
end
