import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports

class BoatControllerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Scheduling Ships Controller")
        self.root.geometry("500x450")
        self.root.configure(bg="#1e1e2e")  # Dark background

        # Modern Style
        self.style = ttk.Style()
        self.style.theme_use('clam')
        
        # Configure styles
        self.style.configure("TFrame", background="#1e1e2e")
        self.style.configure("TLabel", background="#1e1e2e", foreground="#cdd6f4", font=("Helvetica", 11))
        self.style.configure("Header.TLabel", font=("Helvetica", 16, "bold"), foreground="#89b4fa")
        self.style.configure("TRadiobutton", background="#1e1e2e", foreground="#cdd6f4", font=("Helvetica", 10))
        self.style.configure("Send.TButton", font=("Helvetica", 11, "bold"), background="#a6e3a1", foreground="#1e1e2e")
        self.style.configure("Clear.TButton", font=("Helvetica", 11, "bold"), background="#f38ba8", foreground="#1e1e2e")

        # Serial Connection
        self.ser = None
        self.port = "/dev/ttyUSB0"  # Default port
        self.baud = 115200

        self.setup_ui()
        self.init_serial()

    def setup_ui(self):
        # Main Container
        main_frame = ttk.Frame(self.root, padding="30")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Header
        header = ttk.Label(main_frame, text="Ship Scheduler Interface", style="Header.TLabel")
        header.pack(pady=(0, 20))

        # Channel Selection
        channel_frame = ttk.LabelFrame(main_frame, text=" Channel Selection ", padding="15")
        channel_frame.pack(fill=tk.X, pady=10)
        
        self.channel_var = tk.StringVar(value="left")
        rb_left = ttk.Radiobutton(channel_frame, text="Left Channel", variable=self.channel_var, value="left")
        rb_right = ttk.Radiobutton(channel_frame, text="Right Channel", variable=self.channel_var, value="right")
        
        rb_left.pack(side=tk.LEFT, padx=20)
        rb_right.pack(side=tk.LEFT, padx=20)

        # Boat List Input
        input_frame = ttk.Frame(main_frame)
        input_frame.pack(fill=tk.X, pady=20)

        lbl_boats = ttk.Label(input_frame, text="Boats List (S: Standard, F: Fishing, P: Patrol)")
        lbl_boats.pack(anchor=tk.W)

        self.boat_entry = tk.Entry(input_frame, bg="#313244", fg="#cdd6f4", 
                                  insertbackground="white", font=("Courier", 12),
                                  relief=tk.FLAT, borderwidth=10)
        self.boat_entry.pack(fill=tk.X, pady=5)
        self.boat_entry.insert(0, "F,S,P,P,F,S")

        # Buttons
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X, pady=20)

        self.btn_send = ttk.Button(btn_frame, text="SEND ARRAY", command=self.send_data, style="Send.TButton")
        self.btn_send.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(0, 10))

        self.btn_clear = ttk.Button(btn_frame, text="CLEAR (C)", command=self.send_clear, style="Clear.TButton")
        self.btn_clear.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(10, 0))

        # Status Bar
        self.status_var = tk.StringVar(value="Disconnected")
        self.status_lbl = tk.Label(self.root, textvariable=self.status_var, bd=1, relief=tk.SUNKEN, 
                                  anchor=tk.W, bg="#11111b", fg="#fab387", font=("Helvetica", 9))
        self.status_lbl.pack(side=tk.BOTTOM, fill=tk.X)

    def init_serial(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            self.status_var.set(f"Connected to {self.port} at {self.baud} baud")
        except Exception as e:
            self.status_var.set(f"Error: Could not open {self.port}")
            print(f"Serial Error: {e}")

    def send_data(self):
        if not self.ser or not self.ser.is_open:
            messagebox.showerror("Serial Error", "Serial port is not open.")
            return

        raw_input = self.boat_entry.get().upper()
        # Clean input: keep only S, F, P and commas
        cleaned = "".join([c for c in raw_input if c in "SFP,"])
        
        # Format as [F,S,P]
        # First, split by comma and remove empty strings
        parts = [p.strip() for p in cleaned.split(",") if p.strip()]
        formatted_array = "[" + ",".join(parts) + "]"
        
        # Prepend channel info if needed, but the prompt says "send the previous array"
        # I'll send it as: CHANNEL:[F,S,P]
        message = f"{self.channel_var.get().upper()}:{formatted_array}\n"
        
        try:
            self.ser.write(message.encode('utf-8'))
            print(f"Sent: {message.strip()}")
            self.status_var.set(f"Last Sent: {message.strip()}")
        except Exception as e:
            messagebox.showerror("Send Error", f"Failed to send data: {e}")

    def send_clear(self):
        if not self.ser or not self.ser.is_open:
            messagebox.showerror("Serial Error", "Serial port is not open.")
            return

        try:
            message = "C\n"
            self.ser.write(message.encode('utf-8'))
            print("Sent: C")
            self.status_var.set("Last Sent: C")
        except Exception as e:
            messagebox.showerror("Send Error", f"Failed to send clear command: {e}")

if __name__ == "__main__":
    root = tk.Tk()
    app = BoatControllerGUI(root)
    root.mainloop()
