#include<stdio.h>
#include<string.h>
#include<stdarg.h>
#include<fcntl.h>
#include<scsi/sg.h>
#include<scsi/scsi.h>
#include<error.h>
#include<errno.h>

#include <scsi/scsi_ioctl.h> 

typedef struct{
 unsigned char vendor[32];
 unsigned char product[32];
 unsigned char revision[32];
 unsigned char serial[32];
 int host;
 int channel;
 int id;
 int lun;
 long size;
 long ifree;
}device_info;

typedef struct scsi_id
{
    int a[2];
} SCSI_ID;


static int prin_debug_output(int line, const char *function, const char *fmt,...);
static int storage_scsi_inquiry(int fd, device_info *curdevice);

#define prin_info( ARGS... )    prin_debug_output(__LINE__, __FUNCTION__, ##ARGS )
#define OK 0
#define ERR -1
#define SEN_LEN 254
#define BLOCK_LEN 254
#define PATH "/dev/sr0"

int main()
{
 int fd=-1;
 device_info dev={0};
 fd = open(PATH, O_RDONLY);
 if(fd<0)
 {
  prin_info("err:%s",strerror(errno));
 }

 storage_scsi_inquiry(fd,&dev);
 return OK;
}


static int storage_scsi_inquiry(int fd, device_info *curdev)
{
 unsigned char cdb[12],data_buffer[BLOCK_LEN],sense_buffer[SEN_LEN],*ptr;
 unsigned char EVPD=0,page_code=0;
 int ret=0,len=0,i=0;
 SCSI_ID scsi_id;
 struct sg_io_hdr p_hdr;
 
 //date 和sense　的地址不是４字节对其ioctl会失败，有时候不会失败,具体什么原因目前还未明白，等有时间将内核代码好好研究下
 prin_info("fd=%d cdb=%p data=%p sense=%p",fd, cdb, data_buffer, sense_buffer );
 /*set inquiry  EVPD=0 page_code=0 是取基本信息
 EVPD=1,将返回对应 page code 字段的特定于供应商的数据*/
 cdb[0] = 0x71;
 cdb[1] = 0x01;    
 cdb[2] = 0x00;
 cdb[3] = 0x00;
 cdb[4] = 0x00;
 cdb[5] = 0x00;

 memset(data_buffer,0,BLOCK_LEN);
 memset(sense_buffer,0,SEN_LEN);
 p_hdr.interface_id   = 'S';    
 p_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
 p_hdr.flags  = SG_FLAG_LUN_INHIBIT; /* 将LUN放到cdb指令区域的第二字节   */  
 p_hdr.cmdp  = cdb;      
 p_hdr.cmd_len = 6;    
 p_hdr.dxferp = data_buffer;   
 p_hdr.dxfer_len = BLOCK_LEN ;  
 p_hdr.sbp  = sense_buffer;    
 p_hdr.mx_sb_len = SEN_LEN; 

  ret = ioctl(fd,SG_IO,&p_hdr);   /*SG_IO send scsi inquiry*/
 if(ERR == ret)
 {
  prin_info("scsi inquiry failed %s",strerror(errno));
  return ERR;
 }

 ptr = p_hdr.dxferp;
 memcpy(curdev->vendor,ptr+8,8);//8-15 供应商ID
 memcpy(curdev->product,ptr+16,16);//16-31 产品ID
 memcpy(curdev->revision,ptr+32,4);//32-35产品版本
 prin_info("V:%s P:%s R:%s",curdev->vendor,curdev->product,curdev->revision);
 
 
 EVPD = 1;
 page_code = 0x80;
 cdb[1] = EVPD;   
 cdb[2] = page_code;

 ret = ioctl(fd,SG_IO,&p_hdr);   /*SG_IO send scsi inquiry*/
 if(ERR == ret)
 {
  prin_info("scsi inquiry failed!!");
  return ERR;
 }

 len = ptr[3];
 prin_info("len=%d",len);
 memcpy(curdev->serial,ptr+4,len);
 prin_info("S:%s",curdev->serial);  //序列号

 
    memset(&scsi_id, 0, sizeof(SCSI_ID));
    ret = ioctl(fd, SCSI_IOCTL_GET_IDLUN, &scsi_id);
    if (ERR == ret)
    {
        prin_info("SCSI_IOCTL_GET_IDLUN ioctl failed");
        return ERR;
    }
 
    /*返回的格式为(scsi_device_id | (lun << 8) | (channel << 16) | (host_no << 24))*/
    curdev->host    = (scsi_id.a[0] >> 24) &0xff;
    curdev->channel = (scsi_id.a[0] >> 16) &0xff;
    curdev->lun     = (scsi_id.a[0] >> 8 ) &0xff;
    curdev->id      = (scsi_id.a[0]      ) &0xff;

 
 prin_info("H:scsi%d C:%02d I:%02d L:%02d",curdev->host,curdev->channel,curdev->id,curdev->lun);

 return OK;
 
}

/**************************************************************************
*打印信息[函数名][行号]+其它信息
***************************************************************************/
static int prin_debug_output(int line, const char *function, const char *fmt,...)
{
 char msg[512];
 char *ptr;
 int len=-1;
 va_list args;

 ptr  = msg;
 len  = sprintf(ptr,"[%s]",function);
 ptr += len;
 len  = sprintf(ptr,"[%d]",line);
 ptr += len;

 va_start(args,fmt);
 len  = vsprintf(ptr,fmt,args);
 va_end(args);
 ptr += len;
 *ptr ='\0';
 printf("%s\n",msg);
 fflush(stdout);
 return OK;
}
