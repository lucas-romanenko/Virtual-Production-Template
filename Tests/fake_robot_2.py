import socket
import struct
import time
import math

PORT = 30005

while True:
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('127.0.0.1', PORT))
    server.listen(1)
    print(f"\nFake Dobot Robot 2 listening on 127.0.0.1:{PORT}")
    print("Movement: Figure-8 pattern")
    print("Waiting for connection...")

    conn, addr = server.accept()
    print(f"Connected by {addr}")
    print("Sending fake robot data...")

    frame = 0
    try:
        while True:
            fake_packet = bytearray(1440)

            t = frame * 0.08
            x = 300 + math.sin(t) * 150
            y = 300 + math.sin(t) * math.cos(t) * 150
            z = 350 + math.sin(t * 0.3) * 80
            rx = math.sin(t * 0.5) * 15
            ry = math.cos(t * 0.7) * 10
            rz = 0.0

            struct.pack_into('<d', fake_packet, 624, x)
            struct.pack_into('<d', fake_packet, 632, y)
            struct.pack_into('<d', fake_packet, 640, z)
            struct.pack_into('<d', fake_packet, 648, rx)
            struct.pack_into('<d', fake_packet, 656, ry)
            struct.pack_into('<d', fake_packet, 664, rz)

            conn.send(bytes(fake_packet))

            if frame % 125 == 0:
                print(f"Frame {frame}: X={x:.1f} Y={y:.1f} Z={z:.1f}")

            frame += 1
            time.sleep(0.008)

    except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError, OSError) as e:
        print(f"Client disconnected: {e}")
        print("Waiting for new connection...")
    except KeyboardInterrupt:
        print("\nStopping fake robot 2...")
        break
    finally:
        conn.close()
        server.close()
