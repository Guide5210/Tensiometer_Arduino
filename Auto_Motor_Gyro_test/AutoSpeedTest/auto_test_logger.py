"""
Automatic Speed Test Data Logger
บันทึกข้อมูลจาก 7 ความเร็วอัตโนมัติ
แยกไฟล์แต่ละความเร็ว + สรุปรวม
"""

import serial
import csv
import matplotlib.pyplot as plt
from datetime import datetime
import numpy as np
import os

class AutoTestLogger:
    def __init__(self, port='COM5', baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.is_running = True
        
        # สร้างโฟลเดอร์
        self.base_folder = f'auto_test_{datetime.now().strftime("%Y%m%d_%H%M%S")}'
        os.makedirs(self.base_folder, exist_ok=True)
        
        # เก็บข้อมูลแต่ละ test
        self.current_test_data = {
            'times': [],
            'positions': [],
            'speeds': [],
            'vibrations': []
        }
        self.current_test_name = ""
        self.test_count = 0
        
        # เก็บสรุปทั้งหมด
        self.all_tests_summary = []
        
    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"✅ Connected to {self.port}")
            print("Waiting for Arduino...")
            
            while True:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    print(line)
                    if 'Ready!' in line or 'Commands' in line:
                        break
            
            print("\n🚀 System Ready!")
            print("\nPress 'A' in terminal or send via Serial Monitor to start")
            return True
            
        except Exception as e:
            print(f"❌ Error: {e}")
            return False
    
    def run(self):
        if not self.connect():
            return
        
        print(f"\n📂 Saving data to: {self.base_folder}/")
        print("\n" + "="*50)
        print("Commands:")
        print("  A = Start AUTO TEST")
        print("  S = Stop test")
        print("  0 = Set HOME")
        print("  H = Go HOME")
        print("="*50)
        print("\nListening for auto test...")
        
        in_test = False
        
        # เริ่ม thread สำหรับรับคำสั่งจากผู้ใช้
        import threading
        input_thread = threading.Thread(target=self.user_input_thread, daemon=True)
        input_thread.start()
        
        try:
            while self.is_running:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    # แสดงข้อความจาก Arduino
                    if any(x in line for x in ['>>>', '===', '🚀', '✓', '🎉', '❌', '-----']):
                        print(f"\n{line}")
                        continue
                    
                    # เริ่ม test ใหม่
                    if line == 'DATA_START':
                        in_test = True
                        self.current_test_data = {
                            'times': [],
                            'positions': [],
                            'speeds': [],
                            'vibrations': []
                        }
                        continue
                    
                    # จบ test
                    if line == 'DATA_END':
                        if in_test:
                            self.save_test_data()
                            in_test = False
                        continue
                    
                    # บันทึกข้อมูล
                    if in_test and ',' in line:
                        try:
                            parts = line.split(',')
                            if len(parts) >= 5:
                                time_val = float(parts[0])
                                pos_val = float(parts[1])
                                speed_val = float(parts[2])
                                vib_val = float(parts[3])
                                test_name = parts[4].strip()
                                
                                self.current_test_data['times'].append(time_val)
                                self.current_test_data['positions'].append(pos_val)
                                self.current_test_data['speeds'].append(speed_val)
                                self.current_test_data['vibrations'].append(vib_val)
                                self.current_test_name = test_name
                                
                                # แสดงทุก 2 วินาที
                                if len(self.current_test_data['times']) % 100 == 0:
                                    print(f"  {time_val:.1f}s | Pos: {pos_val:.3f}mm | Vib: {vib_val:.3f}m/s²")
                        except:
                            pass
                    
                    # แสดงข้อความอื่นๆ
                    if line and not line.startswith('Time'):
                        if 'Test' in line or 'Speed' in line or 'Distance' in line or 'Moving' in line:
                            print(line)
        
        except KeyboardInterrupt:
            print("\n\n🛑 Stopped by user")
        
        finally:
            if self.ser:
                self.ser.close()
            
            # สร้างสรุปและกราฟ
            if self.test_count > 0:
                self.create_summary()
                self.create_plots()
            
            print(f"\n✅ All data saved in: {self.base_folder}/")
    
    def user_input_thread(self):
        """รับคำสั่งจากผู้ใช้"""
        while self.is_running:
            try:
                cmd = input().strip().upper()
                if cmd and self.ser and self.ser.is_open:
                    self.ser.write(cmd.encode())
                    print(f"\n📤 Sent: {cmd}")
            except:
                break
    
    def save_test_data(self):
        if not self.current_test_data['times']:
            return
        
        self.test_count += 1
        test_name = self.current_test_name
        
        # บันทึก CSV
        filename = f"{self.base_folder}/{self.test_count}_{test_name}.csv"
        with open(filename, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['Time(sec)', 'Position(mm)', 'Speed(mm/s)', 'Vibration(m/s²)'])
            
            for i in range(len(self.current_test_data['times'])):
                writer.writerow([
                    self.current_test_data['times'][i],
                    self.current_test_data['positions'][i],
                    self.current_test_data['speeds'][i],
                    self.current_test_data['vibrations'][i]
                ])
        
        # คำนวณสถิติ
        vibs = self.current_test_data['vibrations']
        summary = {
            'test_num': self.test_count,
            'name': test_name,
            'duration': max(self.current_test_data['times']),
            'samples': len(vibs),
            'max_vib': max(vibs),
            'mean_vib': np.mean(vibs),
            'std_vib': np.std(vibs),
            'speed': self.current_test_data['speeds'][0]
        }
        self.all_tests_summary.append(summary)
        
        print(f"\n✅ Saved: {filename}")
        print(f"   Samples: {summary['samples']}, Max Vib: {summary['max_vib']:.4f} m/s²")
    
    def create_summary(self):
        # สร้างไฟล์สรุป
        summary_file = f"{self.base_folder}/SUMMARY.txt"
        with open(summary_file, 'w', encoding='utf-8') as f:
            f.write("=" * 60 + "\n")
            f.write("AUTO SPEED TEST SUMMARY\n")
            f.write("=" * 60 + "\n\n")
            
            for s in self.all_tests_summary:
                f.write(f"Test {s['test_num']}: {s['name']}\n")
                f.write(f"  Speed: {s['speed']:.6f} mm/s\n")
                f.write(f"  Duration: {s['duration']:.2f} sec\n")
                f.write(f"  Samples: {s['samples']}\n")
                f.write(f"  Max Vibration: {s['max_vib']:.4f} m/s²\n")
                f.write(f"  Mean Vibration: {s['mean_vib']:.4f} m/s²\n")
                f.write(f"  Std Deviation: {s['std_vib']:.4f} m/s²\n")
                f.write("\n")
            
            f.write("=" * 60 + "\n")
        
        print(f"\n✅ Summary saved: {summary_file}")
    
    def create_plots(self):
        # กราฟเปรียบเทียบ
        fig, axes = plt.subplots(2, 1, figsize=(14, 10))
        
        # กราฟ 1: Max Vibration vs Speed
        ax1 = axes[0]
        speeds = [s['speed'] for s in self.all_tests_summary]
        max_vibs = [s['max_vib'] for s in self.all_tests_summary]
        names = [s['name'] for s in self.all_tests_summary]
        
        colors = plt.cm.viridis(np.linspace(0, 1, len(speeds)))
        bars = ax1.bar(range(len(speeds)), max_vibs, color=colors)
        ax1.set_xticks(range(len(speeds)))
        ax1.set_xticklabels(names, rotation=45, ha='right')
        ax1.set_ylabel('Max Vibration (m/s²)')
        ax1.set_title('Maximum Vibration by Speed', fontweight='bold')
        ax1.grid(True, alpha=0.3)
        
        # เพิ่มค่าบนแท่ง
        for i, (bar, val) in enumerate(zip(bars, max_vibs)):
            height = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width()/2., height,
                    f'{val:.3f}',
                    ha='center', va='bottom', fontsize=9)
        
        # กราฟ 2: Mean Vibration vs Speed
        ax2 = axes[1]
        mean_vibs = [s['mean_vib'] for s in self.all_tests_summary]
        
        bars2 = ax2.bar(range(len(speeds)), mean_vibs, color=colors)
        ax2.set_xticks(range(len(speeds)))
        ax2.set_xticklabels(names, rotation=45, ha='right')
        ax2.set_ylabel('Mean Vibration (m/s²)')
        ax2.set_title('Average Vibration by Speed', fontweight='bold')
        ax2.grid(True, alpha=0.3)
        
        for i, (bar, val) in enumerate(zip(bars2, mean_vibs)):
            height = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width()/2., height,
                    f'{val:.4f}',
                    ha='center', va='bottom', fontsize=9)
        
        plt.tight_layout()
        
        plot_file = f"{self.base_folder}/comparison.png"
        plt.savefig(plot_file, dpi=300, bbox_inches='tight')
        print(f"✅ Comparison plot saved: {plot_file}")
        
        plt.show()

if __name__ == "__main__":
    PORT = 'COM5'  # เปลี่ยนตามพอร์ตของคุณ
    
    logger = AutoTestLogger(port=PORT)
    logger.run()