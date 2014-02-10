/*
	Thanks to http://www.lrr.in.tum.de/~acher/quickcam/ for providing most of the logic
	and STV interfacing. And thanks to libusb.com for making USB interfacing simple. Thanks
	to https://chromium.googlesource.com/chromiumos/third_party/kernel-staging/+/25985edcedea6396277003854657b5f3cb31a628/drivers/media/video/gspca/stv06xx
	for a full list of all macros.
*/
#include <stdio.h>
#include <stdlib.h>
#include <libusb.h>
#include <unistd.h>

#define CAM_VENDOR_ID	0x046D
#define CAM_PRODUCT_ID	0x0840
#define CAM_INTERFACE 	0x0
#define CAM_ALTERNATE	0x1

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
#define STV_REG23 		0x1423

#define PB_ADDR			0xba
/* I2C Registers */
#define PB_IDENT			0x00	/* Chip Version */
#define PB_RSTART			0x01	/* Row Window Start */
#define PB_CSTART			0x02	/* Column Window Start */
#define PB_RWSIZE			0x03	/* Row Window Size */
#define PB_CWSIZE			0x04	/* Column  Window Size */
#define PB_CFILLIN			0x05	/* Column Fill-In */
#define PB_VBL				0x06	/* Vertical Blank Count */
#define PB_CONTROL			0x07	/* Control Mode */
#define PB_FINTTIME			0x08	/* Integration Time/Frame Unit Count */
#define PB_RINTTIME			0x09	/* Integration Time/Row Unit Count */
#define PB_ROWSPEED			0x0a	/* Row Speed Control */
#define PB_ABORTFRAME		0x0b	/* Abort Frame */
#define PB_R12				0x0c	/* Reserved */
#define PB_RESET			0x0d	/* Reset */
#define PB_EXPGAIN			0x0e	/* Exposure Gain Command */
#define PB_R15				0x0f	/* Expose0 */
#define PB_R16				0x10	/* Expose1 */
#define PB_R17				0x11	/* Expose2 */
#define PB_R18				0x12	/* Low0_DAC */
#define PB_R19				0x13	/* Low1_DAC */
#define PB_R20				0x14	/* Low2_DAC */
#define PB_R21				0x15	/* Threshold11 */
#define PB_R22				0x16	/* Threshold0x */
#define PB_UPDATEIN			0x17	/* Update Interval */
#define PB_R24				0x18	/* High_DAC */
#define PB_R25				0x19	/* Trans0H */
#define PB_R26				0x1a	/* Trans1L */
#define PB_R27				0x1b	/* Trans1H */
#define PB_R28				0x1c	/* Trans2L */
#define PB_R29				0x1d	/* Reserved */
#define PB_R30				0x1e	/* Reserved */
#define PB_R31				0x1f	/* Wait to Read */
#define PB_PREADCTRL		0x20	/* Pixel Read Control Mode */
#define PB_R33				0x21	/* IREF_VLN */
#define PB_R34				0x22	/* IREF_VLP */
#define PB_R35				0x23	/* IREF_VLN_INTEG */
#define PB_R36				0x24	/* IREF_MASTER */
#define PB_R37				0x25	/* IDACP */
#define PB_R38				0x26	/* IDACN */
#define PB_R39				0x27	/* DAC_Control_Reg */
#define PB_R40				0x28	/* VCL */
#define PB_R41				0x29	/* IREF_VLN_ADCIN */
#define PB_R42				0x2a	/* Reserved */
#define PB_G1GAIN			0x2b	/* Green 1 Gain */
#define PB_BGAIN			0x2c	/* Blue Gain */
#define PB_RGAIN			0x2d	/* Red Gain */
#define PB_G2GAIN			0x2e	/* Green 2 Gain */
#define PB_R47				0x2f	/* Dark Row Address */
#define PB_R48				0x30	/* Dark Row Options */
#define PB_R49				0x31	/* Reserved */
#define PB_R50				0x32	/* Image Test Data */
#define PB_ADCMAXGAIN		0x33	/* Maximum Gain */
#define PB_ADCMINGAIN		0x34	/* Minimum Gain */
#define PB_ADCGLOBALGAIN	0x35	/* Global Gain */
#define PB_R54				0x36	/* Maximum Frame */
#define PB_R55				0x37	/* Minimum Frame */
#define PB_R56				0x38	/* Reserved */
#define PB_VOFFSET			0x39	/* VOFFSET */
#define PB_R58				0x3a	/* Snap-Shot Sequence Trigger */
#define PB_ADCGAINH			0x3b	/* VREF_HI */
#define PB_ADCGAINL			0x3c	/* VREF_LO */
#define PB_R61				0x3d	/* Reserved */
#define PB_R62				0x3e	/* Reserved */
#define PB_R63				0x3f	/* Reserved */
#define PB_R64				0x40	/* Red/Blue Gain */
#define PB_R65				0x41	/* Green 2/Green 1 Gain */
#define PB_R66				0x42	/* VREF_HI/LO */
#define PB_R67				0x43	/* Integration Time/Row Unit Count */
#define PB_R240				0xf0	/* ADC Test */
#define PB_R241				0xf1	/* Chip Enable */
#define PB_R242				0xf2	/* Reserved */

