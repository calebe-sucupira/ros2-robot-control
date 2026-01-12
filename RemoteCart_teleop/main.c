#include <stdio.h>
#include <math.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>
#include <rmw_microros/rmw_microros.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/pwm.h"
#include "picow_udp_transports.h"

#define WIFI_SSID "Calebe"
#define WIFI_PASS "88111447"

#define ROS_AGENT_IP "10.42.0.1"
#define ROS_AGENT_PORT 8888

#define MOTOR_A_IN1 18   
#define MOTOR_A_IN2 19  
#define MOTOR_B_IN1 20  
#define MOTOR_B_IN2 16   
#define MOTOR_A_PWM 9  
#define MOTOR_B_PWM 8  

rcl_subscription_t subscriber;
geometry_msgs__msg__Twist msg;

uint motor_a_slice, motor_b_slice;
uint motor_a_channel, motor_b_channel;

void setup_motors_pwm() {
    gpio_init(MOTOR_A_IN1);
    gpio_init(MOTOR_A_IN2);
    gpio_init(MOTOR_B_IN1);
    gpio_init(MOTOR_B_IN2);
    
    gpio_set_dir(MOTOR_A_IN1, GPIO_OUT);
    gpio_set_dir(MOTOR_A_IN2, GPIO_OUT);
    gpio_set_dir(MOTOR_B_IN1, GPIO_OUT);
    gpio_set_dir(MOTOR_B_IN2, GPIO_OUT);
    
    gpio_set_function(MOTOR_A_PWM, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR_B_PWM, GPIO_FUNC_PWM);
    
    motor_a_slice = pwm_gpio_to_slice_num(MOTOR_A_PWM);
    motor_b_slice = pwm_gpio_to_slice_num(MOTOR_B_PWM);
    motor_a_channel = pwm_gpio_to_channel(MOTOR_A_PWM);
    motor_b_channel = pwm_gpio_to_channel(MOTOR_B_PWM);
    
    pwm_set_clkdiv(motor_a_slice, 25.0f);   // 125MHz / 25 = 5MHz
    pwm_set_wrap(motor_a_slice, 999);       // 5MHz / 1000 = 5kHz
    pwm_set_clkdiv(motor_b_slice, 25.0f);
    pwm_set_wrap(motor_b_slice, 999);
    

    pwm_set_enabled(motor_a_slice, true);
    pwm_set_enabled(motor_b_slice, true);
    
    gpio_put(MOTOR_A_IN1, 0);
    gpio_put(MOTOR_A_IN2, 0);
    gpio_put(MOTOR_B_IN1, 0);
    gpio_put(MOTOR_B_IN2, 0);
    pwm_set_chan_level(motor_a_slice, motor_a_channel, 0);
    pwm_set_chan_level(motor_b_slice, motor_b_channel, 0);
    
    printf("Motores PWM configurados para 5kHz (L298N)!\n");
}

// Controlar motor A com PWM (velocidade de -1.0 a 1.0)
void set_motor_a_pwm(float speed) {
    // Limita a velocidade entre -1 e 1
    if (speed > 1.0f) speed = 1.0f;
    if (speed < -1.0f) speed = -1.0f;
    
    uint16_t pwm_value = (uint16_t)(fabsf(speed) * 999.0f);
    
    if (speed > 0.01f) {
        // Para frente
        gpio_put(MOTOR_A_IN1, 1);
        gpio_put(MOTOR_A_IN2, 0);
        pwm_set_chan_level(motor_a_slice, motor_a_channel, pwm_value);
    } else if (speed < -0.01f) {
        // Para trás
        gpio_put(MOTOR_A_IN1, 0);
        gpio_put(MOTOR_A_IN2, 1);
        pwm_set_chan_level(motor_a_slice, motor_a_channel, pwm_value);
    } else {
        // Parado
        gpio_put(MOTOR_A_IN1, 0);
        gpio_put(MOTOR_A_IN2, 0);
        pwm_set_chan_level(motor_a_slice, motor_a_channel, 0);
    }
}

// Controlar motor B com PWM (velocidade de -1.0 a 1.0)
void set_motor_b_pwm(float speed) {
    // Limita a velocidade entre -1 e 1
    if (speed > 1.0f) speed = 1.0f;
    if (speed < -1.0f) speed = -1.0f;
    
    uint16_t pwm_value = (uint16_t)(fabsf(speed) * 999.0f);
    
    if (speed > 0.01f) {
        // Para frente 
        gpio_put(MOTOR_B_IN1, 0); 
        gpio_put(MOTOR_B_IN2, 1); 
        pwm_set_chan_level(motor_b_slice, motor_b_channel, pwm_value);
    } else if (speed < -0.01f) {
        // Para trás 
        gpio_put(MOTOR_B_IN1, 1); 
        gpio_put(MOTOR_B_IN2, 0); 
        pwm_set_chan_level(motor_b_slice, motor_b_channel, pwm_value);
    } else {
        // Parado
        gpio_put(MOTOR_B_IN1, 0);
        gpio_put(MOTOR_B_IN2, 0);
        pwm_set_chan_level(motor_b_slice, motor_b_channel, 0);
    }
}

