function angle = DiffAngle(angle1, angle2)
angle = mod(angle1 - angle2 + pi, 2 * pi) - pi;

if angle <= -pi
    angle = pi;
end
end
