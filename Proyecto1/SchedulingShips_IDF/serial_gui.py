import tkinter as tk
import json
import serial
# =========================================================
# CONFIGURACION GENERAL GUI
# =========================================================

WINDOW_WIDTH = 1400
WINDOW_HEIGHT = 800

# Coordenadas del canal
CHANNEL_X1 = 350
CHANNEL_X2 = 1050
CHANNEL_Y = 400

# Canal discreto
CHANNEL_CELLS = 12
CELL_SIZE = 55

# Tamaño visual barcos
SHIP_WIDTH = 40
SHIP_HEIGHT = 20

# =========================================================
# CONFIG SERIAL UART
# =========================================================

SERIAL_PORT = "/dev/ttyUSB0"

BAUD_RATE = 115200  #configurar segun dispositivo

SERIAL_TIMEOUT = 0.01

serial_connection = None

# =========================================================
# ESTADO GLOBAL DEL SISTEMA
# =========================================================
#
# IMPORTANTE:
# ---------------------------------------------------------
# Este diccionario eventualmente sera actualizado por:
#
# - UART
# - Serial
# - Socket
# - Pipe
# - JSON
#
# proveniente del ESP32/FreeRTOS.
#
# LA GUI NO DEBE IMPLEMENTAR LOGICA DE SCHEDULING.
# SOLO DEBE REPRESENTAR VISUALMENTE EL ESTADO.
#
# =========================================================

system_state = {

    # ---------------------------------------------
    # CONFIGURACION GENERAL
    # ---------------------------------------------

    "scheduler": "RR",

    "flow_control": "FAIRNESS",

    "config": {

        "Channel Length": 100,
        "Standard Speed": 1,
        "Fishing Speed": 2,
        "Patrol Speed": 3,
        "Quantum RR": 4,
        "Parameter W": 2,
        "Sign Duration": 5
    },

    # ---------------------------------------------
    # COLAS READY
    # ---------------------------------------------

    "left_queue": [

        {
            "id": 1,
            "type": "STANDARD",
            "state": "READY"
        },

        {
            "id": 2,
            "type": "FISHING",
            "state": "READY"
        }
    ],

    "right_queue": [

        {
            "id": 3,
            "type": "PATROL",
            "state": "READY"
        }
    ],

    # ---------------------------------------------
    # BARCOS DENTRO DEL CANAL
    # ---------------------------------------------
    #
    # position:
    # celda discreta del canal
    #
    # direction:
    # LEFT / RIGHT
    #
    # state:
    # RUNNING / BLOCKED / etc
    #
    # IMPORTANTE:
    # NO pueden existir 2 barcos
    # con la misma position.
    #
    # El backend debe garantizar eso.
    #
    # ---------------------------------------------

    "channel_ships": [

        {
            "id": 10,
            "type": "FISHING",
            "direction": "RIGHT",
            "position": 4,
            "state": "RUNNING"
        }
    ]
}

# =========================================================
# UTILIDADES
# =========================================================

def ship_color(ship_type):

    if ship_type == "STANDARD":
        return "white"

    elif ship_type == "FISHING":
        return "orange"

    elif ship_type == "PATROL":
        return "red"

    return "gray"

# =========================================================
# CONECTAR UART
# =========================================================
#
# IMPORTANTE:
# ---------------------------------------------------------
# Esta funcion abre el puerto serial conectado
# al ESP32.
#
# El ESP32 enviara snapshots JSON del sistema.
#
# =========================================================

def connect_serial():

    global serial_connection

    try:

        serial_connection = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUD_RATE,
            timeout=SERIAL_TIMEOUT
        )

        print(f"Connected to {SERIAL_PORT}")

    except Exception as e:

        print(f"Serial connection error: {e}")

        serial_connection = None

# =========================================================
# LEER DATOS SERIAL
# =========================================================
#
# IMPORTANTE:
# ---------------------------------------------------------
# El ESP32 enviara:
#
# JSON + '\n'
#
# Ejemplo:
#
# {
#   "channel_ships":[...]
# }
#
# Cada linea serial representa
# un snapshot completo del sistema.
#
# =========================================================

def receive_serial_data():

    global serial_connection
    global system_state

    if serial_connection is None:

        return

    try:

        # ---------------------------------------------
        # Leer linea completa
        # ---------------------------------------------

        if serial_connection.in_waiting > 0:

            line = serial_connection.readline()

            json_string = line.decode("utf-8").strip()

            # Evitar lineas vacias

            if json_string == "":

                return

            # -----------------------------------------
            # Parse JSON
            # -----------------------------------------

            new_state = json.loads(json_string)

            # -----------------------------------------
            # Actualizar estado global
            # -----------------------------------------

            system_state.update(new_state)

    except Exception as e:

        print(f"Serial read error: {e}")

# =========================================================
# GUI PRINCIPAL
# =========================================================

