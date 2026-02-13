import matplotlib.pyplot as plt
import numpy as np
import time
from collections import deque
from scipy import stats

# Maximum number of samples to keep in history
MAX_HISTORY_SIZE = 250

# Read data from file
def read_avg_power_file(filename):
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
            
            if len(lines) < 4:  # Now need 4 lines minimum
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
            
            # Read size from third line (not used, but file has it)
            try:
                _ = int(lines[2].strip())  # Read but ignore
            except ValueError:
                return None, None, None
            
            # Read average power from fourth line (remove trailing comma if present)
            try:
                avg_power_str = lines[3].strip()
                if avg_power_str.endswith(','):
                    avg_power_str = avg_power_str[:-1]
                avg_power = float(avg_power_str)
            except ValueError:
                return None, None, None
                
            return center_freq, bandwidth, avg_power
    except Exception as e:
        # Silently ignore read errors (file being written)
        return None, None, None

# Setup plot
plt.ion()  # Interactive mode
fig, ax = plt.subplots(figsize=(7, 3))

# Initial setup
filename = 'build/avg_power_output.txt'

# Wait for valid data
print("Waiting for data...")
center_freq, bandwidth, avg_power = None, None, None
while avg_power is None:
    center_freq, bandwidth, avg_power = read_avg_power_file(filename)
    time.sleep(0.1)

# Initialize history with deque (efficient for append/pop operations)
power_history = deque(maxlen=MAX_HISTORY_SIZE)
power_history.append(avg_power)

# Initial histogram (empty since we need at least 2 points for diff)
n_bins = 50
counts, bins, patches = ax.hist([0], bins=n_bins, edgecolor='black', alpha=0.7, color='steelblue')

ax.set_xlabel('Power Difference (dB)')
ax.set_ylabel('Count')
ax.set_title(f'Power Delta Histogram (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz, N=0)')
ax.grid(True, alpha=0.3, axis='y')

fig.canvas.draw()
fig.canvas.flush_events()

print(f"Plotting started.")
print(f"Center Frequency: {center_freq/1e6:.2f} MHz")
print(f"Bandwidth: {bandwidth/1e6:.2f} MHz")
print(f"Initial Average Power: {avg_power:.2f} dB")
print(f"History Size Limit: {MAX_HISTORY_SIZE} samples")
print("Press Ctrl+C to stop.")

# Track last read value to avoid duplicates
last_avg_power = avg_power

# Update loop
update_counter = 0
try:
    while True:
        center_freq_new, bandwidth_new, avg_power_new = read_avg_power_file(filename)
        
        if avg_power_new is not None:
            # Only add if it's a new value (avoid reading same value multiple times)
            if avg_power_new != last_avg_power:
                power_history.append(avg_power_new)
                last_avg_power = avg_power_new
                
                # Update frequency parameters if changed
                if center_freq_new != center_freq or bandwidth_new != bandwidth:
                    center_freq = center_freq_new
                    bandwidth = bandwidth_new
                
                # Update histogram (redraw every update for smooth animation)
                update_counter += 1
                if update_counter % 1 == 0:  # Update every sample (can reduce for performance)
                    # Calculate differences (delta between consecutive samples)
                    if len(power_history) > 1:
                        power_array = np.array(power_history)
                        power_diffs = np.diff(power_array)  # Calculate differences
                        
                        ax.clear()
                        
                        # Calculate statistics for normal curve
                        mean_diff = np.mean(power_diffs)
                        med_diff = np.median(power_diffs)
                        std_diff = np.std(power_diffs)
                        min_diff = np.min(power_diffs)
                        max_diff = np.max(power_diffs)
                        
                        # Create histogram of differences with counts (not density)
                        counts, bins, patches = ax.hist(power_diffs, bins=n_bins, 
                                                        edgecolor='black', alpha=0.7, 
                                                        color='steelblue', density=False,
                                                        label='Histogram')
                        
                        # Generate normal curve
                        x_range = np.linspace(min_diff - std_diff, max_diff + std_diff, 200)
                        normal_curve = stats.norm.pdf(x_range, mean_diff, std_diff)
                        
                        # Scale the normal curve to match histogram counts
                        bin_width = bins[1] - bins[0]
                        normal_curve_scaled = normal_curve * len(power_diffs) * bin_width
                        
                        # Plot scaled normal curve
                        # ax.plot(x_range, normal_curve_scaled, 'r-', linewidth=2.5, 
                        #        label=f'Normal Distribution\n(μ={mean_diff:.4f}, σ={std_diff:.4f})')
                        
                        # Add statistics text for differences
                        stats_text = f'Mean: {mean_diff:.4f} dB\nStd: {std_diff:.4f} dB\nMin: {min_diff:.4f} dB\nMax: {max_diff:.4f} dB'
                        ax.text(0.98, 0.97, stats_text, transform=ax.transAxes,
                               verticalalignment='top', horizontalalignment='right',
                               bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5),
                               fontsize=10, family='monospace')
                        
                        # Add vertical line for mean
                        ax.axvline(mean_diff, color='darkred', linestyle='--', linewidth=2, 
                                  label=f'Mean: {mean_diff:.4f} dB', alpha=0.7)
                        
                        ax.axvline(med_diff, color='darkred', linestyle='--', linewidth=2, 
                                  label=f'Med: {mean_diff:.4f} dB', alpha=0.7)
                                                                                                                            
                        # Add vertical line at zero
                        ax.axvline(0, color='green', linestyle=':', linewidth=1.5, 
                                  label='Zero', alpha=0.7)
                        
                        ax.set_xlabel('Power Difference (dB)')
                        ax.set_ylabel('Count')
                        ax.set_title(f'Power Delta Histogram with Normal Curve (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz, N={len(power_diffs)})')
                        ax.grid(True, alpha=0.3, axis='y')
                        ax.legend(loc='upper left')
                        
                        fig.canvas.draw()
                        fig.canvas.flush_events()
        
        time.sleep(0.01)  # Update every 10ms
except KeyboardInterrupt:
    print("\nStopped by user")
    if len(power_history) > 1:
        power_array = np.array(power_history)
        power_diffs = np.diff(power_array)
        print(f"\nFinal Statistics (Power Differences):")
        print(f"  Samples collected: {len(power_history)}")
        print(f"  Differences calculated: {len(power_diffs)}")
        print(f"  Mean difference: {np.mean(power_diffs):.4f} dB")
        print(f"  Med difference: {np.median(power_diffs):.4f} dB")
        print(f"  Std deviation: {np.std(power_diffs):.4f} dB")
        print(f"  Min difference: {np.min(power_diffs):.4f} dB")
        print(f"  Max difference: {np.max(power_diffs):.4f} dB")
    plt.ioff()
    plt.show()