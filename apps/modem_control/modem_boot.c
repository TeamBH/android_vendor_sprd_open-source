#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utils/Log.h>
#include <cutils/sockets.h>
#include <time.h>
#include "packet.h"

enum {
	MODEM_SOFT_RESET = 0x1,
	MODEM_HARD_RESET,
	MODEM_MAX
};

struct image_info {
	char *image_path;
	char *image_path_bak;
	unsigned int image_size;
	unsigned int address;
	unsigned int mask;
};

typedef struct  _NV_HEADER {
	unsigned int magic;
	unsigned int len;
	unsigned int checksum;
	unsigned int version;
}nv_header_t;
#define NV_HEAD_MAGIC   0x00004e56

//#define __TEST_SPI_ONLY__
#define	MODEM_POWER_PATH	"/sys/devices/platform/modem_interface/modempower"
#define	MODEM_RESET_PATH	         "/sys/devices/platform/modem_interface/modemreset"
#define	MODEM_STATE_PATH	        "/sys/devices/platform/modem_interface/state"

#define	DLOADER_PATH		"/dev/dloader"
#define	UART_DEVICE_NAME	"/dev/ttyS2"

#define  TD_MODEM_ALIVE_STR          "_TD Modem _Alive"
#define  LTE_MODEM_ALIVE_STR         "LTE Modem _Alive"
#define  LTE_MODEM_ASSERT_STR        "LTE Modem Assert"
#define  TD_MODEM_ASSERT_STR         "_TD Modem Assert"
#define  LTE_MODEM_RESET_STR         "LTE Modem _Reset"
#define  WTD_MODEM_RESET_STR         "WDG Modem _Reset"

#define FDL_OFFSET		0
#define	FDL_PACKET_SIZE 	256
#define HS_PACKET_SIZE		(32*1024)

#define FDL_CP_PWRON_DLY	(160*1000)//us
#define FDL_CP_UART_TIMEOUT	(3000)//(200) //ms
#define FDL_CP_UART_REC_DLY	(5*1000) //us

#define	DL_FAILURE		(-1)
#define DL_SUCCESS		(0)

char test_buffer[HS_PACKET_SIZE+128]={0};

static int modem_images_count=11;
static char *uart_dev = UART_DEVICE_NAME;
static int fdl_cp_poweron_delay = FDL_CP_PWRON_DLY;
static int modem_power_status = 0;
static pthread_t t1;
static int capabilities_flag = 0xff;

extern void set_modem_assert_information(char *assert_info,int size);
extern void broadcast_modem_state(char *message,int size);