class SchedulingShipsGUI:

    def __init__(self, root):

        self.root = root

        self.root.title("Scheduling Ships")

        self.root.geometry(
            f"{WINDOW_WIDTH}x{WINDOW_HEIGHT}"
        )

        self.root.configure(bg="#87CEEB")

        # =================================================
        # CANVAS PRINCIPAL
        # =================================================

        self.canvas = tk.Canvas(
            root,
            width=WINDOW_WIDTH,
            height=WINDOW_HEIGHT,
            bg="#87CEEB",
            highlightthickness=0
        )

        self.canvas.pack(fill="both", expand=True)

        # =================================================
        # TECLA SALIDA
        # =================================================

        self.root.bind("w", self.close_program)

        # =================================================
        # RENDER INICIAL
        # =================================================

        self.render()

        # =================================================
        # LOOP GUI
        # =================================================

        self.update_loop()

    # =====================================================
    # RENDER GENERAL
    # =====================================================
    #
    # IMPORTANTE:
    # -----------------------------------------------------
    # Toda la GUI se reconstruye desde system_state.
    #
    # El backend solo debe modificar system_state.
    #
    # =====================================================

    def render(self):

        self.canvas.delete("all")

        self.draw_title()

        self.draw_info_panel()

        self.draw_channel()

        self.draw_queues()

        self.draw_channel_ships()

    # =====================================================
    # TITULO
    # =====================================================

    def draw_title(self):

        self.canvas.create_text(
            WINDOW_WIDTH // 2,
            30,
            text="SCHEDULING SHIPS",
            font=("Arial", 24, "bold")
        )

    # =====================================================
    # PANEL INFORMACION
    # =====================================================

    def draw_info_panel(self):

        x = 20
        y = 20

        self.canvas.create_rectangle(
            x,
            y,
            x + 280,
            y + 300,
            fill="white"
        )

        self.canvas.create_text(
            x + 140,
            y + 20,
            text="SYSTEM INFO",
            font=("Arial", 14, "bold")
        )

        info_y = y + 60

        self.canvas.create_text(
            x + 10,
            info_y,
            anchor="w",
            text=f"Scheduler: {system_state['scheduler']}"
        )

        info_y += 25

        self.canvas.create_text(
            x + 10,
            info_y,
            anchor="w",
            text=f"Flow: {system_state['flow_control']}"
        )

        info_y += 40

        for key, value in system_state["config"].items():

            self.canvas.create_text(
                x + 10,
                info_y,
                anchor="w",
                text=f"{key}: {value}"
            )

            info_y += 20

    # =====================================================
    # CANAL
    # =====================================================

    def draw_channel(self):

        self.canvas.create_rectangle(
            CHANNEL_X1,
            CHANNEL_Y - 50,
            CHANNEL_X2,
            CHANNEL_Y + 50,
            fill="#666666"
        )

        # Dibujar celdas discretas

        for i in range(CHANNEL_CELLS):

            x = CHANNEL_X1 + i * CELL_SIZE

            self.canvas.create_line(
                x,
                CHANNEL_Y - 50,
                x,
                CHANNEL_Y + 50
            )

    # =====================================================
    # COLAS
    # =====================================================

    def draw_queues(self):

        # LEFT QUEUE

        x = 40

        for ship in system_state["left_queue"]:

            self.draw_ship(
                x,
                CHANNEL_Y,
                ship
            )

            x += 60

        # RIGHT QUEUE

        x = 1300

        for ship in system_state["right_queue"]:

            self.draw_ship(
                x,
                CHANNEL_Y,
                ship
            )

            x -= 60

    # =====================================================
    # BARCOS EN CANAL
    # =====================================================

    def draw_channel_ships(self):

        occupied_positions = set()

        for ship in system_state["channel_ships"]:

            position = ship["position"]

            # -----------------------------------------
            # PROTECCION ANTI COLISIONES
            # -----------------------------------------

            if position in occupied_positions:

                print(
                    f"ERROR: collision detected at "
                    f"position {position}"
                )

                continue

            occupied_positions.add(position)

            # -----------------------------------------
            # CONVERSION LOGICA -> VISUAL
            # -----------------------------------------

            x = CHANNEL_X1 + position * CELL_SIZE

            self.draw_ship(
                x,
                CHANNEL_Y,
                ship
            )

    # =====================================================
    # DIBUJAR BARCO
    # =====================================================

    def draw_ship(self, x, y, ship):

        color = ship_color(ship["type"])

        self.canvas.create_rectangle(
            x,
            y,
            x + SHIP_WIDTH,
            y + SHIP_HEIGHT,
            fill=color,
            outline="black"
        )

        self.canvas.create_text(
            x + 20,
            y + 10,
            text=str(ship["id"]),
            font=("Arial", 8, "bold")
        )

    # =====================================================
    # LOOP PRINCIPAL GUI
    # =====================================================
    #
    # IMPORTANTE:
    # -----------------------------------------------------
    # Aqui eventualmente se llamara:
    #
    # - read_serial()
    # - receive_socket()
    # - parse_json()
    #
    # para actualizar system_state.
    #
    # =====================================================

    def update_loop(self):

        #Leer estado desde ESP32
        receive_serial_data()

        #Redibujar
        self.render()

        self.root.after(50, self.update_loop)

    # =====================================================
    # SALIDA LIMPIA
    # =====================================================

    def close_program(self, event=None):
        global serial_connection
        print("Closing GUI...")

        self.root.destroy()
        if serial_connection is not None:
            serial_connection.close()
        self.root.destroy()

# =========================================================
# MAIN
# =========================================================

def main():

    connect_serial()

    root = tk.Tk()

    app = SchedulingShipsGUI(root)

    root.mainloop()

if __name__ == "__main__":
    main()