#define USB_HOST_TO_DEVICE	0x0
#define USB_INTERFACE_REQ 	0x1
#define USB_SET_INTERFACE	0xb
#define USB_TIMEOUT			500
#define USB_REQ				0x04
#define USB_INDEX			0x0
#define WRITE_REQ			0x40
#define READ_REQ			0xC0

//allocate some data
unsigned char * data_alloc(int len){
	unsigned char *data = (unsigned char*)malloc(len);
	for (int i = 0; i < len; ++i)
	{
		data[i] = 0;
	}
	return data;
}

//print data values in hex
void print_data(unsigned char *data, int dataLen){
	for (int i = 0; i < dataLen; ++i)
	{
		printf("%02x ", data[i]);
	}
	printf("\n");
}

//find the first quckcam express camera attached to usb
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

//read a 1byte register
unsigned char qc_read_reg1(libusb_device_handle *handle, int reg){
	int dataLen = 1;
	unsigned char *data = data_alloc(dataLen);
	data[0] = 0xAA;
	int bytes = libusb_control_transfer(handle, READ_REQ, USB_REQ, reg, USB_INDEX, data, 1, USB_TIMEOUT);
	if(bytes < 0)
		printf("err: %s\n", libusb_error_name(bytes));
	return data[0];
}

//read a 2byte register
int qc_read_reg2(libusb_device_handle *handle, int reg){
	int dataLen = 2;
	unsigned char *data = data_alloc(dataLen);
	data[0] = 0xAA;
	data[1] = 0xAA;
	int bytes = libusb_control_transfer(handle, READ_REQ, USB_REQ, reg, USB_INDEX, data, 2, USB_TIMEOUT);
	if(bytes < 0)
		printf("err: %s\n", libusb_error_name(bytes));
	int val = data[0] + (data[1] << 8);
	return val;
}

//sets a 1byte register to a value
void qc_set_reg1(libusb_device_handle *handle, int reg, unsigned char val){
	unsigned char data[1];
	data[0] = val;
	int bytes = libusb_control_transfer(handle, WRITE_REQ, USB_REQ, reg, USB_INDEX, data, 1, USB_TIMEOUT);
	if(bytes < 0)
		printf("err: %s\n", libusb_error_name(bytes));
	/*unsigned char a = qc_read_reg1(handle, reg);
	printf("--STV1-\n");
	printf("reg: 0x%02x\n", reg);
	printf("write: 0x%02x\n", val);
	printf("value: 0x%02x\n", a);*/
}

//sets a 2byte register to a value
void qc_set_reg2(libusb_device_handle *handle, int reg, int val){
	unsigned char data[2];
	data[0] = val & 0xFF;
	data[1] = (val >> 8) & 0xFF;
	int bytes = libusb_control_transfer(handle, WRITE_REQ, USB_REQ, reg, USB_INDEX, data, 2, USB_TIMEOUT);
	if(bytes < 0)
		printf("err: %s\n", libusb_error_name(bytes));
	/*int a = qc_read_reg2(handle, reg);
	printf("--STV2-\n");
	printf("reg: 0x%02x\n", reg);
	printf("write: 0x%02x\n", val);
	printf("value: 0x%02x\n", a);*/
}

//write a data buffer to the STV
int qc_write_data(libusb_device_handle *handle, unsigned char data[], int dataLen){
	int bytes = libusb_control_transfer(handle, WRITE_REQ, USB_REQ, STV_I2C_WRITE, USB_INDEX, data, dataLen, USB_TIMEOUT);
	usleep(100);
	return bytes;
}

//get a 2byte value from an I2C register
int qc_read_i2c_reg(libusb_device_handle *handle, int reg){
	//get the STV to push the register onto it's bus via I2C
	int dataLen = 0x23;
	unsigned char *data = data_alloc(dataLen);
	data[0x00] = reg;
	data[0x20] = PB_ADDR;
	data[0x21]=0;		//1 value
	data[0x22]=3;		//read cmd

	qc_write_data(handle, data, dataLen);
	//read the register value
	return qc_read_reg2(handle, STV_I2C_READ);
}