struct image_info download_image_info[] = {
	{ //fdl
		"/dev/block/platform/sprd-sdhci.3/by-name/fdl",
		"/dev/block/platform/sprd-sdhci.3/by-name/fdl",
		0x3900,
		0x20000000,
		0xff,
	},
	{ //PARM DSP
		"/dev/block/platform/sprd-sdhci.3/by-name/tddsp",
		"/dev/block/platform/sprd-sdhci.3/by-name/tddsp",
		0x3E0000,
		0x80020000,
		0x01,
	},
	{ //PARM
		"/dev/block/platform/sprd-sdhci.3/by-name/tdmodem",
		"/dev/block/platform/sprd-sdhci.3/by-name/tdmodem",
		0x7c0000,
		0x80400000,
		0x01,
	},
	{ //	PARM fixvn
		"/dev/block/platform/sprd-sdhci.3/by-name/tdfixnv1",
		"/dev/block/platform/sprd-sdhci.3/by-name/tdfixnv1",
		0x20000,
		0x80A10000,
		0x01,
       },
       { //	PARM runtimevn
		"/dev/block/platform/sprd-sdhci.3/by-name/tdruntimenv1",
		"/dev/block/platform/sprd-sdhci.3/by-name/tdruntimenv1",
		0x40000,
		0x80A30000,
		0x01,
       },
       { //	cmdline
		"/proc/cmdline",
		"/proc/cmdline",
		0x10000,
		0x80A70000,
		0x01,
       },
       /*{ //ca5 DSP
		"/dev/block/platform/sprd-sdhci.3/by-name/lt_dsp",
		"/dev/block/platform/sprd-sdhci.3/by-name/lt_dsp",
		0x60000,
		0x81B20000,
	},*/
	{ //	ca5
		"/dev/block/platform/sprd-sdhci.3/by-name/l_modem",
		"/dev/block/platform/sprd-sdhci.3/by-name/l_modem",
		0x800000,
		0x81F00000,
		0xff,
	},
	{ //ca5 fixvn
		"/dev/block/platform/sprd-sdhci.3/by-name/l_fixnv1",
		"/dev/block/platform/sprd-sdhci.3/by-name/l_fixnv1",
		0x20000,
		0x82690000,
		0xff,
	},
	{ //ca5 runtimevn
		"/dev/block/platform/sprd-sdhci.3/by-name/l_runtimenv1",
		"/dev/block/platform/sprd-sdhci.3/by-name/l_runtimenv1",
		0x40000,
		0x826B0000,
		0xff,
	},
	{ //	cmdline
		"/proc/cmdline",
		"/proc/cmdline",
		0x10000,
		0x826F0000,
		0xff,
       },
	{ //ca5 DSP
		"/dev/block/platform/sprd-sdhci.3/by-name/l_ldsp",
		"/dev/block/platform/sprd-sdhci.3/by-name/l_ldsp",
		0x0200000,
		0x82700000,
		0xff,
	},
	{
		NULL,
		NULL,
		0,
		0,
		0,
	},
};
static int modem_interface_fd = -1;
int speed_arr[] = {B921600,B115200, B38400, B19200, B9600, B4800, B2400, B1200, B300,
                   B921600,B115200, B38400, B19200, B9600, B4800, B2400, B1200, B300, };
int name_arr[] = {921600,115200,38400,  19200, 9600,  4800,  2400,  1200,  300,
        921600, 115200,38400,  19200,  9600, 4800, 2400, 1200,  300, };

static int get_modem_capabilities(void)
{
   MODEM_LOGD("%s capabilities_flag = %d\n", __func__, capabilities_flag);
   return capabilities_flag;
}
void  set_modem_capabilities( int flag)
{
   MODEM_LOGD("%s capabilities_flag = %d\n", __func__, flag);
   capabilities_flag = flag;
}

int get_modem_images_info_bak(void)
{
    FILE *fp;
    int images_count = 0;
    char line[256];
    unsigned long address,length;
    struct image_info *info = download_image_info;
    int max_item = sizeof(download_image_info)/sizeof(download_image_info[0]);

    if(max_item == 0)
        return 0;

    if (!(fp = fopen("/modem_images.info", "r"))) {
        return 0;
    }
    MODEM_LOGD("start parse modem images file\n");

    while(fgets(line, sizeof(line), fp)) {
        const char *delim = " \t";
        char *save_ptr;
        char *filename, *address_ptr, *length_ptr;


        line[strlen(line)-1] = '\0';

        if (line[0] == '#' || line[0] == '\0')
            continue;

        if (!(filename = strtok_r(line, delim, &save_ptr))) {
            MODEM_LOGE("Error parsing type");
            break;
        }
        if (!(length_ptr = strtok_r(NULL, delim, &save_ptr))) {
            break;
        }
        if (!(address_ptr = strtok_r(NULL, delim, &save_ptr))) {
            MODEM_LOGE("Error parsing label");
            break;
        }
        MODEM_LOGD("%s:%s:%s\n",filename,&length_ptr[2],&address_ptr[2]);
        info[images_count].image_path = strdup(filename);
        info[images_count].image_size = strtol(&length_ptr[2],&save_ptr,16);
        info[images_count].address = strtol(&address_ptr[2],&save_ptr,16);
        if((info[images_count].address == 0) || (info[images_count].image_size==0)){
                /*get tty device number from modem_images.info*/
                uart_dev = info[images_count].image_path;
                MODEM_LOGD("UART Device = %s",uart_dev);
                if(info[images_count].image_size != 0){
                        /*get cp power delay param from modem_images.info*/
                        fdl_cp_poweron_delay = info[images_count].image_size;
                }
        }else {
                images_count++;
        }
        if(images_count >= max_item) break;
    }
    fclose(fp);
    modem_images_count = images_count;
    return images_count;
}


