# Ableton Link Synchronization

CLIFT supports Ableton Link for real-time synchronization with other music software, enabling tight integration with DAWs, DJ software, and live performance tools.

## Overview

Ableton Link is a technology that synchronizes:
- **Tempo (BPM)** - Shared across all connected applications
- **Beat grid** - Synchronized beat positions
- **Start/Stop** - Transport control across applications
- **Phase alignment** - Precise beat and bar alignment

### Benefits for VJ Performances
- **Tight sync** with DJ sets and live music
- **Automatic BPM detection** from connected software
- **Beat-perfect** scene changes and effects
- **Professional workflow** integration

## Prerequisites

### Ableton Link SDK
CLIFT requires the Ableton Link SDK for compilation:

```bash
# Download Ableton Link SDK
cd /tmp
git clone https://github.com/Ableton/link.git
cd link
git submodule update --init --recursive

# Copy to system location (optional)
sudo cp -r include/ableton /usr/local/include/
sudo cp -r modules /usr/local/include/ableton/
```

### Supported Applications
CLIFT can sync with any Link-enabled application:

#### DAWs (Digital Audio Workstations)
- **Ableton Live** - Native Link support
- **Logic Pro** - Built-in Link support
- **Reaper** - Via JS plugins
- **Bitwig Studio** - Native Link support
- **Reason** - Native Link support

#### DJ Software
- **Traktor Pro** - Native Link support
- **Serato DJ** - Link support in newer versions
- **djay Pro** - Native Link support
- **Mixxx** - Open-source with Link support
- **VirtualDJ** - Link support available

#### Live Coding Environments
- **SuperCollider** - Via Link Quark
- **TidalCycles** - Via Link integration
- **Sonic Pi** - Native Link support
- **Pure Data** - Via Link externals
- **Max/MSP** - Via Link external

#### Mobile Apps
- **Ableton Note** - Native Link support
- **KORG Gadget** - Link support
- **Novation Launchpad** - Link support
- **Figure** - Link support

## Building CLIFT with Link Support

### Install Dependencies
```bash
# Ubuntu/Debian
sudo apt install build-essential cmake libasound2-dev

# Arch Linux
sudo pacman -S base-devel cmake alsa-lib

# macOS
brew install cmake
```

### Compile with Link
```bash
cd clift_terminal/scripts

# Build with Link support (automatic SDK detection)
./build_with_pipewire.sh

# Or build manually with Link
g++ -std=c++14 -o ../clift \
    ../src/clift_engine.c ../src/link_wrapper.cpp \
    -I/usr/local/include \
    -lncurses -lwebsockets -lpipewire-0.3 -pthread
```

### Verify Link Support
```bash
# Check if Link is available
./clift --help | grep -i link

# Should show "Ableton Link: Enabled"
```

## Using Ableton Link in CLIFT

### Enable Link Synchronization
```bash
# Start CLIFT
./clift

# In CLIFT, press 'l' to toggle Link
# Look for Link status in the UI:
# - "Link: OFF" - Link disabled
# - "Link: ON (0 peers)" - Link enabled, no connections
# - "Link: ON (2 peers)" - Link enabled, 2 other apps connected
```

### Link Status Display
The CLIFT UI shows Link information:
- **BPM**: Current Link tempo (shared across apps)
- **Beat**: Current beat position
- **Peers**: Number of connected Link applications
- **Phase**: Beat phase alignment

### Automatic BPM Sync
When Link is enabled:
1. **BPM changes** in any connected app affect CLIFT
2. **Beat detection** uses Link timing instead of audio analysis
3. **Scene timing** syncs to Link beats
4. **Effects** trigger on Link beats

## Link Session Management

### Starting a Link Session
```bash
# Method 1: Start CLIFT first
./clift
# Press 'l' to enable Link
# Other apps will automatically discover CLIFT

# Method 2: Join existing session
# Start other Link-enabled app first
# Start CLIFT and press 'l'
# CLIFT joins the existing session
```

### Session Discovery
Link uses **zero-configuration networking**:
- Applications automatically discover each other on the local network
- **No manual configuration** required
- Works across **WiFi** and **Ethernet**
- **Firewall-friendly** (uses standard ports)

