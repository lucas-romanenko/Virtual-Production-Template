# Dobot LiveLink — Unreal Engine Plugin

A custom Unreal Engine 5.7 LiveLink plugin for real-time camera tracking using the Dobot Nova 5 (CR-series) robot arm. Designed for LED wall virtual production workflows.

---

## Overview

Dobot LiveLink connects a Dobot Nova 5 robot arm to Unreal Engine via TCP/IP, streaming real-time position and rotation data to drive a virtual camera. The plugin includes a native editor settings panel, per-camera connection management, DeckLink output routing, auto-reconnect, and delay compensation — all accessible from a single dockable tab.

### Key Features

- **Real-time TCP tracking** — Reads 1440-byte packets from the Dobot's port 30004 at 125Hz (8ms)
- **Native editor panel** — Dockable settings tab under Window → Virtual Production → Dobot LiveLink
- **Per-camera connections** — Each camera independently connects to a robot with its own IP, port, and subject name
- **Multi-camera support** — Multiple cameras can share one robot or connect to separate robots
- **Connection health monitoring** — Four-state status: Connected (green), Disconnected (red), Connection Lost (orange), Reconnecting (yellow)
- **Auto-connect & auto-reconnect** — Automatically reconnects on connection loss and on editor startup (configurable per camera)
- **Delay compensation** — Configurable tracking delay buffer with interpolation for latency matching
- **DeckLink output routing** — Global port routing matrix: assign cameras to DeckLink SDI output ports with per-port Start/Stop
- **Spawn cameras** — Create tracked cameras directly from the panel with one click
- **Delta-based tracking** — Camera movement is relative to starting position, so cameras can be placed anywhere in the level
- **Unique subject naming** — Automatic naming (DobotCamera, DobotCamera_2, etc.) with duplicate prevention

---

## Installation

### Plugin Only (for existing projects)

1. Copy the `DobotLiveLink` folder into your project's `Plugins/` directory
2. Open your project in Unreal Engine 5.7
3. Go to **Edit → Plugins**, search for "Dobot LiveLink" and enable it
4. Also enable **Blackmagic Media Player** if you plan to use DeckLink output
5. Restart the editor

### Template Project (pre-configured)

1. Copy the entire `Virtual_Production` project folder to your machine
2. Open `Virtual_Production.uproject` in Unreal Engine 5.7
3. The plugin is already enabled and a demo level with tracked cameras is ready

---

## Quick Start

### 1. Open the Settings Panel

Go to **Window → Virtual Production → Dobot LiveLink**. This opens the main control panel.

### 2. Select or Create a Camera

- If cameras with a DobotLiveLinkCamera component exist in the level, they appear in the **Active Camera** dropdown
- If no cameras are found, a yellow message will prompt you to click **"+ Add Camera"** to create one
- The **Subject Name** field shows the LiveLink subject name for the selected camera — each camera must have a unique name

### 3. Configure Camera Settings

Adjust focal length, aperture, sensor width, and sensor height in the **Camera Settings** section. Changes apply to the selected camera in real-time.

### 4. Connect to a Robot

1. Click **"Add Dobot Connection"** in the Dobot Connection section
2. Enter the robot's **Dobot IP Address** (default: 192.168.5.1)
3. Enter the **Dobot Port** (default: 30004)
4. Optionally set a **Tracking Delay** in milliseconds
5. Check **Auto-Connect** if you want the plugin to reconnect automatically on startup and after connection loss
6. Click **Connect**

Tracking enables automatically when you connect. The status indicator shows:

- 🟢 **Connected — Receiving Data** — Robot is streaming tracking data
- 🔴 **Disconnected** — No active connection
- 🟠 **Connection Lost** — Was connected but the robot stopped sending data
- 🟡 **Reconnecting...** — Actively trying to reconnect (every 3 seconds)

### 5. DeckLink Output

The DeckLink Output section is global — it's the same regardless of which camera is selected above.

1. Set **Number of Outputs** to match your DeckLink card's port count
2. For each port, select a camera from the dropdown
3. Click **Settings** to open the BlackmagicMediaOutput asset editor and configure the device/port/resolution
4. Click **Start** to begin capturing, **Stop** to end

Each port operates independently — you can have Port 1 outputting Camera 1 while Port 2 outputs Camera 2.

---

## Network Setup

### Dobot Nova 5 Network Ports

| Port | Purpose | Update Rate |
|-------|---------|-------------|
| 29999 | Dashboard — control commands (enable/disable robot) | On request |
| 30003 | Motion commands (MovJ, MovL, etc.) | On request |
| 30004 | **Real-time feedback** (used by this plugin) | 8ms (125Hz) |
| 30005 | Feedback (slower) | 200ms |
| 30006 | Configurable feedback | 50ms (default) |

### Connection Requirements

