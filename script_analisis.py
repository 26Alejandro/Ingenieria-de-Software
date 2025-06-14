#!/usr/bin/env python3
"""
timing_analysis.py - Script para análisis de temporización del sistema embebido
Autor: Laboratorio de Software Embebido

Este script analiza los datos de temporización del sistema para validar
que cumple con los requerimientos de tiempo real.
"""

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from datetime import datetime
import serial
import time
import re

class TimingAnalyzer:
    def __init__(self, port='/dev/ttyACM0', baudrate=115200):
        """
        Inicializa el analizador de temporización
        
        Args:
            port: Puerto serie del microcontrolador
            baudrate: Velocidad de comunicación
        """
        self.port = port
        self.baudrate = baudrate
        self.data = []
        self.requirements = {
            'sensor_reading': 500,      # ms - período de lectura
            'processing_time': 10,      # ms - tiempo máximo de procesamiento
            'actuation_delay': 100,     # ms - tiempo máximo de actuación
            'communication_period': 1000 # ms - período de comunicación
        }
        
    def connect_serial(self):
        """Establece conexión serie con el microcontrolador"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"Conectado a {self.port} a {self.baudrate} baud")
            time.sleep(2)  # Esperar inicialización
            return True
        except Exception as e:
            print(f"Error conectando al puerto serie: {e}")
            return False
    
    def parse_system_message(self, message):
        """
        Parsea mensajes del sistema embebido
        
        Formato esperado: "Estado: X, Temp: XX.X°C, Fan: XX%, Time: XXXXX"
        """
        pattern = r'Estado: (\d+), Temp: ([\d.]+)°C, Fan: (\d+)%, Time: (\d+)'
        match = re.match(pattern, message.strip())
        
        if match:
            return {
                'timestamp': int(match.group(4)),
                'state': int(match.group(1)),
                'temperature': float(match.group(2)),
                'fan_speed': int(match.group(3)),
                'system_time': datetime.now()
            }
        return None
    
    def collect_data(self, duration_seconds=60):
        """
        Recolecta datos del sistema durante un período específico
        
        Args:
            duration_seconds: Duración de la recolección en segundos
        """
        if not hasattr(self, 'ser'):
            if not self.connect_serial():
                return False
        
        print(f"Recolectando datos por {duration_seconds} segundos...")
        start_time = time.time()
        
        while (time.time() - start_time) < duration_seconds:
            try:
                if self.ser.in_waiting > 0:
                    line = self.ser.readline().decode('utf-8', errors='ignore')
                    parsed_data = self.parse_system_message(line)
                    
                    if parsed_data:
                        self.data.append(parsed_data)
                        print(f"Datos recibidos: Estado={parsed_data['state']}, "
                              f"Temp={parsed_data['temperature']:.1f}°C, "
                              f"Fan={parsed_data['fan_speed']}%")
                        
            except Exception as e:
                print(f"Error leyendo datos: {e}")
                continue
        
        print(f"Recolección completada. {len(self.data)} muestras obtenidas.")
        return len(self.data) > 0
    
    def analyze_timing_performance(self):
        """Analiza el rendimiento temporal del sistema"""
        if not self.data:
            print("No hay datos para analizar")
            return None
        
        df = pd.DataFrame(self.data)
        
        # Calcular intervalos entre muestras
        df['interval'] = df['timestamp'].diff()
        
        # Análisis estadístico
        analysis = {
            'total_samples': len(df),
            'communication_intervals': {
                'mean': df['interval'].mean(),
                'std': df['interval'].std(),
                'min': df['interval'].min(),
                'max': df['interval'].max(),
                'target': self.requirements['communication_period']
            },
            'state_transitions': df['state'].value_counts().to_dict(),
            'temperature_range': {
                'min': df['temperature'].min(),
                'max': df['temperature'].max(),
                'mean': df['temperature'].mean(),
                'std': df['temperature'].std()
            }
        }
        
        # Verificar cumplimiento de requisitos
        analysis['requirements_check'] = {
            'communication_timing': abs(analysis['communication_intervals']['mean'] - 
                                      self.requirements['communication_period']) < 100,
            'jitter_acceptable': analysis['communication_intervals']['std'] < 50,
            'no_missed_deadlines': analysis['communication_intervals']['max'] < 1500
        }
        
        return analysis
    
    def generate_timing_report(self, analysis):
        """Genera reporte de análisis de temporización"""
        if not analysis:
            return "No se pudo generar el reporte - datos insuficientes"
        
        report = f"""
REPORTE DE ANÁLISIS DE TEMPORIZACIÓN
====================================
Fecha: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

ESTADÍSTICAS GENERALES:
- Muestras totales: {analysis['total_samples']}
- Duración de prueba: {analysis['total_samples']} segundos aprox.

ANÁLISIS DE COMUNICACIÓN:
- Intervalo promedio: {analysis['communication_intervals']['mean']:.1f} ms
- Desviación estándar: {analysis['communication_intervals']['std']:.1f} ms
- Intervalo mínimo: {analysis['communication_intervals']['min']:.1f} ms
- Intervalo máximo: {analysis['communication_intervals']['max']:.1f} ms
- Objetivo: {analysis['communication_intervals']['target']} ms

