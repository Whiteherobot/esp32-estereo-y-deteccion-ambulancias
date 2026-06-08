"""
find_cameras.py — Detecta automáticamente los ESP32-CAM en la red local.
Escanea el rango 192.168.0.1–254 buscando dispositivos que respondan
al endpoint /ping con el firmware ML_STEREO.
Uso: python find_cameras.py
"""
import concurrent.futures
import requests
import socket

SUBNET   = "172.20.10"
TIMEOUT  = 1.5
FW_MAGIC = "ML_STEREO"   # el /ping debe devolver un string que empiece con esto


def probe(ip: str) -> dict | None:
    url = f"http://{ip}/ping"
    try:
        r = requests.get(url, timeout=TIMEOUT)
        if r.status_code == 200 and FW_MAGIC in r.text:
            info_url = f"http://{ip}/info"
            info = {}
            try:
                info = requests.get(info_url, timeout=TIMEOUT).json()
            except Exception:
                pass
            return {"ip": ip, "fw": r.text.strip(), "info": info}
    except Exception:
        pass
    return None


def main():
    print(f"[*] Escaneando {SUBNET}.1 – {SUBNET}.254 buscando ESP32-CAM...")
    ips = [f"{SUBNET}.{i}" for i in range(1, 15)]

    found = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=64) as ex:
        for result in ex.map(probe, ips):
            if result:
                found.append(result)

    if not found:
        print("[!] No se encontraron ESP32-CAM en la red.")
        print("    Verificar que estén encendidos y conectados al mismo WiFi.")
        return

    print(f"\n[OK] {len(found)} cámara(s) encontrada(s):\n")
    for cam in found:
        print(f"  IP  : {cam['ip']}")
        print(f"  FW  : {cam['fw']}")
        if cam['info']:
            i = cam['info']
            print(f"  AEC/AGC/AWB : {i.get('aec')}/{i.get('agc')}/{i.get('awb')}")
            print(f"  Exposure: {i.get('exposure')}  Gain: {i.get('gain')}")
        print(f"  Capture: http://{cam['ip']}/capture")
        print()

    if len(found) >= 2:
        left  = found[0]['ip']
        right = found[1]['ip']
        print("=" * 50)
        print("Comando para lanzar el sistema estéreo:")
        print(f"  ./stereo_measure ../../config/stereo_calib.yml \\")
        print(f"      http://{left}/capture \\")
        print(f"      http://{right}/capture")

if __name__ == "__main__":
    main()
