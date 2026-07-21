import logging
from collections import deque
from threading import Event, Lock
from typing import Deque

LOGGER = logging.getLogger(__name__)

try:
    import rclpy
    from nav_msgs.msg import Odometry
    from rclpy.node import Node
except ModuleNotFoundError:
    ROS_AVAILABLE = False
else:
    ROS_AVAILABLE = True


class TelemetryBuffer:
    def __init__(self, max_points: int):
        self._samples: Deque[dict[str, float]] = deque(maxlen=max_points)
        self._lock = Lock()
        self._started_at: float | None = None
        self._last_timestamp: float | None = None

    def append(self, timestamp: float, x: float, y: float) -> None:
        with self._lock:
            if (
                self._last_timestamp is not None
                and timestamp < self._last_timestamp
            ):
                self._samples.clear()
                self._started_at = None

            if self._started_at is None:
                self._started_at = timestamp

            self._last_timestamp = timestamp
            self._samples.append(
                {
                    "t": max(timestamp - self._started_at, 0.0),
                    "x": x,
                    "y": y,
                }
            )

    def snapshot(self) -> list[dict[str, float]]:
        with self._lock:
            return list(self._samples)


if ROS_AVAILABLE:
    class OdomListener(Node):
        def __init__(self, telemetry: TelemetryBuffer):
            super().__init__("interface_fisica_web_listener")
            self._telemetry = telemetry
            self._subscription = self.create_subscription(
                Odometry,
                "/odom",
                self._listener_callback,
                10,
            )

        def _listener_callback(self, message: Odometry) -> None:
            timestamp = (
                message.header.stamp.sec
                + message.header.stamp.nanosec * 1e-9
            )

            if timestamp == 0.0:
                timestamp = (
                    self.get_clock().now().nanoseconds * 1e-9
                )

            self._telemetry.append(
                timestamp=timestamp,
                x=float(message.pose.pose.position.x),
                y=float(message.pose.pose.position.y),
            )


def run_ros(
    telemetry: TelemetryBuffer,
    stop_event: Event,
) -> None:
    if not ROS_AVAILABLE:
        LOGGER.warning("ROS 2 indisponível; listener não iniciado")
        return

    node = None

    try:
        rclpy.init()
        node = OdomListener(telemetry)

        while rclpy.ok() and not stop_event.is_set():
            rclpy.spin_once(node, timeout_sec=0.2)
    except Exception:
        LOGGER.exception("Falha no listener de odometria")
    finally:
        if node is not None:
            node.destroy_node()

        if rclpy.ok():
            rclpy.shutdown()