DISTRIBUCIÓN DE ESTADOS:
"""
        for state, count in analysis['state_transitions'].items():
            state_names = {0: 'IDLE', 1: 'WARNING', 2: 'CRITICAL', 3: 'ERROR'}
            report += f"- {state_names.get(state, f'Estado {state}')}: {count} muestras\n"
        
        report += f"""
ANÁLISIS DE TEMPERATURA:
- Temperatura mínima: {analysis['temperature_range']['min']:.1f}°C
- Temperatura máxima: {analysis['temperature_range']['max']:.1f}°C
- Temperatura promedio: {analysis['temperature_range']['mean']:.1f}°C
- Desviación estándar: {analysis['temperature_range']['std']:.1f}°C

VERIFICACIÓN DE REQUISITOS:
"""
        for req, passed in analysis['requirements_check'].items():
            status = "✅ CUMPLE" if passed else "❌ NO CUMPLE"
            report += f"- {req}: {status}\n"
        
        return report
    
    def plot_timing_data(self, save_path=None):
        """Genera gráficas de análisis temporal"""
        if not self.data:
            print("No hay datos para graficar")
            return
        
        df = pd.DataFrame(self.data)
        
        # Crear subplots
        fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 10))
        fig.suptitle('Análisis de Temporización del Sistema Embebido', fontsize=16)
        
        # Gráfica 1: Temperatura vs Tiempo
        ax1.plot(df.index, df['temperature'], 'b-', linewidth=1)
        ax1.axhline(y=25, color='y', linestyle='--', label='Umbral Warning')
        ax1.axhline(y=28, color='r', linestyle='--', label='Umbral Critical')
        ax1.set_xlabel('Muestra')
        ax1.set_ylabel('Temperatura (°C)')
        ax1.set_title('Evolución de Temperatura')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # Gráfica 2: Estados del Sistema
        state_counts = df['state'].value_counts().sort_index()
        state_names = ['IDLE', 'WARNING', 'CRITICAL', 'ERROR']
        colors = ['green', 'yellow', 'red', 'orange']
        ax2.bar(range(len(state_counts)), state_counts.values, color=colors[:len(state_counts)])
        ax2.set_xlabel('Estado del Sistema')
        ax2.set_ylabel('Número de Muestras')
        ax2.set_title('Distribución de Estados')
        ax2.set_xticks(range(len(state_counts)))
        ax2.set_xticklabels([state_names[i] for i in state_counts.index])
        
        # Gráfica 3: Intervalos de Comunicación
        df['interval'] = df['timestamp'].diff()
        ax3.plot(df.index[1:], df['interval'][1:], 'g-', linewidth=1)
        ax3.axhline(y=1000, color='r', linestyle='--', label='Objetivo (1000ms)')
        ax3.set_xlabel('Muestra')
        ax3.set_ylabel('Intervalo (ms)')
        ax3.set_title('Intervalos de Comunicación')
        ax3.legend()
        ax3.grid(True, alpha=0.3)
        
        # Gráfica 4: Histograma de Jitter
        jitter = df['interval'][1:] - 1000  # Desviación del objetivo
        ax4.hist(jitter, bins=20, alpha=0.7, color='purple', edgecolor='black')
        ax4.axvline(x=0, color='r', linestyle='--', label='Objetivo')
        ax4.set_xlabel('Jitter (ms)')
        ax4.set_ylabel('Frecuencia')
        ax4.set_title('Distribución de Jitter')
        ax4.legend()
        
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Gráficas guardadas en: {save_path}")
        
        plt.show()
    
    def run_complete_analysis(self, duration=60, save_plots=True):
        """Ejecuta análisis completo del sistema"""
        print("Iniciando análisis completo del sistema...")
        
        # Recolectar datos
        if not self.collect_data(duration):
            print("Error: No se pudieron recolectar datos")
            return None
        
        # Analizar rendimiento
        analysis = self.analyze_timing_performance()
        
        # Generar reporte
        report = self.generate_timing_report(analysis)
        print(report)
        
        # Guardar reporte
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        report_filename = f'timing_report_{timestamp}.txt'
        with open(report_filename, 'w') as f:
            f.write(report)
        print(f"Reporte guardado en: {report_filename}")
        
        # Generar gráficas
        if save_plots:
            plot_filename = f'timing_analysis_{timestamp}.png'
            self.plot_timing_data(plot_filename)
        
        return analysis

def main():
    """Función principal para ejecutar análisis desde línea de comandos"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Análisis de temporización del sistema embebido')
    parser.add_argument('--port', default='/dev/ttyACM0', help='Puerto serie del microcontrolador')
    parser.add_argument('--baudrate', type=int, default=115200, help='Velocidad de comunicación')
    parser.add_argument('--duration', type=int, default=60, help='Duración de análisis en segundos')
    parser.add_argument('--no-plots', action='store_true', help='No generar gráficas')
    
    args = parser.parse_args()
    
    # Crear analizador
    analyzer = TimingAnalyzer(args.port, args.baudrate)
    
    # Ejecutar análisis
    try:
        analyzer.run_complete_analysis(
            duration=args.duration,
            save_plots=not args.no_plots
        )
    except KeyboardInterrupt:
        print("\nAnálisis interrumpido por el usuario")
    except Exception as e:
        print(f"Error durante análisis: {e}")
    finally:
        if hasattr(analyzer, 'ser'):
            analyzer.ser.close()
            print("Conexión serie cerrada")

if __name__ == "__main__":
    main()
