#!/usr/bin/env python3
"""
Test script to help identify segfault location in CLIFT
"""
import subprocess
import time
import signal
import os

def test_clift_with_gdb():
    """Run CLIFT under gdb to catch segfault"""
    print("Running CLIFT under gdb to catch segfault...")
    print("When it crashes, gdb will show where the segfault occurred")
    print("-" * 50)
    
    gdb_commands = """
run
bt
quit
"""
    
    # Write gdb commands to a temporary file
    with open('/tmp/gdb_commands.txt', 'w') as f:
        f.write(gdb_commands)
    
    # Run CLIFT under gdb
    try:
        subprocess.run(['gdb', '-batch', '-x', '/tmp/gdb_commands.txt', './clift_engine'], 
                      cwd='/home/crashserver/SESSION/APPS/NEWAPP/clift')
    except Exception as e:
        print(f"Error running gdb: {e}")
    finally:
        if os.path.exists('/tmp/gdb_commands.txt'):
            os.remove('/tmp/gdb_commands.txt')

def test_specific_scenes():
    """Test specific scenes that might cause issues"""
    print("\nTesting specific scenes...")
    print("This will help identify if a specific scene causes the crash")
    print("-" * 50)
    
    # Scenes to test (including the ones we modified)
    test_scenes = [
        (149, "Escher Waterfall"),
        (148, "Escher Stairs"),
        (140, "Penrose Triangle"),
        (0, "Matrix Rain"),
        (1, "Fire"),
    ]
    
    for scene_id, scene_name in test_scenes:
        print(f"\nTesting scene {scene_id}: {scene_name}")
        print("Run CLIFT and navigate to this scene manually")
        print("Press Ctrl+C if it crashes or works fine")
        try:
            input("Press Enter when ready to test next scene...")
        except KeyboardInterrupt:
            print("\nSkipping to next test...")

def check_core_dump():
    """Check if there's a core dump we can analyze"""
    print("\nChecking for core dumps...")
    
    # Check if core dumps are enabled
    result = subprocess.run(['ulimit', '-c'], shell=True, capture_output=True, text=True)
    print(f"Core dump limit: {result.stdout.strip()}")
    
    # Look for core files
    core_files = subprocess.run(['find', '.', '-name', 'core*', '-type', 'f'], 
                               capture_output=True, text=True)
    if core_files.stdout:
        print("Found core dumps:")
        print(core_files.stdout)
        print("\nTo analyze: gdb ./clift_engine core_file")
    else:
        print("No core dumps found")
        print("To enable core dumps: ulimit -c unlimited")

def test_websocket_overlay():
    """Test if the segfault is related to websocket/overlay"""
    print("\nTesting websocket and overlay functionality...")
    print("-" * 50)
    print("1. Start CLIFT")
    print("2. DON'T enable websocket or overlay")
    print("3. Navigate through scenes")
    print("4. If no crash, then enable websocket (Tab -> Monitor -> W)")
    print("5. If no crash, then enable overlay (O)")
    print("6. Send websocket data with test_websocket.py")
    print("\nThis will help identify if the issue is with our recent changes")

def main():
    print("CLIFT Segfault Debugging Helper")
    print("=" * 50)
    
    while True:
        print("\nChoose a test:")
        print("1. Run CLIFT under gdb (shows exact crash location)")
        print("2. Test specific scenes manually")
        print("3. Check for core dumps")
        print("4. Test websocket/overlay features")
        print("5. Exit")
        
        choice = input("\nEnter choice (1-5): ")
        
        if choice == '1':
            test_clift_with_gdb()
        elif choice == '2':
            test_specific_scenes()
        elif choice == '3':
            check_core_dump()
        elif choice == '4':
            test_websocket_overlay()
        elif choice == '5':
            break
        else:
            print("Invalid choice")

if __name__ == "__main__":
    main()