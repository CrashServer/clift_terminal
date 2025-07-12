# Audio Setup and PipeWire Integration

CLIFT provides real-time audio visualization and beat-reactive scenes through PipeWire integration. This guide covers audio setup, troubleshooting, and optimization for the best visual experience.

## Overview

CLIFT's audio features include:
- **64-band spectrum analyzer** - Real-time frequency analysis
- **Beat detection** - Automatic BPM detection and sync
- **Audio level monitoring** - Bass, Mid, Treble, and overall volume
- **Audio-reactive scenes** - Scenes that respond to music
- **Visual feedback** - Bar graphs and level indicators

## Prerequisites

### System Requirements
- **PipeWire** - Modern Linux audio server
- **PipeWire development libraries** - For compilation
- **Audio source** - Music player, microphone, or system audio

### Supported Audio Systems
- **PipeWire** (Recommended) - Modern, low-latency audio
- **JACK** (Limited) - Professional audio (via PipeWire JACK bridge)
- **PulseAudio** (Legacy) - Via PipeWire compatibility layer

## Installation

### Ubuntu/Debian
```bash
# Install PipeWire and development packages
sudo apt update
sudo apt install pipewire pipewire-pulse pipewire-jack
sudo apt install libpipewire-0.3-dev libspa-0.2-dev

# Development tools
sudo apt install build-essential pkg-config

# Optional: PipeWire utilities
sudo apt install pipewire-tools qpwgraph
```

### Arch Linux
```bash
# Install PipeWire
sudo pacman -S pipewire pipewire-pulse pipewire-jack
sudo pacman -S pipewire-dev

# Development packages
sudo pacman -S base-devel pkgconf

# Optional: GUI tools
sudo pacman -S qpwgraph helvum
```

### Fedora
```bash
# Install PipeWire (usually pre-installed)
sudo dnf install pipewire pipewire-pulseaudio pipewire-jack-audio-connection-kit
sudo dnf install pipewire-devel

# Development tools
sudo dnf install gcc gcc-c++ make pkgconf-pkg-config

# Optional: Connection tools
sudo dnf install qpwgraph
```

## Building CLIFT with Audio Support

### Compile with PipeWire
```bash
cd clift_terminal/scripts
chmod +x build_with_pipewire.sh
./build_with_pipewire.sh
```

### Manual Build
```bash
# Compile with PipeWire support
gcc -o ../clift ../src/clift_engine.c ../src/audio_pipewire.c \
    -lncurses -lwebsockets -lpipewire-0.3 -lspa-0.2 -pthread

# Or use the Makefile
make -C ../src -f Makefile
```

### Verify Audio Support
```bash
# Check if audio support is compiled
./clift --help | grep -i audio

# Look for "Audio support: PipeWire" in the help output
```

## Audio Connection Setup

### Automatic Connection
```bash
# Use the provided script
cd clift_terminal/scripts
chmod +x connect_audio.sh
./connect_audio.sh
```

The script will:
1. Start CLIFT in background (if not running)
2. Detect available audio sources
3. Connect default audio output to CLIFT input
4. Verify connections

### Manual Connection

#### List Available Audio Sources
```bash
# List all PipeWire nodes
pw-link --input

# Find audio output monitors
pw-link --input | grep monitor

# Example output:
# alsa_output.pci-0000_00_1f.3.analog-stereo:monitor_FL
# alsa_output.pci-0000_00_1f.3.analog-stereo:monitor_FR
```

#### Connect Audio to CLIFT
```bash
# Connect system audio to CLIFT
pw-link "alsa_output.pci-0000_00_1f.3.analog-stereo:monitor_FL" "clift:input_FL"
pw-link "alsa_output.pci-0000_00_1f.3.analog-stereo:monitor_FR" "clift:input_FR"

# Alternative: Connect specific application
pw-link "firefox:output_FL" "clift:input_FL"
pw-link "firefox:output_FR" "clift:input_FR"
```

#### Verify Connections
```bash
# Check active connections
pw-link --links | grep clift

# Should show:
# source_name:monitor_FL -> clift:input_FL
# source_name:monitor_FR -> clift:input_FR
```

### GUI Connection Tools

#### qpwgraph (Recommended)
```bash
# Install and run
sudo apt install qpwgraph  # Ubuntu/Debian
qpwgraph
```

1. Open qpwgraph
2. Find audio source outputs (right side)
3. Find CLIFT inputs (left side)
4. Drag connections between matching channels

#### Helvum (Alternative)
```bash
# Install and run
sudo apt install helvum  # or flatpak install helvum
helvum
```

Similar interface to qpwgraph with different styling.

## Audio Sources Configuration

### System Audio (Most Common)
Connect the system's audio output monitor to CLIFT:
```bash
# Find system audio output
pw-link --input | grep "alsa_output.*monitor"

# Connect stereo channels
pw-link "alsa_output.DEVICE:monitor_FL" "clift:input_FL"
pw-link "alsa_output.DEVICE:monitor_FR" "clift:input_FR"
```

