
import matplotlib.pyplot as plt
import numpy as np
import time
from collections import deque
from scipy import stats


# Maximum number of samples to keep in history
MAX_HISTORY_SIZE = 250


# ------------------------------------------------------------
# File readers
# ------------------------------------------------------------

def read_avg_power_file(filename):
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()

        if len(lines) < 4:
            return None, None, None

        try:
            center_freq = float(lines[0].strip())
            bandwidth = float(lines[1].strip())
            _ = int(lines[2].strip())

            avg_power_str = lines[3].strip()

            if avg_power_str.endswith(','):
                avg_power_str = avg_power_str[:-1]
                avg_power_str = avg_power_str.split(",")[0]

            avg_power = float(avg_power_str)

            return center_freq, bandwidth, avg_power

        except ValueError:
            return None, None, None

    except Exception:
        # File may be temporarily incomplete while being written.
        return None, None, None


def read_student_t_params(filename):
    """
    Read regular Student-t parameters:

        line 1: location (x0)
        line 2: scale (sigma)
        line 3: degrees of freedom (nu)
    """

    try:
        with open(filename, 'r') as f:
            lines = f.readlines()

        if len(lines) < 3:
            return None, None, None

        try:
            x0 = float(lines[0].strip())
            sigma = float(lines[1].strip())
            nu = float(lines[2].strip())

            return x0, sigma, nu

        except ValueError:
            return None, None, None

    except Exception:
        # File may be temporarily incomplete while being written.
        return None, None, None


# ------------------------------------------------------------
# Regular Student-t PDF
# ------------------------------------------------------------

def student_t_pdf(x, x0, sigma, nu):
    """
    Regular Student-t PDF.

    z = (x - x0) / sigma

    f(x) = t_pdf(z, nu) / sigma
    """

    if sigma <= 0 or nu <= 0:
        return np.zeros_like(x, dtype=float)

    x = np.asarray(x)

    z = (x - x0) / sigma

    return stats.t.pdf(z, df=nu) / sigma


# ------------------------------------------------------------
# Setup plot
# ------------------------------------------------------------

plt.ion()

fig, ax = plt.subplots(figsize=(7, 3))

filename = 'build/avg_power_output.txt'
student_t_filename = 'build/student_t_dist.txt'


# ------------------------------------------------------------
# Wait for valid data
# ------------------------------------------------------------

print("Waiting for data...")

center_freq = None
bandwidth = None
avg_power = None

while avg_power is None:
    center_freq, bandwidth, avg_power = \
        read_avg_power_file(filename)

    time.sleep(0.1)


# ------------------------------------------------------------
# Initialize history
# ------------------------------------------------------------

power_history = deque(
    maxlen=MAX_HISTORY_SIZE
)

power_history.append(avg_power)


# ------------------------------------------------------------
# Initial histogram
# ------------------------------------------------------------

n_bins = 30

counts, bins, patches = ax.hist(
    [avg_power],
    bins=n_bins,
    edgecolor='black',
    alpha=0.7,
    color='steelblue'
)

ax.set_xlabel('Power')
ax.set_ylabel('Count')

ax.set_title(
    f'Power Histogram '
    f'(Center: {center_freq / 1e6:.2f} MHz, '
    f'BW: {bandwidth / 1e6:.2f} MHz, N=1)'
)

ax.grid(
    True,
    alpha=0.3,
    axis='y'
)

fig.canvas.draw()
fig.canvas.flush_events()


print("Plotting started.")
print(
    f"Center Frequency: "
    f"{center_freq / 1e6:.2f} MHz"
)
print(
    f"Bandwidth: "
    f"{bandwidth / 1e6:.2f} MHz"
)
print(
    f"Initial Average Power: "
    f"{avg_power:.2f}"
)
print(
    f"History Size Limit: "
    f"{MAX_HISTORY_SIZE} samples"
)
print("Press Ctrl+C to stop.")


# ------------------------------------------------------------
# Update loop
# ------------------------------------------------------------

last_avg_power = avg_power
update_counter = 0