### Tempo Control
In a Link session:
- **Any application** can change the BPM
- **Changes propagate** to all connected applications
- **CLIFT displays** the current session BPM
- **Beat-sync features** use the session tempo

## Integration Examples

### Ableton Live + CLIFT
```bash
# 1. Start Ableton Live
# 2. Enable Link in Live (Options → Link MIDI → Link: ON)
# 3. Start CLIFT and press 'l'
# 4. Verify connection in Live (should show "1 Link")
# 5. Set tempo in Live - CLIFT follows automatically
```

### Traktor + CLIFT
```bash
# 1. Start Traktor Pro
# 2. Enable Link (Preferences → Mixing → Ableton Link)
# 3. Start CLIFT and press 'l'
# 4. Both applications now share BPM and beat grid
# 5. Beat-matched mixing with synchronized visuals
```

### SuperCollider + CLIFT
```SuperCollider
// Install Link Quark in SuperCollider
Quarks.install("Link")

// Enable Link in SuperCollider
(
~link = LinkClock(120); // 120 BPM
~link.enable;
)

// Start CLIFT with Link enabled
// Both will share tempo and beat synchronization
```

### TidalCycles + CLIFT
```haskell
-- Enable Link in TidalCycles boot file
tidal <- startTidal (superdirtTarget {oLatency = 0.1, oAddress = "127.0.0.1"}) 
         (defaultConfig {cFrameTimespan = 1/20, cTempoAddr = "127.0.0.1", cCtrlAddr = "127.0.0.1"})

-- Set Link-compatible tempo
setcps (120/60/4) -- 120 BPM

-- Start CLIFT with Link enabled for synchronized visuals
```

### Mixxx + CLIFT
```bash
# 1. Install Mixxx with Link support
sudo apt install mixxx

# 2. Enable Link in Mixxx
# Preferences → BPM Detection → Ableton Link → Enable

# 3. Start CLIFT and enable Link
./clift
# Press 'l' in CLIFT

# 4. Beat-matched DJing with real-time visual sync
```

## Advanced Link Features

### Transport Synchronization
Some Link applications support start/stop sync:

```bash
# In CLIFT, Link transport controls:
# - Spacebar: Start/stop sync (if supported by session)
# - Beat alignment: Automatic alignment to Link beat grid
```

### Beat Phase Alignment
Link provides precise beat alignment:
- **Downbeat sync**: All applications align to beat 1
- **Bar alignment**: 4/4 time signature support
- **Sub-beat precision**: Timing accurate to milliseconds

### Session Tempo Changes
Handle tempo changes gracefully:
```bash
# Monitor tempo changes in CLIFT
# - BPM display updates automatically
# - Audio analysis adapts to new tempo
# - Scene timing adjusts to maintain sync
```

## Performance Optimization

### Network Configuration
For optimal Link performance:

```bash
# Ensure multicast is enabled
sudo sysctl net.ipv4.conf.all.mc_forwarding=1

# Check network interface
ip addr show | grep -E "inet.*brd"

# For WiFi networks, ensure multicast works
sudo iwconfig wlan0 power off  # Disable power saving
```

### Latency Optimization
```bash
# Run CLIFT with higher priority for better timing
sudo nice -n -10 ./clift

# Or use real-time scheduling
sudo chrt -f 50 ./clift
```

### Buffer Settings
```bash
# Optimize audio buffers for Link sync
export PIPEWIRE_LATENCY=64/48000
./clift
```

## Troubleshooting

### Link Not Connecting

#### Check Network Connectivity
```bash
# Verify local network
ping -c 3 $(ip route | grep default | awk '{print $3}')

# Check if other devices can see each other
# Use Link-enabled mobile app to test
```

#### Firewall Issues
```bash
# Link uses UDP port 20808
sudo ufw allow 20808/udp

# Or temporarily disable firewall for testing
sudo ufw disable
```

#### Application Discovery
```bash
# Restart network services
sudo systemctl restart NetworkManager

# Or restart CLIFT Link
# Press 'l' twice in CLIFT (off, then on)
```

