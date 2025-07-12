#!/bin/bash

# Helper script to connect audio sources to CLIFT

# Check if running in quiet mode (when called from CLIFT)
QUIET=false
if [ "$2" = "quiet" ]; then
    QUIET=true
fi

if [ "$QUIET" = false ]; then
    echo "CLIFT Audio Connection Helper"
    echo "============================"
fi

# Check if CLIFT is running
if ! pw-link -i | grep -q "CLIFT"; then
    if [ "$QUIET" = false ]; then
        echo "Error: CLIFT is not running or audio is not enabled."
        echo "Please start CLIFT and enable audio input (press 'i' on Audio page)"
    fi
    exit 1
fi

# Show available options
# Check if argument provided (for automated use)
if [ -n "$1" ]; then
    choice=$1
else
    # Interactive mode
    echo ""
    echo "1. Connect to system audio monitor (hear all system sounds)"
    echo "2. Connect to specific application"
    echo "3. List all available audio sources"
    echo "4. Disconnect all"
    echo ""
    read -p "Choose option (1-4): " choice
fi

case $choice in
    1)
        # Find default monitor
        MONITOR=$(pw-link -o | grep -E "Monitor.*FL" | head -1 | cut -d' ' -f1)
        if [ -n "$MONITOR" ]; then
            MONITOR_BASE=$(echo $MONITOR | sed 's/_FL$//')
            echo "Connecting to system monitor: $MONITOR_BASE"
            pw-link "${MONITOR_BASE}_FL" "CLIFT:input_FL" 2>/dev/null
            pw-link "${MONITOR_BASE}_FR" "CLIFT:input_FR" 2>/dev/null
            [ "$QUIET" = false ] && echo "Connected!"
        else
            echo "No monitor found. Trying alternative method..."
            pw-link -o | grep "FL" | while read port rest; do
                if echo "$port" | grep -q "Monitor"; then
                    BASE=$(echo $port | sed 's/_FL$//')
                    pw-link "${BASE}_FL" "CLIFT:input_FL" 2>/dev/null
                    pw-link "${BASE}_FR" "CLIFT:input_FR" 2>/dev/null
                    echo "Connected to $BASE"
                    break
                fi
            done
        fi
        ;;
    2)
        echo ""
        echo "Available applications:"
        pw-link -o | grep -E "output.*FL" | sed 's/:output_FL//' | sort -u | nl
        echo ""
        read -p "Enter application name (or part of it): " app
        
        pw-link -o | grep -i "$app.*FL" | while read port rest; do
            BASE=$(echo $port | sed 's/_FL$//')
            echo "Connecting $BASE to CLIFT..."
            pw-link "${BASE}_FL" "CLIFT:input_FL" 2>/dev/null
            pw-link "${BASE}_FR" "CLIFT:input_FR" 2>/dev/null
        done
        echo "Done!"
        ;;
    3)
        echo ""
        echo "Available audio sources:"
        echo "======================="
        pw-link -o | grep -E "(FL|output_1)" | sort
        ;;
    4)
        echo "Disconnecting all inputs from CLIFT..."
        pw-link -d | grep "CLIFT:input" | while read line; do
            # Parse the connection
            src=$(echo $line | cut -d' ' -f1)
            dst=$(echo $line | cut -d' ' -f3)
            pw-link -d "$src" "$dst" 2>/dev/null
        done
        echo "Disconnected!"
        ;;
    *)
        echo "Invalid option"
        exit 1
        ;;
esac

echo ""
echo "Current CLIFT connections:"
pw-link | grep "CLIFT:" || echo "No connections"