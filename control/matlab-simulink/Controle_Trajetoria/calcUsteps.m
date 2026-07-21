function dU = calcUsteps(U, Nu, delta)
dU = zeros(2, Nu * 4);

for index = 1:Nu
    column = 4 * (index - 1);

    dU(:, column + 1) = [
        U(1, index) + delta
        U(2, index)
    ];

    dU(:, column + 2) = [
        U(1, index) - delta
        U(2, index)
    ];

    dU(:, column + 3) = [
        U(1, index)
        U(2, index) + delta
    ];

    dU(:, column + 4) = [
        U(1, index)
        U(2, index) - delta
    ];
end
end
