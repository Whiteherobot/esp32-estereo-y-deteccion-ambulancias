// 00_Serial_USB_Test.ino
// Prueba mínima de puerto serial/USB para ESP32

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("[OK] Serial USB activo");
  Serial.println("Si ves este mensaje en Serial Monitor, el puerto y subida funcionan.");
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last > 1000) {
    last = millis();
    Serial.printf("Heartbeat ms=%lu\n", millis());
  }
}
