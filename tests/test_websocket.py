#!/usr/bin/env python3
"""
Simple Python WebSocket client to test CLIFT's live coding websocket server.
Usage: python3 test_websocket.py
"""

import asyncio
import websockets
import json
import time

async def test_clift_websocket():
    uri = "ws://localhost:8080"
    
    try:
        print(f"Connecting to CLIFT at {uri}...")
        async with websockets.connect(uri) as websocket:
            print("✅ Connected to CLIFT websocket server!")
            
            # Test sending player data
            test_messages = [
                {
                    "player": 0,
                    "code": "synth :saw, note: :c4\nplay :kick\nsleep 0.5",
                    "executed": "sleep 0.5",
                    "active": True
                },
                {
                    "player": 1,
                    "code": "live_loop :bass do\n  use_synth :fm\n  play :c2\n  sleep 1\nend",
                    "executed": "sleep 1",
                    "active": True
                }
            ]
            
            for i, message in enumerate(test_messages):
                print(f"\n📤 Sending Player {message['player']} data:")
                print(f"   Code: {repr(message['code'][:30] + '...')}")
                
                await websocket.send(json.dumps(message))
                print(f"✅ Sent message {i+1}")
                
                # Wait a bit between messages
                await asyncio.sleep(1)
            
            # Send some updates
            print(f"\n🔄 Sending live updates...")
            for i in range(5):
                update = {
                    "player": i % 2,
                    "code": f"# Live update {i+1}\nplay :bd_haus\nsleep 0.25",
                    "executed": "sleep 0.25",
                    "active": True
                }
                
                await websocket.send(json.dumps(update))
                print(f"📤 Sent update {i+1} for Player {update['player']}")
                await asyncio.sleep(2)
            
            print(f"\n✅ Test completed successfully!")
            
    except (ConnectionRefusedError, OSError) as e:
        print("❌ Connection refused. Is CLIFT running with websocket server enabled?")
        print("   Start CLIFT, press Tab to go to Monitor page, then press 'W' to enable websocket server")
    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    print("CLIFT WebSocket Test Client")
    print("=" * 40)
    
    try:
        asyncio.run(test_clift_websocket())
    except KeyboardInterrupt:
        print("\n👋 Test interrupted by user")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")