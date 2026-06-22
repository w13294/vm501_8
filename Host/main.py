import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import serial
import serial.tools.list_ports
import threading
import queue
import re
import csv
import time
from datetime import datetime
import os
import shutil
import collections
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

class HostApp:
    def __init__(self, root):
        self.root = root
        self.root.title("STM32 多通道传感器数据监控系统")
        self.root.geometry("1000x700")
        self.root.minsize(800, 600)
        
        # 串口相关
        self.serial_port = None
        self.is_connected = False
        self.read_thread = None
        self.data_queue = queue.Queue()
        
        # 历史记录 CSV
        self.csv_file = None
        self.csv_writer = None
        self.init_csv()

        self.setup_ui()
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        
        # 图表数据缓冲
        self.chart_data_freq = {i: collections.deque(maxlen=100) for i in range(1, 9)}
        self.chart_data_temp = {i: collections.deque(maxlen=100) for i in range(1, 9)}
        
        # 定时更新 UI 和 图表
        self.root.after(100, self.update_ui_from_queue)
        self.root.after(1000, self.update_chart)

    def init_csv(self):
        # 初始化默认的 CSV 记录文件
        if not os.path.exists("logs"):
            os.makedirs("logs")
        filename = datetime.now().strftime("logs/data_log_%Y%m%d_%H%M%S.csv")
        self.csv_file = open(filename, mode='a', newline='', encoding='utf-8-sig')
        self.csv_writer = csv.writer(self.csv_file)
        self.csv_writer.writerow(["时间", "通道", "频率 (Hz)", "温度 (C)", "状态"])
        self.current_csv_path = filename

    def setup_ui(self):
        # 主框架
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # ---------------- 顶部：控制区 ----------------
        control_frame = ttk.LabelFrame(main_frame, text="串口控制")
        control_frame.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(control_frame, text="端口:").grid(row=0, column=0, padx=5, pady=5)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(control_frame, textvariable=self.port_var, width=15)
        self.port_combo.grid(row=0, column=1, padx=5, pady=5)
        self.refresh_ports()

        ttk.Button(control_frame, text="刷新", command=self.refresh_ports).grid(row=0, column=2, padx=5, pady=5)

        ttk.Label(control_frame, text="波特率:").grid(row=0, column=3, padx=5, pady=5)
        self.baud_var = tk.StringVar(value="115200")
        baud_combo = ttk.Combobox(control_frame, textvariable=self.baud_var, width=10)
        baud_combo['values'] = ("9600", "19200", "38400", "57600", "115200", "256000")
        baud_combo.grid(row=0, column=4, padx=5, pady=5)

        self.btn_connect = ttk.Button(control_frame, text="打开串口", command=self.toggle_connection)
        self.btn_connect.grid(row=0, column=5, padx=15, pady=5)
        
        self.btn_export = ttk.Button(control_frame, text="导出历史数据", command=self.export_data)
        self.btn_export.grid(row=0, column=6, padx=15, pady=5)
        
        self.lbl_status = ttk.Label(control_frame, text="未连接", foreground="red")
        self.lbl_status.grid(row=0, column=7, padx=15, pady=5)

        # ---------------- 中部：实时数据面板 (8个通道) ----------------
        realtime_frame = ttk.LabelFrame(main_frame, text="实时监控面板")
        realtime_frame.pack(fill=tk.X, pady=(0, 10))
        
        self.channel_labels = {}
        for i in range(8):
            ch = i + 1
            frame = ttk.Frame(realtime_frame, borderwidth=2, relief="groove")
            frame.grid(row=i // 4, column=i % 4, padx=10, pady=10, sticky="nsew")
            
            realtime_frame.grid_columnconfigure(i % 4, weight=1)
            
            lbl_title = ttk.Label(frame, text=f"通道 {ch}", font=("Helvetica", 12, "bold"))
            lbl_title.pack(pady=(5, 0))
            
            lbl_data = ttk.Label(frame, text="等待数据...", font=("Helvetica", 11), foreground="gray")
            lbl_data.pack(pady=(5, 10))
            
            self.channel_labels[ch] = lbl_data

        # ---------------- 底部：Notebook (数据表格 & 图表) ----------------
        self.notebook = ttk.Notebook(main_frame)
        self.notebook.pack(fill=tk.BOTH, expand=True)
        
        # --- Tab 1: 数据表格 ---
        history_frame = ttk.Frame(self.notebook)
        self.notebook.add(history_frame, text="历史数据表格")

        # 表格滚动条
        tree_scroll = ttk.Scrollbar(history_frame)
        tree_scroll.pack(side=tk.RIGHT, fill=tk.Y)

        self.tree = ttk.Treeview(history_frame, yscrollcommand=tree_scroll.set, 
                                 columns=("Time", "Channel", "Freq", "Temp", "Status"), show="headings")
        tree_scroll.config(command=self.tree.yview)
        
        self.tree.heading("Time", text="时间")
        self.tree.heading("Channel", text="通道")
        self.tree.heading("Freq", text="频率 (Hz)")
        self.tree.heading("Temp", text="温度 (°C)")
        self.tree.heading("Status", text="状态")

        self.tree.column("Time", width=150, anchor=tk.CENTER)
        self.tree.column("Channel", width=80, anchor=tk.CENTER)
        self.tree.column("Freq", width=120, anchor=tk.CENTER)
        self.tree.column("Temp", width=120, anchor=tk.CENTER)
        self.tree.column("Status", width=150, anchor=tk.CENTER)
        
        self.tree.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        lbl_info = ttk.Label(history_frame, text=f"日志正自动保存至: {self.current_csv_path}")
        lbl_info.pack(anchor=tk.W, padx=5, pady=2)
        
        # --- Tab 2: 实时图表 ---
        chart_frame = ttk.Frame(self.notebook)
        self.notebook.add(chart_frame, text="实时折线图")
        self.setup_chart(chart_frame)

    def setup_chart(self, parent):
        top_bar = ttk.Frame(parent)
        top_bar.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(top_bar, text="选择通道:").pack(side=tk.LEFT)
        self.chart_ch_var = tk.StringVar(value="1")
        ch_combo = ttk.Combobox(top_bar, textvariable=self.chart_ch_var, values=[str(i) for i in range(1, 9)], width=5, state="readonly")
        ch_combo.pack(side=tk.LEFT, padx=5)
        ch_combo.bind("<<ComboboxSelected>>", lambda e: self.update_chart(force=True))
        
        # Matplotlib 图形对象
        self.fig, (self.ax_freq, self.ax_temp) = plt.subplots(2, 1, figsize=(6, 4), dpi=100)
        self.canvas = FigureCanvasTkAgg(self.fig, master=parent)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        self.fig.tight_layout()

    def export_data(self):
        if not os.path.exists(self.current_csv_path):
            messagebox.showwarning("警告", "当前还没有日志文件产生。")
            return
            
        save_path = filedialog.asksaveasfilename(
            defaultextension=".csv",
            filetypes=[("CSV 文件", "*.csv"), ("所有文件", "*.*")],
            initialfile=os.path.basename(self.current_csv_path),
            title="导出数据记录"
        )
        if save_path:
            try:
                if self.csv_file:
                    self.csv_file.flush()
                shutil.copy2(self.current_csv_path, save_path)
                messagebox.showinfo("成功", f"数据已成功导出至:\n{save_path}")
            except Exception as e:
                messagebox.showerror("错误", f"导出失败: {e}")

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_list = [port.device for port in ports]
        self.port_combo['values'] = port_list
        if port_list:
            self.port_combo.current(0)
        else:
            self.port_var.set("")

    def toggle_connection(self):
        if not self.is_connected:
            # 连接串口
            port = self.port_var.get()
            baud = self.baud_var.get()
            if not port:
                messagebox.showerror("错误", "请先选择一个串口")
                return
            try:
                self.serial_port = serial.Serial(port, int(baud), timeout=1)
                self.is_connected = True
                self.btn_connect.config(text="关闭串口")
                self.lbl_status.config(text="已连接", foreground="green")
                self.port_combo.config(state="disabled")
                
                # 开启读取线程
                self.read_thread = threading.Thread(target=self.serial_read_loop, daemon=True)
                self.read_thread.start()
            except Exception as e:
                messagebox.showerror("串口错误", f"无法打开串口: {e}")
        else:
            # 断开串口
            self.is_connected = False
            if self.serial_port:
                self.serial_port.close()
            self.btn_connect.config(text="打开串口")
            self.lbl_status.config(text="未连接", foreground="red")
            self.port_combo.config(state="normal")

    def serial_read_loop(self):
        # 正则表达式匹配两种可能的输出
        # [FreeRTOS] Channel 1: Freq = 2885.50 Hz, Temp = 25.7 C
        # [FreeRTOS] Channel 7: Read Timeout or Error!
        pattern_data = re.compile(r"\[FreeRTOS\] Channel (\d+): Freq = ([\d.]+) Hz, Temp = ([\d.]+) C")
        pattern_error = re.compile(r"\[FreeRTOS\] Channel (\d+): Read Timeout or Error!")

        while self.is_connected and self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8', errors='replace').strip()
                if line:
                    now_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                    
                    match_data = pattern_data.search(line)
                    if match_data:
                        ch = int(match_data.group(1))
                        freq = match_data.group(2)
                        temp = match_data.group(3)
                        self.data_queue.put({
                            "type": "data",
                            "time": now_str,
                            "ch": ch,
                            "freq": freq,
                            "temp": temp,
                            "status": "OK"
                        })
                        continue
                    
                    match_error = pattern_error.search(line)
                    if match_error:
                        ch = int(match_error.group(1))
                        self.data_queue.put({
                            "type": "error",
                            "time": now_str,
                            "ch": ch,
                            "status": "Timeout/Error"
                        })
            except Exception as e:
                # 串口可能被拔出
                pass

    def update_ui_from_queue(self):
        while not self.data_queue.empty():
            data = self.data_queue.get()
            ch = data["ch"]
            now_str = data["time"]
            
            if data["type"] == "data":
                freq = data["freq"]
                temp = data["temp"]
                status = data["status"]
                
                # 更新实时面板
                if ch in self.channel_labels:
                    self.channel_labels[ch].config(
                        text=f"频率: {freq} Hz\n温度: {temp} °C",
                        foreground="black"
                    )
                
                # 存入图表缓存
                self.chart_data_freq[ch].append(float(freq))
                self.chart_data_temp[ch].append(float(temp))
                
                # 记录 CSV
                self.csv_writer.writerow([now_str, ch, freq, temp, status])
                
                # 更新表格
                self.tree.insert("", 0, values=(now_str, ch, freq, temp, status))
                
            elif data["type"] == "error":
                status = data["status"]
                
                # 更新实时面板
                if ch in self.channel_labels:
                    self.channel_labels[ch].config(
                        text="读取超时/错误",
                        foreground="red"
                    )
                
                # 记录 CSV
                self.csv_writer.writerow([now_str, ch, "", "", status])
                
                # 更新表格
                self.tree.insert("", 0, values=(now_str, ch, "-", "-", status))
            
            self.csv_file.flush() # 确保实时写入磁盘
            
            # 限制表格显示行数，防止内存占用过大
            children = self.tree.get_children()
            if len(children) > 1000:
                self.tree.delete(children[-1])

        # 循环调用
        self.root.after(100, self.update_ui_from_queue)

    def update_chart(self, force=False):
        if not self.is_connected and not force:
            self.root.after(1000, self.update_chart)
            return
            
        try:
            ch = int(self.chart_ch_var.get())
        except ValueError:
            ch = 1
            
        self.ax_freq.clear()
        self.ax_temp.clear()
        
        self.ax_freq.plot(list(self.chart_data_freq[ch]), 'b-', label=f'Freq (Hz) - CH{ch}')
        self.ax_freq.legend(loc='upper left')
        self.ax_freq.grid(True)
        self.ax_freq.ticklabel_format(useOffset=False, style='plain', axis='y')
        self.ax_freq.xaxis.set_major_locator(plt.MaxNLocator(integer=True))
        
        self.ax_temp.plot(list(self.chart_data_temp[ch]), 'r-', label=f'Temp (°C) - CH{ch}')
        self.ax_temp.legend(loc='upper left')
        self.ax_temp.grid(True)
        self.ax_temp.ticklabel_format(useOffset=False, style='plain', axis='y')
        self.ax_temp.xaxis.set_major_locator(plt.MaxNLocator(integer=True))
        
        self.fig.tight_layout()
        self.canvas.draw()
        
        if not force:
            self.root.after(1000, self.update_chart)

    def on_closing(self):
        self.is_connected = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        if self.csv_file:
            self.csv_file.close()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    # 尝试使用主题使界面更美观
    style = ttk.Style(root)
    if 'clam' in style.theme_names():
        style.theme_use('clam')
    
    app = HostApp(root)
    root.mainloop()
