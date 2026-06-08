#!/usr/bin/env bash
set -euo pipefail

echo "Usuario: $USER"
echo "Grupos:"
id -nG

echo "\nPuertos serial detectados:"
found=0
for p in /dev/ttyUSB* /dev/ttyACM*; do
  if [ -e "$p" ]; then
    ls -l "$p"
    found=1
  fi
done
if [ "$found" -eq 0 ]; then
  echo "No hay /dev/ttyUSB* ni /dev/ttyACM* visibles ahora"
fi

echo "\nRecomendación:"
echo "- Si no ves puertos, cambia cable USB-C por uno de datos (no solo carga)"
echo "- Prueba otro puerto USB-C del computador"
echo "- Revisa con: dmesg | tail -n 50"
