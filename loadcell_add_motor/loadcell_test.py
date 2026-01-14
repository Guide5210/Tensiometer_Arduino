#!/usr/bin/env python3
"""
Surface Tension Tester - Full Python Control
Arduino only sends raw data, Python controls everything
"""

import serial
import serial.tools.list_ports
import json
import time
import os
from collections import defaultdict
from datetime import datetime
import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import make_interp_spline
from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter
import threading

class SurfaceTensionController:
    def __init__(self, port='COM5', baudrate=115200):
        """Initialize the controller"""
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.running = False
        
        # Test profiles
        self.tests = {
            '1': {'name': 'ULTRA_FAST', 'speed': 1600, 'speed_mms': 1.000, 'desc': 'Ultra Fast (1 mm/s)'},
            '2': {'name': 'FAST_UP', 'speed': 1200, 'speed_mms': 0.750, 'desc': 'Fast Up (750 µm/s)'},
            '3': {'name': 'FAST_DOWN', 'speed': 400, 'speed_mms': 0.250, 'desc': 'Fast Down (250 µm/s)'},
            '4': {'name': 'MEASURE_F', 'speed': 50.0, 'speed_mms': 0.03125, 'desc': 'Measure Fast (31.25 µm/s)'},
            '5': {'name': 'MEASURE_M', 'speed': 20.0, 'speed_mms': 0.0125, 'desc': 'Measure Medium (12.5 µm/s)'},
            '6': {'name': 'MEASURE_U', 'speed': 10.0, 'speed_mms': 0.00625, 'desc': 'Measure Ultra (6.25 µm/s)'},
            '7': {'name': 'MEASURE_X', 'speed': 5.0, 'speed_mms': 0.003125, 'desc': 'Measure Extra (3.125 µm/s)'},
            '8': {'name': 'MEASURE_Z', 'speed': 2.0, 'speed_mms': 0.00125, 'desc': 'Measure Zero (1.25 µm/s)'},
        }
        
        # Data storage
        self.all_test_data = {}  # {speed_name: {'times': [], 'forces': [], 'positions': []}}
        self.current_test_data = None
        self.current_test_info = None
        
        # Output directory
        self.output_dir = r"C:\Users\srnan\Documents\pythontest"
        self.ensure_output_dir()
        
        # Connection
        self.connect_serial()
        
    def ensure_output_dir(self):
        """Ensure output directory exists"""
        try:
            os.makedirs(self.output_dir, exist_ok=True)
            print(f"✓ Output directory: {self.output_dir}")
        except Exception as e:
            print(f"⚠ Warning: Could not create directory: {e}")
            self.output_dir = os.getcwd()
    
    def connect_serial(self):
        """Connect to Arduino"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            time.sleep(2)  # Wait for Arduino to reset
            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            return True
        except Exception as e:
            print(f"✗ Error connecting to {self.port}: {e}")
            self.list_ports()
            return False
    
    def list_ports(self):
        """List available ports"""
        print("\nAvailable ports:")
        ports = [port.device for port in serial.tools.list_ports.comports()]
        if ports:
            for port in ports:
                print(f"  - {port}")
        else:
            print("  No ports found")
    
    def send_command(self, cmd):
        """Send command to Arduino"""
        try:
            if self.ser and self.ser.is_open:
                self.ser.write(cmd.encode() + b'\n')
                time.sleep(0.1)
                return True
        except Exception as e:
            print(f"✗ Error sending command: {e}")
        return False
    
    def read_data_stream(self, duration=60):
        """Read data stream from Arduino for a duration"""
        print(f"\n⏱ Reading data for {duration} seconds...")
        self.current_test_data = {'times': [], 'forces': [], 'positions': []}
        
        start_time = time.time()
        last_print = start_time
        data_count = 0
        
        try:
            while (time.time() - start_time) < duration and self.ser.is_open:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if not line:
                        continue
                    
                    # Parse JSON data
                    if line.startswith('{') and line.endswith('}'):
                        try:
                            data = json.loads(line)
                            time_val = data.get('t', 0)
                            force = data.get('f', 0)
                            position = data.get('p', 0)
                            
                            self.current_test_data['times'].append(time_val)
                            self.current_test_data['forces'].append(force)
                            self.current_test_data['positions'].append(position)
                            data_count += 1
                            
                            # Print progress every 5 seconds
                            if (time.time() - last_print) >= 5:
                                elapsed = time.time() - start_time
                                print(f"  {elapsed:.1f}s: {data_count} data points, Force: {force:.5f}N")
                                last_print = time.time()
                        except json.JSONDecodeError:
                            pass
                    
                    # Check for end marker
                    elif "END_STREAM" in line:
                        break
                    
                    # Print any other messages
                    elif line and not line.startswith("="):
                        print(f"  >> {line}")
                
                time.sleep(0.01)
        
        except KeyboardInterrupt:
            print("\n✗ Data reading interrupted")
        
        print(f"✓ Data reading complete: {data_count} points collected")
        return len(self.current_test_data['times']) > 0
    
    def calculate_peak_force(self):
        """Calculate peak force from current test data"""
        if not self.current_test_data or not self.current_test_data['forces']:
            return 0
        return max(self.current_test_data['forces'])
    
    def save_to_excel(self):
        """Save all test data to Excel file"""
        if not self.all_test_data:
            print("✗ No data to save")
            return False
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = os.path.join(self.output_dir, f"surface_tension_results_{timestamp}.xlsx")
        
        try:
            wb = Workbook()
            wb.remove(wb.active)  # Remove default sheet
            
            # Create a sheet for each test
            for test_name, data in self.all_test_data.items():
                ws = wb.create_sheet(title=test_name[:31])  # Excel sheet name limit
                
                # Header
                ws['A1'] = f"Surface Tension Test Results - {test_name}"
                ws['A1'].font = Font(bold=True, size=14, color="FFFFFF")
                ws['A1'].fill = PatternFill(start_color="1F4E78", end_color="1F4E78", fill_type="solid")
                ws.merge_cells('A1:D1')
                ws['A1'].alignment = Alignment(horizontal='center', vertical='center')
                
                # Test info
                ws['A3'] = "Test Information:"
                ws['A3'].font = Font(bold=True, size=11)
                
                ws['A4'] = f"Speed (µm/s):"
                ws['B4'] = data['speed_mms'] * 1000
                ws['A5'] = f"Peak Force (N):"
                ws['B5'] = data.get('peak_force', 0)
                ws['A6'] = f"Duration (s):"
                ws['B6'] = data['times'][-1] if data['times'] else 0
                ws['A7'] = f"Data Points:"
                ws['B7'] = len(data['times'])
                
                # Data table header
                ws['A9'] = "Time (s)"
                ws['B9'] = "Force (N)"
                ws['C9'] = "Position (mm)"
                
                # Style header
                header_fill = PatternFill(start_color="D9E1F2", end_color="D9E1F2", fill_type="solid")
                header_font = Font(bold=True)
                for col in ['A', 'B', 'C']:
                    ws[f'{col}9'].fill = header_fill
                    ws[f'{col}9'].font = header_font
                    ws[f'{col}9'].alignment = Alignment(horizontal='center')
                
                # Data rows
                for row_idx, (time, force, pos) in enumerate(zip(data['times'], data['forces'], data['positions']), start=10):
                    ws[f'A{row_idx}'] = round(time, 3)
                    ws[f'B{row_idx}'] = round(force, 5)
                    ws[f'C{row_idx}'] = round(pos, 4)
                    
                    # Center alignment
                    for col in ['A', 'B', 'C']:
                        ws[f'{col}{row_idx}'].alignment = Alignment(horizontal='center')
                
                # Border
                thin_border = Border(
                    left=Side(style='thin'),
                    right=Side(style='thin'),
                    top=Side(style='thin'),
                    bottom=Side(style='thin')
                )
                
                for row in ws.iter_rows(min_row=9, max_row=len(data['times'])+9, min_col=1, max_col=3):
                    for cell in row:
                        cell.border = thin_border
                
                # Auto width
                ws.column_dimensions['A'].width = 15
                ws.column_dimensions['B'].width = 15
                ws.column_dimensions['C'].width = 15
            
            wb.save(filename)
            print(f"✓ Data saved to: {filename}")
            return filename
        
        except Exception as e:
            print(f"✗ Error saving Excel file: {e}")
            return False
    
    def smooth_curve(self, x, y, points=300):
        """Create smooth curve from points using spline interpolation"""
        if len(x) < 4:
            return x, y
        
        try:
            # Sort by x values
            sorted_pairs = sorted(zip(x, y))
            x_sorted = np.array([p[0] for p in sorted_pairs])
            y_sorted = np.array([p[1] for p in sorted_pairs])
            
            # Create spline
            spl = make_interp_spline(x_sorted, y_sorted, k=min(3, len(x_sorted)-1))
            x_smooth = np.linspace(x_sorted.min(), x_sorted.max(), points)
            y_smooth = spl(x_smooth)
            
            return x_smooth, y_smooth
        except:
            return x, y
    
    def plot_results(self):
        """Plot all test results with smooth curves"""
        if not self.all_test_data:
            print("✗ No data to plot")
            return
        
        num_tests = len(self.all_test_data)
        
        # Set style
        plt.style.use('seaborn-v0_8-darkgrid')
        fig, axes = plt.subplots(num_tests, 1, figsize=(14, 5.5*num_tests))
        fig.patch.set_facecolor('white')
        
        # Handle single test
        if num_tests == 1:
            axes = [axes]
        
        fig.suptitle('Surface Tension Test Results', fontsize=18, fontweight='bold', y=0.995)
        
        for idx, (test_name, data) in enumerate(self.all_test_data.items()):
            ax = axes[idx]
            
            times = np.array(data['times'])
            forces = np.array(data['forces'])
            positions = np.array(data['positions'])
            peak_force = data.get('peak_force', 0)
            speed = data['speed_mms'] * 1000
            
            # Smooth curves
            times_smooth, forces_smooth = self.smooth_curve(times, forces, points=500)
            times_pos_smooth, positions_smooth = self.smooth_curve(times, positions, points=500)
            
            # Plot force with smooth curve
            ax.plot(times_smooth, forces_smooth, '-', color='#E74C3C', linewidth=3, label='Force (N)', zorder=2)
            ax.scatter(times, forces, color='#C0392B', s=15, alpha=0.4, zorder=1)
            
            # Peak force line
            ax.axhline(y=peak_force, color='#E74C3C', linestyle='--', linewidth=2, alpha=0.6, 
                       label=f'Peak: {peak_force:.5f}N')
            
            # Secondary axis for position
            ax2 = ax.twinx()
            ax2.plot(times_pos_smooth, positions_smooth, '-', color='#3498DB', linewidth=3, 
                    label='Position (mm)', zorder=2)
            ax2.scatter(times, positions, color='#2980B9', s=15, alpha=0.4, zorder=1)
            
            # Labels and title
            ax.set_xlabel('Time (s)', fontsize=12, fontweight='bold')
            ax.set_ylabel('Force (N)', fontsize=12, fontweight='bold', color='#E74C3C')
            ax2.set_ylabel('Position (mm)', fontsize=12, fontweight='bold', color='#3498DB')
            ax.set_title(f'{test_name} - Speed: {speed:.3f} µm/s', fontsize=13, fontweight='bold', pad=15)
            
            ax.tick_params(axis='y', labelcolor='#E74C3C', labelsize=10)
            ax2.tick_params(axis='y', labelcolor='#3498DB', labelsize=10)
            ax.tick_params(axis='x', labelsize=10)
            
            # Grid
            ax.grid(True, alpha=0.3, linestyle='--', linewidth=0.7)
            ax.set_axisbelow(True)
            
            # Legend
            lines1, labels1 = ax.get_legend_handles_labels()
            lines2, labels2 = ax2.get_legend_handles_labels()
            ax.legend(lines1 + lines2, labels1 + labels2, loc='upper left', fontsize=10, framealpha=0.95)
            
            # Background color
            ax.set_facecolor('#F8F9FA')
        
        plt.tight_layout()
        plt.show()
    
    def show_menu(self):
        """Display main menu"""
        print("\n" + "="*70)
        print("SURFACE TENSION TESTER - Full Python Control".center(70))
        print("="*70)
        print("\nAvailable Tests:")
        for key, test in self.tests.items():
            print(f"  {key}. {test['desc']}")
        print("\nCommands:")
        print("  T     - Tare scale (zero force)")
        print("  0     - Set home position")
        print("  H     - Go home manually")
        print("  A     - Run all tests (1-8) automatically")
        print("  C     - Clear all data")
        print("  S     - Show current results and exit")
        print("  Q     - Quit without saving")
        print("="*70)
    
    def run(self):
        """Main control loop"""
        if not self.ser or not self.ser.is_open:
            print("✗ Failed to connect to Arduino")
            return
        
        print("\n✓ System Ready! Waiting for Arduino initialization...")
        time.sleep(2)
        
        # Read initial messages from Arduino
        print("\nInitializing Arduino...")
        init_time = time.time()
        while (time.time() - init_time) < 3:
            if self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if "System Ready" in line or "✓" in line:
                    print(f"  {line}")
            time.sleep(0.1)
        
        while True:
            self.show_menu()
            choice = input("\nEnter command (1-8/T/0/H/A/C/S/Q): ").strip().upper()
            
            if choice == 'Q':
                print("\n✗ Exiting without saving...")
                break
            
            elif choice == 'S':
                print("\nSaving and displaying results...")
                excel_file = self.save_to_excel()
                if excel_file:
                    self.plot_results()
                break
            
            elif choice == 'C':
                self.all_test_data = {}
                print("✓ All data cleared")
            
            elif choice == 'T':
                print("\n>>> Sending Tare command to Arduino...")
                self.send_command('t')
                time.sleep(2)
                print("✓ Tare complete")
            
            elif choice == '0':
                print("\n>>> Setting HOME position...")
                self.send_command('0')
                time.sleep(1)
                print("✓ Home position set")
            
            elif choice == 'H':
                print("\n>>> Going HOME...")
                self.send_command('h')
                time.sleep(3)
                print("✓ Home reached")
            
            elif choice == 'A':
                print("\n🚀 Starting Auto Test Mode (All 8 speeds)...")
                for test_num in ['1', '2', '3', '4', '5', '6', '7', '8']:
                    self.run_single_test(test_num)
                    if test_num != '8':
                        input("Press Enter to continue to next test...")
            
            elif choice in self.tests:
                self.run_single_test(choice)
            
            else:
                print("✗ Invalid command")
    
    def run_single_test(self, test_num):
        """Run a single test"""
        if test_num not in self.tests:
            print("✗ Invalid test number")
            return
        
        test_info = self.tests[test_num]
        test_name = test_info['name']
        speed_mms = test_info['speed_mms']
        
        print(f"\n{'='*70}")
        print(f"TEST {test_num}: {test_info['desc']}")
        print(f"{'='*70}")
        
        # Send command to Arduino
        self.send_command(test_num)
        time.sleep(1)
        
        # Estimate test duration (15mm / speed)
        estimated_duration = (15.0 / speed_mms) + 5  # Add 5s buffer for return
        estimated_duration = int(estimated_duration) + 10
        
        print(f"Speed: {speed_mms*1000:.3f} µm/s")
        print(f"Estimated duration: ~{estimated_duration}s")
        
        # Read data
        if self.read_data_stream(duration=estimated_duration):
            peak_force = self.calculate_peak_force()
            
            # Store data
            self.all_test_data[test_name] = {
                'times': self.current_test_data['times'],
                'forces': self.current_test_data['forces'],
                'positions': self.current_test_data['positions'],
                'speed_mms': speed_mms,
                'peak_force': peak_force,
            }
            
            # Display results
            print(f"\n📊 Test Results:")
            print(f"  Peak Force: {peak_force:.5f} N ⭐")
            print(f"  Duration: {self.current_test_data['times'][-1]:.2f} s")
            print(f"  Data Points: {len(self.current_test_data['times'])}")
            print(f"  ✓ Data saved for {test_name}")
        else:
            print(f"✗ Failed to collect data for {test_name}")


def main():
    """Main entry point"""
    print("\n" + "="*70)
    print("SURFACE TENSION TESTER - Full Python Control System".center(70))
    print("="*70)
    
    # Auto-detect port
    ports = [port.device for port in serial.tools.list_ports.comports()]
    port = ports[0] if ports else 'COM5'
    
    print(f"\nConnecting to Arduino on {port}...")
    
    controller = SurfaceTensionController(port=port)
    controller.run()
    
    if controller.ser and controller.ser.is_open:
        controller.ser.close()
        print("\n✓ Serial connection closed")


if __name__ == '__main__':
    main()