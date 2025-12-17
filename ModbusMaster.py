import tkinter as tk
from tkinter import ttk, messagebox
import socket
import struct
import threading
import time

# --- CẤU HÌNH ---
DEFAULT_IP = "192.168.4.1"  # IP của ESP32 (AP Mode)
DEFAULT_PORT = 502
REFRESH_RATE = 100          # Tốc độ làm mới (ms)

# --- CLASS TỰ XỬ LÝ MODBUS TCP (KHÔNG DÙNG THƯ VIỆN NGOÀI) ---
class SimpleModbusClient:
    def __init__(self):
        self.sock = None
        self.transaction_id = 0
        self.ip = ""
        self.port = 502

    def connect(self, ip, port):
        self.ip = ip
        self.port = port
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(1.0) # Timeout 1 giây
            self.sock.connect((self.ip, self.port))
            return True
        except Exception as e:
            print(f"Lỗi kết nối: {e}")
            self.sock = None
            return False

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except:
                pass
            self.sock = None

    def read_holding_registers(self, start_addr, count):
        """
        Gửi lệnh Function 03 (Read Holding Registers)
        Trả về: List các giá trị int hoặc None nếu lỗi
        """
        if not self.sock:
            return None

        try:
            self.transaction_id = (self.transaction_id + 1) & 0xFFFF
            
            # Cấu trúc Modbus TCP Header (MBAP) + PDU
            # [TransID][ProtID][Len][UnitID] + [FuncCode][StartAddr][Count]
            # Format struct: >HHHBBHH (>: Big Endian, H: ushort 2 byte, B: uchar 1 byte)
            # Length = UnitID(1) + FuncCode(1) + StartAddr(2) + Count(2) = 6 byte
            
            req = struct.pack('>HHHBBHH', 
                              self.transaction_id, # Transaction ID
                              0,                   # Protocol ID (0 = Modbus)
                              6,                   # Length (Số byte phía sau)
                              1,                   # Unit ID
                              3,                   # Function Code (03)
                              start_addr,          # Địa chỉ bắt đầu
                              count)               # Số lượng thanh ghi
            
            self.sock.sendall(req)

            # --- ĐỌC PHẢN HỒI ---
            # 1. Đọc Header (7 byte đầu: TID, PID, Len, UID)
            header = self.sock.recv(7)
            if len(header) < 7: return None
            
            tid, pid, length, uid = struct.unpack('>HHHB', header)
            
            # 2. Đọc phần dữ liệu còn lại (PDU)
            remaining_len = length - 1
            pdu = self.sock.recv(remaining_len)
            
            if len(pdu) < remaining_len: return None

            # 3. Parse dữ liệu
            func_code = pdu[0]
            if func_code != 3: 
                return None # Lỗi hoặc Exception code
            
            # byte_count = pdu[1]
            data_bytes = pdu[2:]
            
            # Chuyển đổi byte thành list các số nguyên 16-bit (Big Endian)
            values = list(struct.unpack(f'>{count}H', data_bytes))
            return values

        except Exception as e:
            print(f"Lỗi đọc dữ liệu: {e}")
            self.close() # Ngắt kết nối để thử lại sau
            return None

