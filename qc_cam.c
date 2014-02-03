/*
	Thanks to http://www.lrr.in.tum.de/~acher/quickcam/ for providing most of the logic
	and macros. And thanks to libusb.com for making USB interfacing simple.
*/
#include <stdio.h>
#include <libusb.h>
#include <stdlib.h>

#define CAM_VENDOR_ID	0x046D
#define CAM_PRODUCT_ID	0x0840

#define STV_I2C_WRITE 	0x400
#define STV_I2C_READ 	0x1410
#define STV_I2C_LENGTH	0x23
#define STV_ISO_ENABLE	0x1440
#define STV_SCAN_RATE	0x1443

#define STV_ISO_SIZE	0x15c1
#define STV_Y_CTRL		0x15c3
#define STV_X_CTRL		0x1680

#define STV_REG00		0x1500
#define STV_REG01		0x1501
#define STV_REG02		0x1502
#define STV_REG03		0x1503
#define STV_REG04		0x1504
#define STV_REG23 		0x0423

#define HDCS_ADDR 		0xaa
#define HDCS_IDENT 		0x00

#define PB_ADDR 		0xba
#define PB_IDENT 		0x00
#define PB_RESET		0x0d
#define PB_EXPGAIN		0x0e
#define PB_VOFFSET		0x39
#define PB_PREADCTRL	0x20
#define PB_ADCGAINH		0x3b
#define PB_ADCGAINL		0x3c
#define PB_RGAIN		0x2d
#define PB_G1GAIN		0x2b
#define PB_G2GAIN		0x2e
#define PB_BGAIN		0x2c
#define PB_RSTART		0x01	// First row
#define PB_CSTART		0x02	// First column
#define PB_RWSIZE		0x03	// Row window size
#define PB_CWSIZE		0x04	// Col window size
#define PB_ROWSPEED		0x0a	// Row speed control
#define PB_CFILLIN		0x05	// Col fill-in
#define PB_VBL			0x06	// Vertical blank count
#define PB_FINTTIME		0x08	// Frame integration time
#define PB_RINTTIME		0x09	// Row frame integration time

#define USB_TIMEOUT		500
#define USB_REQ			0x04
#define USB_INDEX		0x0
#define WRITE_REQ		0x40
#define READ_REQ		0xC0

unsigned char * data_alloc(int len){
	unsigned char *data = (unsigned char*)malloc(len);
	for (int i = 0; i < len; ++i)
	{
		data[i] = 0;
	}
	return data;
}

void print_data(unsigned char *data, int dataLen){
	for (int i = 0; i < dataLen; ++i)
	{
		printf("%02x ", data[i]);
	}
	printf("\n");
}

struct libusb_device *qc_get_device(libusb_context *context, libusb_device **devices){
	
	//Get List of USB devices
	ssize_t cnt = libusb_get_device_list(context, &devices);
	if (cnt < 1)
	{
		printf("No USB Devices Found\n");
	}else{
		printf("Found %d USB devices\n", (int)cnt);
	}
	//Find QuickCam Express Camera
	for (int i = 0; i < cnt; ++i)
	{
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(devices[i], &desc);
		if (desc.idVendor == CAM_VENDOR_ID && desc.idProduct == CAM_PRODUCT_ID)
		{
			printf("Found A QuickCam Express (%d)\n", i);
			return devices[i];
		}
	}
	return NULL;
}

//sets a 2byte register to a value
void qc_set_reg(libusb_device_handle *handle, int reg, int val){
	unsigned char data[2];
	data[0] = val & 0xFF;
	data[1] = (val >> 8) & 0xFF;
	libusb_control_transfer(handle, WRITE_REQ, USB_REQ, reg, USB_INDEX, data, 2, USB_TIMEOUT);
}

//sets a 2byte I2C register to a value
void qc_set_i2c_reg(libusb_device_handle *handle, int reg, int val){
	unsigned char data[STV_I2C_LENGTH];
	for (int i = 0; i < STV_I2C_LENGTH; ++i)
	{
		data[i] = 0;
	}
	data[0x00] = reg;
	data[0x10] = val & 0xFF;
	data[0x11] = (val >> 8) & 0xFF;
	data[0x20] = PB_ADDR;
	data[0x21] = 0;		//1 value
	data[0x22] = 3;		//write cmd
	libusb_control_transfer(handle, WRITE_REQ, USB_REQ, STV_I2C_WRITE, USB_INDEX, data, STV_I2C_LENGTH, USB_TIMEOUT);
}

//get a 2byte value from a register
int qc_read_reg(libusb_device_handle *handle, int reg){
	unsigned char data[2];
	data[0] = 0xAA;
	data[1] = 0xAA;
	libusb_control_transfer(handle, READ_REQ, USB_REQ, reg, USB_INDEX, data, 2, USB_TIMEOUT);
	int val = data[0] + (data[1] << 8);
	return val;
}

void qc_write_data(libusb_device_handle *handle, unsigned char data[], int dataLen){
	libusb_control_transfer(handle, WRITE_REQ, USB_REQ, STV_I2C_WRITE, USB_INDEX, data, dataLen, USB_TIMEOUT);
}

