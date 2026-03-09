"""
Dobot Nova 5 / CR-Series — Real-Time Feedback Diagnostic Tool
Connects to port 30004 and parses the full 1440-byte packet.
Displays all known fields for debugging and verifying data format.

Usage:
    python read_dobot.py                    # Uses default IP 192.168.5.1
    python read_dobot.py 192.168.5.1        # Specify IP
    python read_dobot.py 192.168.5.1 30004  # Specify IP and port
"""

import socket
import struct
import sys
import time
import json
import os

# ---- Configuration ----
ROBOT_IP = sys.argv[1] if len(sys.argv) > 1 else "192.168.5.1"
ROBOT_PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 30004

# ---- 1440-byte Packet Field Definitions ----
# Based on Dobot CR-Series TCP/IP Protocol Documentation
# Each field: (name, offset, type, count, unit/description)
# Types: 'h' = int16, 'H' = uint16, 'i' = int32, 'I' = uint32, 'q' = int64, 'Q' = uint64, 'd' = float64

PACKET_FIELDS = [
    # Header
    ("MessageSize",         0,    'H',  1,  "bytes - Total message size"),
    ("Reserve1",            2,    'H',  3,  "Reserved"),

    # Time
    ("DigitalInputs",       8,    'Q',  1,  "Digital input bitmask"),
    ("DigitalOutputs",      16,   'Q',  1,  "Digital output bitmask"),
    ("RobotMode",           24,   'Q',  1,  "1=Init 2=Brake 4=Disabled 5=Enabled 6=Drag 7=Running 8=Recording 9=Error 10=Pause 11=Jog"),

    # Reserved / internal
    ("TimeStamp",           32,   'Q',  1,  "Timestamp"),
    ("Reserve2",            40,   'Q',  1,  "Reserved"),
    ("TestValue",           48,   'Q',  1,  "Validation: should be 0x123456789ABCDEF"),
    ("Reserve3",            56,   'Q',  1,  "Reserved"),
    ("SpeedScaling",        64,   'd',  1,  "Speed scaling factor (0-100)"),

    # Joint positions (actual)
    ("QActual_J1",          72,   'd',  1,  "deg - Joint 1 actual angle"),
    ("QActual_J2",          80,   'd',  1,  "deg - Joint 2 actual angle"),
    ("QActual_J3",          88,   'd',  1,  "deg - Joint 3 actual angle"),
    ("QActual_J4",          96,   'd',  1,  "deg - Joint 4 actual angle"),
    ("QActual_J5",          104,  'd',  1,  "deg - Joint 5 actual angle"),
    ("QActual_J6",          112,  'd',  1,  "deg - Joint 6 actual angle"),

    # Joint speeds (actual)
    ("QDActual_J1",         120,  'd',  1,  "deg/s - Joint 1 speed"),
    ("QDActual_J2",         128,  'd',  1,  "deg/s - Joint 2 speed"),
    ("QDActual_J3",         136,  'd',  1,  "deg/s - Joint 3 speed"),
    ("QDActual_J4",         144,  'd',  1,  "deg/s - Joint 4 speed"),
    ("QDActual_J5",         152,  'd',  1,  "deg/s - Joint 5 speed"),
    ("QDActual_J6",         160,  'd',  1,  "deg/s - Joint 6 speed"),

    # Joint target accelerations
    ("QDDActual_J1",        168,  'd',  1,  "deg/s² - Joint 1 acceleration"),
    ("QDDActual_J2",        176,  'd',  1,  "deg/s² - Joint 2 acceleration"),
    ("QDDActual_J3",        184,  'd',  1,  "deg/s² - Joint 3 acceleration"),
    ("QDDActual_J4",        192,  'd',  1,  "deg/s² - Joint 4 acceleration"),
    ("QDDActual_J5",        200,  'd',  1,  "deg/s² - Joint 5 acceleration"),
    ("QDDActual_J6",        208,  'd',  1,  "deg/s² - Joint 6 acceleration"),

    # Joint currents
    ("IActual_J1",          216,  'd',  1,  "mA - Joint 1 current"),
    ("IActual_J2",          224,  'd',  1,  "mA - Joint 2 current"),
    ("IActual_J3",          232,  'd',  1,  "mA - Joint 3 current"),
    ("IActual_J4",          240,  'd',  1,  "mA - Joint 4 current"),
    ("IActual_J5",          248,  'd',  1,  "mA - Joint 5 current"),
    ("IActual_J6",          256,  'd',  1,  "mA - Joint 6 current"),

    # Tool Accelerometer
    ("ToolAccelX",          264,  'd',  1,  "Tool accelerometer X"),
    ("ToolAccelY",          272,  'd',  1,  "Tool accelerometer Y"),
    ("ToolAccelZ",          280,  'd',  1,  "Tool accelerometer Z"),

    # Reserved block
    # 288 - 623: various reserved/internal fields

    # ---- TOOL POSE (what we use for camera tracking) ----
    ("ToolVectorActual_X",  624,  'd',  1,  "mm - Tool X position"),
    ("ToolVectorActual_Y",  632,  'd',  1,  "mm - Tool Y position"),
    ("ToolVectorActual_Z",  640,  'd',  1,  "mm - Tool Z position"),
    ("ToolVectorActual_Rx", 648,  'd',  1,  "deg - Tool Rx rotation"),
    ("ToolVectorActual_Ry", 656,  'd',  1,  "deg - Tool Ry rotation"),
    ("ToolVectorActual_Rz", 664,  'd',  1,  "deg - Tool Rz rotation"),

    # Target tool pose
    ("ToolVectorTarget_X",  672,  'd',  1,  "mm - Target tool X"),
    ("ToolVectorTarget_Y",  680,  'd',  1,  "mm - Target tool Y"),
    ("ToolVectorTarget_Z",  688,  'd',  1,  "mm - Target tool Z"),
    ("ToolVectorTarget_Rx", 696,  'd',  1,  "deg - Target tool Rx"),
    ("ToolVectorTarget_Ry", 704,  'd',  1,  "deg - Target tool Ry"),
    ("ToolVectorTarget_Rz", 712,  'd',  1,  "deg - Target tool Rz"),

    # Joint target positions
    ("QTarget_J1",          720,  'd',  1,  "deg - Joint 1 target"),
    ("QTarget_J2",          728,  'd',  1,  "deg - Joint 2 target"),
    ("QTarget_J3",          736,  'd',  1,  "deg - Joint 3 target"),
    ("QTarget_J4",          744,  'd',  1,  "deg - Joint 4 target"),
    ("QTarget_J5",          752,  'd',  1,  "deg - Joint 5 target"),
    ("QTarget_J6",          760,  'd',  1,  "deg - Joint 6 target"),

    # Joint temperatures
    ("JointTemp_J1",        768,  'd',  1,  "°C - Joint 1 temperature"),
    ("JointTemp_J2",        776,  'd',  1,  "°C - Joint 2 temperature"),
    ("JointTemp_J3",        784,  'd',  1,  "°C - Joint 3 temperature"),
    ("JointTemp_J4",        792,  'd',  1,  "°C - Joint 4 temperature"),
    ("JointTemp_J5",        800,  'd',  1,  "°C - Joint 5 temperature"),
    ("JointTemp_J6",        808,  'd',  1,  "°C - Joint 6 temperature"),

    # Robot status flags
    ("RobotMode2",          816,  'd',  1,  "Robot mode (duplicate)"),
    ("EnableStatus",        824,  'd',  1,  "1=Enabled 0=Disabled"),
    ("DragStatus",          832,  'd',  1,  "1=Dragging 0=Not dragging"),
    ("RunningStatus",       840,  'd',  1,  "1=Running 0=Not running"),
    ("ErrorStatus",         848,  'd',  1,  "1=Error 0=No error"),
    ("JogStatus",           856,  'd',  1,  "1=Jogging 0=Not jogging"),
]