- Your computer and the Dobot robot must be on the **same network segment**
- Default Dobot IP when connected via **LAN1**: `192.168.5.1`
- Default Dobot IP when connected via **LAN2**: `192.168.100.1`
- Default Dobot IP via **WiFi**: `192.168.1.6`
- Controller firmware must be **v3.5.2 or later** for port 30004 support
- Ensure port 30004 is not blocked by firewall

### Typical Network Setup

```
[Dobot Nova 5] --ethernet--> [Network Switch] <--ethernet-- [Unreal PC]
   192.168.5.1                                                192.168.5.x
```

---

## Plugin Architecture

### Source Files

```
Plugins/DobotLiveLink/
├── Resources/
│   └── Icon128.png
├── Source/
│   ├── DobotLiveLink/                    # Runtime module
│   │   ├── Public/
│   │   │   ├── DobotLiveLink.h
│   │   │   ├── DobotLiveLinkCameraComponent.h
│   │   │   ├── DobotLiveLinkSource.h
│   │   │   └── DobotLiveLinkSettings.h
│   │   ├── Private/
│   │   │   ├── DobotLiveLink.cpp
│   │   │   ├── DobotLiveLinkCameraComponent.cpp
│   │   │   ├── DobotLiveLinkSource.cpp
│   │   │   └── DobotLiveLinkSettings.cpp
│   │   └── DobotLiveLink.Build.cs
│   └── DobotLiveLinkEditor/             # Editor module
│       ├── DobotLiveLinkEditor.h
│       ├── DobotLiveLinkEditor.cpp
│       ├── DobotLiveLinkSourceFactory.h
│       ├── DobotLiveLinkSourceFactory.cpp
│       ├── SDobotLiveLinkSourceFactory.h
│       ├── SDobotLiveLinkSourceFactory.cpp
│       └── DobotLiveLinkEditor.Build.cs
└── DobotLiveLink.uplugin
```

### Key Classes

**FDobotLiveLinkSource** — The LiveLink source that manages the TCP connection to the robot. Runs on a background thread, receives 1440-byte packets, parses position/rotation data, and pushes it to LiveLink. Supports delay buffering with interpolation. Detects socket disconnect and signals connection loss.

**UDobotLiveLinkCameraComponent** — Actor component that reads LiveLink transform data and applies delta-based tracking to a CineCameraComponent. Stores per-camera connection settings (IP, port, delay) and manages connect/disconnect lifecycle. Monitors connection health, auto-reconnects on loss, and auto-connects on editor startup.

**UDobotLiveLinkSettings** — Singleton UObject that stores camera settings (focal length, aperture, sensor size), auto-connect preferences, and DeckLink output port routing. Provides functions for finding/spawning cameras, managing output assets, and starting/stopping per-port captures.

**FDobotLiveLinkEditorModule** — Editor module that registers the dockable settings panel under Window → Virtual Production → Dobot LiveLink. Builds the entire UI with camera selector, settings, connection controls, and DeckLink output routing.

---

## Auto-Connect & Auto-Reconnect

The plugin supports hands-free operation for 24/7 production environments.

**Auto-Connect** (per camera, stored in config): When enabled, the plugin automatically connects to the robot when the editor opens. If the robot isn't available yet, it retries every 3 seconds until it connects.

**Auto-Reconnect**: When a connection drops (robot reboots, network blip, etc.), the plugin automatically detects the loss, cleans up the dead LiveLink source, and starts retrying every 3 seconds. When the robot comes back, it reconnects and tracking resumes without human intervention.

**To enable**: Check the "Auto-Connect" checkbox in the Dobot Connection section of the panel. This setting is saved in the plugin config file and persists across editor restarts independent of Blueprint or level saves.

**Crash recovery**: If Unreal crashes and is reopened, cameras with Auto-Connect enabled will automatically reconnect to their robots after a 5-second startup delay.

---

## Template Project

The template project includes pre-configured Blueprints for quick setup:

### BP_VirtualProductionStage

An all-in-one Blueprint based on CineCameraActor that includes a LED wall mesh, floor mesh, robot base mesh, DobotLiveLinkCamera component, and Scene Capture for LED wall preview. Drag into any level for a complete virtual production stage setup.

### BP_DobotTrackedCamera

A standalone tracked camera Blueprint based on CineCameraActor with a DobotLiveLinkCamera component. Use for additional cameras or cameras without the stage meshes.

### Demo Level (Main)

Pre-configured level with BP_VirtualProductionStage and BP_DobotTrackedCamera already placed and ready to use.

---

## Testing Without a Robot

Two Python scripts are included for testing without a physical Dobot robot. Both auto-reconnect when a client disconnects.

### fake_robot.py — Circle Pattern (Port 30004)

```bash
python fake_robot.py
```

Simulates a robot doing circular movements at 125Hz on port 30004.

### fake_robot_2.py — Figure-8 Pattern (Port 30005)

```bash
python fake_robot_2.py
```

Simulates a second robot doing figure-8 movements with rotation on port 30005. Use this to test multi-camera setups with two independent robots.

### Testing Steps

