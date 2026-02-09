#!/bin/bash

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Change to the script directory
cd "$SCRIPT_DIR"

# Check if virtual environment exists
if [ -d "venv" ]; then
    PYTHON="./venv/bin/python"
else
    PYTHON="python3"
fi

# Function to cleanup background processes on exit
cleanup() {
    echo "Stopping all processes..."
    kill $(jobs -p) 2>/dev/null
    exit
}

# Set up trap to catch Ctrl+C and clean up
trap cleanup SIGINT SIGTERM

# Start each script in the background
echo "Starting power.py..."
$PYTHON power.py &
PID1=$!

sleep 0.5

echo "Starting power_delta.py..."
$PYTHON power_delta.py &
PID2=$!

sleep 0.5

echo "Starting psd.py..."
$PYTHON psd.py &
PID3=$!

echo ""
echo "All scripts running (PIDs: $PID1, $PID2, $PID3)"
echo "Press Ctrl+C to stop all scripts"
echo ""

# Wait for all background processes
wait