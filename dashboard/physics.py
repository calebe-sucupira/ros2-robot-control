import numpy as np
import pandas as pd

MINIMUM_POINTS = 5
POSITION_WINDOW = 10
DERIVATIVE_WINDOW = 5
MAX_RADIUS_METERS = 50.0
MINIMUM_CURVATURE = 0.001


def empty_result() -> dict[str, list[float]]:
    return {
        "t": [],
        "x": [],
        "y": [],
        "xs": [],
        "ys": [],
        "v": [],
        "a": [],
        "deceleration": [],
        "R": [],
    }


def smooth(values: np.ndarray, window: int) -> np.ndarray:
    return (
        pd.Series(values)
        .rolling(window=window, min_periods=1, center=True)
        .mean()
        .to_numpy()
    )


def finite_list(values: np.ndarray) -> list[float]:
    sanitized = np.nan_to_num(
        values,
        nan=0.0,
        posinf=0.0,
        neginf=0.0,
    )
    return sanitized.astype(float).tolist()


def process_samples(
    samples: list[dict[str, float]],
) -> dict[str, list[float]]:
    if len(samples) < MINIMUM_POINTS:
        return empty_result()

    frame = (
        pd.DataFrame(samples)
        .replace([np.inf, -np.inf], np.nan)
        .dropna(subset=["t", "x", "y"])
        .drop_duplicates(subset="t", keep="last")
        .sort_values("t")
    )

    if len(frame) < MINIMUM_POINTS:
        return empty_result()

    time = frame["t"].to_numpy(dtype=float)
    x = frame["x"].to_numpy(dtype=float)
    y = frame["y"].to_numpy(dtype=float)

    x_smooth = smooth(x, POSITION_WINDOW)
    y_smooth = smooth(y, POSITION_WINDOW)

    velocity_x = np.gradient(x_smooth, time)
    velocity_y = np.gradient(y_smooth, time)

    speed = smooth(
        np.hypot(velocity_x, velocity_y),
        DERIVATIVE_WINDOW,
    )

    acceleration = smooth(
        np.gradient(speed, time),
        POSITION_WINDOW,
    )

    acceleration_x = np.gradient(velocity_x, time)
    acceleration_y = np.gradient(velocity_y, time)

    denominator = np.power(
        velocity_x**2 + velocity_y**2,
        1.5,
    )

    curvature = np.zeros_like(denominator)
    valid_velocity = denominator > 1e-9

    curvature[valid_velocity] = (
        np.abs(
            velocity_x[valid_velocity]
            * acceleration_y[valid_velocity]
            - velocity_y[valid_velocity]
            * acceleration_x[valid_velocity]
        )
        / denominator[valid_velocity]
    )

    radius = np.full_like(curvature, MAX_RADIUS_METERS)
    curved = curvature > MINIMUM_CURVATURE
    radius[curved] = np.minimum(
        1.0 / curvature[curved],
        MAX_RADIUS_METERS,
    )

    return {
        "t": finite_list(time),
        "x": finite_list(x),
        "y": finite_list(y),
        "xs": finite_list(x_smooth),
        "ys": finite_list(y_smooth),
        "v": finite_list(speed),
        "a": finite_list(acceleration),
        "deceleration": finite_list(
            np.maximum(-acceleration, 0.0)
        ),
        "R": finite_list(radius),
    }
