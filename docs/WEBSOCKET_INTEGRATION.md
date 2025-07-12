# WebSocket Integration Guide

CLIFT supports real-time WebSocket communication for live coding overlays and external control. This enables integration with live coding environments, custom controllers, and web-based interfaces.

## Overview

The WebSocket server runs on **port 8080** and provides a JSON-based API for:
- Displaying code overlays on visuals
- Showing execution results
- Controlling player decks
- Real-time interaction during performances

## Starting the WebSocket Server

### In CLIFT Application
1. Start CLIFT: `./clift`
2. Press **'w'** to toggle WebSocket server
3. Look for confirmation message in the UI
4. Server runs on `localhost:8080`

### Server Status
- **Green indicator**: Server is running and accepting connections
- **Red indicator**: Server is stopped or failed to start
- **Port conflicts**: If port 8080 is busy, CLIFT will show an error

## JSON Protocol

### Message Format
All WebSocket messages use JSON format:

```json
{
  "player": 0,                    // Deck selection: 0=A, 1=B
  "code": "print('Hello World')", // Code to display as overlay
  "executed": "Hello World",      // Execution result/output
  "active": true                  // Show/hide the overlay
}
```

### Parameters

#### `player` (integer)
- **0** - Target Deck A (left deck)
- **1** - Target Deck B (right deck)
- Controls which deck displays the overlay

#### `code` (string)
- Source code to display as an overlay
- Supports any programming language
- Multi-line code is supported
- Special characters are handled properly

#### `executed` (string)
- Output/result of code execution
- Can be program output, error messages, or status
- Displayed below the code overlay

#### `active` (boolean)
- **true** - Show the overlay on the selected deck
- **false** - Hide the overlay (clear the display)

## Basic Usage Examples

### Python Client Example

```python
import websocket
import json
import time

def send_code_overlay():
    ws = websocket.WebSocket()
    ws.connect("ws://localhost:8080")
    
    # Send code to Deck A
    message = {
        "player": 0,
        "code": "for i in range(10):\n    print(f'Step {i}')",
        "executed": "Step 0\nStep 1\n...\nStep 9",
        "active": True
    }
    
    ws.send(json.dumps(message))
    ws.close()

send_code_overlay()
```

### JavaScript/Browser Example

```javascript
const ws = new WebSocket('ws://localhost:8080');

ws.onopen = function() {
    console.log('Connected to CLIFT');
    
    // Send live coding overlay
    const message = {
        player: 1,  // Deck B
        code: 'const beat = Date.now() % 1000;',
        executed: '> 347',
        active: true
    };
    
    ws.send(JSON.stringify(message));
};

ws.onmessage = function(event) {
    console.log('Response:', event.data);
};
```

### Command Line with curl

```bash
# Not directly supported - WebSocket required
# Use websocat tool instead:

echo '{"player":0,"code":"print(42)","executed":"42","active":true}' | \
websocat ws://localhost:8080
```

## Advanced Integration

### Live Coding Environment Integration

#### SuperCollider Integration
```supercollider
// SuperCollider WebSocket client
(
var ws = WebSocketClient.new;
ws.onOpen = {
    "Connected to CLIFT".postln;
};

// Send code on pattern execution
Pbind(
    \instrument, \default,
    \freq, Pseq([440, 880, 660], inf),
    \dur, 0.5,
    \callback, { |event|
        var code = "Freq: " ++ event[\freq].asString;
        var msg = JSON.stringify(Dict[
            \player -> 0,
            \code -> code,
            \executed -> "♪ " ++ event[\freq],
            \active -> true
        ]);
        ws.send(msg);
    }
).play;
)
```

#### Processing/p5.js Integration
```javascript
// Processing WebSocket integration
let ws;
let frameCounter = 0;

function setup() {
    createCanvas(400, 400);
    ws = new WebSocket('ws://localhost:8080');
    
    ws.onopen = () => console.log('Connected to CLIFT');
}

function draw() {
    background(220);
    
    // Send code every 30 frames
    if (frameCounter % 30 === 0) {
        const code = `fill(${red(get(mouseX, mouseY))});
circle(${mouseX}, ${mouseY}, 50);`;
        
        const message = {
            player: frameCounter % 2,  // Alternate decks
            code: code,
            executed: `Mouse: (${mouseX}, ${mouseY})`,
            active: true
        };
        
        ws.send(JSON.stringify(message));
    }
    
    frameCounter++;
}
```

### Custom Controller Integration

#### MIDI Controller Bridge
```python
import mido
import websocket
import json
import threading

class MIDIToWebSocket:
    def __init__(self):
        self.ws = websocket.WebSocket()
        self.ws.connect("ws://localhost:8080")
        
    def handle_midi(self, msg):
        if msg.type == 'note_on':
            code = f"note_on({msg.note}, {msg.velocity})"
            overlay = {
                "player": 0 if msg.channel == 0 else 1,
                "code": code,
                "executed": f"♪ Note {msg.note}",
                "active": True
            }
            self.ws.send(json.dumps(overlay))
            
            # Clear after 2 seconds
            threading.Timer(2.0, self.clear_overlay).start()
            
    def clear_overlay(self):
        overlay = {"player": 0, "code": "", "executed": "", "active": False}
        self.ws.send(json.dumps(overlay))

# Usage
controller = MIDIToWebSocket()
with mido.open_input() as inport:
    for msg in inport:
        controller.handle_midi(msg)
```

