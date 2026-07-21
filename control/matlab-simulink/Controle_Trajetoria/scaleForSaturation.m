function adjustedControl = scaleForSaturation( ...
    control, wheelDistance, controlHorizon, maximumWheelVelocity)

adjustedControl = zeros(2, controlHorizon);

for index = 1:controlHorizon
    linearVelocity = control(1, index);
    angularVelocity = control(2, index);

    leftWheelVelocity = ...
        linearVelocity + wheelDistance * angularVelocity / 2;

    rightWheelVelocity = ...
        linearVelocity - wheelDistance * angularVelocity / 2;

    maximumVelocity = max(leftWheelVelocity, rightWheelVelocity);
    minimumVelocity = min(leftWheelVelocity, rightWheelVelocity);

    if maximumVelocity > maximumWheelVelocity
        maximumScale = maximumVelocity / maximumWheelVelocity;
    else
        maximumScale = 1;
    end

    if minimumVelocity < -maximumWheelVelocity
        minimumScale = minimumVelocity / -maximumWheelVelocity;
    else
        minimumScale = 1;
    end

    scale = max(maximumScale, minimumScale);

    leftWheelVelocity = leftWheelVelocity / scale;
    rightWheelVelocity = rightWheelVelocity / scale;

    adjustedControl(1, index) = ...
        (leftWheelVelocity + rightWheelVelocity) / 2;

    adjustedControl(2, index) = ...
        (leftWheelVelocity - rightWheelVelocity) / wheelDistance;
end
end
