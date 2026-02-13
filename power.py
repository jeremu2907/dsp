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

# Read Cauchy distribution parameters from file
def read_cauchy_params(filename):
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
            
            if len(lines) < 3:
                return None, None, None
            
            center = float(lines[0].strip())
            scale = float(lines[1].strip())
            skew = float(lines[2].strip())
            
            return center, scale, skew
    except Exception as e:
        return None, None, None

# Setup plot
plt.ion()  # Interactive mode
fig, ax = plt.subplots(figsize=(7, 3))

# Initial setup
filename = 'build/avg_power_output.txt'
cauchy_filename = 'build/cauchy_dist.txt'

# Wait for valid data
print("Waiting for data...")
center_freq, bandwidth, avg_power = None, None, None
while avg_power is None:
    center_freq, bandwidth, avg_power = read_avg_power_file(filename)
    time.sleep(0.1)

# Initialize history with deque (efficient for append/pop operations)
power_history = deque(maxlen=MAX_HISTORY_SIZE)
power_history.append(avg_power)

# Initial histogram
n_bins = 50
counts, bins, patches = ax.hist([avg_power], bins=n_bins, edgecolor='black', alpha=0.7, color='steelblue')

ax.set_xlabel('Power ()')
ax.set_ylabel('Count')
ax.set_title(f'Power Histogram (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz, N=1)')
ax.grid(True, alpha=0.3, axis='y')

fig.canvas.draw()
fig.canvas.flush_events()

print(f"Plotting started.")
print(f"Center Frequency: {center_freq/1e6:.2f} MHz")
print(f"Bandwidth: {bandwidth/1e6:.2f} MHz")
print(f"Initial Average Power: {avg_power:.2f} ")
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
                    # Use power values directly
                    power_array = np.array(power_history)
                    
                    ax.clear()
                    
                    # Calculate statistics
                    mean_power = np.mean(power_array)
                    med_power = np.median(power_array)
                    std_power = np.std(power_array)
                    min_power = np.min(power_array)
                    max_power = np.max(power_array)
                    MAD = stats.median_abs_deviation(power_array)
                    
                    # Create histogram with counts (not density)
                    counts, bins, patches = ax.hist(power_array, bins=n_bins, 
                                                    edgecolor='black', alpha=0.7, 
                                                    color='steelblue', density=False,
                                                    label='Histogram')
                    
                    # Read Cauchy parameters from file
                    cauchy_center, cauchy_scale, cauchy_skew = read_cauchy_params(cauchy_filename)
                    
                    if cauchy_center is not None and cauchy_scale is not None and cauchy_skew is not None:
                        # Generate x range for plotting
                        x_range = np.linspace(min_power - std_power, max_power + std_power, 200)
                        
                        # Generate Cauchy curve using parameters from file
                        cauchy_curve = stats.skewcauchy.pdf(x_range, a=cauchy_skew, loc=cauchy_center, scale=cauchy_scale)
                        
                        # Scale the Cauchy curve to match histogram counts
                        bin_width = bins[1] - bins[0]
                        cauchy_curve_scaled = cauchy_curve * len(power_array) * bin_width
                        
                        # Plot scaled Cauchy curve
                        ax.plot(x_range, cauchy_curve_scaled, 'r-', linewidth=2.5, 
                               label=f'Skewed Cauchy Distribution\n(c={cauchy_center:.6f}, s={cauchy_scale:.6f}, λ={cauchy_skew:.4f})')
                    
                    # Add statistics text
                    stats_text = f'Mean: {mean_power:.4f} \nMedian: {med_power:.4f} \nStd: {std_power:.4f} \nMAD: {MAD:.4f} '
                    ax.text(0.98, 0.97, stats_text, transform=ax.transAxes,
                           verticalalignment='top', horizontalalignment='right',
                           bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5),
                           fontsize=10, family='monospace')
                    
                    # Add vertical line for mean and median
                    ax.axvline(mean_power, color='darkred', linestyle='--', linewidth=2, 
                              label=f'Mean: {mean_power:.4f} ', alpha=0.7)
                    
                    ax.axvline(med_power, color='darkgreen', linestyle='--', linewidth=2, 
                              label=f'Median: {med_power:.4f} ', alpha=0.7)
                    
                    ax.set_xlabel('Power)')
                    ax.set_ylabel('Count')
                    ax.set_title(f'Power Histogram with Skewed Cauchy (Center: {center_freq/1e6:.2f} MHz, BW: {bandwidth/1e6:.2f} MHz, N={len(power_array)})')
                    ax.grid(True, alpha=0.3, axis='y')
                    ax.legend(loc='upper left', fontsize=8)
                    
                    fig.canvas.draw()
                    fig.canvas.flush_events()
        
        time.sleep(0.01)  # Update every 10ms
except KeyboardInterrupt:
    print("\nStopped by user")
    if len(power_history) > 0:
        power_array = np.array(power_history)
        print(f"\nFinal Statistics (Power):")
        print(f"  Samples collected: {len(power_history)}")
        print(f"  Mean power: {np.mean(power_array):.4f} ")
        print(f"  Median power: {np.median(power_array):.4f} ")
        print(f"  Std deviation: {np.std(power_array):.4f} ")
        print(f"  MAD: {stats.median_abs_deviation(power_array):.4f} ")
        print(f"  Min power: {np.min(power_array):.4f} ")
        print(f"  Max power: {np.max(power_array):.4f} ")
    plt.ioff()
    plt.show()