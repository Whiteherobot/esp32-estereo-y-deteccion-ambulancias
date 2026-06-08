/**
 * ESP32_CAM_Stereo.ino — Firmware de producción para visión estereoscópica
 * =========================================================================
 * Universidad Politécnica Salesiana — Visión Artificial (G8)
 * Autores: Michael Lata · Jordy Romero
 *
 * DECISIONES DE DISEÑO (justificación para rúbrica):
 *
 *   AEC/AGC/AWB = OFF (exposición y ganancia MANUALES):
 *     El algoritmo SGBM calcula disparidad comparando la intensidad de píxeles
 *     correspondientes en los dos frames L y R. Si cada cámara ajusta su propia
 *     exposición (AEC) de forma independiente, el mismo punto físico produce
 *     valores de píxel distintos en L y R, lo que introduce errores sistemáticos
 *     en el mapa de disparidad (fantasmas, huecos negros). Con valores fijos e
 *     iguales en ambas cámaras se garantiza paridad lumínica: SGBM recibe pares
 *     comparables y el mapa de disparidad mejora significativamente.
 *
 *   XCLK = 6 MHz (reducido desde 20 MHz por defecto):
 *     A mayor frecuencia de reloj, el sensor demanda más corriente y genera más
 *     calor, lo que aumenta el jitter en la transmisión WiFi. A 6 MHz el ESP32
 *     prioriza la integridad del paquete sobre la velocidad, reduciendo el
 *     desincronismo entre frames L y R que degrada la triangulación.
 *
 *   Endpoints disponibles:
 *     GET /            — página HTML de estado
 *     GET /ping        — devuelve versión del firmware (útil para check_esp32_streams.py)
 *     GET /capture     — captura un JPEG (usado por HttpCameraSource en C++)
 *     GET /stream      — MJPEG continuo (para visualización manual)
 *     GET /info        — JSON con todos los parámetros actuales
 *     GET /setexposure?v=N  — ajusta exposición en caliente (0–1200)
 *     GET /setgain?v=N      — ajusta ganancia en caliente (0–30)
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ====== CONFIG WIFI ======
// Cambiar CAM_ID según la cámara que se flashea: 'L' para izquierda, 'R' para derecha.
const char* ssid      = "Michi";
const char* password  = "michiawa";
const char  CAM_ID    = 'L';   // <-- cambiar a 'R' al flashear la cámara derecha

WebServer server(80);
const char* FW_VERSION = "ML_STEREO_v4_stereo_fixed";

// ====== PARÁMETROS DE CÁMARA PARA ESTÉREO ======
// AEC/AGC/AWB desactivados → exposición manual igual en ambas cámaras.
// Ajustar CAMERA_EXPOSURE según iluminación del entorno antes de calibrar.
constexpr int  CAMERA_XCLK_HZ   = 6000000;  // 6 MHz — reduce jitter WiFi
constexpr bool CAMERA_AEC        = false;    // AUTO-EXPOSICIÓN OFF → manual
constexpr bool CAMERA_AGC        = false;    // AUTO-GANANCIA OFF   → manual
constexpr bool CAMERA_AWB        = false;    // AUTO-BALANCE OFF    → color fijo
constexpr bool CAMERA_AWB_GAIN   = false;
constexpr int  CAMERA_EXPOSURE   = 400;      // 0–1200; ajustar por ambiente
constexpr int  CAMERA_GAIN       = 0;        // 0–30; mínimo = menos ruido
constexpr int  CAMERA_BRIGHTNESS = 0;        // neutro
constexpr int  CAMERA_SATURATION = 0;        // neutro

// Valores en caliente (modificables sin re-flash vía /setexposure y /setgain)
int g_exposure = CAMERA_EXPOSURE;
int g_gain     = CAMERA_GAIN;

// ====== PINOUT AI Thinker ESP32-CAM ======
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ---------------------------------------------------------------------------
// Aplicar parámetros de sensor (llamado al init y en caliente)
// ---------------------------------------------------------------------------
static void applySensorSettings(int exposure, int gain) {
  sensor_t* s = esp_camera_sensor_get();
  if (!s) return;

  s->set_vflip(s, 1);
  s->set_hmirror(s, 0);

  // Controles automáticos: todos OFF para estéreo
  s->set_exposure_ctrl(s, CAMERA_AEC  ? 1 : 0);
  s->set_gain_ctrl    (s, CAMERA_AGC  ? 1 : 0);
  s->set_whitebal     (s, CAMERA_AWB  ? 1 : 0);
  s->set_awb_gain     (s, CAMERA_AWB_GAIN ? 1 : 0);
  s->set_aec2         (s, 0);   // night mode OFF

  // Valores fijos manuales
  s->set_aec_value(s, exposure);
  s->set_agc_gain (s, gain);
  s->set_brightness(s, CAMERA_BRIGHTNESS);
  s->set_saturation(s, CAMERA_SATURATION);
}

// ---------------------------------------------------------------------------
// Handlers HTTP
// ---------------------------------------------------------------------------

void handlePing() {
  server.send(200, "text/plain", FW_VERSION);
}

void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  // Respuesta HTTP manual — evita headers duplicados de WebServer
  WiFiClient client = server.client();
  client.printf("HTTP/1.1 200 OK\r\n");
  client.printf("Content-Type: image/jpeg\r\n");
  client.printf("Content-Length: %u\r\n", fb->len);
  client.printf("Connection: close\r\n\r\n");
  client.write(fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

void handleStream() {
  WiFiClient client = server.client();
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: ");
  client.print(STREAM_CONTENT_TYPE);
  client.print("\r\n\r\n");

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { delay(10); continue; }

    char partBuf[64];
    size_t hlen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);

    bool ok = (client.write((const uint8_t*)partBuf, hlen) > 0)
           && (client.write(fb->buf, fb->len) > 0)
           && (client.write((const uint8_t*)STREAM_BOUNDARY,
                             strlen(STREAM_BOUNDARY)) > 0);
    esp_camera_fb_return(fb);
    if (!ok) break;
    delay(1);
  }
}

void handleInfo() {
  // Devuelve JSON con todos los parámetros actuales — útil para documentación
  char buf[512];
  snprintf(buf, sizeof(buf),
    "{"
      "\"fw\":\"%s\","
      "\"xclk_hz\":%d,"
      "\"aec\":%s,"
      "\"agc\":%s,"
      "\"awb\":%s,"
      "\"exposure\":%d,"
      "\"gain\":%d,"
      "\"brightness\":%d,"
      "\"saturation\":%d,"
      "\"frame_size\":\"VGA\","
      "\"psram\":%s"
    "}",
    FW_VERSION,
    CAMERA_XCLK_HZ,
    CAMERA_AEC  ? "true" : "false",
    CAMERA_AGC  ? "true" : "false",
    CAMERA_AWB  ? "true" : "false",
    g_exposure, g_gain,
    CAMERA_BRIGHTNESS, CAMERA_SATURATION,
    psramFound() ? "true" : "false"
  );
  server.send(200, "application/json", buf);
}

void handleSetExposure() {
  if (!server.hasArg("v")) {
    server.send(400, "text/plain", "Falta parametro ?v=0..1200");
    return;
  }
  int v = server.arg("v").toInt();
  v = constrain(v, 0, 1200);
  g_exposure = v;
  applySensorSettings(g_exposure, g_gain);
  char msg[64];
  snprintf(msg, sizeof(msg), "exposure=%d OK", g_exposure);
  Serial.printf("[SET] exposure=%d\n", g_exposure);
  server.send(200, "text/plain", msg);
}

void handleSetGain() {
  if (!server.hasArg("v")) {
    server.send(400, "text/plain", "Falta parametro ?v=0..30");
    return;
  }
  int v = server.arg("v").toInt();
  v = constrain(v, 0, 30);
  g_gain = v;
  applySensorSettings(g_exposure, g_gain);
  char msg[64];
  snprintf(msg, sizeof(msg), "gain=%d OK", g_gain);
  Serial.printf("[SET] gain=%d\n", g_gain);
  server.send(200, "text/plain", msg);
}

void handleRoot() {
  String html;
  html += "<html><head><meta charset='utf-8'>";
  html += "<title>ESP32-CAM Stereo</title></head><body>";
  html += "<h2>ESP32-CAM — Firmware Estéreo</h2>";
  html += "<p><b>FW:</b> "; html += FW_VERSION; html += "</p>";
  html += "<p><b>IP:</b> "; html += WiFi.localIP().toString(); html += "</p>";
  html += "<p><b>Exposure:</b> "; html += g_exposure; html += " / 1200</p>";
  html += "<p><b>Gain:</b> "; html += g_gain; html += " / 30</p>";
  html += "<hr>";
  html += "<p><a href='/capture'>Captura JPEG (/capture)</a></p>";
  html += "<p><a href='/stream'>Stream MJPEG (/stream)</a></p>";
  html += "<p><a href='/info'>Info JSON (/info)</a></p>";
  html += "<p><a href='/setexposure?v=200'>/setexposure?v=200</a></p>";
  html += "<p><a href='/setgain?v=0'>/setgain?v=0</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ---------------------------------------------------------------------------
// Inicialización de cámara
// ---------------------------------------------------------------------------
bool initCamera() {
  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sccb_sda  = SIOD_GPIO_NUM;
  config.pin_sccb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = CAMERA_XCLK_HZ;
  config.pixel_format  = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;   // 640×480 — balance calidad/latencia
    config.jpeg_quality = 10;              // 0=mejor, 63=peor; 10 es buena calidad
    config.fb_count     = 2;              // double-buffer: reduce latencia de captura
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[FAIL] esp_camera_init: 0x%x\n", err);
    return false;
  }

  applySensorSettings(g_exposure, g_gain);
  return true;
}

// ---------------------------------------------------------------------------
// Setup y Loop
// ---------------------------------------------------------------------------
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // desactiva detección de brownout
  Serial.begin(115200);
  delay(1000);

  if (!initCamera()) {
    Serial.println("[FAIL] No se pudo iniciar camara. Reiniciando...");
    delay(1500);
    ESP.restart();
  }

  // --- Conectar al hotspot "Michi" ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Conectando a '%s'", ssid);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 60) {
    delay(500);
    Serial.print('.');
    ++retries;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[FAIL] WiFi no conectado. Reiniciando en 3s...");
    delay(3000);
    ESP.restart();
  }

  server.on("/",            HTTP_GET, handleRoot);
  server.on("/ping",        HTTP_GET, handlePing);
  server.on("/capture",     HTTP_GET, handleCapture);
  server.on("/stream",      HTTP_GET, handleStream);
  server.on("/info",        HTTP_GET, handleInfo);
  server.on("/setexposure", HTTP_GET, handleSetExposure);
  server.on("/setgain",     HTTP_GET, handleSetGain);
  server.begin();

  Serial.println("--- LISTO ---");
  Serial.printf("FW      : %s\n", FW_VERSION);
  Serial.printf("CAM_ID  : %c\n", CAM_ID);
  Serial.printf("AEC/AGC/AWB : %s/%s/%s\n",
    CAMERA_AEC ? "ON" : "OFF",
    CAMERA_AGC ? "ON" : "OFF",
    CAMERA_AWB ? "ON" : "OFF");
  Serial.printf("Exposure: %d  Gain: %d\n", g_exposure, g_gain);
  Serial.print("URL     : http://"); Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
}
