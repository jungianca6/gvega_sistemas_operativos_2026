# Instalar ESP-IDF en Linux
1. sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
2. Cmake minimo 3.22
3. Python instalado en linux
4. mkdir -p ~/esp
5. cd ~/esp
6. git clone --recursive https://github.com/espressif/esp-idf.git
7. cd esp-idf
8. ./install.sh esp32c6 (aca revisas tambien el esp32 suyo. Ej: esp32d6)
9. . ./export.sh
10. Luego hacés cd al proyecto (ej: cd gvega_sistemas_operativos_2026/Proyecto1/SchedulingShips_IDF) y hacés uso del makefile

11. Si lo haces en otra carpeta, estos son los comandos
12. idf.py set-target esp32c6 (igual, esto cambia según su ESP)
13. idf.py build (Cada vez que haces cambios en codigo, tenes que poner esto en terminal)
14. idf.py flash (Acá tiene que estar conectado el ESP)
15. idf.py monitor (Serial monitor básicamente)
