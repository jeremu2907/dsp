import matplotlib.pyplot as plt
import numpy as np
import time

# Read data from file
def read_psd_file(filename):
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
            
            if len(lines) < 4:  # Now need 4 lines minimum
                return None, None, None, None
            
            # Read center frequency from first line
            try:
                center_freq = float(lines[0].strip())
            except ValueError:
                return None, None, None, None
            
            # Read bandwidth from second line
            try:
                bandwidth = float(lines[1].strip())
            except ValueError:
                return None, None, None, None
            
            # Read expected size from third line
            try:
                expected_size = int(lines[2].strip())
            except ValueError:
                return None, None, None, None
            
            # Read PSD values from fourth line onwards
            content = ''.join(lines[3:]).strip()
            
            # Remove trailing comma if present
            if content.endswith(','):
                content = content[:-1]
            
            # Simplified parsing - just try to convert each value
            values = []
            for x in content.split(','):
                x = x.strip()
                if x:  # Only check if not empty
                    try:
                        values.append(float(x))
                    except ValueError:
                        continue
            
            # Validate we got the expected number of values
            if len(values) != expected_size:
                print(f"Warning: Expected {expected_size} values, got {len(values)}")
                return None, None, None, None
            
            if len(values) == 0:
                return None, None, None, None
                
            return center_freq, bandwidth, expected_size, np.array(values)
    except Exception as e:
        # Silently ignore read errors (file being written)
        return None, None, None, None

# Setup plot
plt.ion()  # Interactive mode
fig, ax = plt.subplots(figsize=(6, 3))
line, = ax.plot([], [], linewidth=1)

# Initial setup
filename = 'build/psd_output.txt'

# Wait for valid data
print("Waiting for data...")
center_freq, bandwidth, n, y = None, None, None, None
while y is None:
    center_freq, bandwidth, n, y = read_psd_file(filename)
    time.sleep(0.1)

# Calculate frequency axis (convert Hz to MHz)
freq_start_hz = center_freq - bandwidth / 2
freq_end_hz = center_freq + bandwidth / 2
freq_start_mhz = freq_start_hz / 1e6
freq_end_mhz = freq_end_hz / 1e6
x = np.linspace(freq_start_mhz, freq_end_mhz, n)

# Set initial limits
x_min, x_max = freq_start_mhz, freq_end_mhz

# Filter out invalid initial values
valid_y = y[np.isfinite(y)]
if len(valid_y) > 0:
    y_min = np.min(valid_y) - 5
    y_max = np.max(valid_y) + 5
else:
    y_min, y_max = -100, 0  # Default fallback

ax.set_xlim(x_min, x_max)
ax.set_ylim(y_min, y_max)
ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Power Spectral Density (dB)')
ax.set_title(f'Real-time PSD Output (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz, Bins: {n})')
ax.grid(True, alpha=0.3)

# Initial plot
line.set_data(x, y)
fig.canvas.draw()
fig.canvas.flush_events()

print(f"Plotting started.")
print(f"Center Frequency: {center_freq/1e6:.2f} MHz")
print(f"Bandwidth: {bandwidth/1e6:.2f} MHz")
print(f"Frequency Range: {freq_start_mhz:.2f} to {freq_end_mhz:.2f} MHz")
print(f"FFT Bins: {n}")
print("Press Ctrl+C to stop.")

# Smoothing parameters for y-axis limits
ALPHA = 0.1  # Smoothing factor (0.0 = no change, 1.0 = instant change)
MARGIN = 5   # Margin in dB above/below data

# Update loop
try:
    while True:
        center_freq_new, bandwidth_new, n_new, y_new = read_psd_file(filename)
        
        if y_new is not None:
            n_new = len(y_new)
            
            # Check if FFT bin count changed
            if n_new != n:
                print(f"FFT bin count changed: {n} -> {n_new}")
                n = n_new
                x = np.linspace(freq_start_mhz, freq_end_mhz, n)
            
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
                ax.set_title(f'Real-time PSD Output (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz, Bins: {n})')
                print(f"Frequency parameters changed - Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz")
            
            # Filter out NaN and Inf values before calculating limits
            valid_y = y_new[np.isfinite(y_new)]
            
            if len(valid_y) > 0:  # Only update if we have valid data
                # Calculate target limits based on current data
                target_y_min = np.min(valid_y) - MARGIN
                target_y_max = np.max(valid_y) + MARGIN
                
                # Smoothly interpolate towards target (exponential moving average)
                y_min = y_min * (1 - ALPHA) + target_y_min * ALPHA
                y_max = y_max * (1 - ALPHA) + target_y_max * ALPHA
                
                ax.set_ylim(y_min, y_max)
                
                # Update plot
                line.set_data(x, y_new)
                fig.canvas.draw()
                fig.canvas.flush_events()
            else:
                print("Warning: All PSD values are NaN or Inf, skipping update")
        
        time.sleep(0.001)
except KeyboardInterrupt:
    print("\nStopped by user")
    plt.ioff()
    plt.show()