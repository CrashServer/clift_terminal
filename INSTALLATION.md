# Quick Installation Guide

## Prerequisites

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential libncurses5-dev libncursesw5-dev
```

### Optional: Audio Support
```bash
sudo apt install libpipewire-0.3-dev libspa-0.2-dev
```

## Build and Run

```bash
cd scripts
./build.sh
../clift
```

For audio features:
```bash
./build_with_pipewire.sh
./connect_audio.sh
../clift
```

## Quick Start

- **Space** - Switch decks
- **0-9** - Select scenes
- **←/→** - Navigate categories  
- **f** - Fullscreen
- **w** - WebSocket server
- **q** - Quit

See README.md for complete documentation.