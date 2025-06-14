

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

/* Definiciones y constantes */
#define TEMP_QUEUE_SIZE         5
#define CONTROL_QUEUE_SIZE      3
#define TEMP_THRESHOLD_WARNING  25.0f
#define TEMP_THRESHOLD_CRITICAL 28.0f
#define SENSOR_READ_PERIOD      500   // ms
#define PROCESSING_PERIOD       100   // ms
#define CONTROL_PERIOD          50    // ms
#define COMM_PERIOD            1000   // ms

/* Estados del sistema */
typedef enum {
    STATE_IDLE = 0,
    STATE_WARNING,
    STATE_CRITICAL,
    STATE_ERROR
} SystemState_t;

/* Estructura de datos del sensor */
typedef struct {
    float temperature;
    uint32_t timestamp;
    uint8_t sensor_status;
} SensorData_t;

/* Comandos de control */
typedef enum {
    CMD_UPDATE_LEDS = 0,
    CMD_CONTROL_FAN,
    CMD_SET_STATE
} ControlCommand_t;

typedef struct {
    ControlCommand_t command;
    SystemState_t new_state;
    uint8_t fan_speed;  // 0-100%
} ControlMessage_t;

/* Variables globales */
QueueHandle_t xTempQueue;
QueueHandle_t xControlQueue;
SemaphoreHandle_t xUartMutex;

SystemState_t currentState = STATE_IDLE;
float currentTemperature = 20.0f;
uint8_t fanSpeed = 0;

/* Handles de periféricos */
UART_HandleTypeDef huart2;
TIM_HandleTypeDef htim3;

/* Prototipos de funciones */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_Init(void);

/* Tareas FreeRTOS */
void TaskSensing(void *pvParameters);
void TaskProcessing(void *pvParameters);
void TaskControl(void *pvParameters);
void TaskCommunication(void *pvParameters);

/* Funciones auxiliares */
float ReadTemperatureSensor(void);
void UpdateLEDs(SystemState_t state);
void ControlFan(uint8_t speed);
void SendUARTMessage(const char* message);
SystemState_t DetermineState(float temperature);

/**
 * @brief Función principal
 */
int main(void)
{
    /* Inicialización del sistema */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM3_Init();
    
    /* Inicio del timer para PWM */
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    
    /* Creación de colas FreeRTOS */
    xTempQueue = xQueueCreate(TEMP_QUEUE_SIZE, sizeof(SensorData_t));
    xControlQueue = xQueueCreate(CONTROL_QUEUE_SIZE, sizeof(ControlMessage_t));
    xUartMutex = xSemaphoreCreateMutex();
    
    /* Verificación de creación de colas */
    if(xTempQueue == NULL || xControlQueue == NULL || xUartMutex == NULL) {
        Error_Handler();
    }
    
    /* Creación de tareas con prioridades específicas */
    xTaskCreate(TaskSensing, "SensorTask", 256, NULL, 2, NULL);
    xTaskCreate(TaskProcessing, "ProcessTask", 256, NULL, 3, NULL);
    xTaskCreate(TaskControl, "ControlTask", 256, NULL, 4, NULL);
    xTaskCreate(TaskCommunication, "CommTask", 512, NULL, 1, NULL);
    
    /* Mensaje de inicio */
    SendUARTMessage("Sistema de Monitoreo iniciado\r\n");
    
    /* Inicio del scheduler */
    vTaskStartScheduler();
    
    /* No debería llegar aquí */
    while(1) {
        Error_Handler();
    }
}

/**
 * @brief Tarea de sensado - Lee temperatura cada 500ms
 */
