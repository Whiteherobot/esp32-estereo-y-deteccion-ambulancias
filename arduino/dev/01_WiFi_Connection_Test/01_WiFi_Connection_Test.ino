// 01_WiFi_Connection_Test.ino
// Prueba de conexión WiFi sin cámara

#include <WiFi.h>

const char* ssid = "Michi";
const char* password = "michiawa";

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println("Conectando a WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print('.');
    tries++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[OK] WiFi conectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("[FAIL] No conectó a WiFi. Revisa SSID/PASSWORD o cobertura.");
  }
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last > 3000) {
    last = millis();
    Serial.printf("WiFi status=%d, IP=%s\n", WiFi.status(), WiFi.localIP().toString().c_str());
  }
}
