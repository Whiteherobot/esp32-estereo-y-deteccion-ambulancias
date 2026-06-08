# Arduino - Pruebas rápidas ESP32/ESP32-CAM

## Archivos creados
1. `00_Serial_USB_Test/00_Serial_USB_Test.ino`
   - Verifica que el puerto serial y carga de sketch funcionen.

2. `01_WiFi_Connection_Test/01_WiFi_Connection_Test.ino`
   - Verifica conexión WiFi y muestra IP.

3. `02_ESP32_CAM_AI_THINKER_Snapshot_Server/02_ESP32_CAM_AI_THINKER_Snapshot_Server.ino`
   - Servidor mínimo de captura para ESP32-CAM AI Thinker.
   - Endpoint: `http://IP/capture`

4. `03_ESP32_CAM_Stream_Capture_NoMirror/03_ESP32_CAM_Stream_Capture_NoMirror.ino`
   - Servidor completo (root + snapshot + stream) para pruebas estéreo.
   - Endpoints:
     - `http://IP/`
     - `http://IP/capture`
     - `http://IP/stream`
   - Configurado sin espejo horizontal (`hmirror=0`).

---

## ¿Necesitas librerías extra?
### No adicionales por ahora
Con el **core ESP32 de Arduino** instalado, estos sketches compilan usando:
- `WiFi.h`
- `esp_camera.h`
- `WebServer.h`

Estas vienen con el paquete de placas ESP32 (Boards Manager).

---

## Ajustes manuales obligatorios
1. Reemplaza en cada sketch:
```cpp
const char* ssid = "REEMPLAZAR_SSID";
const char* password = "REEMPLAZAR_PASSWORD";
```

2. Selecciona placa correcta:
- ESP32-CAM-MB: normalmente **AI Thinker ESP32-CAM**
- XIAO ESP32-S3: **XIAO_ESP32S3** o **ESP32S3 Dev Module** (según tu instalación)

3. Selecciona puerto correcto `/dev/ttyUSBx`.

4. Si usas ESP32-CAM clásico y falla upload, prueba:
- `Upload Speed: 115200`
- `Partition Scheme: Huge APP (3MB No OTA)`
- presionar botón BOOT durante inicio de carga (según módulo/programador).

---

## Orden recomendado de prueba
1. Cargar `00_Serial_USB_Test` → confirmar mensajes en Serial Monitor.
2. Cargar `01_WiFi_Connection_Test` → confirmar IP en red.
3. Cargar `03_ESP32_CAM_Stream_Capture_NoMirror` en ambas cámaras.
4. Verificar en navegador:
   - `http://IP/`
   - `http://IP/capture`
   - `http://IP/stream`

Cuando estos 3 endpoints funcionen en ambas cámaras, pasa a validar con Python (`check_esp32_streams.py`).
