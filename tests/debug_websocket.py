#!/usr/bin/env python3
"""
Debug script to test CLIFT websocket step by step
"""

import socket
import time

def test_tcp_connection():
    """Test basic TCP connection"""
    try:
        print("1. Testing TCP connection...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)  # 5 second timeout
        sock.connect(('localhost', 8080))
        print("✅ TCP connection successful")
        
        print("2. Sending WebSocket handshake...")
        handshake = (
            "GET / HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        )
        
        sock.send(handshake.encode())
        print("✅ Handshake sent")
        
        print("3. Waiting for response...")
        sock.settimeout(10)  # 10 seconds to receive response
        response = sock.recv(1024)
        print(f"✅ Received response ({len(response)} bytes):")
        print(response.decode('utf-8', errors='ignore'))
        
        sock.close()
        return True
        
    except socket.timeout:
        print("❌ Timeout waiting for response")
        return False
    except ConnectionRefusedError:
        print("❌ Connection refused")
        return False
    except Exception as e:
        print(f"❌ Error: {e}")
        return False

if __name__ == "__main__":
    print("CLIFT WebSocket Debug Test")
    print("=" * 30)
    test_tcp_connection()