void TaskSensing(void *pvParameters)
{
    SensorData_t sensorData;
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(SENSOR_READ_PERIOD);
    
    xLastWakeTime = xTaskGetTickCount();
    
    while(1) {
        /* Lectura del sensor */
        sensorData.temperature = ReadTemperatureSensor();
        sensorData.timestamp = HAL_GetTick();
        sensorData.sensor_status = (sensorData.temperature > -50.0f && 
                                   sensorData.temperature < 100.0f) ? 1 : 0;
        
        /* Envío a cola de procesamiento */
        if(xQueueSend(xTempQueue, &sensorData, pdMS_TO_TICKS(10)) != pdPASS) {
            /* Cola llena - dato perdido */
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET); // LED Error
            vTaskDelay(pdMS_TO_TICKS(100));
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Tarea de procesamiento - Analiza datos y determina acciones
 */
void TaskProcessing(void *pvParameters)
{
    SensorData_t receivedData;
    ControlMessage_t controlMsg;
    SystemState_t newState;
    
    while(1) {
        /* Esperar datos del sensor */
        if(xQueueReceive(xTempQueue, &receivedData, pdMS_TO_TICKS(200)) == pdPASS) {
            
            if(receivedData.sensor_status == 1) {
                /* Sensor funcionando correctamente */
                currentTemperature = receivedData.temperature;
                newState = DetermineState(currentTemperature);
                
                /* Enviar comando de cambio de estado si es necesario */
                if(newState != currentState) {
                    controlMsg.command = CMD_SET_STATE;
                    controlMsg.new_state = newState;
                    controlMsg.fan_speed = (newState == STATE_CRITICAL) ? 100 : 0;
                    
                    xQueueSend(xControlQueue, &controlMsg, pdMS_TO_TICKS(50));
                }
            } else {
                /* Error en sensor */
                controlMsg.command = CMD_SET_STATE;
                controlMsg.new_state = STATE_ERROR;
                controlMsg.fan_speed = 0;
                xQueueSend(xControlQueue, &controlMsg, pdMS_TO_TICKS(50));
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(PROCESSING_PERIOD));
    }
}

/**
 * @brief Tarea de control - Maneja actuadores con máxima prioridad
 */
void TaskControl(void *pvParameters)
{
    ControlMessage_t controlMsg;
    
    while(1) {
        /* Procesar comandos de control */
        if(xQueueReceive(xControlQueue, &controlMsg, pdMS_TO_TICKS(100)) == pdPASS) {
            
            switch(controlMsg.command) {
                case CMD_SET_STATE:
                    currentState = controlMsg.new_state;
                    UpdateLEDs(currentState);
                    ControlFan(controlMsg.fan_speed);
                    fanSpeed = controlMsg.fan_speed;
                    break;
                    
                case CMD_CONTROL_FAN:
                    ControlFan(controlMsg.fan_speed);
                    fanSpeed = controlMsg.fan_speed;
                    break;
                    
                case CMD_UPDATE_LEDS:
                    UpdateLEDs(currentState);
                    break;
                    
                default:
                    break;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(CONTROL_PERIOD));
    }
}

/**
 * @brief Tarea de comunicación - Maneja UART y comandos remotos
 */
void TaskCommunication(void *pvParameters)
{
    char txBuffer[128];
    uint8_t rxBuffer[32];
    
    while(1) {
        /* Enviar estado del sistema */
        if(xSemaphoreTake(xUartMutex, pdMS_TO_TICKS(100)) == pdPASS) {
            
            snprintf(txBuffer, sizeof(txBuffer), 
                    "Estado: %d, Temp: %.1f°C, Fan: %d%%, Time: %lu\r\n",
                    currentState, currentTemperature, fanSpeed, HAL_GetTick());
            
            HAL_UART_Transmit(&huart2, (uint8_t*)txBuffer, strlen(txBuffer), 100);
            xSemaphoreGive(xUartMutex);
        }
        
        /* Verificar comandos entrantes (implementación básica) */
        if(HAL_UART_Receive(&huart2, rxBuffer, 1, 10) == HAL_OK) {
            if(rxBuffer[0] == 'F') {
                /* Comando para forzar ventilador */
                ControlMessage_t msg = {CMD_CONTROL_FAN, STATE_IDLE, 50};
                xQueueSend(xControlQueue, &msg, 0);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(COMM_PERIOD));
    }
}

/**
 * @brief Simula lectura de sensor de temperatura
 * @return Temperatura en grados Celsius
 */
float ReadTemperatureSensor(void)
{
    /* Simulación: lectura basada en potenciómetro o valor fijo + ruido */
    static float baseTemp = 24.0f;
    static int direction = 1;
    
    /* Simulación de variación de temperatura */
    baseTemp += (direction * 0.1f);
    if(baseTemp > 30.0f) direction = -1;
    if(baseTemp < 20.0f) direction = 1;
    
    return baseTemp;
}

/**
 * @brief Actualiza LEDs según el estado del sistema
 */
void UpdateLEDs(SystemState_t state)
{
    /* Apagar todos los LEDs */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // Verde
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET); // Amarillo
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET); // Rojo
    
    switch(state) {
        case STATE_IDLE:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // Verde
            break;
        case STATE_WARNING:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET); // Amarillo
            break;
        case STATE_CRITICAL:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET); // Rojo
            break;
        case STATE_ERROR:
            /* LED rojo parpadeante manejado por otra tarea */
            break;
    }
}

/**
 * @brief Controla la velocidad del ventilador
 */
void ControlFan(uint8_t speed)
{
    uint32_t pulse = (htim3.Init.Period * speed) / 100;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
}

/**
 * @brief Envía mensaje por UART de forma thread-safe
 */
void SendUARTMessage(const char* message)
{
    if(xSemaphoreTake(xUartMutex, pdMS_TO_TICKS(100)) == pdPASS) {
        HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), 100);
        xSemaphoreGive(xUartMutex);
    }
}

/**
 * @brief Determina el estado del sistema basado en temperatura
 */
SystemState_t DetermineState(float temperature)
{
    if(temperature < TEMP_THRESHOLD_WARNING) {
        return STATE_IDLE;
    } else if(temperature < TEMP_THRESHOLD_CRITICAL) {
        return STATE_WARNING;
    } else {
        return STATE_CRITICAL;
    }
}

/* Configuraciones de periféricos generadas por STM32CubeMX */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
    
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void MX_TIM3_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 83;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 999;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim3);
    
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig);
    HAL_TIM_PWM_Init(&htim3);
    
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);
    
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);
    
    GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while(1) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_7);
        HAL_Delay(200);
    }
}
