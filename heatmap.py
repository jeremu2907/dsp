import os
import sys
import time
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import LinearSegmentedColormap, Normalize
from mpl_toolkits.axes_grid1 import make_axes_locatable
import rasterio
from pyproj import CRS, Transformer
from scipy.ndimage import gaussian_filter
import contextily as cx

# ============================================================
# Map & Hardware Parameters
# ============================================================

TX_HEIGHT_M = 1.5      # Transmitter antenna height above ground (m)
RX_HEIGHT_M = 1.0       # Receiver antenna height above ground (m)
MAP_RADIUS_KM = 3.0    # Map radius around transmitter (km)
GRID_SIZE = 300         # Downsampled DTED grid resolution (NxN nodes)
PROFILE_SAMPLES = 100    # Number of DTED terrain profile steps along each ray
SMOOTHING_SIGMA = 1.0   # Spatial Gaussian blur factor
FADE_MARGIN_DB = 0.1    # Soft threshold fade window in dB
REFRESH_INTERVAL_SEC = 10  # Auto-refresh timer in seconds
FIGURE_SIZE = (10, 8)


# ============================================================
# Configuration File Reader
# ============================================================

def read_tx_config(filename):
    """Read 5-line transmitter configuration file."""
    try:
        with open(filename, 'r') as f:
            lines = [line.strip() for line in f if line.strip() and not line.startswith('#')]

        if len(lines) < 5:
            raise ValueError(
                f'{filename} must contain 5 lines: Power, Frequency, Longitude, Latitude, Threshold.'
            )

        tx_power_w = float(lines[0])
        freq_mhz = float(lines[1])
        tx_lon_deg = float(lines[2])
        tx_lat_deg = float(lines[3])
        threshold_w = float(lines[4])

        return tx_power_w, freq_mhz, tx_lon_deg, tx_lat_deg, threshold_w

    except Exception as e:
        print(f'Error reading TX configuration file "{filename}": {e}')
        return None


# ============================================================
# Geographic Grid Generation
# ============================================================

def make_local_projection(tx_lon, tx_lat):
    """Create local Azimuthal Equidistant projection grid centered on TX."""
    local_crs = CRS.from_proj4(
        f'+proj=aeqd +lat_0={tx_lat} +lon_0={tx_lon} +datum=WGS84 +units=m'
    )
    forward = Transformer.from_crs('EPSG:4326', local_crs, always_xy=True)
    inverse = Transformer.from_crs(local_crs, 'EPSG:4326', always_xy=True)
    return forward, inverse


def create_grid(tx_lon, tx_lat, radius_km, grid_size):
    """Generate metric grid and transform to geographic coordinates."""
    radius_m = radius_km * 1000.0
    _, inverse = make_local_projection(tx_lon, tx_lat)

    x = np.linspace(-radius_m, radius_m, grid_size)
    y = np.linspace(-radius_m, radius_m, grid_size)
    x_grid, y_grid = np.meshgrid(x, y)

    lon_grid, lat_grid = inverse.transform(x_grid, y_grid)
    distance_grid_m = np.sqrt(x_grid**2 + y_grid**2)

    return lon_grid, lat_grid, distance_grid_m


# ============================================================
# DTED Terrain Elevation Loader
# ============================================================

def load_dted_elevation(dted_path, lon_grid, lat_grid):
    """Extract elevation array (meters MSL) from DTED file (.dt2)."""
    if not os.path.exists(dted_path):
        print(f"Warning: DTED file '{dted_path}' not found. Using flat ground at 0m MSL.")
        return np.zeros_like(lon_grid)

    try:
        with rasterio.open(dted_path) as src:
            dataset_crs = src.crs if src.crs else CRS.from_epsg(4326)
            transformer = Transformer.from_crs('EPSG:4326', dataset_crs, always_xy=True)

            x_coords, y_coords = transformer.transform(lon_grid, lat_grid)
            pts = list(zip(x_coords.flatten(), y_coords.flatten()))

            raw_samples = list(src.sample(pts))
            elevations = np.array([s[0] for s in raw_samples], dtype=float).reshape(lon_grid.shape)

            nodata_val = src.nodata if src.nodata is not None else -32767
            invalid_mask = (elevations == nodata_val) | (elevations < -500) | (elevations > 9000)

            if np.all(invalid_mask):
                print("Warning: Map region falls entirely outside DTED tile. Using 0m MSL.")
                return np.zeros_like(lon_grid)

            valid_median = np.nanmedian(elevations[~invalid_mask])
            elevations[invalid_mask] = valid_median
            return elevations

    except Exception as e:
        print(f"Error reading DTED raster ({e}). Defaulting to flat terrain.")
        return np.zeros_like(lon_grid)


