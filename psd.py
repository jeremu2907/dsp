import matplotlib.pyplot as plt
import numpy as np
import time

# Read data from file
def read_psd_file(filename):
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
            
            if len(lines) < 3:
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
            
            # Read PSD values from third line onwards
            content = ''.join(lines[2:]).strip()
            
            # Remove trailing comma if present
            if content.endswith(','):
                content = content[:-1]
            
            # Filter out empty strings and non-numeric values
            values = []
            for x in content.split(','):
                x = x.strip()
                if x and x.replace('.', '').replace('-', '').replace('e', '').replace('+', '').isdigit():
                    try:
                        values.append(float(x))
                    except ValueError:
                        continue
            
            if len(values) == 0:
                return None, None, None
                
            return center_freq, bandwidth, np.array(values)
    except Exception as e:
        # Silently ignore read errors (file being written)
        return None, None, None

# Setup plot
plt.ion()  # Interactive mode
fig, ax = plt.subplots(figsize=(12, 6))
line, = ax.plot([], [], linewidth=1)

# Initial setup
filename = 'build/psd_output.txt'

# Wait for valid data
print("Waiting for data...")
center_freq, bandwidth, y = None, None, None
while y is None:
    center_freq, bandwidth, y = read_psd_file(filename)
    time.sleep(0.1)

n = len(y)

# Calculate frequency axis (convert Hz to MHz)
# Frequencies span from (center_freq - bandwidth/2) to (center_freq + bandwidth/2)
freq_start_hz = center_freq - bandwidth / 2
freq_end_hz = center_freq + bandwidth / 2
freq_start_mhz = freq_start_hz / 1e6
freq_end_mhz = freq_end_hz / 1e6
x = np.linspace(freq_start_mhz, freq_end_mhz, n)

# Set fixed limits
x_min, x_max = freq_start_mhz, freq_end_mhz
y_min = np.min(y) - 5  # Add margin
y_max = np.max(y) + 5

ax.set_xlim(x_min, x_max)
ax.set_ylim(y_min, y_max)
ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Power Spectral Density (dB)')
ax.set_title(f'Real-time PSD Output (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz)')
ax.grid(True, alpha=0.3)

# Initial plot
line.set_data(x, y)
fig.canvas.draw()
fig.canvas.flush_events()

print(f"Plotting started.")
print(f"Center Frequency: {center_freq/1e6:.2f} MHz")
print(f"Bandwidth: {bandwidth/1e6:.2f} MHz")
print(f"Frequency Range: {freq_start_mhz:.2f} to {freq_end_mhz:.2f} MHz")
print("Press Ctrl+C to stop.")

# Update loop
try:
    while True:
        center_freq_new, bandwidth_new, y_new = read_psd_file(filename)
        
        if y_new is not None and len(y_new) == n:
            # Check if frequency parameters changed
            if center_freq_new != center_freq or bandwidth_new != bandwidth:
                center_freq = center_freq_new
                bandwidth = bandwidth_new
                freq_start_hz = center_freq - bandwidth / 2
                freq_end_hz = center_freq + bandwidth / 2
                freq_start_mhz = freq_start_hz / 1e6
                freq_end_mhz = freq_end_hz / 1e6
                x = np.linspace(freq_start_mhz, freq_end_mhz, n)
                ax.set_xlim(freq_start_mhz, freq_end_mhz)
                ax.set_title(f'Real-time PSD Output (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz)')
            
            # Update y limits if needed to capture all values
            new_y_min = np.min(y_new)
            new_y_max = np.max(y_new)
            
            if new_y_min < y_min or new_y_max > y_max:
                y_min = min(y_min, new_y_min) - 5
                y_max = max(y_max, new_y_max) + 5
                ax.set_ylim(y_min, y_max)
            
            # Update plot
            line.set_data(x, y_new)
            fig.canvas.draw()
            fig.canvas.flush_events()
        
        time.sleep(0.001)  # Update every 50ms
except KeyboardInterrupt:
    print("\nStopped by user")
    plt.ioff()
    plt.show()