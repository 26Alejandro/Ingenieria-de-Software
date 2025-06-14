![image](https://github.com/user-attachments/assets/bc9e5733-b13d-4751-833e-f515b6903270)## 1. Descripción del Sistema

### Caso de Estudio
Sistema embebido de monitoreo de temperatura que controla automáticamente un ventilador basado en lecturas de sensores, con alarmas visuales y comunicación serie para monitoreo remoto.

### Objetivos del Sistema
- Monitorear temperatura ambiente continuamente
- Activar ventilador cuando la temperatura supere 28°C
- Proporcionar alarmas visuales (LED) para diferentes estados
- Comunicar estado del sistema vía UART
- Garantizar respuesta en tiempo real (< 100ms para actuación crítica)

---

## 2. Arquitectura del Sistema
### Diagrama de Bloques
![image](https://github.com/user-attachments/assets/d4ad7547-3c21-4dfe-bca2-24ce364a19a0)

### Componentes Principales
- **Módulo de Sensado**: DHT22/DS18B20 para lectura de temperatura
- **Módulo de Control**: Lógica de decisión y control de actuadores
- **Módulo de Comunicación**: Interface UART para debugging
- **Módulo de Actuación**: Control de ventilador y LEDs indicadores

---

## 3. Patrones Arquitectónicos Seleccionados

### 3.1. Producer-Consumer Pattern
**Justificación**: Desacoplar la adquisición de datos del procesamiento permite mejor gestión de recursos y timing predecible.

**Implementación**:
- **Producer**: Task de sensado que lee temperatura cada 500ms
- **Consumer**: Task de procesamiento que analiza datos y toma decisiones
- **Buffer**: Cola FreeRTOS para comunicación thread-safe

### 3.2. State Machine Pattern
**Justificación**: El sistema tiene estados bien definidos que requieren transiciones controladas.

**Estados Definidos**:
- **IDLE**: Temperatura normal (< 25°C) → LED Verde
- **WARNING**: Temperatura elevada (25-28°C) → LED Amarillo
- **CRITICAL**: Temperatura crítica (> 28°C) → LED Rojo + Ventilador ON
- **ERROR**: Fallo en sensor → LED Rojo parpadeante

### 3.3. Command Pattern
**Justificación**: Permite comandos remotos vía UART y facilita testing.

**Comandos Soportados**:
- `GET_TEMP`: Obtener temperatura actual
- `GET_STATUS`: Estado del sistema
- `SET_THRESHOLD`: Cambiar umbral de temperatura
- `FORCE_FAN`: Control manual del ventilador

---

## 4. Especificaciones de Temporización

### Requisitos de Tiempo Real
| Tarea            | Límite de Tiempo | Crítica |
|------------------|------------------|---------|
| Lectura sensor   | 500ms            | No      |
| Procesamiento    | < 10ms           | Semi    |
| Actuación        | < 100ms          | Sí      |
| Comunicación     | < 200ms          | No      |

### Prioridades de Tareas (FreeRTOS)
1. **TaskControl**: Prioridad 4 (más alta) - Control de actuadores
2. **TaskProcessing**: Prioridad 3 - Procesamiento de datos
3. **TaskSensing**: Prioridad 2 - Lectura de sensores
4. **TaskCommunication**: Prioridad 1 - Comunicación UART

---

## 5. Diagrama de Flujo del Sistema
![image](https://github.com/user-attachments/assets/004b22fd-6b26-46b0-9c54-a513e10dd123)

---

## 6. Recursos de Hardware Utilizados

### Microcontrolador: STM32F401RE
- **CPU**: ARM Cortex-M4 @ 84MHz
- **RAM**: 96KB
- **Flash**: 512KB
- **Periféricos**: UART2, GPIO, ADC, Timers

### Sensores y Actuadores
- **Sensor**: DS18B20 (1-Wire, precisión ±0.5°C)
- **Ventilador**: Controlado por PWM vía MOSFET
- **LEDs**: Verde (IDLE), Amarillo (WARNING), Rojo (CRITICAL/ERROR)
- **Comunicación**: UART2 a 115200 baud

### Pines Asignados
| Pin   | Función                          |
|-------|----------------------------------|
| PA5   | LED Verde (IDLE)                |
| PA6   | LED Amarillo (WARNING)          |
| PA7   | LED Rojo (CRITICAL/ERROR)       |
| PA8   | PWM Control Ventilador          |
| PA9   | UART2_TX (Debug)                |
| PA10  | UART2_RX (Comandos)             |
| PB0   | OneWire Data (Sensor DS18B20)   |

