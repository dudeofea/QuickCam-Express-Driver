all:
	gcc -Wno-implicit-function-declaration -o qc_cam -std=c99 qc_cam.c `pkg-config --libs --cflags libusb-1.0`