1. Start one or both Python scripts
2. Open the Dobot LiveLink panel in Unreal
3. Select a camera, add a Dobot connection
4. Set IP to `127.0.0.1`, port to `30004` (or `30005` for the second script)
5. Click Connect — the camera should start moving
6. Close the Python script to test connection loss detection (status goes orange then yellow "Reconnecting")
7. Restart the script — auto-reconnects and tracking resumes
8. Close Unreal, reopen — with Auto-Connect enabled, it reconnects after 5 seconds

---

## Data Protocol

### Packet Format

Each packet from port 30004 is exactly **1440 bytes**. The plugin reads the tool position at byte offset 624:

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 624 | 8 bytes | double | X position (mm) |
| 632 | 8 bytes | double | Y position (mm) |
| 640 | 8 bytes | double | Z position (mm) |
| 648 | 8 bytes | double | Rx rotation (degrees) |
| 656 | 8 bytes | double | Ry rotation (degrees) |
| 664 | 8 bytes | double | Rz rotation (degrees) |

### Coordinate Mapping

```
Dobot → Unreal:
  X (mm) → X (cm) = X / 10
  Y (mm) → Y (cm) = Y / 10
  Z (mm) → Z (cm) = Z / 10

Position: FVector(-Y, X, Z)
Rotation: FRotator(Ry, Rz, Rx)
```

This mapping may need adjustment when setting up on a real robot depending on the robot's orientation relative to the LED wall and subject.

### Delta-Based Tracking

The component uses relative tracking rather than absolute positioning. On first frame after connecting, it records the robot's starting position and the camera's starting position. On each subsequent frame, it calculates the delta from the robot's start and applies it to the camera's start. This means the virtual camera can be placed anywhere in the level — movement is always relative.

---

## Delay Compensation

The tracking delay buffer compensates for latency between the physical camera and the LED wall render. Incoming transforms are buffered with timestamps. When pushing to LiveLink, the system looks back by the configured delay amount and interpolates between the two nearest samples. Set to 0ms for no buffering (lowest latency).

---

## Multi-Camera Setup

### Two Cameras, Two Robots

1. Place two camera BPs in the level (each gets a unique subject name automatically)
2. Open the Dobot LiveLink panel
3. Select Camera 1 → Add Connection → IP: 192.168.5.1, Port: 30004 → Connect
4. Select Camera 2 → Add Connection → IP: 192.168.5.2, Port: 30004 → Connect
5. Each camera tracks its own robot independently

### Two Cameras, One Robot (Shared Tracking)

1. Place two camera BPs with the **same Subject Name**
2. Connect Camera 1 to the robot
3. Connect Camera 2 with the same IP/port/subject — the plugin reuses the existing LiveLink source
4. Both cameras receive the same tracking data but can have different camera settings

---

## DeckLink Output Routing

The DeckLink Output section is a global port routing matrix, independent of the camera selector above.

1. Set **Number of Outputs** to match your DeckLink card (e.g., 4 for a DeckLink Quad)
2. Each port row shows: camera dropdown, status dot, Start/Stop buttons, Settings button
3. Assign cameras to ports by selecting from the dropdown
4. Click **Settings** on a port to open (or create) the BlackmagicMediaOutput asset and configure device/port/resolution
5. Start/Stop each port independently

Port assignments and number of outputs persist in the plugin config file across editor restarts.

---

## Troubleshooting

### Camera Not Moving After Connect

- Check that **Enable Tracking** is checked in the Dobot Connection section
- Verify the **Subject Name** matches between the source and the camera
- Check the Output Log for connection errors

### Connection Lost (Orange/Yellow Status)

- The robot stopped sending data or the network connection dropped
- If Auto-Connect is enabled, the plugin automatically retries every 3 seconds
- The dead source is automatically cleaned up from LiveLink

### Duplicate Subject Names

- Each camera must have a unique subject name
- The panel prevents duplicate names — you'll see a red on-screen message if you try

### DeckLink Output Not Starting

- Ensure the **Blackmagic Media Player** plugin is enabled in Edit → Plugins
- Click **Settings** on the port to create/configure a BlackmagicMediaOutput asset
- A DeckLink card must be installed and recognized by the system
- Configure the output asset with your device and resolution before starting

### Auto-Connect Not Working

- Make sure the Auto-Connect checkbox is checked in the panel
- This setting is stored in the plugin config, not on the Blueprint — changes apply immediately
- On startup, there is a 5-second delay before the first connection attempt

---

## Requirements

- **Unreal Engine**: 5.7
- **Dobot Firmware**: v3.5.2 or later (for port 30004 support)
- **Operating System**: Windows 10/11
- **Optional**: Blackmagic DeckLink card and Blackmagic Media Player plugin for SDI output
- **Optional**: Python 3.x for running test scripts

---

## License

This plugin is provided as-is for virtual production use with Dobot Nova 5 robot arms.

---

## Credits

Developed by Lucas Romanenko for ARRISE Powering Pragmatic Play virtual production workflows.
