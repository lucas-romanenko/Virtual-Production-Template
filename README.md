# Virtual Production Template — Unreal Engine 5.7

A ready-to-go Unreal Engine 5.7 template project for LED wall virtual production with Dobot Nova 5 robot camera tracking. Includes the **Dobot LiveLink** plugin pre-installed and pre-compiled — clone, open, and start tracking.

---

## What's Included

- **Dobot LiveLink Plugin** — custom LiveLink plugin for real-time Dobot Nova 5 camera tracking (pre-compiled, no Visual Studio needed)
- **BP_VirtualProductionStage** — all-in-one Blueprint with LED wall, floor, robot base, tracked camera, and Scene Capture
- **BP_DobotTrackedCamera** — standalone tracked camera Blueprint for adding extra cameras
- **Demo Level** — pre-configured level with stage and camera ready to go
- **Fake Robot Scripts** — Python TCP servers that simulate Dobot robot data for testing without hardware
- **DeckLink Output Routing** — assign cameras to Blackmagic SDI output ports from within the plugin panel

---

## Requirements

- **Unreal Engine 5.7**
- **Windows 10/11**
- No Visual Studio or other dev tools required — the plugin is pre-compiled

### Optional

- Dobot Nova 5 robot arm (firmware v3.5.2+)
- Blackmagic DeckLink card + Blackmagic Media Player plugin for SDI output
- Python 3.x for running fake robot test scripts

---

## Getting Started

### 1. Clone and Open

```bash
git clone https://github.com/lucas-romanenko/Virtual-Production-Template.git
```

Open **Virtual_Production.uproject** in Unreal Engine 5.7. Wait for shaders to compile on first open.

### 2. Open the Dobot LiveLink Panel

Go to **Window → Virtual Production → Dobot LiveLink**

This is the main control panel. Everything — camera selection, settings, robot connections, and DeckLink output — is managed from here.

### 3. Select or Create a Camera

Cameras in the level with a DobotLiveLinkCamera component show up in the **Active Camera** dropdown. The template level already has cameras placed. You can also click **+ Add Camera** to spawn a new one.

Each camera gets a unique **Subject Name** (DobotCamera, DobotCamera_2, etc.).

### 4. Connect to a Dobot Robot

1. Click **Add Dobot Connection**
2. Enter the **Dobot IP Address** (default: 192.168.5.1 for LAN1)
3. Enter the **Dobot Port** (default: 30004)
4. Check **Auto-Connect** to reconnect automatically after restarts or drops
5. Click **Connect**

Tracking enables automatically. The status shows:

- 🟢 **Connected — Receiving Data**
- 🔴 **Disconnected**
- 🟠 **Connection Lost**
- 🟡 **Reconnecting...**

### 5. DeckLink Output (Optional)

The DeckLink Output section at the bottom of the panel routes cameras to SDI output ports.

1. Set **Number of Outputs** to match your DeckLink card
2. Assign a camera to each port
3. Click **Settings** to configure the Blackmagic output asset
4. Click **Start** to begin outputting

---

## Testing Without Hardware

Two Python scripts simulate Dobot robot data over TCP, so you can test the full tracking pipeline without a physical robot.

### Start a Fake Robot

```bash
python Tests/fake_robot.py       # Circle pattern on port 30004
python Tests/fake_robot_2.py     # Figure-8 pattern on port 30005
```

In the Dobot LiveLink panel, set IP to `127.0.0.1`, port to `30004`, and connect. The camera will start moving.

Close the Python script to test disconnect detection. Restart it to test auto-reconnect.

---

## Network Setup

Connect your PC and the Dobot robot to the same network.

| Connection | Default Dobot IP |
|-----------|-----------------|
| LAN1 (ethernet) | 192.168.5.1 |
| LAN2 (ethernet) | 192.168.100.1 |
| WiFi | 192.168.1.6 |

The robot streams position data on **port 30004** at 125Hz (8ms). Make sure this port is not blocked by firewall.

```
[Dobot Nova 5] --ethernet--> [Network Switch] <--ethernet-- [Unreal PC]
   192.168.5.1                                                192.168.5.x
```

---

## Multi-Camera Setup

### Independent Tracking (two robots, two cameras)

Select Camera 1, connect to robot at 192.168.5.1:30004. Select Camera 2, connect to a different robot at 192.168.5.2:30004. Each camera tracks independently.

### Shared Tracking (one robot, two cameras)

Give both cameras the same Subject Name and connect both to the same robot. The plugin reuses the LiveLink source — one TCP connection, both cameras move together but can have different focal lengths and settings.

---

## Auto-Connect & 24/7 Operation

For production environments that run around the clock:

- Check **Auto-Connect** on each camera that should reconnect automatically
- If the connection drops, the plugin retries every 3 seconds
- If Unreal crashes and reopens, cameras with Auto-Connect reconnect after a 5-second startup delay
- Auto-Connect is stored in the plugin config — persists across restarts independent of level saves

---

## Dobot LiveLink Plugin

The plugin is included in `Plugins/DobotLiveLink/`. For details on the plugin architecture, data protocol, coordinate mapping, delay compensation, and DeckLink routing, see the full [Dobot LiveLink Plugin Documentation](Plugins/DobotLiveLink/README.md).

For using the plugin in your own project (without this template), a standalone plugin repository will be available separately.

---

## Troubleshooting

**Panel won't open** — Try Window → Load Layout → Default Editor Layout, then reopen.

**Camera not moving** — Check Enable Tracking is checked. Verify Subject Name matches. Check Output Log for errors.

**Connection keeps dropping** — Check network cables. Make sure the robot is powered on and streaming data.

**Auto-connect not working** — Make sure the checkbox is checked in the panel. There's a 5-second delay on startup.

**DeckLink output not starting** — Enable Blackmagic Media Player plugin. Click Settings to configure the output. A DeckLink card must be installed.

**Duplicate subject names** — Each camera needs a unique name. The panel shows a red message if you try to use one that's taken.

---

## Credits

Developed by Lucas Romanenko for ARRISE Powering Pragmatic Play virtual production workflows.
