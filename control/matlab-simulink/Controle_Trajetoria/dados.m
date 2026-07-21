clearvars
close all
clc

n = 1;
kt = 10.913e-3;
r = 1.5506;
je = 9.356e-3;

np = n * kt / (r * je);
dd = kt^2 / (r * je);
dp = [1, dd];

f = tf(np, dp);
