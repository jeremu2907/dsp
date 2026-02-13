import matplotlib.pyplot as plt
import numpy as np
import time
from collections import deque
from datetime import datetime

# Read data from file
def read_avg_power_file(filename):
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
            
            if len(lines) < 4:
                return None, None, None
            
            # Read center frequency from first line
            try:
                center_freq = float(lines[0].strip())
            except ValueError:
                return None, None, None
            
            # Read bandwidth from second line
            try:
                bandwidth = float(lines[1].strip())
            except ValueError:
                return None, None, None
            
            # Skip third line (not needed)
            
            # Read power value from fourth line
            power_str = lines[3].strip()
            if power_str.endswith(','):
                power_str = power_str[:-1]
            
            try:
                power = float(power_str)
            except ValueError:
                return None, None, None
            
            return center_freq, bandwidth, power
    except Exception as e:
        # Silently ignore read errors (file being written)
        return None, None, None

# Setup plot
plt.ion()  # Interactive mode
fig, ax = plt.subplots(figsize=(10, 4))
line, = ax.plot([], [], linewidth=1.5, marker='o', markersize=2)

# Initial setup
filename = 'build/avg_power_output.txt'

# Wait for valid data
print("Waiting for data...")
center_freq, bandwidth, power = None, None, None
while power is None:
    center_freq, bandwidth, power = read_avg_power_file(filename)
    time.sleep(0.1)

print(f"Monitoring started.")
print(f"Center Frequency: {center_freq/1e6:.2f} MHz")
print(f"Bandwidth: {bandwidth/1e6:.2f} MHz")
print("Press Ctrl+C to stop.")

# Data storage (store timestamps and powers separately)
WINDOW_SECONDS = 10  # Keep last 10 seconds of data
timestamps = deque()
powers = deque()

# Store initial data point
start_time = time.time()
timestamps.append(0)
powers.append(power)

# Set initial plot configuration
ax.set_xlabel('Time (seconds)')
ax.set_ylabel('Average Power (dB)')
ax.set_title(f'Average Power over Time (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz)')
ax.grid(True, alpha=0.3)

# Y-axis parameters
MARGIN = 1   # Smaller margin in dB (was 5, now 1)
EXPAND_FACTOR = 1.1  # Smaller expansion factor (was 1.2, now 1.1)

# Initial limits - tighter range
y_min = power - 0.3
y_max = power + 0.3
ax.set_ylim(y_min, y_max)

# Update loop
last_power = power  # Track last valid power to avoid duplicate points
try:
    while True:
        center_freq_new, bandwidth_new, power_new = read_avg_power_file(filename)
        
        if power_new is not None and power_new != last_power:
            # New data point received
            current_time = time.time() - start_time
            timestamps.append(current_time)
            powers.append(power_new)
            last_power = power_new
            
            # Remove data older than WINDOW_SECONDS
            while len(timestamps) > 0 and timestamps[0] < current_time - WINDOW_SECONDS:
                timestamps.popleft()
                powers.popleft()
            
            # Check if frequency parameters changed
            if center_freq_new != center_freq or bandwidth_new != bandwidth:
                center_freq = center_freq_new
                bandwidth = bandwidth_new
                ax.set_title(f'Average Power over Time (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz)')
                print(f"Frequency parameters changed - Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz")
            
            # Convert deque to numpy arrays for plotting
            x_data = np.array(timestamps)
            y_data = np.array(powers)
            
            # Update x-axis to show 10-second window
            if len(x_data) > 0:
                x_max = max(WINDOW_SECONDS, x_data[-1])
                ax.set_xlim(x_max - WINDOW_SECONDS, x_max)
            
            # Dynamic Y-axis scaling with tighter margins
            if len(y_data) > 0:
                data_min = np.min(y_data)
                data_max = np.max(y_data)
                
                needs_resize = False
                
                if data_min < y_min + MARGIN:
                    needs_resize = True
                    range_span = y_max - y_min
                    y_min = data_min - MARGIN - (range_span * (EXPAND_FACTOR - 1) / 2)
                    
                if data_max > y_max - MARGIN:
                    needs_resize = True
                    range_span = y_max - y_min
                    y_max = data_max + MARGIN + (range_span * (EXPAND_FACTOR - 1) / 2)
                
                if needs_resize:
                    ax.set_ylim(y_min, y_max)
                    print(f"Y-axis resized: [{y_min:.2f}, {y_max:.2f}] dB")
            
            # Update plot
            line.set_data(x_data, y_data)
            fig.canvas.draw()
            fig.canvas.flush_events()
            
        time.sleep(0.01)  # Poll every 50ms
        
except KeyboardInterrupt:
    print("\nStopped by user")
    print(f"Total points in window: {len(powers)}")
    plt.ioff()
    plt.show()