### OSC to WebSocket Bridge
```python
from pythonosc import dispatcher, server
import websocket
import json

class OSCBridge:
    def __init__(self):
        self.ws = websocket.WebSocket()
        self.ws.connect("ws://localhost:8080")
        
    def handle_osc(self, unused_addr, *args):
        code = f"OSC: {args[0]}"
        overlay = {
            "player": int(args[1]) if len(args) > 1 else 0,
            "code": code,
            "executed": f"Value: {args[0]}",
            "active": True
        }
        self.ws.send(json.dumps(overlay))

# Setup OSC server
bridge = OSCBridge()
disp = dispatcher.Dispatcher()
disp.map("/clift/*", bridge.handle_osc)

server = server.osc.ThreadingOSCUDPServer(("127.0.0.1", 5005), disp)
print("OSC server running on port 5005")
server.serve_forever()
```

## Testing the WebSocket Interface

### Basic Connection Test
```bash
cd tests
python3 simple_websocket_test.py
```

### Full Feature Test
```bash
python3 test_websocket.py
```

### Debug Mode
```bash
python3 debug_websocket.py
```

### Web Interface Test
1. Open `tests/websocket_test.html` in a web browser
2. Enter code and click "Send to CLIFT"
3. Toggle between Deck A and Deck B
4. Test show/hide functionality

## Performance Considerations

### Message Rate Limits
- **Recommended**: Max 30 messages/second per deck
- **Burst**: Up to 60 messages/second briefly
- **Continuous**: 10-15 messages/second for best performance

### Message Size Limits
- **Code field**: Up to 4KB of text
- **Executed field**: Up to 2KB of text
- **Total message**: Under 8KB recommended

### Memory Usage
- Each active overlay uses ~50KB of memory
- Old messages are automatically cleaned up
- No persistent storage of messages

## Overlay Display Behavior

### Positioning
- Overlays appear in the **top-right corner** of each deck
- Code appears **above** execution results
- Text automatically wraps at deck boundaries

### Styling
- **Code**: Monospace font, syntax highlighting
- **Results**: Regular font, highlighted background
- **Colors**: Adapt to current scene palette

### Duration
- Overlays persist until explicitly cleared
- New messages replace previous ones
- Use `"active": false` to clear displays

## Security Considerations

### Local Network Only
- WebSocket server binds to **localhost only**
- Not accessible from external networks
- Safe for live performance environments

### No Authentication
- No password or token required
- Suitable for trusted local environments
- Consider firewall rules for additional security

### Input Validation
- JSON messages are validated
- Invalid messages are safely ignored
- No code execution on CLIFT side

## Troubleshooting

### Connection Issues
```bash
# Check if port is available
netstat -ln | grep :8080

# Test with websocat
websocat ws://localhost:8080

# Check CLIFT logs
# Look for WebSocket status messages in CLIFT UI
```

### Message Not Appearing
1. Verify WebSocket connection is established
2. Check JSON format is valid
3. Ensure `"active": true` is set
4. Try different deck (player 0 or 1)

### Performance Issues
1. Reduce message frequency
2. Shorten code/executed text
3. Clear overlays when not needed
4. Check for network congestion

### Browser CORS Issues
```bash
# Serve test files from local HTTP server
cd tests
python3 -m http.server 8000
# Open http://localhost:8000/websocket_test.html
```

## Integration Examples

### Hydra Live Coding
```javascript
// Hydra → CLIFT bridge
let ws = new WebSocket('ws://localhost:8080');

// Override console.log to send to CLIFT
const originalLog = console.log;
console.log = function(...args) {
    originalLog(...args);
    
    const message = {
        player: 0,
        code: document.querySelector('#code-editor').value,
        executed: args.join(' '),
        active: true
    };
    
    ws.send(JSON.stringify(message));
};
```

### TidalCycles Integration
```haskell
-- TidalCycles → WebSocket (requires custom Haskell module)
-- Send current pattern to CLIFT overlay

import Network.WebSockets
import Data.Text

sendToClift :: Pattern -> IO ()
sendToClift pattern = do
    conn <- connect "localhost" 8080 "/"
    let message = encode $ object 
            [ "player" .= (0 :: Int)
            , "code" .= show pattern
            , "executed" .= "♪ Playing"
            , "active" .= True
            ]
    sendTextData conn message
    close conn
```

### Max/MSP Integration
```javascript
// Max/MSP WebSocket external
// Use [jweb] or [sadam.websocket] external

function send_to_clift(code, result) {
    var message = {
        "player": 0,
        "code": code,
        "executed": result,
        "active": true
    };
    
    websocket.send(JSON.stringify(message));
}

// Example usage in Max patch
// [metro 1000] → [send_to_clift "random(100)" "42"]
```

## API Reference Summary

### Connection
- **URL**: `ws://localhost:8080`
- **Protocol**: WebSocket (RFC 6455)
- **Format**: JSON messages

### Required Fields
- `player`: 0 (Deck A) or 1 (Deck B)
- `active`: true/false

### Optional Fields
- `code`: Source code string
- `executed`: Result/output string

### Example Messages
```json
// Show code on Deck A
{"player": 0, "code": "print('hello')", "executed": "hello", "active": true}

// Hide overlay on Deck B
{"player": 1, "active": false}

// Show only result
{"player": 0, "executed": "Processing...", "active": true}
```