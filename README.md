# CLIFT Terminal VJ Software


CLIFT 0.1 is in developpement terminal-based VJ software for live visual performances using ASCII art. Features 190 visual scenes, dual-deck mixing, effects, and WebSocket integration for live coding overlays. It's made by Crash Server (https://crashserver.fr) and released under MIT License. Feel free to improve and fix if you're interested. 

Best used with Cool Retro Term or some other terminal. Even better with GPU support. Have fun :) 

If you want to support : https://coff.ee/crashserver

## Features

- **190 Visual Scenes** - Organized in 19 categories with 10 scenes each
- **Dual-Deck Mixing** - Professional A/B deck system with crossfader
- **15 Post-Processing Effects** - Real-time visual effects
- **Audio Reactive** - Real-time audio visualization with PipeWire
- **Ableton Link Support** - Sync with other music software
- **WebSocket Integration** - Live coding overlay support
- **BPM Sync** - Automatic beat detection and synchronization
- **Preset System** - Save and load your favorite configurations

## System Requirements

### Dependencies
- **ncurses** - Terminal graphics library
- **libwebsockets** - WebSocket server functionality
- **PipeWire** (optional) - For audio input and visualization
- **Ableton Link SDK** (optional) - For music synchronization
- **GCC/G++** - C/C++ compiler

### Supported Platforms
- Linux (Primary platform)
- macOS (With modifications)
- Any Unix-like system with ncurses support

## Installation

### Quick Install (Linux)

```bash
# Clone or extract CLIFT
cd clift_terminal

# Install dependencies (Ubuntu/Debian)
sudo apt update
sudo apt install build-essential libncurses5-dev libncursesw5-dev libwebsockets-dev

# Install PipeWire support (optional but recommended)
sudo apt install libpipewire-0.3-dev libspa-0.2-dev

# Build CLIFT
cd scripts
chmod +x *.sh
./build_with_pipewire.sh

# Run CLIFT
../clift
```

### Manual Build

```bash
# Basic build (no audio)
cd scripts
./build.sh

# Build with PipeWire audio support
./build_with_pipewire.sh

# Build with custom options
make -f ../src/clift_engine.c -o ../clift -lncurses -lwebsockets
```

### Audio Setup (PipeWire)

For audio-reactive scenes, you need to connect an audio source:

```bash
# Auto-connect default audio
./connect_audio.sh

# List available audio sources
pw-link --input

# Manual connection (replace SOURCE_NAME)
pw-link "SOURCE_NAME:monitor_FL" "clift:input_FL"
pw-link "SOURCE_NAME:monitor_FR" "clift:input_FR"
```

## Basic Usage

### Starting CLIFT

```bash
# Normal mode with UI
./clift

# Fullscreen mode (hide UI)
./clift --fullscreen

# Show help
./clift --help
```

### Essential Controls

- **Space** - Switch between Deck A and Deck B
- **0-9** - Select scene within current category
- **←/→** - Navigate between scene categories
- **f** - Toggle fullscreen mode
- **q** - Quit application

### Scene Navigation

CLIFT has 190 scenes organized in 19 categories (0-18):
- Use **number keys 0-9** to select scenes within the current category
- Use **arrow keys ←/→** to change categories
- Scene numbering: Category × 10 + Scene (e.g., Category 5, Scene 3 = Scene 53)

## Advanced Features

### Dual-Deck Mixing

CLIFT features a professional dual-deck system:

- **Deck A** - Left deck (default active)
- **Deck B** - Right deck
- **Crossfader** - Blend between decks
- **Tab** - Enter crossfader edit mode
- **Space** - Switch active deck

### Effects System

15 real-time post-processing effects:

- **e** - Cycle through effects
- **E** (Shift+e) - Reverse cycle effects
- Effects apply to the final mixed output

### WebSocket Integration

Enable live coding overlays and external control:

```bash
# In CLIFT, press 'w' to start WebSocket server
# Server runs on localhost:8080
```

WebSocket JSON Protocol:
```json
{
  "player": 0,           // 0 or 1 for deck selection
  "code": "print('hello')",  // Code to display
  "executed": "hello",   // Execution result
  "active": true         // Show/hide overlay
}
```

### Audio Reactive Scenes

Special audio-reactive scenes (requires PipeWire):

- **Scene 0** - Audio Bars (spectrum analyzer)
- **Scenes 180-189** - Audio-reactive category
- Real-time 64-band spectrum analysis
- Beat detection and BPM sync

### Ableton Link Synchronization

Sync with other music software:

- Automatic network discovery
- BPM synchronization
- Start/stop sync
- Beat and phase alignment

## Complete Scene Reference