# ============================================================
# Point-to-Point (P2P) Longley-Rice Propagation Engine
# ============================================================

def compute_p2p_longley_rice_dbm(
    distance_grid_m, freq_mhz, tx_power_w, elev_grid, tx_h, rx_h
):
    """Vectorized Point-to-Point Longley-Rice ITM model calculating received power in dBm."""
    grid_size = elev_grid.shape[0]
    tx_r, tx_c = grid_size // 2, grid_size // 2

    t = np.linspace(0.0, 1.0, PROFILE_SAMPLES)[:, None, None]

    rows = np.arange(grid_size)[:, None]
    cols = np.arange(grid_size)[None, :]

    ray_rows = np.clip(np.round(tx_r + t * (rows - tx_r)).astype(int), 0, grid_size - 1)
    ray_cols = np.clip(np.round(tx_c + t * (cols - tx_c)).astype(int), 0, grid_size - 1)

    profile_elevs = elev_grid[ray_rows, ray_cols]

    d_m = np.maximum(distance_grid_m, 1.0)
    d_km = np.maximum(d_m / 1000.0, 0.001)
    d_k = t * d_m

    # Free Space Path Loss (FSPL)
    fspl_db = 20.0 * np.log10(d_km) + 20.0 * np.log10(freq_mhz) + 32.44

    tx_ground_msl = profile_elevs[0, tx_r, tx_c]
    tx_msl = tx_ground_msl + tx_h
    rx_msl = profile_elevs[-1] + rx_h

    los_msl = tx_msl + t * (rx_msl - tx_msl)

    # 4/3 Earth Curvature drop
    r_earth = 8500000.0
    earth_drop = (d_k * (d_m - d_k)) / (2.0 * r_earth)
    eff_terrain_msl = profile_elevs + earth_drop

    h_obs = eff_terrain_msl - los_msl

    wavelength = 0.3 / (freq_mhz / 1000.0)
    d1 = np.maximum(d_k, 1.0)
    d2 = np.maximum(d_m - d_k, 1.0)
    r_fresnel = np.sqrt(wavelength * d1 * d2 / d_m)

    v = h_obs / np.maximum(r_fresnel, 1e-3)

    v_inner = v[1:-1]
    v_max = np.max(v_inner, axis=0)

    # Knife-Edge Terrain Diffraction Loss J(v)
    v_safe = np.maximum(v_max, -2.0)
    diffraction_loss_db = np.where(
        v_max > -0.7,
        6.9 + 20.0 * np.log10(np.sqrt((v_safe - 0.1)**2 + 1.0) + v_safe - 0.1),
        0.0
    )

    # Two-Ray Ground Reflection
    tx_eff_h = np.maximum(tx_msl - profile_elevs[-1], 1.0)
    rx_eff_h = np.maximum(rx_h, 1.0)
    two_ray_db = 40.0 * np.log10(d_m) - 20.0 * np.log10(tx_eff_h * rx_eff_h)

    base_loss_db = np.maximum(fspl_db, two_ray_db)
    total_loss_db = base_loss_db + np.maximum(diffraction_loss_db, 0.0)

    # Power in dBm
    tx_power_dbm = 10.0 * np.log10(tx_power_w * 1000.0)
    received_power_dbm = tx_power_dbm - total_loss_db
    return received_power_dbm


# ============================================================
# Live Heatmap Execution Loop
# ============================================================