### Specific Applications
Connect audio from specific applications:
```bash
# Music players
pw-link "spotify:output_FL" "clift:input_FL"
pw-link "rhythmbox:output_FL" "clift:input_FL"
pw-link "vlc:output_FL" "clift:input_FL"

# Web browsers
pw-link "firefox:output_FL" "clift:input_FL"
pw-link "chromium:output_FL" "clift:input_FL"

# DJ Software
pw-link "mixxx:Master_L" "clift:input_FL"
pw-link "mixxx:Master_R" "clift:input_FR"
```

### Microphone Input
Connect microphone for live audio visualization:
```bash
# Find microphone input
pw-link --output | grep input

# Connect microphone to CLIFT
pw-link "alsa_input.DEVICE:capture_FL" "clift:input_FL"
pw-link "alsa_input.DEVICE:capture_FR" "clift:input_FR"
```

### JACK Applications
Connect JACK-based applications through PipeWire:
```bash
# List JACK ports
jack_lsp  # or pw-jack jack_lsp

# Connect JACK application
pw-link "system:playback_1" "clift:input_FL"
pw-link "system:playback_2" "clift:input_FR"
```

## Audio-Reactive Scenes

### Scene 0: Audio Bars
The primary audio visualization scene:
- **Real-time spectrum analyzer** with 64 frequency bands
- **Stereo audio processing** (Left/Right channels)
- **Dynamic scaling** adjusts to audio levels
- **Visual representation**: `[x...]` to `[xxxx]` based on intensity

### Audio Reactive Category (Scenes 180-189)
Specialized scenes that respond to audio:
- **180** - Audio 3D Cubes: 3D cubes sized by frequency bands
- **181** - Audio Strobes: Beat-synchronized strobe effects
- **182** - Audio Explosions: Beat-triggered explosion effects
- **183** - Audio Wave Tunnel: Tunnel that pulses with audio
- **184** - Audio Spectrum 3D: 3D frequency spectrum visualization
- **185** - Audio Particles: Particle systems driven by audio
- **186** - Audio Pulse Rings: Concentric rings synchronized to beats
- **187** - Audio Waveform 3D: 3D waveform representation
- **188** - Audio Matrix Grid: Matrix effect responding to audio
- **189** - Audio Fractals: Fractal patterns changing with music

### Beat Detection
CLIFT automatically detects beats and provides:
- **BPM calculation** - Displayed in the UI
- **Beat indicators** - Visual cues on beat detection
- **Beat synchronization** - For scenes and effects

### Audio Level Monitoring
Four-band audio analysis:
- **Bass** - Low frequencies (20-250 Hz)
- **Mid** - Mid frequencies (250-4000 Hz)  
- **Treble** - High frequencies (4000-20000 Hz)
- **Volume** - Overall audio level

## Performance Optimization

### Buffer Size Configuration
Adjust PipeWire buffer settings for optimal performance:

```bash
# Edit PipeWire configuration
mkdir -p ~/.config/pipewire
cp /usr/share/pipewire/pipewire.conf ~/.config/pipewire/

# Edit ~/.config/pipewire/pipewire.conf
# Adjust these settings:
# default.clock.rate = 48000
# default.clock.quantum = 1024
# default.clock.min-quantum = 32
# default.clock.max-quantum = 2048
```

### Latency Optimization
```bash
# Low latency settings
export PIPEWIRE_LATENCY=32/48000  # 0.67ms latency

# Run CLIFT with low latency
PIPEWIRE_LATENCY=32/48000 ./clift
```

### CPU Usage Optimization
- **Reduce spectrum bands**: Modify source code to use fewer than 64 bands
- **Lower frame rate**: Reduce audio processing frequency
- **Disable effects**: Turn off post-processing effects during audio scenes
- **Terminal size**: Use smaller terminal windows

## Troubleshooting

### No Audio Input

#### Check PipeWire Status
```bash
# Verify PipeWire is running
systemctl --user status pipewire

# Restart if needed
systemctl --user restart pipewire
```

#### Check CLIFT Audio Support
```bash
# Verify audio compilation
./clift --help | grep -i pipewire

# If not found, rebuild with audio support
cd scripts
./build_with_pipewire.sh
```

#### Verify Connections
```bash
# Check if CLIFT input ports exist
pw-link --input | grep clift

# If not found, CLIFT may not be running or compiled without audio
```

### Audio Latency Issues

#### Reduce Buffer Size
```bash
# Try smaller quantum sizes
export PIPEWIRE_LATENCY=64/48000
./clift
```

#### Check Audio Priority
```bash
# Run with higher priority
sudo nice -n -10 ./clift

# Or use real-time priority (advanced)
sudo chrt -f 50 ./clift
```

### Stuttering or Glitches

#### Audio Dropouts
1. **Increase buffer size**: Use larger quantum values
2. **Check CPU usage**: Use `htop` to monitor system load
3. **Close other audio applications**: Reduce audio system load
4. **Update PipeWire**: Ensure latest version

#### Visual Stuttering
1. **Reduce terminal size**: Smaller window = better performance
2. **Disable effects**: Press 'e' to turn off post-processing
3. **Lower frame rate**: Modify source code frame rate settings