# --- GIAO DIỆN CHÍNH ---
class RCDashboard:
    def __init__(self, root):
        self.root = root
        self.root.title("RC MONITOR - NO LIB REQUIRED")
        self.root.geometry("800x500")
        self.root.configure(bg="#1e1e1e")
        
        self.mb_client = SimpleModbusClient()
        self.is_connected = False

        # Style
        self.style = ttk.Style()
        self.style.theme_use('clam')
        self.style.configure("TLabel", background="#1e1e1e", foreground="#00ff00", font=("Consolas", 12))
        self.style.configure("TFrame", background="#1e1e1e")
        self.style.configure("Header.TLabel", font=("Arial", 16, "bold"), foreground="#ffffff")
        
        self.setup_ui()
        self.update_loop()

    def setup_ui(self):
        # 1. Connection Bar
        conn_frame = ttk.Frame(self.root, padding=10)
        conn_frame.pack(fill="x")
        
        ttk.Label(conn_frame, text="IP Address:", foreground="white").pack(side="left", padx=5)
        self.entry_ip = ttk.Entry(conn_frame, width=15)
        self.entry_ip.insert(0, DEFAULT_IP)
        self.entry_ip.pack(side="left", padx=5)
        
        ttk.Label(conn_frame, text="Port:", foreground="white").pack(side="left", padx=5)
        self.entry_port = ttk.Entry(conn_frame, width=6)
        self.entry_port.insert(0, str(DEFAULT_PORT))
        self.entry_port.pack(side="left", padx=5)
        
        self.btn_connect = tk.Button(conn_frame, text="CONNECT", bg="#008000", fg="white", command=self.toggle_connection)
        self.btn_connect.pack(side="left", padx=10)
        
        self.lbl_status = ttk.Label(conn_frame, text="DISCONNECTED", foreground="red")
        self.lbl_status.pack(side="right", padx=10)

        # 2. Main Dashboard Grid
        main_frame = ttk.Frame(self.root, padding=20)
        main_frame.pack(fill="both", expand=True)
        
        # --- Cột Trái: Thông số lái ---
        left_panel = ttk.LabelFrame(main_frame, text=" CONTROL STATUS ", padding=10)
        left_panel.pack(side="left", fill="both", expand=True, padx=5)
        
        # Mode
        self.lbl_mode = ttk.Label(left_panel, text="MODE: ---", font=("Consolas", 20, "bold"), foreground="yellow")
        self.lbl_mode.pack(pady=10)
        
        # Speed
        self.create_bar(left_panel, "SPEED", "lbl_speed_val", "prog_speed")
        # Steer
        self.create_bar(left_panel, "STEER", "lbl_steer_val", "prog_steer")

        # --- Cột Phải: Cảm biến ---
        right_panel = ttk.LabelFrame(main_frame, text=" SENSOR DATA ", padding=10)
        right_panel.pack(side="right", fill="both", expand=True, padx=5)
        
        # Distance (To bự)
        ttk.Label(right_panel, text="DISTANCE (cm)", foreground="cyan").pack(pady=5)
        self.lbl_dist = ttk.Label(right_panel, text="---", font=("Impact", 40), foreground="cyan")
        self.lbl_dist.pack(pady=10)
        
        # Angles
        self.create_value_row(right_panel, "PITCH (Nghiêng trước/sau):", "lbl_pitch")
        self.create_value_row(right_panel, "ROLL  (Nghiêng trái/phải):", "lbl_roll")
        self.create_value_row(right_panel, "YAW   (Hướng xoay):", "lbl_yaw")

    def create_bar(self, parent, title, label_attr, bar_attr):
        frame = ttk.Frame(parent)
        frame.pack(fill="x", pady=10)
        
        header = ttk.Frame(frame)
        header.pack(fill="x")
        ttk.Label(header, text=title, font=("Consolas", 10, "bold")).pack(side="left")
        
        val_lbl = ttk.Label(header, text="0", foreground="white")
        val_lbl.pack(side="right")
        setattr(self, label_attr, val_lbl)
        
        # Progress bar
        canvas = tk.Canvas(frame, height=20, bg="#333", highlightthickness=0)
        canvas.pack(fill="x", pady=5)
        
        # Vạch giữa (Zero point)
        canvas.create_line(150, 0, 150, 20, fill="gray", width=2)
        setattr(self, bar_attr, canvas)

    def create_value_row(self, parent, title, label_attr):
        frame = ttk.Frame(parent)
        frame.pack(fill="x", pady=5)
        ttk.Label(frame, text=title).pack(side="left")
        lbl = ttk.Label(frame, text="0.0°", foreground="#ff00ff")
        lbl.pack(side="right")
        setattr(self, label_attr, lbl)

    def toggle_connection(self):
        if not self.is_connected:
            ip = self.entry_ip.get()
            try:
                port = int(self.entry_port.get())
                if self.mb_client.connect(ip, port):
                    self.is_connected = True
                    self.btn_connect.config(text="DISCONNECT", bg="red")
                    self.lbl_status.config(text="CONNECTED", foreground="#00ff00")
                else:
                    messagebox.showerror("Error", "Could not connect to target")
            except ValueError:
                messagebox.showerror("Error", "Port must be a number")
        else:
            self.mb_client.close()
            self.is_connected = False
            self.btn_connect.config(text="CONNECT", bg="#008000")
            self.lbl_status.config(text="DISCONNECTED", foreground="red")

    def update_bar(self, canvas, value):
        w = canvas.winfo_width()
        if w < 10: w = 300 
        center = w / 2
        
        length = (abs(value) / 100.0) * (w / 2)
        
        canvas.delete("bar")
        color = "#00ff00" if value >= 0 else "#ff4444"
        
        if value > 0:
            canvas.create_rectangle(center, 0, center + length, 20, fill=color, tags="bar")
        else:
            canvas.create_rectangle(center - length, 0, center, 20, fill=color, tags="bar")
            
        canvas.create_line(center, 0, center, 20, fill="white", width=1)

    def to_signed16(self, n):
        n = n & 0xFFFF
        return (n ^ 0x8000) - 0x8000

    def update_loop(self):
        if self.is_connected:
            regs = self.mb_client.read_holding_registers(0, 7)
            
            if regs:
                try:
                    state = regs[0]
                    self.lbl_mode.config(text="READY" if state == 1 else "WAITING", 
                                       foreground="#00ff00" if state == 1 else "yellow")

                    # === ĐÃ SỬA: Đảo dấu (-) vì Joystick lắp ngược ===
                    # Công thức cũ: regs[1] - 100
                    # Công thức mới: -(regs[1] - 100) -> 100 - regs[1]
                    
                    # Steer (Reg 1)
                    steer = 100 - regs[1] 
                    self.lbl_steer_val.config(text=f"{steer}%")
                    self.update_bar(self.prog_steer, steer)

                    # Speed (Reg 2)
                    speed = 100 - regs[2]
                    self.lbl_speed_val.config(text=f"{speed}%")
                    self.update_bar(self.prog_speed, speed)

                    # Angles (Reg 3,4,5)
                    pitch = self.to_signed16(regs[3]) / 10.0
                    roll  = self.to_signed16(regs[4]) / 10.0
                    yaw   = self.to_signed16(regs[5]) / 10.0
                    
                    self.lbl_pitch.config(text=f"{pitch:.1f}°")
                    self.lbl_roll.config(text=f"{roll:.1f}°")
                    self.lbl_yaw.config(text=f"{yaw:.1f}°")

                    # Distance (Reg 6)
                    dist = regs[6]
                    self.lbl_dist.config(text=f"{dist}")
                except Exception as e:
                    print(f"Lỗi hiển thị: {e}")
            else:
                print("Read timeout/error")

        self.root.after(REFRESH_RATE, self.update_loop)

if __name__ == "__main__":
    root = tk.Tk()
    app = RCDashboard(root)
    root.mainloop()