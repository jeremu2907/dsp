import matplotlib.pyplot as plt
import numpy as np
import time

# Read data from file
def read_psd_file(filename):
    try:
        with open(filename, 'r') as f:
            content = f.read().strip()
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
                return None
                
            return np.array(values)
    except Exception as e:
        # Silently ignore read errors (file being written)
        return None

# Setup plot
plt.ion()  # Interactive mode
fig, ax = plt.subplots(figsize=(12, 6))
line, = ax.plot([], [], linewidth=1)

# Initial setup
filename = 'build/psd_output.txt'

# Wait for valid data
print("Waiting for data...")
y = None
while y is None:
    y = read_psd_file(filename)
    time.sleep(0.1)

n = len(y)
x = np.arange(n)  # Bin numbers as x-axis

# Set fixed limits
x_min, x_max = 0, n-1
y_min = np.min(y) - 5  # Add margin
y_max = np.max(y) + 5

ax.set_xlim(x_min, x_max)
ax.set_ylim(y_min, y_max)
ax.set_xlabel('Bin Number')
ax.set_ylabel('Power Spectral Density (dB)')
ax.set_title('Real-time PSD Output')
ax.grid(True, alpha=0.3)

# Initial plot
line.set_data(x, y)
fig.canvas.draw()
fig.canvas.flush_events()

print("Plotting started. Press Ctrl+C to stop.")

# Update loop
try:
    while True:
        y_new = read_psd_file(filename)
        
        if y_new is not None and len(y_new) == n:
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
        
        # time.sleep(0.05)  # Update every 50ms
except KeyboardInterrupt:
    print("\nStopped by user")
    plt.ioff()
    plt.show()