### Timing Issues

#### Sync Drift
```bash
# Check system audio latency
pw-metadata | grep latency

# Reduce audio buffers
export PIPEWIRE_LATENCY=32/48000
./clift
```

#### Beat Misalignment
```bash
# Verify Link is providing beat timing
# Check that CLIFT shows "Link: ON" not just enabled

# Restart Link session:
# 1. Disable Link in all applications
# 2. Enable Link in master tempo app first
# 3. Enable Link in CLIFT
```

### Performance Issues

#### High CPU Usage
```bash
# Monitor Link thread usage
top -H -p $(pgrep clift)

# Reduce Link update rate (modify source code)
# Or use fewer Link features
```

#### Network Congestion
```bash
# Check network traffic
sudo netstat -i

# Use wired connection instead of WiFi
# Or create dedicated network for Link applications
```

## Link Protocol Details

### Technical Specifications
- **Protocol**: Custom UDP-based protocol
- **Port**: 20808 (UDP)
- **Discovery**: Multicast DNS (mDNS)
- **Timing**: High-precision timestamping
- **Latency**: Sub-millisecond synchronization

### Session Architecture
```
[DAW] ←→ [Link Session] ←→ [CLIFT]
  ↑                           ↓
[DJ Software] ←→ [Mobile App] ←→ [Live Coding]
```

### Timing Accuracy
- **Beat precision**: ±1 millisecond
- **BPM stability**: 0.001 BPM accuracy
- **Phase alignment**: Sample-accurate
- **Network tolerance**: Works with 100ms+ network latency

## Testing Link Integration

### Basic Connectivity Test
```bash
# Use test script
cd tests
python3 test_ableton_link.py

# Should show:
# - Link session created
# - BPM setting and reading
# - Peer discovery (if other apps are running)
```

### Multi-Application Test
1. **Start Ableton Live** with Link enabled
2. **Start CLIFT** and enable Link ('l' key)
3. **Verify connection** (Live shows "1 Link", CLIFT shows "1 peer")
4. **Change tempo** in Live, verify CLIFT BPM updates
5. **Test beat sync** by watching beat indicators

### Mobile App Test
1. **Install Link-enabled mobile app** (e.g., Ableton Note)
2. **Enable Link** in mobile app
3. **Start CLIFT** with Link enabled
4. **Verify sync** across all devices
5. **Test tempo changes** from different applications

## Best Practices

### Session Leadership
- **Designate one app** as the tempo master
- **Avoid simultaneous** tempo changes
- **Use consistent** time signatures (4/4 recommended)

### Network Setup
- **Use wired connections** when possible
- **Minimize network hops** between devices
- **Ensure multicast** is enabled on all devices

### Performance Considerations
- **Start Link apps** before beginning performance
- **Test connections** before going live
- **Have backup sync methods** (audio analysis) ready

### Integration Workflow
1. **Plan your setup** - Which apps will control tempo?
2. **Test connections** - Verify all apps can see each other
3. **Set initial tempo** - Establish session BPM
4. **Check synchronization** - Verify beat alignment
5. **Practice transitions** - Test tempo changes during performance

## Link API Reference

### CLIFT Link Controls
- **'l'** - Toggle Link enable/disable
- **Link status** - Displayed in UI
- **BPM sync** - Automatic when Link is enabled
- **Beat sync** - Scene timing follows Link beats

### Link Session Information
```bash
# Information available in CLIFT UI:
# - Link Status: ON/OFF
# - Peer Count: Number of connected applications
# - Session BPM: Current Link tempo
# - Beat Position: Current beat in the session
# - Phase: Beat phase (0.0-1.0)
```

### Integration Points
- **BPM Detection**: Link tempo overrides audio analysis
- **Beat Timing**: Link beats trigger scene events
- **Transport**: Start/stop sync (where supported)
- **Tempo Changes**: Automatic adaptation to session tempo

This comprehensive Link integration enables CLIFT to be a professional part of any Link-enabled performance setup, providing synchronized visuals that stay perfectly in time with the music.