### Category 0: Basic Scenes (0-9)
- **0** - Audio Bars (spectrum analyzer)
- **1** - Rotating Cube
- **2** - DNA Helix
- **3** - Particle Field
- **4** - Torus
- **5** - Fractal Tree
- **6** - Wave Mesh
- **7** - Sphere
- **8** - Spirograph
- **9** - Matrix Rain

### Category 1: Geometric Scenes (10-19)
- **10** - Tunnels
- **11** - Kaleidoscope
- **12** - Mandala
- **13** - Sierpinski
- **14** - Hexagon Grid
- **15** - Tessellations
- **16** - Voronoi Cells
- **17** - Sacred Geometry
- **18** - Polyhedra
- **19** - Maze Generator

### Category 2: Organic Scenes (20-29)
- **20** - Fire Simulation
- **21** - Water Waves
- **22** - Lightning
- **23** - Plasma Clouds
- **24** - Galaxy Spiral
- **25** - Tree of Life
- **26** - Cellular Automata
- **27** - Flocking Birds
- **28** - Wind Patterns
- **29** - Neural Networks

### Category 3: Text/Code Scenes (30-39)
- **30** - Code Rain
- **31** - Terminal Hacking
- **32** - Binary Waterfall
- **33** - ASCII Art Morph
- **34** - Glitch Text
- **35** - Data Streams
- **36** - Circuit Patterns
- **37** - QR Code Rain
- **38** - Font Showcase
- **39** - Terminal Commands

### Category 4: Abstract Scenes (40-49)
- **40** - Noise Field
- **41** - Interference
- **42** - Hologram
- **43** - Digital Rain
- **44** - Glitch Corruption
- **45** - Signal Static
- **46** - Bitmap Fade
- **47** - Pixel Sort
- **48** - Datamosh
- **49** - Buffer Overflow

### Category 5: Infinite Tunnels (50-59)
- **50** - Spiral Tunnel
- **51** - Hex Tunnel
- **52** - Star Tunnel
- **53** - Wormhole
- **54** - Cyber Tunnel
- **55** - Ring Tunnel
- **56** - Matrix Tunnel
- **57** - Speed Tunnel
- **58** - Pulse Tunnel
- **59** - Vortex Tunnel

### Category 6: Nature Scenes (60-69)
- **60** - Ocean Waves
- **61** - Rain Storm
- **62** - Infinite Forest
- **63** - Growing Trees
- **64** - Mountain Range
- **65** - Aurora Borealis
- **66** - Flowing River
- **67** - Desert Dunes
- **68** - Coral Reef
- **69** - Butterfly Garden

### Category 7: Explosions (70-79)
- **70** - Nuclear Blast
- **71** - Building Collapse
- **72** - Meteor Impact
- **73** - Chain Explosions
- **74** - Volcanic Eruption
- **75** - Shockwave Blast
- **76** - Glass Shatter
- **77** - Demolition Blast
- **78** - Supernova Burst
- **79** - Plasma Discharge

### Category 8: Cities (80-89)
- **80** - Cyberpunk City
- **81** - City Lights
- **82** - Skyscraper Forest
- **83** - Urban Decay
- **84** - Future Metropolis
- **85** - City Grid
- **86** - Digital City
- **87** - City Flythrough
- **88** - Neon Districts
- **89** - Urban Canyon

### Category 9: Freestyle (90-99)
- **90** - Black Hole
- **91** - Quantum Field
- **92** - Dimensional Rift
- **93** - Alien Landscape
- **94** - Robot Factory
- **95** - Time Vortex
- **96** - Glitch World
- **97** - Neural Network
- **98** - Cosmic Dance
- **99** - Reality Glitch

### Category 10: Human (100-109)
- **100** - Human Walker
- **101** - Dance Party
- **102** - Martial Arts
- **103** - Human Pyramid
- **104** - Yoga Flow
- **105** - Sports Stadium
- **106** - Robot Dance
- **107** - Crowd Wave
- **108** - Mirror Dance
- **109** - Human Evolution

### Category 11: Warfare (110-119)
- **110** - Fighter Squadron
- **111** - Drone Swarm
- **112** - Strategic Bombing
- **113** - Dogfight
- **114** - Helicopter Assault
- **115** - Stealth Mission
- **116** - Carrier Strike
- **117** - Missile Defense
- **118** - Recon Drone
- **119** - Air Command

### Category 12: Revolution & Eyes (120-129)
- **120** - Street Revolution
- **121** - Barricade Building
- **122** - CCTV Camera
- **123** - Giant Eye
- **124** - Crowd March
- **125** - Displaced Sphere
- **126** - Morphing Cube
- **127** - Protest Rally
- **128** - Surveillance Eyes
- **129** - Fractal Displacement

