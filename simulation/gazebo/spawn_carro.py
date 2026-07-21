from pathlib import Path

import rclpy
from gazebo_msgs.srv import SpawnEntity

ROBOT_NAME = "carrinho"
SPAWN_SERVICE = "/spawn_entity"
URDF_PATH = Path(__file__).with_name("carrinho.urdf")


def main(args=None) -> int:
    rclpy.init(args=args)
    node = rclpy.create_node("spawn_carrinho")

    try:
        if not URDF_PATH.is_file():
            node.get_logger().error(
                f"URDF não encontrado: {URDF_PATH}"
            )
            return 1

        client = node.create_client(
            SpawnEntity,
            SPAWN_SERVICE,
        )

        node.get_logger().info(
            f'Aguardando serviço "{SPAWN_SERVICE}"'
        )

        if not client.wait_for_service(timeout_sec=30.0):
            node.get_logger().error(
                "Serviço de spawn indisponível"
            )
            return 1

        request = SpawnEntity.Request()
        request.name = ROBOT_NAME
        request.xml = URDF_PATH.read_text(encoding="utf-8")
        request.robot_namespace = ""
        request.reference_frame = "world"

        future = client.call_async(request)

        rclpy.spin_until_future_complete(
            node,
            future,
            timeout_sec=15.0,
        )

        if not future.done():
            node.get_logger().error(
                "Tempo limite excedido durante o spawn"
            )
            return 1

        response = future.result()

        if response is None:
            node.get_logger().error(
                "O Gazebo não retornou uma resposta"
            )
            return 1

        if not response.success:
            node.get_logger().error(response.status_message)
            return 1

        node.get_logger().info(
            f'"{ROBOT_NAME}" inserido no Gazebo'
        )
        return 0
    except Exception as error:
        node.get_logger().error(f"Falha no spawn: {error}")
        return 1
    finally:
        node.destroy_node()

        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
