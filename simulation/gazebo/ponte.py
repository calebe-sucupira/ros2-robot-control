from copy import deepcopy

import rclpy
from gazebo_msgs.srv import SetEntityState
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.qos import HistoryPolicy, QoSProfile, ReliabilityPolicy

ROBOT_NAME = "carrinho"
ODOMETRY_TOPIC = "/odom"
GAZEBO_SERVICE = "/gazebo/set_entity_state"
UPDATE_PERIOD_SECONDS = 0.05


class GazeboOdometryBridge(Node):
    def __init__(self):
        super().__init__("gazebo_odometry_bridge")

        qos_profile = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
        )

        self._client = self.create_client(
            SetEntityState,
            GAZEBO_SERVICE,
        )
        self._latest_pose = None
        self._pending_request = None
        self._last_service_warning_ns = 0

        self._subscription = self.create_subscription(
            Odometry,
            ODOMETRY_TOPIC,
            self._on_odometry,
            qos_profile,
        )

        self._timer = self.create_timer(
            UPDATE_PERIOD_SECONDS,
            self._update_gazebo,
        )

        self.get_logger().info(
            f'Aguardando odometria em "{ODOMETRY_TOPIC}"'
        )

    def _on_odometry(self, message: Odometry) -> None:
        self._latest_pose = deepcopy(message.pose.pose)

    def _update_gazebo(self) -> None:
        if self._latest_pose is None:
            return

        if not self._client.service_is_ready():
            self._warn_service_unavailable()
            return

        if (
            self._pending_request is not None
            and not self._pending_request.done()
        ):
            return

        request = SetEntityState.Request()
        request.state.name = ROBOT_NAME
        request.state.pose = deepcopy(self._latest_pose)
        request.state.reference_frame = "world"

        self._pending_request = self._client.call_async(request)
        self._pending_request.add_done_callback(
            self._on_update_complete
        )

    def _warn_service_unavailable(self) -> None:
        now_ns = self.get_clock().now().nanoseconds

        if now_ns - self._last_service_warning_ns >= 5_000_000_000:
            self.get_logger().warning(
                f'Serviço "{GAZEBO_SERVICE}" indisponível'
            )
            self._last_service_warning_ns = now_ns

    def _on_update_complete(self, future) -> None:
        try:
            response = future.result()

            if not response.success:
                self.get_logger().warning(response.status_message)
        except Exception as error:
            self.get_logger().error(
                f"Falha ao atualizar o Gazebo: {error}"
            )
        finally:
            self._pending_request = None


def main(args=None) -> None:
    rclpy.init(args=args)
    node = GazeboOdometryBridge()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()

        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
