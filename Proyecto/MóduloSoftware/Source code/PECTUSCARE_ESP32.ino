#include <WiFi.h>
#include <WebServer.h>
#include "HX711.h"

// ==========================================
// PECTUSCARE - CONFIGURACIÓN
// ==========================================

const char* ssid = "PECTUSCARE";
const char* password = "pectus123";

WebServer server(80);
HX711 scale;

// HX711: DT = D1, SCK = D0
#define HX711_DT D1
#define HX711_SCK D0

// ==========================================
// CALIBRACIÓN EXPERIMENTAL
// ==========================================

// Factor obtenido con la prueba de 5.2 kg
const float RAW_POR_NEWTON = 4405.4;

// Área efectiva: 67.5 cm² = 0.00675 m²
const float AREA_M2 = 0.00675;

// Conversión de Pa a PSI
const float PA_POR_PSI = 6894.757;

// ==========================================
// VARIABLES
// ==========================================

long valorCero = 0;
long rawValue = 0;
long deltaRaw = 0;

float fuerzaN = 0.0;
float presionPa = 0.0;
float presionKpa = 0.0;
float presionPsi = 0.0;

bool sensorConnected = false;

unsigned long ultimaLectura = 0;
const unsigned long intervaloLectura = 250;

// ==========================================
// LECTURA DEL SENSOR
// ==========================================

void actualizarSensor() {

  if (!scale.is_ready()) {
    sensorConnected = false;
    fuerzaN = 0;
    presionPa = 0;
    presionKpa = 0;
    presionPsi = 0;
    return;
  }

  sensorConnected = true;

  rawValue = scale.read_average(10);

  deltaRaw = rawValue - valorCero;

  // Magnitud de la diferencia respecto al cero
  float diferenciaAbsoluta = abs(deltaRaw);

  // Conversión experimental a Newtons
  fuerzaN = diferenciaAbsoluta / RAW_POR_NEWTON;

  // Presión en Pascales
  presionPa = fuerzaN / AREA_M2;

  // Conversión a kPa
  presionKpa = presionPa / 1000.0;

  // Conversión a PSI
  presionPsi = presionPa / PA_POR_PSI;

  Serial.print("RAW: ");
  Serial.print(rawValue);

  Serial.print(" | Diferencia: ");
  Serial.print(deltaRaw);

  Serial.print(" | Fuerza: ");
  Serial.print(fuerzaN, 4);
  Serial.print(" N");

  Serial.print(" | Presion: ");
  Serial.print(presionPsi, 6);
  Serial.println(" PSI");
}

// ==========================================
// RESPUESTA API
// ==========================================

void enviarEstado() {

  String json = "{";

  json += "\"rawValue\":" + String(rawValue) + ",";
  json += "\"zeroValue\":" + String(valorCero) + ",";
  json += "\"deltaRaw\":" + String(deltaRaw) + ",";

  json += "\"forceN\":" + String(fuerzaN, 4) + ",";
  json += "\"pressurePa\":" + String(presionPa, 3) + ",";
  json += "\"pressureKpa\":" + String(presionKpa, 5) + ",";
  json += "\"pressurePsi\":" + String(presionPsi, 6) + ",";

  json += "\"batteryPercent\":null,";
  json += "\"sensorConnected\":";
  json += sensorConnected ? "true" : "false";

  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ==========================================
// CONFIGURACIÓN
// ==========================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("       PECTUSCARE");
  Serial.println("================================");

  // Inicializar HX711
  scale.begin(HX711_DT, HX711_SCK);

  Serial.println("Esperando HX711...");

  while (!scale.is_ready()) {
    delay(500);
    Serial.println("Esperando sensor...");
  }

  Serial.println("HX711 conectado.");

  // IMPORTANTE:
  // El sensor debe estar completamente descargado
  // durante el encendido para establecer el cero.

  Serial.println("Estableciendo cero...");

  valorCero = scale.read_average(30);

  Serial.print("Valor cero: ");
  Serial.println(valorCero);

  // Crear punto de acceso Wi-Fi
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("Wi-Fi iniciado.");

  Serial.print("SSID: ");
  Serial.println(ssid);

  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // Endpoint
  server.on("/api/status", HTTP_GET, enviarEstado);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "PECTUSCARE ESP32 OK");
  });

  server.begin();

  Serial.println("Servidor HTTP iniciado.");
  Serial.println("PECTUSCARE LISTO.");
}

// ==========================================
// BUCLE PRINCIPAL
// ==========================================

void loop() {

  server.handleClient();

  if (millis() - ultimaLectura >= intervaloLectura) {

    ultimaLectura = millis();

    actualizarSensor();
  }
}
