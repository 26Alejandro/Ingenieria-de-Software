# Desarrollo de Proyectos de Software Embebido a Pequeña Escala

## Resumen

Esta guía proporciona un enfoque detallado para el desarrollo de sistemas embebidos a pequeña escala, enfocándose en microcontroladores con recursos limitados, donde la eficiencia, fiabilidad y predictibilidad son esenciales. Se cubren aspectos desde el diseño arquitectónico, uso de RTOS como FreeRTOS, hasta prácticas modernas de control de versiones en GitHub.

---

## 1. Introducción al Desarrollo Embebido a Pequeña Escala

### 1.1 ¿Qué es un Proyecto Embebido a Pequeña Escala?

Se refiere a proyectos que operan sobre microcontroladores con recursos limitados (RAM, Flash, CPU). Suelen ser aplicaciones simples y específicas como:

- Sistemas de monitoreo
- Controladores de actuadores
- Interfaces de comunicación

Estos sistemas demandan eficiencia extrema y diseño optimizado desde el inicio. El uso de un RTOS como FreeRTOS o Zephyr es común por su bajo consumo de memoria y comportamiento determinista.

### 1.2 Ciclo de Vida del Desarrollo

A diferencia del modelo en cascada, el desarrollo embebido es iterativo:

1. Análisis de requisitos
2. Diseño arquitectónico
3. Implementación (con RTOS)
4. Pruebas
5. Documentación

Cada fase retroalimenta a la anterior, con especial énfasis en pruebas tempranas y continuas, esenciales por la cercanía al hardware.

---

## 2. Diseño del Sistema y Planificación Arquitectónica

### 2.1 Definición del Caso y Requisitos

Se debe definir claramente el problema, las entradas (sensores), salidas (actuadores) y comportamientos esperados. Los requisitos bien definidos facilitan la validación objetiva mediante pruebas funcionales y temporales.

### 2.2 Modelado de la Arquitectura

Se recomienda el uso de diagramas de bloques y diagramas de flujo para representar:

- Adquisición de datos
- Procesamiento
- Control de actuadores

Esto simplifica el diseño y la comunicación dentro del equipo de desarrollo.

### 2.3 Patrones Arquitectónicos

Se aplican patrones como:

- **Productor-Consumidor**: separa tareas con velocidades distintas de operación.
- **Máquina de Estados**: ideal para lógica basada en eventos.
- **Arquitectura en Capas**: separa responsabilidades y mejora la mantenibilidad.

### 2.4 Herramientas de Hardware y Software

**Hardware:** STM32, Arduino, Raspberry Pi  
**IDE:** STM32CubeIDE, Arduino IDE  
**RTOS:** FreeRTOS, Zephyr  
**Análisis de tiempo:** Osciloscopios, Tracealyzer

Una buena integración entre herramientas mejora la productividad y depuración.

---

## 3. Integración de RTOS y Gestión de Tareas

### 3.1 Configuración del Entorno

La configuración correcta del archivo `FreeRTOSConfig.h` define aspectos críticos como:

- Asignación de memoria (`heap_1.c`, `heap_4.c`, etc.)
- Tipo de temporizador (`TickType_t`)
- Características habilitadas (colas, tareas, semáforos)

### 3.2 Diseño Orientado a Tareas

Se estructura el sistema como tareas concurrentes gestionadas por el RTOS. Cada tarea debe:

- Ejecutarse indefinidamente
- Tener su propia pila
- Ser gestionada con `xTaskCreate()`

### 3.3 Comunicación entre Tareas

FreeRTOS ofrece:

- **Colas (`xQueueSend`, `xQueueReceive`)**
- **Semáforos y mutexes** (con soporte para herencia de prioridad)
- **Notificaciones a tareas (`xTaskNotify`)**

Elegir la API adecuada (normal o `FromISR`) es clave para evitar errores en tiempo real.

---

## 4. Implementación y Estructura del Repositorio

### 4.1 Mejores Prácticas en C/C++

- Minimizar memoria dinámica
- Evitar salidas de tareas
- Seguir estándares como MISRA C
- Programación defensiva

Esto asegura confiabilidad bajo restricciones de recursos.

### 4.2 Organización Modular del Código

Propuesta de estructura:
/src
/app # lógica del sistema
/drivers # controladores de hardware
/rtos # FreeRTOS y configuración
/hal # abstracción de hardware


Separar claramente facilita el mantenimiento y la reutilización.

### 4.3 Git y GitHub

**Buenas prácticas:**

- `.gitignore` bien configurado
- `README.md` completo
- Commits claros
- Uso de ramas (`feature`, `develop`, `main`)

GitHub permite colaborar, versionar y documentar de forma eficiente.

---

## 5. Análisis de Temporización y Validación

### 5.1 Puntos de Medición

Usar pines GPIO o macros para marcar tiempo en secciones críticas, especialmente en tareas sensibles al tiempo.

### 5.2 Herramientas

- **Osciloscopios y analizadores lógicos**: para señales hardware
- **Tracealyzer**: para monitoreo de tareas, colas y CPU

Combinarlas permite una visión completa del comportamiento del sistema.

### 5.3 Optimización

Ajustar:

- **Prioridades de tareas**
- **Algoritmos**
- **Duración de ejecución**

Evita problemas como la inversión de prioridades.

---

## 6. Pruebas y Aseguramiento de la Calidad

### 6.1 Pruebas Funcionales y de Temporización

Validar que el sistema:

- Se comporta correctamente (funcional)
- Cumple sus plazos (temporal)

Probar bajo condiciones reales y de estrés es clave.

### 6.2 Pruebas Unitarias e Integración

Adoptar "Shift-Left Testing":

- Probar módulos (unitarias) desde el inicio
- Verificar interfaces (integración)

Detectar errores temprano ahorra tiempo y mejora la calidad del producto final.

---

## Conclusiones

Los proyectos embebidos a pequeña escala no son simples; requieren:

- Planificación rigurosa
- Uso estratégico de patrones de diseño
- Configuración cuidadosa del RTOS
- Código eficiente y seguro
- Validación con herramientas avanzadas

Además, adoptar buenas prácticas de gestión con Git/GitHub permite escalar el desarrollo, mejorar la colaboración y mantener la trazabilidad del proyecto.

---
