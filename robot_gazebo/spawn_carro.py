import os
import sys
import rclpy
from gazebo_msgs.srv import SpawnEntity

def main():
    # 1. Pega o caminho do arquivo URDF (Assumindo que está na mesma pasta)
    # Se der erro, coloque o caminho completo absoluto aqui embaixo:
    urdf_file_path = 'carrinho.urdf' 
    
    # Leitura do arquivo XML
    try:
        with open(urdf_file_path, 'r') as infp:
            robot_desc = infp.read()
    except FileNotFoundError:
        print(f"ERRO: Não achei o arquivo {urdf_file_path}. Rode o terminal na mesma pasta do arquivo!")
        return

    rclpy.init()
    node = rclpy.create_node('spawn_entity_node')
    
    # Cria o cliente para spawnar no Gazebo
    client = node.create_client(SpawnEntity, '/spawn_entity')
    
    print("Esperando serviço /spawn_entity...")
    while not client.wait_for_service(timeout_sec=1.0):
        print("Serviço não disponível, esperando...")

    # Configura o pedido de Spawn
    request = SpawnEntity.Request()
    request.name = 'carrinho'   # O nome crucial
    request.xml = robot_desc
    request.robot_namespace = ""
    request.initial_pose.position.x = 0.0
    request.initial_pose.position.y = 0.0
    request.initial_pose.position.z = 0.2 # Nasce um pouco no ar para não bugar no chão

    print("Enviando comando de Spawn...")
    future = client.call_async(request)
    rclpy.spin_until_future_complete(node, future)

    if future.result() is not None:
        print(f"Sucesso: {future.result().status_message}")
    else:
        print("Falha ao chamar serviço.")

    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
