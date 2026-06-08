#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "soc/soc.h"           
#include "soc/rtc_cntl_reg.h"  

const char* ssid = "Michi";
const char* password = "michiawa";

WebServer server(80);

// ====== AJUSTES DE ESTABILIDAD PARA CAPTURA ESTEREO ======
constexpr int CAMERA_XCLK_HZ = 6000000;
constexpr bool CAMERA_AEC = false;
constexpr bool CAMERA_AGC = false;
constexpr bool CAMERA_AWB = false;
constexpr bool CAMERA_AWB_GAIN = false;
constexpr int CAMERA_EXPOSURE = 120;
constexpr int CAMERA_GAIN = 4;
constexpr int CAMERA_BRIGHTNESS = 1;
constexpr int CAMERA_SATURATION = -1;

// Pines AI Thinker (Verifica que no haya cables sueltos en estos pines)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

void handleStream() {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  WiFiClient client = server.client();
  // Enviamos la cabecera manualmente para asegurar que el navegador la reciba
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: ");
  client.print(_STREAM_CONTENT_TYPE);
  client.print("\r\n\r\n");

  while (true) {
    if (!client.connected()) break;

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Fallo en captura");
      break;
    }

    size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
    if(client.write((const char *)part_buf, hlen) <= 0) {
       esp_camera_fb_return(fb);
       break;
    }
    if(client.write((const char *)fb->buf, fb->len) <= 0) {
       esp_camera_fb_return(fb);
       break;
    }
    if(client.write(_STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY)) <= 0) {
       esp_camera_fb_return(fb);
       break;
    }

    esp_camera_fb_return(fb);
    delay(1); 
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = CAMERA_XCLK_HZ;
  config.pixel_format = PIXFORMAT_JPEG;

  // Forzamos baja resolución para asegurar que abra
  config.frame_size = FRAMESIZE_QVGA; 
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Fallo Iniciando Cámara");
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
  s->set_exposure_ctrl(s, CAMERA_AEC ? 1 : 0);
  s->set_gain_ctrl(s, CAMERA_AGC ? 1 : 0);
  s->set_whitebal(s, CAMERA_AWB ? 1 : 0);
  s->set_awb_gain(s, CAMERA_AWB_GAIN ? 1 : 0);
  s->set_aec2(s, 0);
  s->set_aec_value(s, CAMERA_EXPOSURE);
  s->set_agc_gain(s, CAMERA_GAIN);
  s->set_brightness(s, CAMERA_BRIGHTNESS);
  s->set_saturation(s, CAMERA_SATURATION);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

  // Ruta directa. Entra a http://TU_IP/ para ver el stream
  server.on("/", HTTP_GET, handleStream);
  server.begin();
  
  Serial.println("\n--- LISTO ---");
  Serial.print("Entra a: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
}
