#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/sockets.h"

// Micro-ROS headers
#include <uxr/client/profile/transport/custom/custom_transport.h>
#include <rmw_microros/rmw_microros.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <geometry_msgs/msg/twist.h>
#include <nav_msgs/msg/odometry.h>
#include <rosidl_runtime_c/string_functions.h>

extern rmw_ret_t rmw_uros_set_custom_transport(
    bool framing, void *args,
    bool (*open)(struct uxrCustomTransport *),
    bool (*close)(struct uxrCustomTransport *),
    size_t (*write)(struct uxrCustomTransport *, const uint8_t *, size_t, uint8_t *),
    size_t (*read)(struct uxrCustomTransport *, uint8_t *, size_t, int, uint8_t *));

// ============================================
// CONFIGURAÇÕES
// ============================================
#define AGENT_IP "10.243.102.224" 
#define AGENT_PORT 8888
#define WIFI_SSID CONFIG_ESP_WIFI_SSID
#define WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define WIFI_CONNECTED_BIT BIT0

#define MOTOR_ESQ_PWM 25
#define MOTOR_ESQ_IN3 27
#define MOTOR_ESQ_IN4 26
#define MOTOR_DIR_PWM 13
#define MOTOR_DIR_IN1 12
#define MOTOR_DIR_IN2 14
#define ENCODER_ESQ 33
#define ENCODER_DIR 32

#define D_WHEEL 0.065f      // Diâmetro da roda (metros)
#define L_TRACK 0.135f      // Distância entre as rodas (metros)
#define TICKS_PER_REV 300.0f // Resolução do encoder

// Conversão de Ticks <-> Metros
const float METROS_POR_TICK = (M_PI * D_WHEEL) / TICKS_PER_REV;
const float TICKS_PER_METER = 1.0f / ((M_PI * D_WHEEL) / TICKS_PER_REV);

static const char *TAG = "ROBOT_CORE";
static EventGroupHandle_t s_wifi_event_group;

// Variáveis Globais de Estado (Volatile pois são usadas em interrupções/tasks diferentes)
volatile double pos_x = 0, pos_y = 0, pos_theta = 0;
volatile int32_t g_ticks_esq = 0, g_ticks_dir = 0;
double pwr_esq_global = 0, pwr_dir_global = 0;

typedef struct
{
    double setpoint, current_vel, error_sum;
} PID_t;

PID_t pidEsq = {0}, pidDir = {0};

// Ganhos do Controlador
const float KP = 0.10f;          // Ganho Proporcional
const float KI = 0.02f;          // Ganho Integral
const float FILTER_ALPHA = 0.90f; // Filtro passa-baixa para suavizar leitura de velocidade

const float FF_ESQ = 0.085f; // FeedForward Esquerda (
const float FF_DIR = 0.075f; // FeedForward Direita

const float SYNC_GAIN = 0.05f; // Ganho para sincronizar as duas rodas
const float OFFSET_BASE = 7.0f; // Zona morta 
const float MAX_I_TERM = 8.0f;  // Anti-windup do integrador

int64_t last_cmd_time = 0;
#define CMD_TIMEOUT_US 200000 // 200ms de timeout para segurança

// ============================================
// TRANSPORTE UDP 
// ============================================
int sock_fd = -1;
bool esp32_udp_open(struct uxrCustomTransport *t)
{
    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    return (sock_fd >= 0);
}
bool esp32_udp_close(struct uxrCustomTransport *t)
{
    if (sock_fd >= 0) { close(sock_fd); sock_fd = -1; }
    return true;
}
size_t esp32_udp_write(struct uxrCustomTransport *t, const uint8_t *b, size_t l, uint8_t *e)
{
    struct sockaddr_in d = {.sin_addr.s_addr = inet_addr(AGENT_IP), .sin_family = AF_INET, .sin_port = htons(AGENT_PORT)};
    int s = sendto(sock_fd, b, l, 0, (struct sockaddr *)&d, sizeof(d));
    return (s >= 0) ? s : 0;
}
size_t esp32_udp_read(struct uxrCustomTransport *t, uint8_t *b, size_t l, int to, uint8_t *e)
{
    struct timeval tv = {.tv_sec = to / 1000, .tv_usec = (to % 1000) * 1000};
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int r = recvfrom(sock_fd, b, l, 0, NULL, NULL);
    return (r >= 0) ? r : 0;
}