void qc_init(libusb_device_handle *handle){
	int err = 0;
	if (libusb_kernel_driver_active(handle, 0))
	{
		printf("Device busy...detaching...\n"); 
		libusb_detach_kernel_driver(handle,0);
	}else printf("Device free from kernel\n");

	err = libusb_claim_interface(handle, 0);
	if (err){
		printf( "Failed to claim interface. " );
		switch( err ){
		case LIBUSB_ERROR_NOT_FOUND:	printf( "not found\n" );	break;
		case LIBUSB_ERROR_BUSY:			printf( "busy\n" );		break;
		case LIBUSB_ERROR_NO_DEVICE:	printf( "no device\n" );	break;
		default:						printf( "other\n" );		break;
		}
	}
	//Set I2C Registers to 16-bit
	int nEndpoint = 0x0;
	int requestType = 0x40;
	int request = 0x04;
	int val = STV_REG23;
	int index = 0;
	int nTimeout = 500;	//in milliseconds
	int dataLen = 0x01;
	int BytesWritten = 0;
	unsigned char *data = data_alloc(dataLen);
	data[0] = 1;

	qc_set_reg(handle, STV_REG23, 1);

	//Request ID (i think)
	dataLen = 0x23;
	data = data_alloc(dataLen);
	data[0x00] = PB_IDENT;
	data[0x20] = PB_ADDR;
	data[0x21]=0;		// 1 value
	data[0x22]=3;		// Write cmd

	qc_write_data(handle, data, dataLen);

	//Get sensor ID
	int chip_type = qc_read_reg(handle, STV_I2C_READ);
	if (chip_type==0x6403)
	{
		printf("This Quickcam has a Photobit PB100 sensor\n");
	}else{
		printf("Not sure what camera this is...:(\n");
	}

	int mode = 0;

	//set defaults
	qc_set_reg(handle, STV_REG00, 1);
	qc_set_reg(handle, STV_SCAN_RATE, 0);
	//reset sensor
	qc_set_i2c_reg(handle, PB_RESET, 1);
	qc_set_i2c_reg(handle, PB_RESET, 0);
	//enable auto-exposure
	qc_set_i2c_reg(handle, PB_EXPGAIN, 17);
	//set other gain values
	qc_set_i2c_reg(handle, PB_PREADCTRL, 0x1444);
	qc_set_i2c_reg(handle, PB_VOFFSET, 0x14);
	qc_set_i2c_reg(handle, PB_ADCGAINH, 0xd);
	qc_set_i2c_reg(handle, PB_ADCGAINL, 0x1);
	//set individual colour gains
	qc_set_i2c_reg(handle, PB_RGAIN, 0xc0);
	qc_set_i2c_reg(handle, PB_G1GAIN, 0xc0);
	qc_set_i2c_reg(handle, PB_G2GAIN, 0xc0);
	qc_set_i2c_reg(handle, PB_BGAIN, 0xc0);
	//???
	qc_set_reg(handle, STV_REG04, 0x07);
	qc_set_reg(handle, STV_REG03, 0x45);
	qc_set_reg(handle, STV_REG00, 0x11);
	//set screen output size
	qc_set_reg(handle, STV_Y_CTRL, (mode?2:1));		// 1: Y-full, 2: y-half
	qc_set_reg(handle, STV_X_CTRL, (mode?6:0x0a));	// 06/0a : Half/Full
	qc_set_reg(handle, STV_ISO_SIZE, 0x27b);		// ISO-Size 
	//setup sensor window
	qc_set_i2c_reg(handle, PB_RSTART, 0);
	qc_set_i2c_reg(handle, PB_CSTART, 0);
	qc_set_i2c_reg(handle, PB_RWSIZE, 0xf7);
	qc_set_i2c_reg(handle, PB_CWSIZE, 0x13f);
	//scan rate
	qc_set_reg(handle, STV_SCAN_RATE, (mode?0x10:0x20));	// larger -> slower
	//scan/timing for the sensor
	qc_set_i2c_reg(handle, PB_ROWSPEED, 0x1a);
	qc_set_i2c_reg(handle, PB_CFILLIN, 0x2f);
	qc_set_i2c_reg(handle, PB_VBL, 0);
	qc_set_i2c_reg(handle, PB_FINTTIME, 0);
	qc_set_i2c_reg(handle, PB_RINTTIME, 0x7b);
	//???
	qc_set_reg(handle, STV_REG01, 0xc2);
	qc_set_reg(handle, STV_REG02, 0xb0);

	//enable data stream
	qc_set_reg(handle, STV_ISO_ENABLE, 1);

	//TODO: get isochronous data from endpoint 0x81
}

int main(int argc, const char* argv[])
{
	struct libusb_device **devices;
	struct libusb_context *context;
	libusb_init(&context);
	struct libusb_device *qc_cam = qc_get_device(context, devices);
	if (qc_cam)
	{
		libusb_device_handle *handle;
		int err = 0;
		err = libusb_open(qc_cam, &handle);
		if (err)
			printf("Unable to open device, err %d\n", err);
		qc_init(handle);
	}
	//int var = (('q'<<8) | 50);
	//printf("0x%X\n", var);
	libusb_free_device_list(devices, 1);
	libusb_exit(context);
	return 0;
}
