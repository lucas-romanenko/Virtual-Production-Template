# Virtual Production Template — Setup Guide

## Requirements

- Windows 10 or 11
- Unreal Engine 5.7 (install from Epic Games Launcher)
- That's it — no Visual Studio or other tools needed

## Getting Started

1. Unzip the **Virtual_Production** folder to your Documents or any location
2. Double-click **Virtual_Production.uproject** to open in Unreal Engine 5.7
3. If prompted about compiling modules, click **No** — the plugin is already pre-compiled
4. Wait for shaders to compile on first open (this can take a few minutes)

## Opening the Dobot LiveLink Panel

Go to **Window → Virtual Production → Dobot LiveLink**

This is the main control panel for camera tracking and output. Everything is managed from here.

## Setting Up a Camera

1. In the panel, cameras in the level will show in the **Active Camera** dropdown
2. If none exist, click **+ Add Camera** to create one
3. Each camera gets a unique **Subject Name** (DobotCamera, DobotCamera_2, etc.)
4. Adjust camera settings (focal length, aperture, sensor size) as needed

## Connecting to the Dobot Robot

1. Select a camera from the dropdown
2. Click **Add Dobot Connection**
3. Enter the **Dobot IP Address** (default: 192.168.5.1 for LAN1)
4. Enter the **Dobot Port** (default: 30004)
5. Check **Auto-Connect** if you want it to reconnect automatically after restarts or network drops
6. Click **Connect**

The status indicator next to "Dobot Connection" shows the connection state:
- Green = Connected and receiving data
- Red = Disconnected
- Orange = Connection lost
- Yellow = Reconnecting (retries every 3 seconds)

Tracking enables automatically when you connect.

## Multiple Cameras

Each camera can have its own independent Dobot connection. Select a different camera from the dropdown, add a connection with a different IP/port, and connect. Both cameras track independently.

Two cameras can also share the same robot by using the same Subject Name.

## DeckLink Output

The DeckLink Output section at the bottom of the panel is global — it's the same regardless of which camera is selected.

1. Set **Number of Outputs** to match your DeckLink card's port count
2. For each port, select which camera to output
3. Click **Settings** to configure the Blackmagic output (device, resolution, format)
4. Click **Start** to begin outputting, **Stop** to end

Note: The Blackmagic Media Player plugin must be enabled (Edit → Plugins) and a DeckLink card must be installed.

## Testing Without a Robot

Two Python scripts are included in the project folder for testing:

1. Open a command prompt
2. Run: `python fake_robot.py` (simulates a robot on port 30004)
3. In the panel, set IP to `127.0.0.1`, port `30004`, and connect
4. The camera should start moving in a circle pattern

For a second robot: `python fake_robot_2.py` (port 30005, figure-8 pattern)

## Network Setup

Make sure the PC and Dobot robot are on the same network:

| Connection | Default Dobot IP |
|-----------|-----------------|
| LAN1 (ethernet) | 192.168.5.1 |
| LAN2 (ethernet) | 192.168.100.1 |
| WiFi | 192.168.1.6 |

The robot's port 30004 must not be blocked by firewall.

## Troubleshooting

**Panel won't open:** Try Window → Load Layout → Default Editor Layout, then try again.

**Camera not moving:** Check that Enable Tracking is checked in the Dobot Connection section.

**Connection keeps dropping:** Make sure the robot is powered on and sending data. Check network cables.

**DeckLink output not starting:** Make sure the Blackmagic plugin is enabled, a DeckLink card is installed, and the output asset is configured via the Settings button.

**Auto-connect not working on restart:** Make sure the Auto-Connect checkbox is checked. It's stored separately from the level — no need to save the level for it to persist.

## Questions?

Contact Lucas Romanenko