// Função chamada quando recebe comandos do ROS
void subscription_callback(const void *msgin) {
    const geometry_msgs__msg__Twist *msg = (const geometry_msgs__msg__Twist *)msgin;
    
    float linear_x = msg->linear.x;  // Velocidade para frente/trás (-1 a 1)
    float angular_z = msg->angular.z; // Rotação (-1 a 1)
    
    printf("Comando: linear.x=%.2f, angular.z=%.2f\n", linear_x, angular_z);
    
    // Deadzone para evitar movimento com ruído
    float deadzone = 0.05f;
    if (fabsf(linear_x) < deadzone) linear_x = 0.0f;
    if (fabsf(angular_z) < deadzone) angular_z = 0.0f;
    
    // Controle diferencial dos motores
    float left_speed = linear_x - angular_z;
    float right_speed = linear_x + angular_z;
    
    // Limita as velocidades entre -1 e 1
    if (left_speed > 1.0f) left_speed = 1.0f;
    if (left_speed < -1.0f) left_speed = -1.0f;
    if (right_speed > 1.0f) right_speed = 1.0f;
    if (right_speed < -1.0f) right_speed = -1.0f;
    
    // Aplica PWM nos motores
    set_motor_a_pwm(left_speed);
    set_motor_b_pwm(right_speed);
    
    if (linear_x > 0.1f) {
        printf(">>> MOVENDO PARA FRENTE - PWM: %.0f%%\n", fabsf(linear_x) * 100);
    } else if (linear_x < -0.1f) {
        printf("<<< MOVENDO PARA TRÁS - PWM: %.0f%%\n", fabsf(linear_x) * 100);
    } else if (angular_z > 0.1f) {
        printf("<< GIRANDO ESQUERDA - PWM: %.0f%%\n", fabsf(angular_z) * 100);
    } else if (angular_z < -0.1f) {
        printf(">> GIRANDO DIREITA - PWM: %.0f%%\n", fabsf(angular_z) * 100);
    } else {
        printf("*** PARADO\n");
    }
}

int main() {
    stdio_init_all();
    
    // Configura motores com PWM
    setup_motors_pwm();
    printf("Sistema PWM inicializado!\n");
    
    // Inicializa Wi-Fi
    printf("Inicializando Wi-Fi...\n");
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar Wi-Fi\n");
        return 1;
    }
    
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s\n", WIFI_SSID);
    
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Erro ao conectar Wi-Fi\n");
        return 1;
    }
    printf("Conectado ao Wi-Fi!\n");
    
    // Configura transporte UDP para micro-ROS
    ST_PICOW_TRANSPORT_PARAMS params;
    params.ipaddr = (ip_addr_t){0};
    ipaddr_aton(ROS_AGENT_IP, &params.ipaddr);
    params.port = ROS_AGENT_PORT;
    
    rmw_uros_set_custom_transport(
        false, &params,
        picow_udp_transport_open,
        picow_udp_transport_close,
        picow_udp_transport_write,
        picow_udp_transport_read);
    
    // Tenta conectar ao agente micro-ROS
    printf("Procurando agente micro-ROS em %s:%d...\n", ROS_AGENT_IP, ROS_AGENT_PORT);
    const int timeout_ms = 1000;
    const uint8_t attempts = 30;
    
    if (rmw_uros_ping_agent(timeout_ms, attempts) != RCL_RET_OK) {
        printf("Agente micro-ROS não encontrado!\n");
        printf("Execute no PC: micro-ros-agent udp4 --port 8888\n");
    } else {
        printf("Conectado ao agente micro-ROS!\n");
    }
    
    // Inicializa micro-ROS
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;
    rcl_ret_t ret = rclc_support_init(&support, 0, NULL, &allocator);
    
    if (ret != RCL_RET_OK) {
        printf("Falha no suporte micro-ROS: %d\n", ret);
        return ret;
    }
    
    // Cria nó
    rcl_node_t node;
    rclc_node_init_default(&node, "pico_car_pwm", "", &support);
    
    // Cria subscriber para comandos Twist
    rclc_subscription_init_default(
        &subscriber, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        "cmd_vel");
    
    // Cria executor
    rclc_executor_t executor;
    rclc_executor_init(&executor, &support.context, 1, &allocator);
    rclc_executor_add_subscription(&executor, &subscriber, &msg, &subscription_callback, ON_NEW_DATA);
    
    printf("=== SISTEMA PWM PRONTO ===\n");
    printf("Aguardando comandos no tópico /cmd_vel\n");
    printf("Controles do teleop_twist_keyboard:\n");
    printf("  i = Frente | , = Trás | j = Esquerda | l = Direita | k = Parar\n");
    printf("  q/z = Aumentar/Diminuir velocidade máxima\n");
    
    // Loop principal
    while (true) {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
        sleep_ms(10);
    }
    
    return 0;
}