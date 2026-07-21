import sys
import os
import struct
import argparse

try:
    import serial
except ImportError:
    print("Error: falta la libreria 'pyserial'. Instalala ejecutando: pip install pyserial")
    sys.exit(1)

# Agregamos la ruta del proyecto STM32 para importar el protocolo existente
sys.path.append(os.path.join(os.path.dirname(__file__), '../ADEPCUR-RCBSW0/groundcontrol'))
from rover_protocol import FrameDecoder, TC_ARM, TC_SET_THROTTLE, THROTTLE_SCALE

def main():
    parser = argparse.ArgumentParser(description="Analizador del enlace UART de la ESP32")
    parser.add_argument("--port", type=str, default="COM8", help="Puerto COM de la ESP32 (ej: COM8)")
    parser.add_argument("--baud", type=int, default=115200, help="Velocidad (baudrate)")
    args = parser.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"Error abriendo el puerto {args.port}: {e}")
        return

    print(f"=== Escuchando en {args.port} a {args.baud} baudios ===")
    print("Conectate al WiFi del rover y usa los controles web.")
    print("Las tramas decodificadas apareceran aqui... (Ctrl+C para salir)\n")

    decoder = FrameDecoder()

    try:
        while True:
            data = ser.read(64)
            if data:
                for b in data:
                    payload = decoder.push(b)
                    if payload:
                        print() # Salto de linea si venian puntos
                        opcode = payload[0]
                        if opcode == TC_ARM:
                            state = "ARMADO" if payload[1] == 1 else "DESARMADO"
                            print(f"📦 [TC_ARM] => {state}")
                        elif opcode == TC_SET_THROTTLE:
                            # Desempaquetar los 2 enteros de 16-bits (little endian)
                            l, r = struct.unpack("<hh", payload[1:5])
                            left = l / THROTTLE_SCALE
                            right = r / THROTTLE_SCALE
                            print(f"📦 [TC_SET_THROTTLE] => Izquierda: {left:+.2f} | Derecha: {right:+.2f}")
                        else:
                            print(f"📦 [OPCODE DESCONOCIDO] => 0x{opcode:02X}")
                            
    except KeyboardInterrupt:
        print("\nSaliendo...")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()