### Category 13: Film Noir (130-139)
- **130** - Venetian Blinds
- **131** - Silhouette Door
- **132** - Rain Window
- **133** - Detective Coat
- **134** - Femme Fatale
- **135** - Smoke Room
- **136** - Stair Shadows
- **137** - Car Headlights
- **138** - Neon Rain
- **139** - Film Strip

### Category 14: Escher 3D Illusions (140-149)
- **140** - Impossible Stairs
- **141** - Möbius Strip
- **142** - Impossible Cube
- **143** - Penrose Triangle
- **144** - Infinite Corridor
- **145** - Tessellated Reality
- **146** - Gravity Wells
- **147** - Dimensional Shift
- **148** - Fractal Architecture
- **149** - Escher Waterfall

### Category 15: Ikeda-Inspired (150-159)
- **150** - Data Matrix
- **151** - Test Pattern
- **152** - Sine Wave
- **153** - Barcode
- **154** - Pulse
- **155** - Glitch
- **156** - Spectrum
- **157** - Phase
- **158** - Binary
- **159** - Circuit

### Category 16: Giger-Inspired (160-169)
- **160** - Biomech Spine
- **161** - Alien Eggs
- **162** - Mech Tentacles
- **163** - Xenomorph Hive
- **164** - Biomech Skull
- **165** - Face Hugger
- **166** - Biomech Heart
- **167** - Alien Architecture
- **168** - Chestburster
- **169** - Space Jockey

### Category 17: Revolt (170-179)
- **170** - Rising Fists
- **171** - Breaking Chains
- **172** - Crowd March
- **173** - Barricade Building
- **174** - Molotov Cocktails
- **175** - Tear Gas
- **176** - Graffiti Wall
- **177** - Police Line Breaking
- **178** - Flag Burning
- **179** - Victory Dance

### Category 18: Audio Reactive (180-189)
- **180** - Audio 3D Cubes
- **181** - Audio Strobes
- **182** - Audio Explosions
- **183** - Audio Wave Tunnel
- **184** - Audio Spectrum 3D
- **185** - Audio Particles
- **186** - Audio Pulse Rings
- **187** - Audio Waveform 3D
- **188** - Audio Matrix Grid
- **189** - Audio Fractals

## Keyboard Controls Reference

### Navigation
- **0-9** - Select scene within current category
- **←/→** - Change scene category
- **Space** - Switch between Deck A and Deck B

### Display
- **f** - Toggle fullscreen mode
- **h** - Toggle help display
- **i** - Toggle info display

### Effects
- **e** - Next effect
- **E** (Shift+e) - Previous effect
- **r** - Reset effect

### Mixing
- **Tab** - Enter crossfader edit mode
- **+/-** - Adjust crossfader (in edit mode)
- **Enter** - Confirm crossfader setting

### System
- **w** - Toggle WebSocket server
- **a** - Toggle audio input
- **l** - Toggle Ableton Link
- **s** - Save current state
- **Ctrl+S** - Save preset
- **Ctrl+L** - Load preset
- **q** - Quit application

### Scene Parameters
- **1-8** - Adjust scene parameters (when in parameter mode)
- **p** - Enter parameter edit mode

## Testing and Debugging

### WebSocket Testing

```bash
# Test WebSocket connectivity
cd tests
python3 test_websocket.py

# Simple connection test
python3 simple_websocket_test.py

# Debug WebSocket issues
python3 debug_websocket.py

# Web interface test
# Open websocket_test.html in browser
```

### Ableton Link Testing

```bash
# Test Link connectivity
python3 test_ableton_link.py

# Requires: pip install python-abletonlink
```

### Debugging Crashes

```bash
# Run with GDB for crash analysis
python3 test_segfault.py

# Or manually:
gdb ./clift
run
bt  # on crash
```

## Troubleshooting

### Audio Issues

1. **No Audio Reactive Scenes**: Install PipeWire development packages
2. **No Audio Input**: Run `./connect_audio.sh` or check PipeWire connections
3. **Audio Latency**: Adjust PipeWire buffer settings

### WebSocket Issues

1. **Connection Refused**: Check if port 8080 is available
2. **Firewall Blocking**: Open port 8080 for local connections
3. **Browser CORS**: Serve test files from local HTTP server

### Build Issues

1. **Missing ncurses**: Install libncurses5-dev
2. **Missing WebSockets**: Install libwebsockets-dev
3. **C++ Compilation**: Ensure g++ is installed for Link support

### Performance Issues

1. **Slow Rendering**: Reduce terminal size or disable effects
2. **High CPU Usage**: Lower frame rate or disable audio features
3. **Memory Leaks**: Report issues with valgrind output

## License

CLIFT IS RELEASED UNDER MIT LICENSE

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


## Contributing

feel free to fork or contact contact@crashserver.fr

## Credits

Created by Crash Server || 2025 >> crashserver.fr