### Connection Issues

#### Automatic Connection Fails
```bash
# Manual connection with specific device
pw-link --input  # Find exact device names
pw-link "EXACT_DEVICE_NAME:monitor_FL" "clift:input_FL"
```

#### Permission Issues
```bash
# Add user to audio group
sudo usermod -a -G audio $USER

# Logout and login again for changes to take effect
```

#### Port Not Available
```bash
# Check if another application is using the audio
pw-link --links | grep -v clift

# Disconnect conflicting connections
pw-link --disconnect "source:port" "other_app:input"
```

## Advanced Audio Configuration

### Multiple Audio Sources
Connect multiple sources to CLIFT simultaneously:
```bash
# Create a virtual audio mixer
pw-loopback -P "CLIFT Mixer" -C "CLIFT Input"

# Connect multiple sources to the mixer
pw-link "spotify:output_FL" "CLIFT Mixer:input_FL"
pw-link "firefox:output_FL" "CLIFT Mixer:input_FL"

# Connect mixer to CLIFT
pw-link "CLIFT Mixer:output_FL" "clift:input_FL"
```

### Audio Processing Pipeline
Add audio processing between source and CLIFT:
```bash
# Example with LADSPA effects
pw-filter --plugin-path=/usr/lib/ladspa \
    --plugin=amp.so --plugin-args="gain=2.0" \
    --capture="audio_source:output" \
    --playback="clift:input"
```

### Recording Audio with Visuals
Record audio alongside CLIFT output:
```bash
# Record audio to file while visualizing
pw-record --target="audio_source:monitor" audio.wav &
./clift
```

## Integration with DJ Software

### Mixxx Integration
```bash
# Connect Mixxx master output to CLIFT
pw-link "Mixxx:Master" "clift:input_FL"
pw-link "Mixxx:Master" "clift:input_FR"

# Or connect individual decks
pw-link "Mixxx:Deck1" "clift:input_FL"  # Deck 1 → CLIFT Deck A
pw-link "Mixxx:Deck2" "clift:input_FR"  # Deck 2 → CLIFT Deck B
```

### Traktor Integration
```bash
# Traktor via JACK bridge
pw-jack traktor &

# Connect Traktor outputs
pw-link "system:playback_1" "clift:input_FL"
pw-link "system:playback_2" "clift:input_FR"
```

### Virtual DJ Integration
```bash
# VirtualDJ outputs to system audio
# Connect system audio monitor to CLIFT
pw-link "alsa_output.hw_0_0:monitor_FL" "clift:input_FL"
pw-link "alsa_output.hw_0_0:monitor_FR" "clift:input_FR"
```

## Audio Testing and Validation

### Test Audio Input
```bash
# Generate test tone
pw-cat --playback - < /dev/zero | pw-cat --record - | \
    sox -t raw -r 48000 -e signed -b 16 -c 2 - -t wav test_tone.wav trim 0 5

# Play test tone while running CLIFT
aplay test_tone.wav
```

### Verify Spectrum Analysis
1. Start CLIFT and go to Scene 0 (Audio Bars)
2. Play music with clear bass, mid, and treble
3. Verify that frequency bars respond appropriately:
   - **Left side**: Low frequencies (bass)
   - **Middle**: Mid frequencies
   - **Right side**: High frequencies (treble)

### Beat Detection Validation
1. Play music with clear, steady beat
2. Watch the BPM display in CLIFT UI
3. Verify beat indicators flash with the music
4. Test with different BPM ranges (60-180 BPM)

## Performance Monitoring

### Audio Metrics
```bash
# Monitor audio latency
pw-metadata | grep latency

# Check audio format
pw-metadata | grep format

# Monitor buffer usage
pw-metadata | grep quantum
```

### CLIFT Audio Statistics
- Check the UI for audio statistics:
  - **Input level**: Should show activity during audio playback
  - **BPM**: Should display detected tempo
  - **Spectrum**: Frequency bars should respond to audio

### System Performance
```bash
# Monitor CPU usage
htop -p $(pgrep clift)

# Check memory usage
ps aux | grep clift

# Monitor audio system
pw-top
```

## FAQ

**Q: Why is there no audio visualization?**
A: Check that CLIFT is compiled with PipeWire support, audio connections are made, and you're using an audio-reactive scene (Scene 0 or 180-189).

**Q: Audio visualization is delayed/laggy?**
A: Reduce PipeWire buffer size, close other audio applications, or use a smaller terminal window.

**Q: Can I use ALSA instead of PipeWire?**
A: CLIFT is designed for PipeWire. For ALSA, use the PipeWire-ALSA compatibility layer.

**Q: How do I connect Bluetooth audio?**
A: Connect Bluetooth device normally, then use `pw-link` to connect the Bluetooth output monitor to CLIFT input.

**Q: Can I use USB audio interfaces?**
A: Yes, USB audio interfaces work through PipeWire. Use `pw-link --input` to find the device and connect as usual.

**Q: Why does audio visualization work in some scenes but not others?**
A: Only Scene 0 and Scenes 180-189 are audio-reactive. Other scenes don't respond to audio input.