try:
    while True:

        center_freq_new, bandwidth_new, avg_power_new = \
            read_avg_power_file(filename)

        if avg_power_new is not None:

            # Ignore duplicate reads.
            if avg_power_new != last_avg_power:

                power_history.append(avg_power_new)

                last_avg_power = avg_power_new

                # Update frequency parameters.
                if (
                    center_freq_new != center_freq
                    or bandwidth_new != bandwidth
                ):
                    center_freq = center_freq_new
                    bandwidth = bandwidth_new

                update_counter += 1

                if update_counter % 1 == 0:

                    power_array = np.array(
                        power_history
                    )

                    ax.clear()

                    # ------------------------------------------------
                    # Statistics
                    # ------------------------------------------------

                    mean_power = np.mean(power_array)
                    med_power = np.median(power_array)
                    std_power = np.std(power_array)
                    min_power = np.min(power_array)
                    max_power = np.max(power_array)

                    MAD = stats.median_abs_deviation(
                        power_array
                    )

                    # ------------------------------------------------
                    # Histogram
                    # ------------------------------------------------

                    counts, bins, patches = ax.hist(
                        power_array,
                        bins=n_bins,
                        edgecolor='black',
                        alpha=0.7,
                        color='steelblue',
                        density=False,
                        label='Histogram'
                    )

                    # ------------------------------------------------
                    # Read Student-t parameters
                    # ------------------------------------------------

                    (
                        student_t_center,
                        student_t_scale,
                        student_t_nu
                    ) = read_student_t_params(
                        student_t_filename
                    )

                    if (
                        student_t_center is not None
                        and student_t_scale is not None
                        and student_t_nu is not None
                    ):

                        # ------------------------------------------------
                        # Generate x range
                        # ------------------------------------------------

                        x_range = np.linspace(
                            min_power - std_power,
                            max_power + std_power,
                            400
                        )

                        # ------------------------------------------------
                        # Regular Student-t PDF
                        # ------------------------------------------------

                        student_t_curve = student_t_pdf(
                            x_range,
                            student_t_center,
                            student_t_scale,
                            student_t_nu
                        )

                        # ------------------------------------------------
                        # Scale density to histogram counts
                        # ------------------------------------------------

                        bin_width = bins[1] - bins[0]

                        student_t_curve_scaled = (
                            student_t_curve
                            * len(power_array)
                            * bin_width
                        )

                        # ------------------------------------------------
                        # Plot distribution
                        # ------------------------------------------------

                        ax.plot(
                            x_range,
                            student_t_curve_scaled,
                            'r-',
                            linewidth=2.5,
                            label=(
                                'Student-t\n'
                                f'x0={student_t_center:.6f}, '
                                f's={student_t_scale:.6f}\n'
                                f'nu={student_t_nu:.2f}'
                            )
                        )

                    # ------------------------------------------------
                    # Statistics text
                    # ------------------------------------------------

                    stats_text = (
                        f'Mean:   {mean_power:.4f}\n'
                        f'Median: {med_power:.4f}\n'
                        f'Std:    {std_power:.4f}\n'
                        f'MAD:    {MAD:.4f}'
                    )

                    ax.text(
                        0.98,
                        0.97,
                        stats_text,
                        transform=ax.transAxes,
                        verticalalignment='top',
                        horizontalalignment='right',
                        bbox=dict(
                            boxstyle='round',
                            facecolor='wheat',
                            alpha=0.5
                        ),
                        fontsize=10,
                        family='monospace'
                    )

                    # ------------------------------------------------
                    # Mean
                    # ------------------------------------------------

                    ax.axvline(
                        mean_power,
                        color='darkred',
                        linestyle='--',
                        linewidth=2,
                        label=f'Mean: {mean_power:.4f}',
                        alpha=0.7
                    )

                    # ------------------------------------------------
                    # Median
                    # ------------------------------------------------

                    ax.axvline(
                        med_power,
                        color='darkgreen',
                        linestyle='--',
                        linewidth=2,
                        label=f'Median: {med_power:.4f}',
                        alpha=0.7
                    )

                    # ------------------------------------------------
                    # Labels
                    # ------------------------------------------------

                    ax.set_xlabel('Power')
                    ax.set_ylabel('Count')

                    ax.set_title(
                        'Power Histogram with '
                        'Student-t '
                        f'(Center: '
                        f'{center_freq / 1e6:.2f} MHz, '
                        f'BW: '
                        f'{bandwidth / 1e6:.2f} MHz, '
                        f'N={len(power_array)})'
                    )

                    ax.grid(
                        True,
                        alpha=0.3,
                        axis='y'
                    )

                    ax.legend(
                        loc='upper left',
                        fontsize=8
                    )

                    fig.canvas.draw()
                    fig.canvas.flush_events()

        time.sleep(0.01)


# ------------------------------------------------------------
# Exit
# ------------------------------------------------------------

except KeyboardInterrupt:

    print("\nStopped by user")

    if len(power_history) > 0:

        power_array = np.array(
            power_history
        )

        print("\nFinal Statistics (Power):")
        print(
            f"  Samples collected: "
            f"{len(power_history)}"
        )
        print(
            f"  Mean power: "
            f"{np.mean(power_array):.4f}"
        )
        print(
            f"  Median power: "
            f"{np.median(power_array):.4f}"
        )
        print(
            f"  Std deviation: "
            f"{np.std(power_array):.4f}"
        )
        print(
            f"  MAD: "
            f"{stats.median_abs_deviation(power_array):.4f}"
        )
        print(
            f"  Min power: "
            f"{np.min(power_array):.4f}"
        )
        print(
            f"  Max power: "
            f"{np.max(power_array):.4f}"
        )

    plt.ioff()
    plt.show()