def main():
    tx_file = sys.argv[1] if len(sys.argv) > 1 else 'build/heatmap.txt'
    dted_file = sys.argv[2] if len(sys.argv) > 2 else 'dted.dt2'

    print('==================================================')
    print('DTED Live Softened P2P Heatmap (10s Refresh)')
    print('==================================================')

    plt.ion()
    fig, ax = plt.subplots(figsize=FIGURE_SIZE)

    # Create a fixed colorbar axis (cax) pinned to the right side of ax
    divider = make_axes_locatable(ax)
    cax = divider.append_axes("right", size="4%", pad=0.1)

    last_tx_pos = None
    elev_grid = None
    lon_grid, lat_grid, distance_grid_m = None, None, None

    try:
        while True:
            config = read_tx_config(tx_file)
            if config is None:
                print("Retrying config read in 10s...")
                plt.pause(REFRESH_INTERVAL_SEC)
                continue

            tx_power_w, freq_mhz, tx_lon, tx_lat, threshold_w = config
            tx_power_dbm = 10.0 * np.log10(tx_power_w * 1000.0)
            threshold_dbm = 10.0 * np.log10(threshold_w * 1000.0) if threshold_w > 0 else -150.0

            curr_tx_pos = (tx_lon, tx_lat)

            if curr_tx_pos != last_tx_pos:
                print(f'\nTransmitter moved/initialized to: {tx_lat:.6f} N, {tx_lon:.6f} E')
                lon_grid, lat_grid, distance_grid_m = create_grid(tx_lon, tx_lat, MAP_RADIUS_KM, GRID_SIZE)
                print('Loading DTED elevation raster...')
                elev_grid = load_dted_elevation(dted_file, lon_grid, lat_grid)
                last_tx_pos = curr_tx_pos

            print(f'[{time.strftime("%H:%M:%S")}] Computing P2P propagation map...')
            start_time = time.time()
            power_dbm = compute_p2p_longley_rice_dbm(
                distance_grid_m, freq_mhz, tx_power_w, elev_grid, TX_HEIGHT_M, RX_HEIGHT_M
            )
            print(f'Computation finished in {time.time() - start_time:.2f} seconds.')

            smoothed_power = gaussian_filter(power_dbm, sigma=SMOOTHING_SIGMA)

            alpha_threshold = np.clip(
                (smoothed_power - (threshold_dbm - FADE_MARGIN_DB)) / FADE_MARGIN_DB, 0.0, 1.0
            )

            radius_m = MAP_RADIUS_KM * 1000.0
            outer_fade_m = 100.0
            dist_from_edge = radius_m - distance_grid_m
            alpha_radius = np.clip(dist_from_edge / outer_fade_m, 0.0, 1.0)

            combined_alpha = alpha_threshold * alpha_radius
            valid_mask = combined_alpha > 0.01

            # Clear both main plot and colorbar axes cleanly
            ax.clear()
            cax.clear()

            if np.any(valid_mask):
                min_dbm = np.min(smoothed_power[valid_mask])
                max_dbm = np.max(smoothed_power[valid_mask])
                if max_dbm <= min_dbm:
                    max_dbm = min_dbm + 1.0

                norm = Normalize(vmin=min_dbm, vmax=max_dbm)
                colors = ['blue', 'green', 'yellow', 'red']
                cmap = LinearSegmentedColormap.from_list('RedYellowGreenBlue', colors)

                rgba_image = cmap(norm(smoothed_power))
                rgba_image[..., 3] = combined_alpha * 0.7

                extent = [lon_grid.min(), lon_grid.max(), lat_grid.min(), lat_grid.max()]

                ax.imshow(
                    rgba_image,
                    extent=extent,
                    origin='lower',
                    interpolation='bilinear',
                    aspect='auto',
                    zorder=2
                )

                # Draw colorbar onto the dedicated cax without altering ax layout
                sm = plt.cm.ScalarMappable(cmap=cmap, norm=norm)
                sm.set_array([])
                cbar = fig.colorbar(sm, cax=cax)
                cbar.set_label('Received Power (dBm)')

            # Plot Transmitter Icon
            ax.scatter(
                tx_lon, tx_lat,
                marker='^', s=140, c='magenta', edgecolors='white', linewidths=1.5, zorder=10, label='Transmitter'
            )

            ax.set_xlim(lon_grid.min(), lon_grid.max())
            ax.set_ylim(lat_grid.min(), lat_grid.max())

            # Load Satellite Basemap Tile
            try:
                cx.add_basemap(
                    ax,
                    crs='EPSG:4326',
                    source=cx.providers.Esri.WorldImagery,
                    zorder=1
                )
            except Exception as e:
                print(f"Warning: Satellite tile error ({e}).")

            ax.set_xlabel('Longitude (deg)')
            ax.set_ylabel('Latitude (deg)')
            ax.set_title(
                f'Live P2P Coverage Heatmap ({time.strftime("%H:%M:%S")})\n'
                f'Tx = {tx_power_w:.3g} W | Freq = {freq_mhz:.1f} MHz | Threshold = {threshold_dbm:.1f} dBm'
            )
            ax.grid(True, alpha=0.3, color='white', linestyle='--')
            ax.legend(loc='upper right')

            fig.canvas.draw_idle()
            fig.canvas.flush_events()

            print(f"Waiting {REFRESH_INTERVAL_SEC} seconds for next update...\n")
            plt.pause(REFRESH_INTERVAL_SEC)

            if not plt.fignum_exists(fig.number):
                print("Window closed. Exiting...")
                break

    except KeyboardInterrupt:
        print("\nStopped by user.")


if __name__ == '__main__':
    main()