static unsigned short calc_checksum(unsigned char *dat, unsigned long len)
{
	unsigned long checksum = 0;
	unsigned short *pstart, *pend;
	if (0 == (unsigned long)dat % 2)  {
		pstart = (unsigned short *)dat;
		pend = pstart + len / 2;
		while (pstart < pend) {
			checksum += *pstart;
			pstart ++;
		}
		if (len % 2)
			checksum += *(unsigned char *)pstart;
		} else {
		pstart = (unsigned short *)dat;
		while (len > 1) {
			checksum += ((*pstart) | ((*(pstart + 1)) << 8));
			len -= 2;
			pstart += 2;
		}
		if (len)
			checksum += *pstart;
	}
	checksum = (checksum >> 16) + (checksum & 0xffff);
	checksum += (checksum >> 16);
	return (~checksum);
}

/*
	TRUE(1): pass
	FALSE(0): fail
*/
static int _chkEcc(unsigned char* buf, int size)
{
	unsigned short crc,crcOri;
//	crc = __crc_16_l_calc(buf, size-2);
//	crcOri = (uint16)((((uint16)buf[size-2])<<8) | ((uint16)buf[size-1]) );

	crc = calc_checksum(buf,size-4);
	crcOri = (unsigned short)((((unsigned short)buf[size-3])<<8) | ((unsigned short)buf[size-4]) );

	return (crc == crcOri);
}
int _chkImg(char *fileName, int size)
{
	unsigned char* buf;
	int fileHandle = 0;;
	int ret=0;

	buf = malloc(size);
	memset(buf,0xFF,size);
	fileHandle = open(fileName, O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
	if(fileHandle < 0){
		free(buf);
		return 0;
	}
	ret = read(fileHandle, buf, size);
	close(fileHandle);
	// check IO
	if(ret != size){
		free(buf);
		return 0;
	}
	//check crc
	if(_chkEcc(buf, size)){
		free(buf);
		return 1;
	}
	free(buf);
	return 0;
}

int get_modem_images_info(void)
{
	FILE *fp;
	char line[512];
	int cnt;
	unsigned int address,size;
	char originPath[100],bakPath[100];
	 int max_item =( sizeof(download_image_info)/sizeof(download_image_info[0]))-1;

// 1 get image info from config file
	if (!(fp = fopen("/modem_images.info", "r"))) {
		return 0;
	}
	MODEM_LOGD("start parase modem images file\n");

	cnt = 0;
	memset(download_image_info,0,sizeof(download_image_info));
	modem_images_count = 0;
	MODEM_LOGD("\toriginImage\t\tbackupImage\t\tlength\taddress\n");
	while(fgets(line, sizeof(line), fp)) {
		line[strlen(line)-1] = '\0';
		if (line[0] == '#' || line[0] == '\0'){
			continue;
		}
		if(-1 == sscanf(
				line,"%s %s %x %x",
				originPath,
				bakPath,
				&download_image_info[cnt].image_size,
				&download_image_info[cnt].address
			)
		){
			continue;
		}
		download_image_info[cnt].image_path = strdup(originPath);
		download_image_info[cnt].image_path_bak = strdup(bakPath);

		MODEM_LOGD("\t%32s\t%32s\t0x%8x\t0x%8x\n",download_image_info[cnt].image_path,download_image_info[cnt].image_path_bak,download_image_info[cnt].image_size,download_image_info[cnt].address);

		if((0 == download_image_info[cnt].address) || 0 == (download_image_info[cnt].image_size)){
			/*get tty device number from modem_images.info*/
			uart_dev = download_image_info[cnt].image_path;
			MODEM_LOGD("UART Device = %s",uart_dev);
			if(download_image_info[cnt].image_size != 0){
			/*get cp power delay param from modem_images.info*/
			fdl_cp_poweron_delay = download_image_info[cnt].image_size;
			}
		}else {
			cnt++;
		}
		if(max_item <= cnt){
			MODEM_LOGE("Max support %d item, this config has too many item!!!\n",max_item);
			break;
		}
	}
	fclose(fp);
	download_image_info[cnt].image_path		= 0;
	download_image_info[cnt].image_path_bak	= 0;
	download_image_info[cnt].image_size		= 0;
	download_image_info[cnt].address			= 0;
	MODEM_LOGD("end parase %d!\n",cnt);
	modem_images_count = cnt;

// 2 check image file
	for(cnt = 0;  cnt < modem_images_count; cnt++){
		if(!strcmp(download_image_info[cnt].image_path,download_image_info[cnt].image_path_bak)){
			continue;
		}
		if(_chkImg(download_image_info[cnt].image_path, download_image_info[cnt].image_size)){
			continue;
		}
		if(!_chkImg(download_image_info[cnt].image_path_bak, download_image_info[cnt].image_size)){
			continue;
		}
		strcpy(download_image_info[cnt].image_path,download_image_info[cnt].image_path_bak);
	}
// 3 return
	return cnt;
}


void print_modem_image_info(void)
{
        int i;
        struct image_info *info = download_image_info;
        MODEM_LOGD("modem_images_count = %d .\n",modem_images_count);

        for(i=0;i<modem_images_count;i++){
                MODEM_LOGD("image[%d]: %s  size 0x%x  address 0x%x\n",i,info[i].image_path,info[i].image_size,info[i].address);
        }
}

static void poweron_modem(void)
{
	int modem_power_fd;
	int ret;
	//return;
	MODEM_LOGD("poweron modem  %d...\n", modem_power_status);
	if(modem_power_status)
		return;
	modem_power_fd = open(MODEM_POWER_PATH, O_WRONLY);
	ret = write(modem_power_fd,"1",2);
	if(ret != 2) {
		MODEM_LOGE("maybe problem in poweron modem %d, %s \n ", ret, strerror(errno));
	}
	close(modem_power_fd);
	modem_power_status = 1;
}


static void reset_modem(int type)
{
	int modem_reset_fd;
	//return;
	MODEM_LOGD("reset modem  up %d, power on status %d ...\n", type, modem_power_status);
	if(!modem_power_status)
		return;
	modem_reset_fd = open(MODEM_RESET_PATH, O_WRONLY);
        if(modem_reset_fd < 0)
		return;
        if(type == MODEM_SOFT_RESET){
		write(modem_reset_fd,"1",2);
	} else if(type == MODEM_HARD_RESET) {
		write(modem_reset_fd,"2",2);
	}
	close(modem_reset_fd);
}

void delay_ms(int ms)
{
	struct timeval delay;
	delay.tv_sec = 0;
	delay.tv_usec = ms * 1000;
	select(0, NULL, NULL, NULL, &delay);
}

#define MS_IN_SEC 1000
#define NS_IN_MS  1000000

static unsigned int delta_miliseconds(struct timespec *begin, struct timespec *end)
{
    long ns;
    unsigned int ms;
    time_t sec;

    if(NULL == begin || NULL == end){
        return 0;
    }

    ns = end->tv_nsec - begin->tv_nsec;

    sec = end->tv_sec - begin->tv_sec;

    ms = sec * MS_IN_SEC + ns / NS_IN_MS;
	if(ms == 0)
		ms = 1;
	return ms;
}


static int try_to_connect_modem(int uart_fd)
{
	unsigned long hand_shake = 0x7E7E7E7E;
	char buffer[64]={0};
	char *version_string = (char *)buffer;
	char *data = version_string;
	int i,ret;
	int status = 0;
	int loopcount = 0;
	unsigned long long ms_delta;
	struct timespec tm_begin, tm_end;

	MODEM_LOGD("try to connect modem......uart_fd = %d \n", uart_fd);
	poweron_modem();

	if(-1 == clock_gettime(CLOCK_MONOTONIC, &tm_begin)){
		MODEM_LOGE("get tm_begin error \n");
		return -1;
	}

	for(;;){
		if(-1 == clock_gettime(CLOCK_MONOTONIC, &tm_end)){
			MODEM_LOGE("get tm_begin error \n");
			return -1;
		}
		ms_delta = delta_miliseconds(&tm_begin, &tm_end);
		if(ms_delta > FDL_CP_UART_TIMEOUT){
			loopcount++;
			if(loopcount == 5) {
				MODEM_LOGE("need to hard reset \n");
				return -1;
			}
			reset_modem(MODEM_SOFT_RESET);
			usleep(fdl_cp_poweron_delay);
			if(-1 == clock_gettime(CLOCK_MONOTONIC, &tm_begin)){
				MODEM_LOGE("get tm_begin error \n");
				return -1;
			}
		}

		write(uart_fd,&hand_shake,3);
		data = version_string;
		ret = read(uart_fd,version_string,1);
		if(ret == 1){
			MODEM_LOGD("end %d 0x%x\n",ret,version_string[0]);
			if(version_string[0]==0x7E){
				data++;
				do{
					ret = read(uart_fd,data,1);
					if(ret == 1){
				 		if(*data == 0x7E){
							status = 1;
							MODEM_LOGD("Version string received:");

							i=0;
							do{
								MODEM_LOGD("0x%02x",version_string[i]);
								i++;
							}while(data > &version_string[i]);
							MODEM_LOGD("0x%02x",version_string[i]);
							MODEM_LOGD("\n");
							break;
						}
						data++;
						if ( (data - version_string) >= sizeof(buffer)) {
							MODEM_LOGD("invalid version: rubbish data in driver");
							break;
						}
					}  else {
						if(-1 == clock_gettime(CLOCK_MONOTONIC, &tm_end)){
							MODEM_LOGE("get tm_begin error \n");
							return -1;
						}
					}
				}while(delta_miliseconds(&tm_begin, &tm_end) < FDL_CP_UART_TIMEOUT);
			}
		}
		if(status == 1)
			return 0;
	}
}

static int try_to_connect_fdl(int uart_fd)
{
	unsigned long hand_shake = 0x7E7E7E7E;
	char buffer[64]={0};
	char *version_string = (char *)buffer;
	char *data = version_string;
	int i, ret, j;
	int rev = 0;
	MODEM_LOGD("try to connect fdl......uart_fd = %d \n", uart_fd);

	for(j = 0; j < 1000; j++){
		write(uart_fd,&hand_shake,3);
		data = version_string;
		ret = read(uart_fd,version_string,1);
		if(ret == 1){
			MODEM_LOGD("fdl end %d 0x%x\n",ret,version_string[0]);
			if(version_string[0]==0x7E){
				data++;
				do{
					ret = read(uart_fd,data,1);
					if(ret == 1){
						if(*data == 0x7E){
							MODEM_LOGD("Version string received:");
							i=0;
							do{
								MODEM_LOGD("0x%02x",version_string[i]);
								i++;
							}while(data > &version_string[i]);
							MODEM_LOGD("0x%02x",version_string[i]);
							MODEM_LOGD("\n");
							rev = 1;
							break;
						}
						data++;
						if ( (data - version_string) >= sizeof(buffer)) {
							MODEM_LOGE("invalid version: rubbish data in driver");
							break;
						}
					}  else {
						MODEM_LOGE("fdl read data error %d \n", ret);
						break;
					}
				}while(1);
			}
		} else {
			//MODEM_LOGE(" fdl connect read error %d, %s  \n", ret, strerror(errno));
		}
		if(rev)
			break;
		usleep(10*1000);
	}
	return rev;
}


int download_image(int channel_fd,struct image_info *info)
{
	int packet_size;
	int image_fd;
	int read_len;
	char *buffer;
	char nvbuf[512];
	nv_header_t *nv_head;
	int i,image_size;
	int count = 0;
	int ret;

        if(info->image_path == NULL)
                return DL_SUCCESS;

	image_fd = open(info->image_path, O_RDONLY,0);

	if(image_fd < 0){
		MODEM_LOGE("open file: %s error = %d\n", info->image_path,errno);
		return DL_SUCCESS;
	}
	read_len = read(image_fd,nvbuf, 512);
	nv_head = (nv_header_t*) nvbuf;
	if(nv_head->magic != NV_HEAD_MAGIC)
	{
		lseek(image_fd,SEEK_SET,0);
	}
	MODEM_LOGD("Start download image %s image_size 0x%x address 0x%x , nvbuf.magic  0x%x \n",info->image_path,info->image_size,info->address, nv_head->magic);
	image_size = info->image_size;
	count = (image_size+HS_PACKET_SIZE-1)/HS_PACKET_SIZE;
	ret = send_start_message(channel_fd,count*HS_PACKET_SIZE,info->address,1);
	if(ret != DL_SUCCESS){
		close(image_fd);
		return DL_FAILURE;
	}
	for(i=0;i<count;i++){
		packet_size = HS_PACKET_SIZE;
		buffer = (char *)&test_buffer[8];
		do{
			read_len = read(image_fd,buffer,packet_size);
			if(read_len > 0){
				packet_size -= read_len;
				buffer += read_len;
			  } else break;
		}while(packet_size > 0);
		if(image_size < HS_PACKET_SIZE){
			for(i=image_size;i<HS_PACKET_SIZE;i++)
				test_buffer[i+8] = 0xFF;
			image_size = 0;
		}else { image_size -= HS_PACKET_SIZE;}
		ret = send_data_message(channel_fd,test_buffer,HS_PACKET_SIZE,1,HS_PACKET_SIZE,image_fd);
		//break;
		if(ret != DL_SUCCESS){
			close(image_fd);
			return DL_FAILURE;
		}
	}

	ret = send_end_message(channel_fd,1);
	close(image_fd);
	return ret;
}

int download_images(int channel_fd)
{
	struct image_info *info;
	int i ,ret;
	int image_count = modem_images_count - 1;
	int cap_mask = get_modem_capabilities();

	info = &download_image_info[1];
	for(i=0;i<image_count;i++){
		if(info->mask & cap_mask){
			ret = download_image(channel_fd,info);
			if(ret != DL_SUCCESS)
				break;
		}
		info++;
	}
	if(cap_mask & 0x1)
		send_exec_message(channel_fd,0x80400000,1); //parm
	else
		send_exec_message(channel_fd,0x80400001,1); //parm
	return ret;
}

void * load_fdl2memory(int *length)
{
	int fdl_fd;
	int read_len,size;
	char *buffer = NULL;
	char *ret_val = NULL;
	struct image_info *info;

	info = &download_image_info[0];
	fdl_fd = open(info->image_path, O_RDONLY,0);
	if(fdl_fd < 0){
		MODEM_LOGE("open file %s error = %d\n", info->image_path, errno);
		return NULL;
	}
	size = info->image_size;
        buffer = malloc(size+4);
        if(buffer == NULL){
                close(fdl_fd);
                MODEM_LOGE("no memory\n");
                return NULL;
        }
        ret_val = buffer;
	do{
		read_len = read(fdl_fd,buffer,size);
		if(read_len > 0)
		{
			size -= read_len;
			buffer += read_len;
		}
	}while(size > 0);
	close(fdl_fd);
	if(length)
		*length = info->image_size;
	return ret_val;
}
static int download_fdl(int uart_fd)
{
	int size=0,ret;
	int data_size=0;
	int offset=0;
	int translated_size=0;
	int ack_size = 0;
	char *buffer,data = 0;
	char *ret_val = NULL;
	char test_buffer1[256]={0};

	buffer = load_fdl2memory(&size);
	MODEM_LOGD("fdl image info : address %p size %x\n",buffer,size);
	if(buffer == NULL)
		return DL_FAILURE;
	ret_val = buffer;
	ret = send_start_message(uart_fd,size,download_image_info[0].address,0);
	if(ret == DL_FAILURE){
                free(ret_val);
                return ret;
        }
	while(size){
		ret = send_data_message(uart_fd,buffer,FDL_PACKET_SIZE,0,0,0);
		if(ret == DL_FAILURE){
			free(ret_val);
			return ret;
		}
		buffer += FDL_PACKET_SIZE;
		size -= FDL_PACKET_SIZE;
	}
	 MODEM_LOGD("send_end_message\n");
	ret = send_end_message(uart_fd,0);
	if(ret == DL_FAILURE){
		free(ret_val);
		return ret;
	}
       MODEM_LOGD("send_exec_message\n");
	ret = send_exec_message(uart_fd,download_image_info[0].address,0);
	free(ret_val);
	return ret;
}
static void print_log_data(char *buf, int cnt)
{
}
void set_raw_data_speed(int fd, int speed)
{
    unsigned long   	i;
    int   		status;
    struct termios   	Opt;

    tcflush(fd,TCIOFLUSH);
    tcgetattr(fd, &Opt);
    for ( i= 0;  i  < sizeof(speed_arr) / sizeof(int);  i++){
        if  (speed == name_arr[i])
        {
	    //set raw data mode
            Opt.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
            Opt.c_oflag &= ~OPOST;
            Opt.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
            Opt.c_cflag &= ~(CSIZE | PARENB);
            Opt.c_cflag |= CS8;

	    Opt.c_iflag = ~(ICANON|ECHO|ECHOE|ISIG);
	    Opt.c_oflag = ~OPOST;
	    cfmakeraw(&Opt);
            //set baudrate
            cfsetispeed(&Opt, speed_arr[i]);
            cfsetospeed(&Opt, speed_arr[i]);
            status = tcsetattr(fd, TCSANOW, &Opt);
            if  (status != 0)
                perror("tcsetattr fd1");
            return;
        }
    }
 }


int open_uart_device(int polling_mode,int speed)
{
    int fd;
    if(polling_mode == 1)
	fd = open( uart_dev, O_RDWR|O_NONBLOCK );         //| O_NOCTTY | O_NDELAY
    else
	fd = open( uart_dev, O_RDWR);
    if(fd >= 0)
	    set_raw_data_speed(fd,speed);
    return fd;
}

void set_modem_state(char * buf)
{
	int modem_state_fd = 0;
	ssize_t numRead;

	modem_state_fd = open(MODEM_STATE_PATH, O_WRONLY);
	MODEM_LOGE("set_modem_state: modem_state_fd =%d\n", modem_state_fd);
	if(modem_state_fd < 0)
		return ;
	numRead = write(modem_state_fd,buf,2);
	close(modem_state_fd);
}
static void *modemd_listen_uart_thread(void *par)
{
	int modem_uart_fd = 0;
	ssize_t numRead;
	char buffer[256];
	char temp[20];
	int size =256;
       do{
		modem_uart_fd = open(uart_dev, O_RDONLY);
		MODEM_LOGD("modem_uart_fd = %d\n",modem_uart_fd);
       }while(modem_uart_fd<0);

	while(1){
		memset(buffer, 0, size);
		numRead = read(modem_uart_fd,buffer,size);
		//set_modem_assert_information(buffer, strlen(buffer));
		broadcast_modem_state(buffer,strlen(buffer));
		MODEM_LOGD("modemd_listenaccept_uart_thread buffer = %s\n",buffer);
		memcpy(temp, buffer, 16);

		if(!strncmp(temp, TD_MODEM_ALIVE_STR, strlen(TD_MODEM_ALIVE_STR))){
			MODEM_LOGD("modemd_listenaccept_uart_thread===>> _TD Modem _Alive\n");
			set_modem_state("1");
		 }
		else if((!strncmp(temp, LTE_MODEM_ASSERT_STR, strlen(LTE_MODEM_ASSERT_STR))) || (!strncmp(temp, TD_MODEM_ASSERT_STR, strlen(TD_MODEM_ASSERT_STR)))){
			MODEM_LOGD("modemd_listenaccept_uart_thread===>> %s\n", temp);
			set_modem_state("0");
		}
		else if(!strncmp(temp, LTE_MODEM_RESET_STR, strlen(LTE_MODEM_RESET_STR))){
			MODEM_LOGD("modemd_listenaccept_uart_thread===>> LTE Modem _Reset\n");
			//set_modem_state("2");
			close(modem_uart_fd);
			break;
		}

		/*close(modem_uart_fd);
		do{
			modem_uart_fd = open(uart_dev, O_RDONLY);
			MODEM_LOGD("modem_uart_fd = %d\n",modem_uart_fd);
		}while(modem_uart_fd<0);*/
	}
    return 0;
}
int modem_boot(void)
{
    int uart_fd;
    int ret=0;
    unsigned int i;
    int nread;
    char buff[512];
    unsigned long offset = 0,step = 4*1024;

reboot_modem:
#ifndef __TEST_SPI_ONLY__
	reset_modem(MODEM_HARD_RESET);
    uart_fd = open_uart_device(1,115200);
    if(uart_fd < 0)
	    return -1;

	ret = try_to_connect_modem(uart_fd);
	if(ret == 0) {
		ret = send_connect_message(uart_fd,0);
    }
    else {
        close(uart_fd);
	    goto reboot_modem;
    }


    ret = download_fdl(uart_fd);
    if(ret == DL_FAILURE){
	    close(uart_fd);
	    goto reboot_modem;
    }
    if(!try_to_connect_fdl(uart_fd)) {
		close(uart_fd);
		goto reboot_modem;
	}
    close(uart_fd);
    uart_fd = open_uart_device(0,115200);
    if(uart_fd< 0)
	return -1;

    //send message to change modem_intf mode to boot mode
    set_modem_state("0");

    uart_send_change_spi_mode_message(uart_fd,32*1024);
#endif

    modem_interface_fd = open(DLOADER_PATH, O_RDWR);
    if(modem_interface_fd < 0){
	MODEM_LOGD("open dloader device failed retry ......\n");
        for(;;) {
            modem_interface_fd = open(DLOADER_PATH, O_RDWR);
            if(modem_interface_fd>=0) {
                break;
            } else {
                MODEM_LOGD("open dloader device failed retry ......\n");
                sleep(1);
            }
        }
    }
    MODEM_LOGD("open dloader device successfully ... \n");
    close(uart_fd);
    /*if(0 != pthread_create(&t1, NULL, (void*)modemd_listen_uart_thread, NULL)){
				MODEM_LOGE(" modem_listenaccept_thread create error!\n");
    }*/
    ret = download_images(modem_interface_fd);
    MODEM_LOGD("MODEM boot finished ......\n");
    close(modem_interface_fd);
    if(ret == DL_FAILURE){
	sleep(2);
	goto reboot_modem;
    }
    return 0;
}