def recv_exact(sock, size):
    """Receive exactly `size` bytes from socket."""
    data = b''
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("Connection closed by robot")
        data += chunk
    return data


def parse_field(data, offset, fmt, count):
    """Parse a field from the packet data."""
    try:
        if count == 1:
            return struct.unpack_from(f'<{fmt}', data, offset)[0]
        else:
            return list(struct.unpack_from(f'<{count}{fmt}', data, offset))
    except struct.error:
        return None


def parse_packet(data):
    """Parse all known fields from the 1440-byte packet."""
    result = {}
    for name, offset, fmt, count, desc in PACKET_FIELDS:
        val = parse_field(data, offset, fmt, count)
        result[name] = val
    return result


def print_header():
    """Print a nice header."""
    print("=" * 80)
    print("  DOBOT NOVA 5 / CR-SERIES — REAL-TIME FEEDBACK DIAGNOSTIC TOOL")
    print(f"  Target: {ROBOT_IP}:{ROBOT_PORT}")
    print("=" * 80)


def print_full_dump(fields, frame):
    """Print all parsed fields."""
    os.system('cls' if os.name == 'nt' else 'clear')
    print_header()
    print(f"  Frame: {frame}")
    print("-" * 80)

    # Validation
    test_val = fields.get('TestValue', 0)
    valid = "VALID" if test_val == 0x123456789ABCDEF else f"INVALID (got {hex(test_val) if test_val else 'None'})"
    print(f"  Packet Validation: {valid}")
    print(f"  Message Size: {fields.get('MessageSize', 'N/A')} bytes")
    print()

    # Robot State
    mode = fields.get('RobotMode', 0)
    mode_names = {1: "Init", 2: "Brake Released", 4: "Disabled", 5: "Enabled/Idle",
                  6: "Drag Mode", 7: "Running", 8: "Recording", 9: "ERROR", 10: "Paused", 11: "Jogging"}
    mode_str = mode_names.get(int(mode) if mode else 0, f"Unknown ({mode})")
    print(f"  Robot Mode:     {mode_str}")
    print(f"  Enabled:        {'Yes' if fields.get('EnableStatus', 0) else 'No'}")
    print(f"  Running:        {'Yes' if fields.get('RunningStatus', 0) else 'No'}")
    print(f"  Dragging:       {'Yes' if fields.get('DragStatus', 0) else 'No'}")
    print(f"  Error:          {'YES!' if fields.get('ErrorStatus', 0) else 'No'}")
    print(f"  Speed Scaling:  {fields.get('SpeedScaling', 'N/A')}%")
    print()

    # Tool Position (what we use for tracking)
    print("  ---- TOOL POSITION (used for camera tracking) ----")
    print(f"  X:  {fields.get('ToolVectorActual_X', 0):10.3f} mm    Rx: {fields.get('ToolVectorActual_Rx', 0):10.3f} deg")
    print(f"  Y:  {fields.get('ToolVectorActual_Y', 0):10.3f} mm    Ry: {fields.get('ToolVectorActual_Ry', 0):10.3f} deg")
    print(f"  Z:  {fields.get('ToolVectorActual_Z', 0):10.3f} mm    Rz: {fields.get('ToolVectorActual_Rz', 0):10.3f} deg")
    print()

    # Tool Target
    print("  ---- TOOL TARGET ----")
    print(f"  X:  {fields.get('ToolVectorTarget_X', 0):10.3f} mm    Rx: {fields.get('ToolVectorTarget_Rx', 0):10.3f} deg")
    print(f"  Y:  {fields.get('ToolVectorTarget_Y', 0):10.3f} mm    Ry: {fields.get('ToolVectorTarget_Ry', 0):10.3f} deg")
    print(f"  Z:  {fields.get('ToolVectorTarget_Z', 0):10.3f} mm    Rz: {fields.get('ToolVectorTarget_Rz', 0):10.3f} deg")
    print()

    # Joint Angles
    print("  ---- JOINT ANGLES (actual) ----")
    for i in range(1, 7):
        actual = fields.get(f'QActual_J{i}', 0)
        target = fields.get(f'QTarget_J{i}', 0)
        speed = fields.get(f'QDActual_J{i}', 0)
        print(f"  J{i}:  {actual:8.2f}°  target: {target:8.2f}°  speed: {speed:8.2f}°/s")
    print()

    # Joint Currents
    print("  ---- JOINT CURRENTS ----")
    for i in range(1, 7):
        current = fields.get(f'IActual_J{i}', 0)
        print(f"  J{i}: {current:8.2f} mA", end="   ")
    print()
    print()

    # Joint Temperatures
    print("  ---- JOINT TEMPERATURES ----")
    for i in range(1, 7):
        temp = fields.get(f'JointTemp_J{i}', 0)
        print(f"  J{i}: {temp:6.1f}°C", end="   ")
    print()
    print()

    # Tool Accelerometer
    print("  ---- TOOL ACCELEROMETER ----")
    print(f"  X: {fields.get('ToolAccelX', 0):8.4f}   Y: {fields.get('ToolAccelY', 0):8.4f}   Z: {fields.get('ToolAccelZ', 0):8.4f}")
    print()

    # Digital I/O
    di = fields.get('DigitalInputs', 0)
    do = fields.get('DigitalOutputs', 0)
    if di is not None:
        print(f"  Digital Inputs:  {bin(int(di))[2:].rjust(16, '0')}")
    if do is not None:
        print(f"  Digital Outputs: {bin(int(do))[2:].rjust(16, '0')}")
    print()

    # UE mapping reference
    print("  ---- UNREAL ENGINE MAPPING (current ParseDobotPacket) ----")
    x = fields.get('ToolVectorActual_X', 0) / 10.0
    y = fields.get('ToolVectorActual_Y', 0) / 10.0
    z = fields.get('ToolVectorActual_Z', 0) / 10.0
    rx = fields.get('ToolVectorActual_Rx', 0)
    ry = fields.get('ToolVectorActual_Ry', 0)
    rz = fields.get('ToolVectorActual_Rz', 0)
    print(f"  UE Location: X={-y:8.2f}  Y={x:8.2f}  Z={z:8.2f}  (cm)")
    print(f"  UE Rotation: Pitch={ry:8.2f}  Yaw={rz:8.2f}  Roll={rx:8.2f}  (deg)")
    print()
    print("-" * 80)
    print("  Press Ctrl+C to stop  |  Refreshing 3x/sec")