//sets a 2byte I2C register to a value
void qc_set_i2c_reg(libusb_device_handle *handle, int reg, int val){
	unsigned char *data = data_alloc(STV_I2C_LENGTH);
	data[0x00] = reg;
	data[0x10] = val & 0xFF;
	data[0x11] = (val >> 8) & 0xFF;
	data[0x20] = PB_ADDR;
	data[0x21] = 0;		//1 value
	data[0x22] = 1;		//write cmd
	int bytes = qc_write_data(handle, data, STV_I2C_LENGTH);
	if(bytes < 0)
		printf("err: %s\n", libusb_error_name(bytes));
	/*int a = qc_read_i2c_reg(handle, reg);
	printf("--I2C--\n");
	printf("reg: 0x%02x\n", reg);
	printf("write: 0x%02x\n", val);
	printf("value: 0x%02x\n", a);*/
}

//callback function for iso transfer
void cb(struct libusb_transfer *transfer)
{
	printf("hey!\n");
	int *completed = transfer->user_data;
	*completed = 1;
}

// debugging function to display libusb_transfer
void print_libusb_transfer(struct libusb_transfer *p_t)
{	int i;
	if ( NULL == p_t){
		printf("No libusb_transfer...\n");
	}
	else {
		printf("libusb_transfer structure:\n");
		printf("flags   =%x \n", p_t->flags);	
		printf("endpoint=%x \n", p_t->endpoint); 
		printf("type    =%x \n", p_t->type);
		printf("timeout =%d \n", p_t->timeout);
		// length, and buffer are commands sent to scanner
		printf("length        =%d \n", p_t->length);
		printf("actual_length =%d \n", p_t->actual_length);
		printf("buffer    =%p \n", p_t->buffer);

		for (i=0; i < p_t->length; i++){ 
			//printf("buffer[%d] =%x \n", i, p_t->buffer[i]);
		}
	}	
	return;
}

void active_config(struct libusb_device *dev,struct libusb_device_handle *handle)    
{    
    struct libusb_device_handle *hDevice_req;    
    struct libusb_config_descriptor *config;    
    struct libusb_endpoint_descriptor *endpoint;    
    int altsetting_index,interface_index=0,ret_active;    
    int i,ret_print;    

    hDevice_req = handle;    

    ret_active = libusb_get_active_config_descriptor(dev,&config);    
    //ret_print = print_configuration(hDevice_req,config);
    struct libusb_endpoint_desriptor *ep;

    for(interface_index=0;interface_index<config->bNumInterfaces;interface_index++)    
    {    
        const struct libusb_interface *iface = &config->interface[interface_index];
        for(int altsetting_index=0;altsetting_index<iface->num_altsetting;altsetting_index++)    
        {    
            const struct libusb_interface_descriptor *altsetting = &iface->altsetting[altsetting_index];    
            printf("Interface #: %d\n", altsetting->bInterfaceNumber);
            printf("Alternate Setting: %d\n", altsetting->bAlternateSetting);
            for(int endpoint_index=0;endpoint_index<altsetting->bNumEndpoints;endpoint_index++)
            {    
				printf("EndPoint Descriptors:");    
				printf("\n\tSize of EndPoint Descriptor : %d", (int)altsetting->endpoint[endpoint_index].bLength);    
				printf("\n\tType of Descriptor : %d", (int)altsetting->endpoint[endpoint_index].bDescriptorType);    
				printf("\n\tEndpoint Address : 0x0%x", (int)altsetting->endpoint[endpoint_index].bEndpointAddress);    
				printf("\n\tMaximum Packet Size: %x", (int)altsetting->endpoint[endpoint_index].wMaxPacketSize);    
				printf("\n\tAttributes applied to Endpoint: %d", (int)altsetting->endpoint[endpoint_index].bmAttributes);    
				printf("\n\tInterval for Polling for data Tranfer : %d\n", (int)altsetting->endpoint[endpoint_index].bInterval);   
            }    
        }    
    }    
    libusb_free_config_descriptor(NULL); 
}

