/*
 * Proyecto: Calmdog - Dispositivo anti-estrés para perros
 * Plataforma: ESP32-WROVER-DEV
 * Autor: Asistente AI (optimizado para bajo consumo y prototipado rápido)
 * Fecha: 2025
 * Función: Detecta ruido fuerte > umbral, reproduce audio relajante desde SD.
 */

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include "driver/dac.h"

// Pines
#define PIN_MIC   34    // Entrada analógica del micrófono (KY-038 A0)
#define PIN_DAC   25    // Salida DAC para audio (PAM8403 L_IN)
#define PIN_SD_CS 5     // Chip Select de la tarjeta SD

// Parámetros de detección
#define CALIBRATION_TIME 5000  // Tiempo de calibración inicial (ms)
#define THRESHOLD_MULTIPLIER 2.5 // Umbral = promedio * multiplicador
#define TRIGGER_DURATION 2000  // Tiempo mínimo de ruido para activar (ms)
#define PLAYBACK_DURATION 30000 // Duración máxima de reproducción (30s)
#define VOLUME_LEVEL 180        // 0-255 (ajusta según parlante y amplificador)

// Variables globales
int umbralRuido = 0;
bool reproduciendo = false;
unsigned long ultimoRuido = 0;
unsigned long inicioReproduccion = 0;

// Función de calibración de umbral
void calibrarUmbral() {
  Serial.println("Calibrando umbral de ruido... mantén silencio.");
  delay(1000);
  
  long suma = 0;
  int muestras = 100;
  
  for (int i = 0; i < muestras; i++) {
    suma += analogRead(PIN_MIC);
    delay(10);
  }
  
  int promedio = suma / muestras;
  umbralRuido = (int)(promedio * THRESHOLD_MULTIPLIER);
  
  Serial.print("Umbral calibrado: ");
  Serial.println(umbralRuido);
}

// Función para reproducir WAV desde SD (8-bit mono)
void reproducirAudio() {
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("Error: No se pudo inicializar la tarjeta SD");
    return;
  }

  File archivo = SD.open("/calm.wav", FILE_READ);
  if (!archivo) {
    Serial.println("Error: No se encontró calm.wav");
    return;
  }

  // Leer cabecera WAV (44 bytes)
  uint8_t header[44];
  archivo.read(header, 44);

  // Validar archivo WAV (opcional, para robustez)
  if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F') {
    Serial.println("Error: Archivo no es WAV válido");
    archivo.close();
    return;
  }

  Serial.println("Reproduciendo audio...");

  dac_output_enable(DAC_CHANNEL_1); // GPIO25
  reproduciendo = true;
  inicioReproduccion = millis();

  // Buffer de lectura
  uint8_t buffer[512];
  int leidos;

  while ((leidos = archivo.read(buffer, sizeof(buffer))) > 0 && reproduciendo) {
    for (int i = 0; i < leidos; i++) {
      dac_output_voltage(DAC_CHANNEL_1, map(buffer[i], 0, 255, 0, VOLUME_LEVEL));
      delayMicroseconds(125); // ~8kHz: 125us entre muestras (ajusta según tasa)
    }

    // Verificar si se excede duración máxima
    if (millis() - inicioReproduccion > PLAYBACK_DURATION) {
      break;
    }

    // Opción: detener si se presiona botón (GPIO35 a GND)
    // if (digitalRead(35) == LOW) { reproduciendo = false; break; }
  }

  archivo.close();
  dac_output_voltage(DAC_CHANNEL_1, 0); // Silencio
  reproduciendo = false;
  Serial.println("Reproducción finalizada.");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_MIC, INPUT);
  dac_output_enable(DAC_CHANNEL_1);

  // Inicializar SD
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("Fallo al montar SD. Verifica conexión.");
    while (1) delay(1000); // Detener si no hay SD
  }

  Serial.println("SD montada correctamente.");

  // Calibrar umbral de ruido
  calibrarUmbral();

  Serial.println("Sistema listo. Esperando ruido fuerte...");
}

void loop() {
  if (reproduciendo) {
    // Si está reproduciendo, no detectar ruido
    if (millis() - inicioReproduccion > PLAYBACK_DURATION) {
      reproduciendo = false;
      Serial.println("Reproducción detenida por tiempo máximo.");
    }
    return;
  }

  int nivelSonido = analogRead(PIN_MIC);

  if (nivelSonido > umbralRuido) {
    if (ultimoRuido == 0) {
      ultimoRuido = millis(); // Marca inicio de ruido
    } else if (millis() - ultimoRuido > TRIGGER_DURATION) {
      // Ruido sostenido: activar reproducción
      Serial.println("¡Ruido detectado! Activando música relajante.");
      reproducirAudio();
      ultimoRuido = 0; // Resetear detección
    }
  } else {
    ultimoRuido = 0; // Resetear si el ruido cesa
  }

  delay(10); // Pequeña pausa para estabilidad
}