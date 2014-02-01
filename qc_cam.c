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

#define STV_REG23 		0x0423

#define HDCS_ADDR 		0xaa
#define HDCS_IDENT 		0x00

#define PB_ADDR 		0xba
#define PB_IDENT 		0x00

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
void qc_set_reg(libusb_device_handle *handle, int reg, unsigned char val){
	unsigned char data[2];
	data[0] = val & 0xFF;
	data[1] = (val >> 8) & 0xFF;
	libusb_control_transfer(handle, WRITE_REQ, USB_REQ, reg, USB_INDEX, data, 2, USB_TIMEOUT);
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

	//set all values to defaults
	//TODO: get all macros from other driver file
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