//init quickcam express camera
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
	err = libusb_set_interface_alt_setting(handle, 0, 1); //set alt setting for second interface
	printf("%s\n", libusb_error_name(err));
	//Set I2C Registers to 16-bit
	qc_set_reg1(handle, STV_REG23, 1);

	//Get sensor ID
	int chip_type = qc_read_i2c_reg(handle, PB_IDENT);
	if (chip_type==0x6403)
	{
		printf("This Quickcam has a Photobit PB100 sensor\n");
	}else{
		printf("Not sure what camera this is...:(\n");
	}

	int mode = 0;

	//set defaults
	qc_set_reg1(handle, STV_REG00, 1);
	qc_set_reg1(handle, STV_SCAN_RATE, 0);
	//reset sensor
	qc_set_i2c_reg(handle, PB_RESET, 1);
	qc_set_i2c_reg(handle, PB_RESET, 0);
	//disable chip
	qc_set_i2c_reg(handle, PB_CONTROL, 0x28);
	//set VREF_LO to get a good black level
	qc_set_i2c_reg(handle, PB_ADCGAINL, 0x3);
	//enable global gain changes
	qc_set_i2c_reg(handle, PB_PREADCTRL, 0x1400);
	//set minimum gain
	qc_set_i2c_reg(handle, PB_ADCMINGAIN, 0x10);
	//set global gain
	qc_set_i2c_reg(handle, PB_ADCGLOBALGAIN, 0x10);
	//allow auto exposure to change the gain
	qc_set_i2c_reg(handle, PB_EXPGAIN, 0x11);
	//???
	qc_set_reg1(handle, STV_REG00, 0x11);
	qc_set_reg1(handle, STV_REG03, 0x45);
	qc_set_reg1(handle, STV_REG04, 0x07);
	//set ISO size
	qc_set_reg2(handle, STV_ISO_SIZE, 0x034f);   //0x027b or 0x034F or 0x039b

	//set to 2/3rds speed
	qc_set_i2c_reg(handle, PB_ROWSPEED, 0x1A);
	//set sensor window
	qc_set_i2c_reg(handle, PB_RSTART, 0x8);
	qc_set_i2c_reg(handle, PB_CSTART, 0x4);
	qc_set_i2c_reg(handle, PB_RWSIZE, 0x11F);
	qc_set_i2c_reg(handle, PB_CWSIZE, 0x15F);

	//set screen output size to full
	qc_set_reg1(handle, STV_Y_CTRL, 0x1);
	qc_set_reg1(handle, STV_X_CTRL, 0xA);
	qc_set_reg1(handle, STV_SCAN_RATE, 0x20);
	//???
	qc_set_reg1(handle, STV_REG01, 0xC2);
	qc_set_reg1(handle, STV_REG02, 0xB0);

	//enable chip
	qc_set_i2c_reg(handle, PB_CONTROL, 0x2A);
	//enable data stream
	qc_set_reg1(handle, STV_ISO_ENABLE, 1);
	qc_set_i2c_reg(handle, PB_EXPGAIN, 0x51);
	qc_set_i2c_reg(handle, 0x15, 0x6);
	qc_set_i2c_reg(handle, 0x16, 0x642);

	int a = qc_read_i2c_reg(handle, PB_CONTROL);
	printf("reg: 0x%02x\n", a);
	a = qc_read_i2c_reg(handle, PB_RESET);
	printf("reg: 0x%02x\n", a);

	printf("QuickCam registers set to default values\n");
	//set interface


	//TODO: get isochronous data from endpoint 0x81
	int nEndpoint = 0x081;
	int requestType = READ_REQ;
	int request = 0x04;
	int val = STV_REG23;
	int index = 0;
	int nTimeout = 500;	//in milliseconds
	int BytesWritten = 0;
	int dataLen = 1024;
	int nPackets = 32;
	unsigned char *data = data_alloc(dataLen);
	static int completed = 0;

	struct libusb_transfer *tx = libusb_alloc_transfer(nPackets);
	//libusb_fill_control_setup(data, requestType, request, 1, 0, 0);
	libusb_fill_iso_transfer(tx, handle, nEndpoint, data, dataLen, nPackets, cb, &completed, nTimeout);
	print_libusb_transfer(tx);
	int r = libusb_submit_transfer(tx);
	print_libusb_transfer(tx);
	while(!completed){
		printf("status: %s\n",libusb_error_name(tx->iso_packet_desc->status));
		usleep(1000000);
	}
	printf("%s\n", libusb_error_name(r));
	libusb_free_transfer(tx);
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
		active_config(qc_cam, handle);
		qc_init(handle);
	}
	//int var = (('q'<<8) | 50);
	//printf("0x%X\n", var);
	libusb_free_device_list(devices, 1);
	libusb_exit(context);
	return 0;
}
