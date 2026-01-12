import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from nav_msgs.msg import Odometry
from gazebo_msgs.srv import SetEntityState

class Ponte(Node):
    def __init__(self):
        super().__init__('ponte_gazebo')
        
        # --- CONFIGURAÇÕES ---
        self.robot_name = 'carrinho'  # Nome definido no seu URDF/Spawn
        topic_name = '/odom'          # Nome definido no seu código C do ESP32
        # ---------------------

        # CONFIGURAÇÃO DE QoS (CRUCIAL PARA MICRO-ROS)
        # Permite ler dados "Best Effort" que vêm do ESP32
        qos_profile = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=10
        )

        # Cliente para comandar o Gazebo
        self.client = self.create_client(SetEntityState, '/gazebo/set_entity_state')
        
        self.get_logger().info('--- INICIANDO PONTE ---')
        self.get_logger().info('Aguardando serviço do Gazebo...')
        while not self.client.wait_for_service(timeout_sec=2.0):
            self.get_logger().warn('Gazebo não encontrado. Verifique se a simulação está aberta.')

        # Assina o tópico do ESP32
        self.subscription = self.create_subscription(
            Odometry,
            topic_name,
            self.listener_callback,
            qos_profile) # <--- O segredo está aqui
            
        self.get_logger().info(f'Ponte conectada! Movendo "{self.robot_name}" conforme dados de {topic_name}')

    def listener_callback(self, msg):
        # Mostra no terminal para você saber que está funcionando
        self.get_logger().info(f'Atualizando Posição -> X: {msg.pose.pose.position.x:.2f} | Y: {msg.pose.pose.position.y:.2f}')

        req = SetEntityState.Request()
        req.state.name = self.robot_name
        req.state.pose = msg.pose.pose
        
        # Zera a física do Gazebo para não ter conflito (gravidade/atrito)
        req.state.twist.linear.x = 0.0
        req.state.twist.linear.y = 0.0
        req.state.twist.linear.z = 0.0
        req.state.twist.angular.x = 0.0
        req.state.twist.angular.y = 0.0
        req.state.twist.angular.z = 0.0
        
        # Envia o comando
        self.client.call_async(req)

def main(args=None):
    rclpy.init(args=args)
    node = Ponte()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