def save_raw_packet(data, filename="raw_packet.bin"):
    """Save a single raw packet for offline analysis."""
    with open(filename, 'wb') as f:
        f.write(data)
    print(f"  Raw packet saved to {filename} ({len(data)} bytes)")


def save_packet_json(fields, filename="packet_fields.json"):
    """Save parsed fields as JSON for reference."""
    # Convert any non-serializable types
    clean = {}
    for k, v in fields.items():
        if isinstance(v, (int, float)):
            clean[k] = v
        else:
            clean[k] = str(v)
    with open(filename, 'w') as f:
        json.dump(clean, f, indent=2)
    print(f"  Parsed fields saved to {filename}")


# ---- Main ----
print_header()
print(f"\n  Connecting to {ROBOT_IP}:{ROBOT_PORT}...")

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(5)

try:
    sock.connect((ROBOT_IP, ROBOT_PORT))
    print("  Connected!\n")
except Exception as e:
    print(f"  FAILED to connect: {e}")
    print(f"\n  Troubleshooting:")
    print(f"    1. Is the robot powered on?")
    print(f"    2. Is your PC on the same network? (try: ping {ROBOT_IP})")
    print(f"    3. Is port {ROBOT_PORT} blocked by firewall?")
    print(f"    4. Is the controller firmware v3.5.2 or later?")
    sys.exit(1)

frame = 0
first_packet_saved = False

try:
    while True:
        data = recv_exact(sock, 1440)

        fields = parse_packet(data)

        # Save first packet for offline analysis
        if not first_packet_saved:
            save_raw_packet(data)
            save_packet_json(fields)
            first_packet_saved = True
            time.sleep(1)

        # Update display ~3 times per second
        if frame % 42 == 0:
            print_full_dump(fields, frame)

        frame += 1

except KeyboardInterrupt:
    print("\n\n  Stopped by user.")
except Exception as e:
    print(f"\n  Error: {e}")
finally:
    sock.close()
    print(f"  Total frames received: {frame}")
    print(f"  Connection closed.")
