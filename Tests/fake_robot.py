import socket
import struct
import time
import math

PORT = 30004

while True:
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('127.0.0.1', PORT))
    server.listen(1)
    print(f"\nFake Dobot robot listening on 127.0.0.1:{PORT}")
    print("Waiting for connection...")

    conn, addr = server.accept()
    print(f"Connected by {addr}")
    print("Sending fake robot data...")

    frame = 0
    try:
        while True:
            fake_packet = bytearray(1440)

            t = frame * 0.1
            x = 300 + math.cos(t) * 100
            y = 300 + math.sin(t) * 100
            z = 400 + math.sin(t * 0.5) * 50

            struct.pack_into('<d', fake_packet, 624, x)
            struct.pack_into('<d', fake_packet, 632, y)
            struct.pack_into('<d', fake_packet, 640, z)
            struct.pack_into('<d', fake_packet, 648, 0.0)
            struct.pack_into('<d', fake_packet, 656, 0.0)
            struct.pack_into('<d', fake_packet, 664, 0.0)

            conn.send(bytes(fake_packet))

            if frame % 125 == 0:
                print(f"Frame {frame}: X={x:.1f} Y={y:.1f} Z={z:.1f}")

            frame += 1
            time.sleep(0.008)

    except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError, OSError) as e:
        print(f"Client disconnected: {e}")
        print("Waiting for new connection...")
    except KeyboardInterrupt:
        print("\nStopping fake robot...")
        break
    finally:
        conn.close()
        server.close()