// ============================================
// MOTORES 
// ============================================
void set_motor_esq(double power, double setpoint)
{
    if (setpoint == 0 && fabs(power) < 1.0)
    {
        gpio_set_level(MOTOR_ESQ_IN3, 0);
        gpio_set_level(MOTOR_ESQ_IN4, 0);
        power = 0;
    }
    else
    {
        if (power > 0)
        {
            power += OFFSET_BASE;
            gpio_set_level(MOTOR_ESQ_IN3, 0);
            gpio_set_level(MOTOR_ESQ_IN4, 1);
        }
        else
        {
            power -= OFFSET_BASE;
            gpio_set_level(MOTOR_ESQ_IN3, 1);
            gpio_set_level(MOTOR_ESQ_IN4, 0);
        }
    }
    double abs_pwr = fmin(fabs(power), 90.0); // Limita PWM em 90%
    uint32_t duty = 1023 - (uint32_t)(1023 * abs_pwr / 100.0); // Lógica invertida no PWM
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void set_motor_dir(double power, double setpoint)
{
    if (setpoint == 0 && fabs(power) < 1.0)
    {
        gpio_set_level(MOTOR_DIR_IN1, 0);
        gpio_set_level(MOTOR_DIR_IN2, 0);
        power = 0;
    }
    else
    {
        if (power > 0)
        {
            power += OFFSET_BASE;
            gpio_set_level(MOTOR_DIR_IN1, 1);
            gpio_set_level(MOTOR_DIR_IN2, 0);
        }
        else
        {
            power -= OFFSET_BASE;
            gpio_set_level(MOTOR_DIR_IN1, 0);
            gpio_set_level(MOTOR_DIR_IN2, 1);
        }
    }
    double abs_pwr = fmin(fabs(power), 90.0);
    uint32_t duty = 1023 - (uint32_t)(1023 * abs_pwr / 100.0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// ============================================
// CONTROLE E ODOMETRIA
// ============================================
static void IRAM_ATTR encoder_isr(void *arg)
{
    if ((uint32_t)arg == ENCODER_ESQ)
        g_ticks_esq++;
    else
        g_ticks_dir++;
}

void control_task(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int32_t last_e = 0, last_d = 0;

    while (1)
    {
        if (esp_timer_get_time() - last_cmd_time > CMD_TIMEOUT_US)
        {
            pidEsq.setpoint = 0;
            pidDir.setpoint = 0;
            pidEsq.error_sum = 0;
            pidDir.error_sum = 0;
        }

        int32_t cur_e = g_ticks_esq;
        int32_t cur_d = g_ticks_dir;

        // Calcula delta ticks
        int32_t de_ticks = cur_e - last_e;
        int32_t dd_ticks = cur_d - last_d;

        last_e = cur_e;
        last_d = cur_d;

        // Converte ticks para metros
        float de_m = (float)de_ticks * METROS_POR_TICK;
        float dd_m = (float)dd_ticks * METROS_POR_TICK;
        
        // Correção de sentido
        if (pwr_esq_global < 0) de_m = -de_m;
        if (pwr_dir_global < 0) dd_m = -dd_m;

        // --- CÁLCULO DE ODOMETRIA ---
        float dist_centro = (de_m + dd_m) / 2.0f; // Distância média percorrida
        float delta_theta = (dd_m - de_m) / L_TRACK; // Variação angular

        // Atualiza posição global
        pos_x += dist_centro * cos(pos_theta + delta_theta / 2.0f);
        pos_y += dist_centro * sin(pos_theta + delta_theta / 2.0f);
        pos_theta += delta_theta;

        // --- CÁLCULO DE VELOCIDADE ATUAL ---
        double ve_raw = (double)de_ticks * 20.0; 
        double vd_raw = (double)dd_ticks * 20.0;
        
        // Filtro Exponencial para reduzir ruído na leitura de velocidade
        pidEsq.current_vel = (pidEsq.current_vel * FILTER_ALPHA) + (ve_raw * (1.0 - FILTER_ALPHA));
        pidDir.current_vel = (pidDir.current_vel * FILTER_ALPHA) + (vd_raw * (1.0 - FILTER_ALPHA));

        // Sync: Tenta manter motores alinhados se o erro relativo for alto
        double sync_error = (double)(cur_d - cur_e);
        double sync_corr = fmax(fmin(sync_error * SYNC_GAIN, 4.0), -4.0);

        // --- CÁLCULO PID + FEEDFORWARD ---
        double pwr_e = 0, pwr_d = 0;
        
        // Motor Esquerdo
        if (fabs(pidEsq.setpoint) > 0.01)
        {
            double err_e = pidEsq.setpoint - pidEsq.current_vel;
            pidEsq.error_sum = fmax(fmin(pidEsq.error_sum + err_e * 0.05, MAX_I_TERM), -MAX_I_TERM); // Integrador com limite
            // FeedForward + Proporcional + Integral + Sincronia
            pwr_e = (pidEsq.setpoint * FF_ESQ) + (KP * err_e) + (KI * pidEsq.error_sum) + sync_corr;
        }
        
        // Motor Direito
        if (fabs(pidDir.setpoint) > 0.01)
        {
            double err_d = pidDir.setpoint - pidDir.current_vel;
            pidDir.error_sum = fmax(fmin(pidDir.error_sum + err_d * 0.05, MAX_I_TERM), -MAX_I_TERM);
            // Sincronia subtraída aqui (ação diferencial)
            pwr_d = (pidDir.setpoint * FF_DIR) + (KP * err_d) + (KI * pidDir.error_sum) - sync_corr;
        }

        // Aplica nos motores
        set_motor_esq(pwr_e, pidEsq.setpoint);
        set_motor_dir(pwr_d, pidDir.setpoint);
        pwr_esq_global = pwr_e;
        pwr_dir_global = pwr_d;

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50)); 
    }
}

// ============================================
// MICRO-ROS (Callback de Velocidade)
// ============================================
void cmd_vel_callback(const void *msvgin)
{
    const geometry_msgs__msg__Twist *msg = (const geometry_msgs__msg__Twist *)msvgin;
    last_cmd_time = esp_timer_get_time(); // Reseta watchdog
    
    // Cinemática Inversa: Converte Twist (v, w) para velocidade de cada roda
    pidEsq.setpoint = (msg->linear.x - (msg->angular.z * L_TRACK / 2.0f)) * TICKS_PER_METER;
    pidDir.setpoint = (msg->linear.x + (msg->angular.z * L_TRACK / 2.0f)) * TICKS_PER_METER;
}

// Task Micro-ROS (Gerencia conexão e tópicos)
void micro_ros_task(void *arg)
{
    // Aguarda Wi-Fi conectar antes de iniciar ROS
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    
    // Configura transporte UDP
    rmw_uros_set_custom_transport(false, NULL, esp32_udp_open, esp32_udp_close, esp32_udp_write, esp32_udp_read);
    
    // Tenta pingar o Agent até conseguir
    while (rmw_uros_ping_agent(100, 1) != RCL_RET_OK)
        vTaskDelay(pdMS_TO_TICKS(1000));

    // Inicialização padrão do ROS 2 (Node, Executor, Support)
    rcl_allocator_t alloc = rcl_get_default_allocator();
    rclc_support_t support;
    rclc_support_init(&support, 0, NULL, &alloc);
    rcl_node_t node;
    rclc_node_init_default(&node, "esp32_robot", "", &support);

    // Configura Publisher de Odometria (/odom)
    nav_msgs__msg__Odometry odom_msg;
    memset(&odom_msg, 0, sizeof(nav_msgs__msg__Odometry));
    rosidl_runtime_c__String__assign(&odom_msg.header.frame_id, "odom");
    rosidl_runtime_c__String__assign(&odom_msg.child_frame_id, "base_link");
    rcl_publisher_t odom_pub;
    rclc_publisher_init_default(&odom_pub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry), "odom");

    // Configura Subscriber de Velocidade (/cmd_vel)
    rcl_subscription_t sub;
    geometry_msgs__msg__Twist tw_msg;
    rclc_subscription_init_default(&sub, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "cmd_vel");
    rclc_executor_t exec;
    rclc_executor_init(&exec, &support.context, 1, &alloc);
    rclc_executor_add_subscription(&exec, &sub, &tw_msg, &cmd_vel_callback, ON_NEW_DATA);

    // Loop principal do ROS
    while (1)
    {
        // Preenche mensagem de odometria com dados calculados na outra task
        int64_t t = esp_timer_get_time();
        odom_msg.header.stamp.sec = t / 1000000;
        odom_msg.header.stamp.nanosec = (t % 1000000) * 1000;
        odom_msg.pose.pose.position.x = pos_x;
        odom_msg.pose.pose.position.y = pos_y;
        
        // Conversão Euler -> Quaternion (simplificada para 2D)
        odom_msg.pose.pose.orientation.z = sin(pos_theta / 2.0);
        odom_msg.pose.pose.orientation.w = cos(pos_theta / 2.0);
        
        (void)rcl_publish(&odom_pub, &odom_msg, NULL);
        
        // Processa callbacks (checa se chegou cmd_vel)
        rclc_executor_spin_some(&exec, RCL_MS_TO_NS(10));
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == WIFI_EVENT_STA_START)
        esp_wifi_connect();
    else if (id == IP_EVENT_STA_GOT_IP)
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    else if (id == WIFI_EVENT_STA_DISCONNECTED)
        esp_wifi_connect();
}

