#!/usr/bin/env python3
"""
Test script for Ableton Link integration in CLIFT.
Run this alongside CLIFT to verify Link synchronization.
"""

import time
import sys

try:
    import link
except ImportError:
    print("Ableton Link Python module not found.")
    print("Install with: pip install python-abletonlink")
    print("\nNote: You can still test Link with other Link-enabled software")
    print("like Ableton Live, Traktor, or other Link-compatible apps.")
    sys.exit(1)

def test_link():
    # Create Link instance with 120 BPM
    link_instance = link.Link(120)
    link_instance.enabled = True
    link_instance.startStopSyncEnabled = True
    
    print("Ableton Link Test Client")
    print("========================")
    print("This will connect to any Link session on the network")
    print("including CLIFT if Link is enabled (press 'L' in Link page)")
    print("\nPress Ctrl+C to exit\n")
    
    try:
        while True:
            session_state = link_instance.captureSessionState()
            tempo = session_state.tempo()
            beat = session_state.beatAtTime(link_instance.clock().micros(), 4.0)
            phase = session_state.phaseAtTime(link_instance.clock().micros(), 4.0)
            peers = link_instance.numPeers()
            playing = session_state.isPlaying()
            
            print(f"\rPeers: {peers} | BPM: {tempo:.1f} | Beat: {beat:.2f} | Phase: {phase:.2f} | Playing: {playing}  ", end='', flush=True)
            
            time.sleep(0.1)
            
    except KeyboardInterrupt:
        print("\n\nStopping Link test client...")
        link_instance.enabled = False

if __name__ == "__main__":
    test_link()