#!/usr/bin/env python3
"""
Simple TCP client that works with CLIFT's simplified websocket implementation.
This bypasses the standard WebSocket handshake validation.
"""

import socket
import json
import time
import base64
import hashlib

def send_websocket_handshake(sock):
    """Send a WebSocket handshake request"""
    # Generate a random WebSocket key (normally this would be random)
    key = base64.b64encode(b"test_key_1234567").decode()
    
    handshake = (
        f"GET / HTTP/1.1\r\n"
        f"Host: localhost:8080\r\n"
        f"Upgrade: websocket\r\n"
        f"Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        f"Sec-WebSocket-Version: 13\r\n"
        f"\r\n"
    )
    
    sock.send(handshake.encode())
    
    # Read the response
    response = sock.recv(1024).decode()
    print(f"Server response:\n{response}")
    
    if "101 Switching Protocols" in response:
        print("✅ WebSocket handshake successful!")
        return True
    else:
        print("❌ WebSocket handshake failed!")
        return False

def create_websocket_frame(payload):
    """Create a simple WebSocket frame for text data"""
    payload_bytes = payload.encode('utf-8')
    payload_len = len(payload_bytes)
    
    # Simple frame: FIN=1, opcode=1 (text), mask=1
    frame = bytearray()
    frame.append(0x81)  # FIN=1, opcode=1
    
    # Payload length and mask
    if payload_len < 126:
        frame.append(0x80 | payload_len)  # mask=1, length
    else:
        # For simplicity, only handle small payloads
        frame.append(0x80 | 126)
        frame.extend(payload_len.to_bytes(2, 'big'))
    
    # Mask (4 bytes) - using simple mask for testing
    mask = b'\x12\x34\x56\x78'
    frame.extend(mask)
    
    # Masked payload
    masked_payload = bytearray()
    for i, byte in enumerate(payload_bytes):
        masked_payload.append(byte ^ mask[i % 4])
    frame.extend(masked_payload)
    
    return bytes(frame)

def test_clift_websocket_simple():
    """Test CLIFT websocket with simplified implementation"""
    try:
        print("Connecting to CLIFT at localhost:8080...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect(('localhost', 8080))
        print("✅ TCP connection established!")
        
        # Perform WebSocket handshake
        if not send_websocket_handshake(sock):
            return
        
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
            
            # Create JSON payload
            json_payload = json.dumps(message)
            
            # Create and send WebSocket frame
            frame = create_websocket_frame(json_payload)
            sock.send(frame)
            print(f"✅ Sent message {i+1} ({len(frame)} bytes)")
            
            # Wait a bit between messages
            time.sleep(1)
        
        print(f"\n✅ Test completed successfully!")
        
    except ConnectionRefusedError:
        print("❌ Connection refused. Is CLIFT running with websocket server enabled?")
        print("   Start CLIFT, press Tab to go to Monitor page, then press 'W' to enable websocket server")
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        try:
            sock.close()
        except:
            pass

if __name__ == "__main__":
    print("CLIFT Simple WebSocket Test Client")
    print("=" * 50)
    
    try:
        test_clift_websocket_simple()
    except KeyboardInterrupt:
        print("\n👋 Test interrupted by user")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")