void app_main(void)
{
    nvs_flash_init();
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    wifi_config_t wifi_cfg = {.sta = {.ssid = WIFI_SSID, .password = WIFI_PASS}};
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();

    // Configuração de GPIOs 
    gpio_config_t io_m = {.mode = GPIO_MODE_OUTPUT, .pin_bit_mask = (1ULL << 12) | (1ULL << 14) | (1ULL << 26) | (1ULL << 27)};
    gpio_config(&io_m);
    gpio_config_t io_e = {.mode = GPIO_MODE_INPUT, .pin_bit_mask = (1ULL << 32) | (1ULL << 33), .intr_type = GPIO_INTR_POSEDGE, .pull_up_en = 1};
    gpio_config(&io_e);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ENCODER_ESQ, encoder_isr, (void *)ENCODER_ESQ);
    gpio_isr_handler_add(ENCODER_DIR, encoder_isr, (void *)ENCODER_DIR);

    // Configuração PWM 
    ledc_timer_config_t lt = {.speed_mode = LEDC_LOW_SPEED_MODE, .timer_num = LEDC_TIMER_0, .duty_resolution = LEDC_TIMER_10_BIT, .freq_hz = 200, .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&lt);
    ledc_channel_config_t c0 = {.channel = LEDC_CHANNEL_0, .gpio_num = MOTOR_DIR_PWM, .speed_mode = LEDC_LOW_SPEED_MODE, .timer_sel = LEDC_TIMER_0, .duty = 1023};
    ledc_channel_config(&c0);
    ledc_channel_config_t c1 = {.channel = LEDC_CHANNEL_1, .gpio_num = MOTOR_ESQ_PWM, .speed_mode = LEDC_LOW_SPEED_MODE, .timer_sel = LEDC_TIMER_0, .duty = 1023};
    ledc_channel_config(&c1);

    xTaskCreatePinnedToCore(control_task, "PID_CTRL", 4096, NULL, 10, NULL, 1); // Core 1 (Cálculo)
    xTaskCreatePinnedToCore(micro_ros_task, "ROS_TASK", 10240, NULL, 5, NULL, 0); // Core 0 (Comunicação)
}