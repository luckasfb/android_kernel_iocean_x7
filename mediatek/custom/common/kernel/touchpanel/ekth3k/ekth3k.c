/* touchscreen/ektf2k_kthread_mtk.c - ELAN EKTF2K touchscreen driver
 * for MTK65xx serial platform.
 *
 * Copyright (C) 2012 Elan Microelectronics Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 2011/12/06: The first release, version 0x0001
 * 2012/2/15:  The second release, version 0x0002 for new bootcode
 * 2012/5/8:   Release version 0x0003 for china market
 *             Integrated 2 and 5 fingers driver code together and
 *             auto-mapping resolution.
 * 2012/8/24:  MTK version
 * 2012/12/1:  Release for MTK6577/6575/6573/6513 Platform
 *             For MTK6577, please used 18bytes of protocol.
 *             For MTK6575/6573/6513, Please used 8+8+2 received packet protocol
 */

//#define SOFTKEY_AXIS_VER
#define MTK6589_DMA
#define ELAN_BUTTON
//#define ELAN_TEN_FINGERS


#ifdef ELAN_TEN_FINGERS
#define PACKET_SIZE		44		/* support 10 fingers packet */
#else
//#define PACKET_SIZE		8 		/* support 2 fingers packet  */
#define PACKET_SIZE		18			/* support 5 fingers packet  */
#endif

#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
//#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/hrtimer.h>

#include <linux/dma-mapping.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#ifndef TPD_NO_GPIO
#include "cust_gpio_usage.h"
#endif

// for linux 2.6.36.3
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/ioctl.h>

//dma
#include <linux/dma-mapping.h>


//#include "ekth3250.h"
#include "tpd.h"
//#include "mach/eint.h"

#include "ekth3k.h"

#include <cust_eint.h>


//#define TPD_CLOSE_POWER_IN_SLEEP

//#define ELAN_DEBUG

#define PWR_STATE_DEEP_SLEEP	0
#define PWR_STATE_NORMAL		1
#define PWR_STATE_MASK			BIT(3)

#define CMD_S_PKT			0x52
#define CMD_R_PKT			0x53
#define CMD_W_PKT			0x54

#define HELLO_PKT			0x55
#define FIVE_FINGERS_PKT	0x5D
#define   MTK_FINGERS_PKT   0x6D    /** 2 Fingers: 5A 5 Fingers 5D, 10 Fingers: 62 **/

#define TWO_FINGERS_PKT      0x5A
#define MTK_FINGERS_PKT      0x6D
#define TEN_FINGERS_PKT		 0x62

#define RESET_PKT			 0x77
#define CALIB_PKT			 0xA8


#define TPD_OK 0
//#define HAVE_TOUCH_KEY
//#define TPD_KEY_COUNT 3
//#define TPD_KEYS   {KEY_MENU,KEY_HOME,KEY_BACK}
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
//#define LCT_VIRTUAL_KEY

#ifdef MTK6589_DMA
static uint8_t *gpDMABuf_va = NULL;
static uint32_t gpDMABuf_pa = NULL;
#endif

#define TPD_HAVE_BUTTON

#ifdef TPD_HAVE_BUTTON

static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

// modify
#define SYSTEM_RESET_PIN_SR 	135

//Add these Define

#define IAP_PORTION           1 // For Firmware Update in driver
#define PAGERETRY  30
#define IAPRESTART 5
#define CMD_54001234	   0


// For Firmware Update from adb
#define ELAN_IOCTLID	0xD0
#define IOCTL_I2C_SLAVE	_IOW(ELAN_IOCTLID,  1, int)
#define IOCTL_MAJOR_FW_VER  _IOR(ELAN_IOCTLID, 2, int)
#define IOCTL_MINOR_FW_VER  _IOR(ELAN_IOCTLID, 3, int)
#define IOCTL_RESET  _IOR(ELAN_IOCTLID, 4, int)
#define IOCTL_IAP_MODE_LOCK  _IOR(ELAN_IOCTLID, 5, int)
#define IOCTL_CHECK_RECOVERY_MODE  _IOR(ELAN_IOCTLID, 6, int)
#define IOCTL_FW_VER  _IOR(ELAN_IOCTLID, 7, int)
#define IOCTL_X_RESOLUTION  _IOR(ELAN_IOCTLID, 8, int)
#define IOCTL_Y_RESOLUTION  _IOR(ELAN_IOCTLID, 9, int)
#define IOCTL_FW_ID  _IOR(ELAN_IOCTLID, 10, int)
#define IOCTL_ROUGH_CALIBRATE  _IOR(ELAN_IOCTLID, 11, int)
#define IOCTL_IAP_MODE_UNLOCK  _IOR(ELAN_IOCTLID, 12, int)
#define IOCTL_I2C_INT  _IOR(ELAN_IOCTLID, 13, int)
#define IOCTL_RESUME  _IOR(ELAN_IOCTLID, 14, int)
#define IOCTL_POWER_LOCK  _IOR(ELAN_IOCTLID, 15, int)
#define IOCTL_POWER_UNLOCK  _IOR(ELAN_IOCTLID, 16, int)
#define IOCTL_FW_UPDATE  _IOR(ELAN_IOCTLID, 17, int)
#define IOCTL_BC_VER  _IOR(ELAN_IOCTLID, 18, int)
#define IOCTL_2WIREICE  _IOR(ELAN_IOCTLID, 19, int)
#define IOCTL_GET_UPDATE_PROGREE	_IOR(CUSTOMER_IOCTLID,  2, int)


#define CUSTOMER_IOCTLID	0xA0
#define IOCTL_CIRCUIT_CHECK  _IOR(CUSTOMER_IOCTLID, 1, int)



#if IAP_PORTION
enum wk_wdt_type {
	WK_WDT_LOC_TYPE,
	WK_WDT_EXT_TYPE,
	WK_WDT_LOC_TYPE_NOLOCK,
	WK_WDT_EXT_TYPE_NOLOCK,	
};

//extern void mpcore_wdt_restart(enum wk_wdt_type type);
extern void mtk_wdt_restart(enum wk_wdt_type type);
#endif

extern struct tpd_device *tpd;

uint8_t RECOVERY=0x00;
int FW_VERSION=0x00;
int X_RESOLUTION=0x00;  
int Y_RESOLUTION=0x00;
int FW_ID=0x00;
int BC_VERSION = 0x00;
int work_lock=0x00;
int power_lock=0x00;
int circuit_ver=0x01;
int button_state = 0;
/*++++i2c transfer start+++++++*/
int file_fops_addr=0x10;//0X15
/*++++i2c transfer end+++++++*/
int tpd_down_flag=0;

BOOL is_inupgrade = FALSE;
BOOL is_suspended = FALSE;

static struct i2c_client *i2c_client = NULL;
static struct task_struct *thread = NULL;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
//#define __CTP_ESD__
#if defined (__CTP_ESD__)  
static wait_queue_head_t esd_wait_queue;
static int esdflag = 0;
DEFINE_SEMAPHORE(sem_suspend);
static struct task_struct *ctp_esd_recovery_task = NULL;
static BOOL esd_kthread_pause = FALSE;


//debug 
//#define TP_DEBUG_MSG
#if defined(TP_DEBUG_MSG)
#define pr_tp(format, args...) printk("<0>" format, ##args)
#define pr_k(format, args...) printk("<0>" format, ##args)
#define pr_ch(format, args...)                      \
    printk("<0>" "%s <%d>,%s(),cheehwa_print:\n\t"  \
           format,__FILE__,__LINE__,__func__, ##args)
#else
#define pr_tp(format, args...)  do {} while (0)
#define pr_ch(format, args...)  do {} while (0)
#undef pr_k(format, args...)
#define pr_k(format, args...)  do {} while (0)
#endif
#endif /* __CTP_ESD__ */
static inline int elan_ktf2k_ts_parse_xy(uint8_t *data,
			uint16_t *x, uint16_t *y);
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);


static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
static int elan_i2c_dma_send_data(struct i2c_client *client, uint8_t *buf,uint8_t len);


static struct elan_ktf2k_ts_data *private_ts;
static int tpd_flag = 0;

static u8 boot_mode;

#if 0
static int key_pressed = -1;

struct osd_offset{
	int left_x;
	int right_x;
	unsigned int key_event;
};

static struct osd_offset OSD_mapping[] = { // Range need define by Case!
	{35, 290,  KEY_MENU},	//menu_left_x, menu_right_x, KEY_MENU
	{303, 467, KEY_HOME},	//home_left_x, home_right_x, KEY_HOME
	{473, 637, KEY_BACK},	//back_left_x, back_right_x, KEY_BACK
	{641, 905, KEY_SEARCH},	//search_left_x, search_right_x, KEY_SEARCH
};
#endif 

#define HALL_CHECK
#ifdef HALL_CHECK
/********start add hall check function ***********/
//declare all the hall check function here
void elan_hall_check_init(void);
void elan_hall_check_callback(unsigned long data);
void elan_hall_check_work_func(void);

//define it for 500ms timer
#define HALL_TIMROUT_PERIOD 500 

//define global variables
static struct timer_list elan_hall_check_timer;
        int curr_hall_state= 1;//0
        int prev_hall_state= 1;//0

//init the hall check timer 
void elan_hall_check_init(void)
{
        init_timer(&elan_hall_check_timer);
        elan_hall_check_timer.function = elan_hall_check_callback;
        elan_hall_check_timer.expires = jiffies + msecs_to_jiffies(HALL_TIMROUT_PERIOD);
        add_timer(&elan_hall_check_timer);
}
//when the hall check timer is timeout, system will call this function. 
void elan_hall_check_callback(unsigned long data)
{
        printk("[elan]:%s change mode enter\n", __func__);
        // 根据Hall状态， 设置TP进入相应模式
        curr_hall_state = mt_get_gpio_in(GPIO_HALL_EINT_PIN);//(GPIO_HALL_1_PIN);//gpio_get_value(139);
        if(curr_hall_state != prev_hall_state)
        {
                //active workqueue here
                tpd_flag = 1;
                wake_up_interruptible(&waiter);
        }
        // 重新激活Timer
        elan_hall_check_timer.expires = jiffies + msecs_to_jiffies(HALL_TIMROUT_PERIOD);
        add_timer(&elan_hall_check_timer);
}
//when the hall state is changed ,call this function to send the cmd
void elan_hall_check_work_func(void)
{
        //elan cmd set --- change CMD for ektf3248
        uint8_t cmd_normal[] = {0x54, 0xC2, 0x00, 0x01}; /*normal mode cmd*/
        uint8_t cmd_half[]   = {0x54, 0xC3, 0x00, 0x01}; /*half   mode cmd*/

        printk("[elan]:%s change mode enter!\n", __func__);
        prev_hall_state = curr_hall_state;
        if(curr_hall_state)
        {

#ifdef MTK6589_DMA		
                //normal mode
                if (elan_i2c_dma_send_data(i2c_client, cmd_normal, sizeof(cmd_normal)) != sizeof(cmd_normal))
                {
                    printk("[elan]:%s DMA send normal mode cmd failed!\n", __func__);
                }
                else
                {
                    printk("[elan]:%s DMA send normal mode cmd OK!\n", __func__);
                }
#else
		//normal mode
                if (i2c_master_send(i2c_client, cmd_normal, sizeof(cmd_normal)) != sizeof(cmd_normal))
                {
                    printk("[elan]:%s send normal mode cmd failed!\n", __func__);
                }
                else
                {
                    printk("[elan]:%s send normal mode cmd OK!\n", __func__);
                }
#endif				
        }
        else
        {
#ifdef MTK6589_DMA	        
                //half mode 
                if (elan_i2c_dma_send_data(i2c_client, cmd_half, sizeof(cmd_half)) != sizeof(cmd_half))
                {
                    printk("[elan]:%s DMA send half mode cmd failed!\n", __func__);
                }
                else
                {
                    printk("[elan]:%s DMA send half mode cmd OK!\n", __func__);
                }
#else
		 //half mode 
                if (i2c_master_send(i2c_client, cmd_half, sizeof(cmd_half)) != sizeof(cmd_half))
                {
                    printk("[elan]:%s send half mode cmd failed!\n", __func__);
                }
                else
                {
                    printk("[elan]:%s send half mode cmd OK!\n", __func__);
                }
#endif
        }
}
/******** end  add hall check function ***********/
#endif //end of HALL_CHECK

#if IAP_PORTION
uint8_t ic_status=0x00;	//0:OK 1:master fail 2:slave fail
int update_progree=0;
uint8_t I2C_DATA[3] = {0x10, 0x20, 0x21};/*I2C devices address*/  
int is_OldBootCode = 0; // 0:new 1:old



static uint8_t file_fw_data[] = {
#include "fw_data.i"
};


enum
{
	PageSize		= 132,
	PageNum		= 249,//347,
	ACK_Fail		= 0x00,
	ACK_OK			= 0xAA,
	ACK_REWRITE		= 0x55,
};

enum
{
	E_FD			= -1,
};
#endif

//Add 0821 start
static const struct i2c_device_id tpd_id[] = 
{
	{ "ektf2k_mtk", 0 },	{ }
};

#if  1
	static struct i2c_board_info __initdata i2c_tpd={ I2C_BOARD_INFO("ektf2k_mtk", (0x20>>1))};
#else
	//unsigned short force[] = {0, 0x20, I2C_CLIENT_END, I2C_CLIENT_END};
	//static const unsigned short *const forces[] = { force, NULL };
	//static struct i2c_client_address_data addr_data = { .forces = forces, };
#endif

static struct i2c_driver tpd_i2c_driver =
{
    .driver = {
        .name = "ektf2k_mtk",
        //.owner = THIS_MODULE,
    },
    .probe = tpd_probe,
    .remove =  tpd_remove,
    .id_table = tpd_id,
    .detect = tpd_detect,
    //.address_data = &addr_data,
};
//Add 0821 end



struct elan_ktf2k_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *elan_wq;
	struct work_struct work;
	struct early_suspend early_suspend;
	int intr_gpio;
// Firmware Information
	int fw_ver;
	int fw_id;
	int bc_ver;
	int x_resolution;
	int y_resolution;
// For Firmare Update 
	struct miscdevice firmware;
	struct hrtimer timer;
};


static int __fw_packet_handler(struct i2c_client *client);
static int elan_ktf2k_ts_rough_calibrate(struct i2c_client *client);
static int tpd_resume(struct i2c_client *client);

#if IAP_PORTION
int Update_FW_One(/*struct file *filp,*/ struct i2c_client *client, int recovery);
static int __hello_packet_handler(struct i2c_client *client);
int IAPReset(struct i2c_client *client);
#endif


#ifdef MTK6589_DMA

static int elan_i2c_dma_recv_data(struct i2c_client *client, uint8_t *buf,uint8_t len)
{
	int rc;
	uint8_t *pReadData = 0;
	unsigned short addr = 0;
	pReadData = gpDMABuf_va;
	addr = client->addr ;
	client->addr |= I2C_DMA_FLAG;	
  if(!pReadData){
		printk("[elan] dma_alloc_coherent failed!\n");
		return -1;
  }
	rc = i2c_master_recv(client, gpDMABuf_pa, len);
	printk("[elan] elan_i2c_dma_recv_data rc=%d!\n",rc);
//	copy_to_user(buf, pReadData, len);
	return rc;
}

static int elan_i2c_dma_send_data(struct i2c_client *client, uint8_t *buf,uint8_t len)
{
	int rc;
	unsigned short addr = 0;
	uint8_t *pWriteData = gpDMABuf_va;
	addr = client->addr ;
	client->addr |= I2C_DMA_FLAG;	
	
  if(!pWriteData){
		printk("[elan] dma_alloc_coherent failed!\n");
		return -1;
  }
//  copy_from_user(pWriteData, ((void*)buf), len);
	strncpy(pWriteData, buf, len);
	printk("elan cmd %02x %02x %02x %02x\n", pWriteData[0], pWriteData[1], pWriteData[2], pWriteData[3]);

	rc = i2c_master_send(client, gpDMABuf_pa, len);
	client->addr = addr;
	printk("[elan] elan_i2c_dma_send_data rc=%d!\n",rc);
	return rc;
}
#endif
// For Firmware Update 
int elan_iap_open(struct inode *inode, struct file *filp){ 

	//printk("[ELAN]into elan_iap_open\n");
	
	if (private_ts == NULL)  printk("private_ts is NULL~~~");
		
	return 0;
}

int elan_iap_release(struct inode *inode, struct file *filp){ 
	//printk("[ELAN]into elan_iap_release\n");

	return 0;
}

static ssize_t elan_iap_write(struct file *filp, const char *buff, size_t count, loff_t *offp){  
    int ret;
    char *tmp;
    unsigned short addr = 0;
    printk("[ELAN]into elan_iap_write\n");
    if (count > 8192)
        count = 8192;

#ifdef MTK6589_DMA    
	addr = private_ts->client->addr ;
	private_ts->client->addr |= I2C_DMA_FLAG;
	copy_from_user(gpDMABuf_va, ((void*)buff), count);

	ret = i2c_master_send(private_ts->client, gpDMABuf_pa, count);
	printk("[elan] elan_iap_write ret=%d!\n",ret);
	private_ts->client->addr = addr;
#else
    tmp = kmalloc(count, GFP_KERNEL);
    
    if (tmp == NULL)
        return -ENOMEM;

     if (copy_from_user(tmp, buff, count)) {
        return -EFAULT;
    }    
    ret = i2c_master_send(private_ts->client, tmp, count);
#endif    
    kfree(tmp);
    return (ret == 1) ? count : ret;

}

ssize_t elan_iap_read(struct file *filp, char *buff, size_t count, loff_t *offp){    
    char *tmp;
    int ret;  
    long rc;
    
   
    //printk("[ELAN]into elan_iap_read count=%d\n",count);
    if (count > 8192)
        count = 8192;

    tmp = kmalloc(count, GFP_KERNEL);

    if (tmp == NULL)
        return -ENOMEM;
#ifdef MTK6589_DMA
    ret = elan_i2c_dma_recv_data(private_ts->client, tmp, count);
    #if 0
    while(count -- )
    {
    	 printk("[ELAN]into elan_iap_read  buff[%d] = %x \n", count, tmp[count]);
    }
    #endif
    if (ret >= 0)
        rc = copy_to_user(buff, gpDMABuf_va, count);    
#else    
    ret = i2c_master_recv(private_ts->client, tmp, count);
    if (ret >= 0)
        rc = copy_to_user(buff, tmp, count);    
#endif  

    kfree(tmp);

    //return ret;
    return (ret == 1) ? count : ret;
	
}

static long elan_iap_ioctl(/*struct inode *inode,*/ struct file *filp,    unsigned int cmd, unsigned long arg){

	int __user *ip = (int __user *)arg;
	//printk("[ELAN]into elan_iap_ioctl\n");
	//printk("cmd value %x\n",cmd);
	
	switch (cmd) {        
		case IOCTL_I2C_SLAVE: 
			private_ts->client->addr = (int __user)arg;
			private_ts->client->addr &= I2C_MASK_FLAG; 
			private_ts->client->addr |= I2C_ENEXT_FLAG;
			//file_fops_addr = 0x15;
			break;   
		case IOCTL_MAJOR_FW_VER:            
			break;        
		case IOCTL_MINOR_FW_VER:            
			break;        
		case IOCTL_RESET:

	   		mt_set_gpio_mode( GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO );
 			mt_set_gpio_dir( GPIO_CTP_RST_PIN, GPIO_DIR_OUT );
    			mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
			mdelay(10);
		//	#if !defined(EVB)
    				mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
		//	#endif
		        mdelay(10);
    			mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );

			break;
		case IOCTL_IAP_MODE_LOCK:
			if(work_lock==0)
			{
				//printk("[elan]%s %x=IOCTL_IAP_MODE_LOCK\n", __func__,IOCTL_IAP_MODE_LOCK);
				work_lock=1;
				disable_irq(CUST_EINT_TOUCH_PANEL_NUM);
				mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
				//cancel_work_sync(&private_ts->work);
			}
			break;
		case IOCTL_IAP_MODE_UNLOCK:
			if(work_lock==1)
			{			
				work_lock=0;
				enable_irq(CUST_EINT_TOUCH_PANEL_NUM);
				mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
			}
			break;
		case IOCTL_CHECK_RECOVERY_MODE:
			return RECOVERY;
			break;
		case IOCTL_FW_VER:
			__fw_packet_handler(private_ts->client);
			return FW_VERSION;
			break;
		case IOCTL_X_RESOLUTION:
			__fw_packet_handler(private_ts->client);
			return X_RESOLUTION;
			break;
		case IOCTL_Y_RESOLUTION:
			__fw_packet_handler(private_ts->client);
			return Y_RESOLUTION;
			break;
		case IOCTL_FW_ID:

			__fw_packet_handler(private_ts->client);
			return FW_ID;
			break;
		case IOCTL_ROUGH_CALIBRATE:
			return elan_ktf2k_ts_rough_calibrate(private_ts->client);
		case IOCTL_I2C_INT:
			put_user(mt_get_gpio_in(GPIO_CTP_EINT_PIN),ip);
			//printk("[elan]GPIO_CTP_EINT_PIN = %d\n", mt_get_gpio_in(GPIO_CTP_EINT_PIN));

			break;	
		case IOCTL_RESUME:
			tpd_resume(private_ts->client);
			break;	
		case IOCTL_CIRCUIT_CHECK:
			return circuit_ver;
			break;
		case IOCTL_POWER_LOCK:
			power_lock=1;
			break;
		case IOCTL_POWER_UNLOCK:
			power_lock=0;
			break;
#if IAP_PORTION		
		case IOCTL_GET_UPDATE_PROGREE:
			update_progree=(int __user)arg;
			break; 

		case IOCTL_FW_UPDATE:
			//RECOVERY = IAPReset(private_ts->client);
			RECOVERY=0;
			Update_FW_One(private_ts->client, RECOVERY);
#endif
		case IOCTL_BC_VER:
			__fw_packet_handler(private_ts->client);
			return BC_VERSION;
			break;
		default:            
			break;   
	}       
	return 0;
}

struct file_operations elan_touch_fops = {    
        .open =         elan_iap_open,    
        .write =        elan_iap_write,    
        .read = 	elan_iap_read,    
        .release =	elan_iap_release,    
	.unlocked_ioctl=elan_iap_ioctl, 
 };
#if IAP_PORTION
int EnterISPMode(struct i2c_client *client, uint8_t  *isp_cmd)
{
	char buff[4] = {0};
	int len = 0;
	
	len = i2c_master_send(private_ts->client, isp_cmd,  sizeof(isp_cmd));
	if (len != sizeof(buff)) {
		//printk("[ELAN] ERROR: EnterISPMode fail! len=%d\r\n", len);
		return -1;
	}
	else
	{
		//printk("[ELAN] IAPMode write data successfully! cmd = [%2x, %2x, %2x, %2x]\n", isp_cmd[0], isp_cmd[1], isp_cmd[2], isp_cmd[3]);
	}
	return 0;
}

int ExtractPage(struct file *filp, uint8_t * szPage, int byte)
{
	int len = 0;

	len = filp->f_op->read(filp, szPage,byte, &filp->f_pos);
	if (len != byte) 
	{
		//printk("[ELAN] ExtractPage ERROR: read page error, read error. len=%d\r\n", len);
		return -1;
	}

	return 0;
}

int WritePage(uint8_t * szPage, int byte)
{
	int len = 0;

	len = i2c_master_send(private_ts->client, szPage,  byte);
	if (len != byte) 
	{
		//printk("[ELAN] ERROR: write page error, write error. len=%d\r\n", len);
		return -1;
	}

	return 0;
}

int GetAckData(struct i2c_client *client)
{
	int len = 0;

	char buff[2] = {0};
	
	len=i2c_master_recv(private_ts->client, buff, sizeof(buff));
	if (len != sizeof(buff)) {
		//printk("[ELAN] ERROR: read data error, write 50 times error. len=%d\r\n", len);
		return -1;
	}

	//printk("[ELAN] GetAckData:%x,%x\n",buff[0],buff[1]);
	if (buff[0] == 0xaa/* && buff[1] == 0xaa*/) 
		return ACK_OK;
	else if (buff[0] == 0x55 && buff[1] == 0x55)
		return ACK_REWRITE;
	else
		return ACK_Fail;

	return 0;
}

void print_progress(int page, int ic_num, int j)
{
	int i, percent,page_tatol,percent_tatol;
	char str[256];
	str[0] = '\0';
	for (i=0; i<((page)/10); i++) {
		str[i] = '#';
		str[i+1] = '\0';
	}
	
	page_tatol=page+249*(ic_num-j);
	percent = ((100*page)/(249));
	percent_tatol = ((100*page_tatol)/(249*ic_num));

	if ((page) == (249))
		percent = 100;

	if ((page_tatol) == (249*ic_num))
		percent_tatol = 100;		

	printk("\rprogress %s| %d%%", str, percent);
	
	if (page == (249))
		printk("\n");
}
/* 
* Restet and (Send normal_command ?)
* Get Hello Packet
*/
int  IAPReset(struct i2c_client *client)
{
			int res;

	   		mt_set_gpio_mode( GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO );
 			mt_set_gpio_dir( GPIO_CTP_RST_PIN, GPIO_DIR_OUT );
    			mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
			mdelay(10);
	 
    			mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	 
	   		mdelay(10);
    			mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
			return 1;

#if 1
	//printk("[ELAN] read Hello packet data!\n"); 	  
	res= __hello_packet_handler(client);
	return res;
#endif 
}

/* Check Master & Slave is "55 aa 33 cc" */
int CheckIapMode(void)
{
	char buff[4] = {0},len = 0;
	//WaitIAPVerify(1000000);
	//len = read(fd, buff, sizeof(buff));
	len=i2c_master_recv(private_ts->client, buff, sizeof(buff));
	if (len != sizeof(buff)) 
	{
		//printk("[ELAN] CheckIapMode ERROR: read data error,len=%d\r\n", len);
		return -1;
	}
	else
	{
		
		if (buff[0] == 0x55 && buff[1] == 0xaa && buff[2] == 0x33 && buff[3] == 0xcc)
		{
			//printk("[ELAN] CheckIapMode is 55 aa 33 cc\n");
			return 0;
		}
		else// if ( j == 9 )
		{
			//printk("[ELAN] Mode= 0x%x 0x%x 0x%x 0x%x\r\n", buff[0], buff[1], buff[2], buff[3]);
			//printk("[ELAN] ERROR:  CheckIapMode error\n");
			return -1;
		}
	}
	//printk("\n");	
}

void elan_kickdog(void *info)
{
	mtk_wdt_restart(WK_WDT_EXT_TYPE);
}



int Update_FW_One(struct i2c_client *client, int recovery)
{
	int res = 0,ic_num = 1;
	int iPage = 0, rewriteCnt = 0; //rewriteCnt for PAGE_REWRITE
	int i = 0;
	uint8_t data;

	int restartCnt = 0, checkCnt = 0; // For IAP_RESTART
	//uint8_t recovery_buffer[4] = {0};
	int byte_count;
	uint8_t *szBuff = NULL;
	int curIndex = 0;
#if CMD_54001234
	uint8_t isp_cmd[] = {0x54, 0x00, 0x12, 0x34};	 //54 00 12 34
#else
	uint8_t isp_cmd[] = {0x45, 0x49, 0x41, 0x50};	 //45 49 41 50
#endif
	uint8_t recovery_buffer[4] = {0};

IAP_RESTART:	

	data=I2C_DATA[0];//Master
	dev_dbg(&client->dev, "[ELAN] %s: address data=0x%x \r\n", __func__, data);

	//if(recovery != 0x80)
	//{
		//printk("[ELAN] Firmware upgrade normal mode !\n");

		IAPReset(client);
	        mdelay(20);	

		res = EnterISPMode(private_ts->client, isp_cmd);	 //enter ISP mode

	res = i2c_master_recv(private_ts->client, recovery_buffer, 4);   //55 aa 33 cc 
	//printk("[ELAN] recovery byte data:%x,%x,%x,%x \n",recovery_buffer[0],recovery_buffer[1],recovery_buffer[2],recovery_buffer[3]);			

        mdelay(10);
#if 1
		//Check IC's status is IAP mode(55 aa 33 cc) or not
		res = CheckIapMode();	 //Step 1 enter ISP mode
		if (res == -1) //CheckIapMode fail
		{	
			checkCnt ++;
			if (checkCnt >= 5)
			{
				printk("[ELAN] ERROR: CheckIapMode %d times fails!\n", IAPRESTART);
				return E_FD;
			}
			else
			{
				printk("[ELAN] CheckIapMode retry %dth times! And restart IAP~~~\n\n", checkCnt);
				goto IAP_RESTART;
			}
		}
		else
			printk("[ELAN]  CheckIapMode ok!\n");
#endif
	//} else
	//	printk("[ELAN] Firmware upgrade recovery mode !\n");


	// Send Dummy Byte	
	//printk("[ELAN] send one byte data:%x,%x",private_ts->client->addr,data);
	res = i2c_master_send(private_ts->client, &data,  sizeof(data));
	if(res!=sizeof(data))
	{
		//printk("[ELAN] dummy error code = %d\n",res);
	}	
	mdelay(50);


	// Start IAP
	for( iPage = 1; iPage <= PageNum; iPage++ ) 
	{
PAGE_REWRITE:
#if 1 
		// 8byte mode
		//szBuff = fw_data + ((iPage-1) * PageSize); 
		for(byte_count=1;byte_count<=17;byte_count++)
		{
			if(byte_count!=17)
			{		
				//printk("[ELAN] byte %d\n",byte_count);	
				//printk("curIndex =%d\n",curIndex);
				szBuff = file_fw_data + curIndex;
				curIndex =  curIndex + 8;

				//ioctl(fd, IOCTL_IAP_MODE_LOCK, data);
				res = WritePage(szBuff, 8);
			}
			else
			{
				//printk("byte %d\n",byte_count);
				//printk("curIndex =%d\n",curIndex);
				szBuff = file_fw_data + curIndex;
				curIndex =  curIndex + 4;
				//ioctl(fd, IOCTL_IAP_MODE_LOCK, data);
				res = WritePage(szBuff, 4); 
			}
		} // end of for(byte_count=1;byte_count<=17;byte_count++)
#endif 
#if 0 // 132byte mode		
		szBuff = file_fw_data + curIndex;
		curIndex =  curIndex + PageSize;
		res = WritePage(szBuff, PageSize);
#endif
#if 1   //huaqi 3257ic must  delay
		if((iPage==249) ||( iPage==1))
		{
		//	mdelay(500); 			 
		}
		else
		{
		//	mdelay(20); 			 
		}
#endif	
		#if 0
		if(iPage%10==0)
		{
			printk("[ELAN] elan kickdog\r\n");
			on_each_cpu((smp_call_func_t)elan_kickdog,NULL,0);
		}
		#endif
		
		res = GetAckData(private_ts->client);

		if (ACK_OK != res) 
		{
			mdelay(50); 
			//printk("[ELAN] ERROR: GetAckData fail! res=%d\r\n", res);
			if ( res == ACK_REWRITE ) 
			{
				rewriteCnt = rewriteCnt + 1;
				if (rewriteCnt == PAGERETRY)
				{
					//printk("[ELAN] ID 0x%02x %dth page ReWrite %d times fails!\n", data, iPage, PAGERETRY);
					return E_FD;
				}
				else
				{
					//printk("[ELAN] ---%d--- page ReWrite %d times!\n",  iPage, rewriteCnt);
					curIndex = curIndex - PageSize;
					goto PAGE_REWRITE;
				}
			}
			else
			{
				restartCnt = restartCnt + 1;
				if (restartCnt >= 5)
				{
					//printk("[ELAN] ID 0x%02x ReStart %d times fails!\n", data, IAPRESTART);
					return E_FD;
				}
				else
				{
					//printk("[ELAN] ===%d=== page ReStart %d times!\n",  iPage, restartCnt);
					goto IAP_RESTART;
				}
			}
		}
		else
		{       printk("  data : 0x%02x ,res=0x%02x",  data,res);  
			rewriteCnt=0;
			print_progress(iPage,ic_num,i);
		}

		mdelay(10);
	} // end of for(iPage = 1; iPage <= PageNum; iPage++)

	if (IAPReset(client) > 0)
		printk("[ELAN] Update ALL Firmware successfully!\n");
	return 0;
}

#endif
// End Firmware Update


#if 0
static void elan_ktf2k_ts_early_suspend(struct early_suspend *h);
static void elan_ktf2k_ts_late_resume(struct early_suspend *h);
#endif

static ssize_t elan_ktf2k_gpio_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct elan_ktf2k_ts_data *ts = private_ts;

	//ret = gpio_get_value(ts->intr_gpio);
	ret = mt_get_gpio_in(GPIO_CTP_EINT_PIN);
	//printk(KERN_DEBUG "GPIO_TP_INT_N=%d\n", ts->intr_gpio);
	sprintf(buf, "GPIO_TP_INT_N=%d\n", ret);
	ret = strlen(buf) + 1;
	return ret;
}

static DEVICE_ATTR(gpio, S_IRUGO, elan_ktf2k_gpio_show, NULL);

static ssize_t elan_ktf2k_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct elan_ktf2k_ts_data *ts = private_ts;

	sprintf(buf, "%s_x%4.4x\n", "ELAN_KTF2K", ts->fw_ver);
	ret = strlen(buf) + 1;
	return ret;
}
#if 0
static DEVICE_ATTR(vendor, S_IRUGO, elan_ktf2k_vendor_show, NULL);

static struct kobject *android_touch_kobj;

static int elan_ktf2k_touch_sysfs_init(void)
{
	int ret ;

	android_touch_kobj = kobject_create_and_add("android_touch", NULL) ;
	if (android_touch_kobj == NULL) {
		printk(KERN_ERR "[elan]%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_gpio.attr);
	if (ret) {
		printk(KERN_ERR "[elan]%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
	if (ret) {
		printk(KERN_ERR "[elan]%s: sysfs_create_group failed\n", __func__);
		return ret;
	}
	return 0 ;
}

static void elan_touch_sysfs_deinit(void)
{
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_gpio.attr);
	kobject_del(android_touch_kobj);
}	
#endif


static int __elan_ktf2k_ts_poll(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int status = 0, retry = 10;

	do {
		//status = gpio_get_value(ts->intr_gpio);
		status = mt_get_gpio_in(GPIO_CTP_EINT_PIN);
		printk("[elan]: %s: status = %d\n", __func__, status);
		retry--;
		mdelay(20);
	} while (status == 1 && retry > 0);

	//printk( "[elan]%s: poll interrupt status %s\n",__func__, status == 1 ? "high" : "low");
	return (status == 0 ? 0 : -ETIMEDOUT);
}

static int elan_ktf2k_ts_poll(struct i2c_client *client)
{
	return __elan_ktf2k_ts_poll(client);
}

static int elan_ktf2k_ts_get_data(struct i2c_client *client, uint8_t *cmd,
			uint8_t *buf, size_t size)
{
	int rc;

	//dev_dbg(&client->dev, "[elan]%s: enter\n", __func__);

	if (buf == NULL)
		return -EINVAL;


	if ((i2c_master_send(client, cmd, 4)) != 4) {
		//dev_err(&client->dev,"[elan]%s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}


	rc = elan_ktf2k_ts_poll(client);
	if (rc < 0)
		return -EINVAL;
	else {

		if (i2c_master_recv(client, buf, size) != size ||
		    buf[0] != CMD_S_PKT)
			return -EINVAL;
	}

	return 0;
}

static int __hello_packet_handler(struct i2c_client *client)
{
	int rc;
	uint8_t buf_recv[8] = { 0 };
	//uint8_t buf_recv1[4] = { 0 };

	mdelay(1500);

	rc = elan_ktf2k_ts_poll(client);
	if (rc < 0) {
		//printk( "[elan] %s: Int poll failed!\n", __func__);
		RECOVERY=0x80;
	//	return RECOVERY;	
	}

	rc = i2c_master_recv(client, buf_recv, 4);

	printk("[elan] %s: Hello Packet %2x:%2X:%2x:%2x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);
/*  Received 8 bytes data will let TP die on old firmware on ektf21xx carbon player and MK5     
    rc = i2c_master_recv(client, buf_recv, 8);
	printk("[elan] %s: hello packet %2x:%2X:%2x:%2x:%2x:%2X:%2x:%2x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3] , buf_recv[4], buf_recv[5], buf_recv[6], buf_recv[7]);
*/
	if(buf_recv[0]!=0x55 || buf_recv[1]!=0x55 )//|| buf_recv[2]!=0x55 || buf_recv[3]!=0x55)
	{
		return -1;
	}
	
	if(buf_recv[0]==0x55 && buf_recv[1]==0x55 && buf_recv[2]==0x80 && buf_recv[3]==0x80)
	{
		RECOVERY=0x80;

		rc = i2c_master_recv(client, buf_recv, 4);

		//printk("[elan] %s: Bootcode Verson %2x:%2X:%2x:%2x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);
		return RECOVERY; 
	}

	return 0;
}

static int __fw_packet_handler(struct i2c_client *client)
{
	int rc;
	int major, minor;
	uint8_t cmd[] = {CMD_R_PKT, 0x00, 0x00, 0x01};/* Get Firmware Version*/
	uint8_t cmd_x[] = {0x53, 0x60, 0x00, 0x00}; /*Get x resolution*/
	uint8_t cmd_y[] = {0x53, 0x63, 0x00, 0x00}; /*Get y resolution*/
	uint8_t cmd_id[] = {0x53, 0xf0, 0x00, 0x01}; /*Get firmware ID*/
	uint8_t cmd_bc[] = {CMD_R_PKT, 0x01, 0x00, 0x01};/* Get BootCode Version*/
	uint8_t buf_recv[4] = {0};

	//printk( "[elan] %s: n", __func__);
// Firmware version
	rc = elan_ktf2k_ts_get_data(client, cmd, buf_recv, 4);
	if (rc < 0)
		return rc;
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
//	ts->fw_ver = major << 8 | minor;
	FW_VERSION = major << 8 | minor;
// Firmware ID
	rc = elan_ktf2k_ts_get_data(client, cmd_id, buf_recv, 4);
	if (rc < 0)
		return rc;
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	//ts->fw_id = major << 8 | minor;
	FW_ID = major << 8 | minor;
// X Resolution
	rc = elan_ktf2k_ts_get_data(client, cmd_x, buf_recv, 4);
	if (rc < 0)
		return rc;
	minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
	//ts->x_resolution =minor;
	X_RESOLUTION = minor;
	
// Y Resolution	
	rc = elan_ktf2k_ts_get_data(client, cmd_y, buf_recv, 4);
	if (rc < 0)
		return rc;
	minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
	//ts->y_resolution =minor;
	Y_RESOLUTION = minor;

// Bootcode version
	rc = elan_ktf2k_ts_get_data(client, cmd_bc, buf_recv, 4);
	if (rc < 0)
		return rc;
	major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	//ts->bc_ver = major << 8 | minor;
	BC_VERSION = major << 8 | minor;
	
	printk( "[elan] %s: firmware version: 0x%4.4x\n",__func__, FW_VERSION);
	printk( "[elan] %s: firmware ID: 0x%4.4x\n",__func__, FW_ID);
	printk( "[elan] %s: x resolution: %d, y resolution: %d\n",__func__, X_RESOLUTION, Y_RESOLUTION);
	printk( "[elan] %s: bootcode version: 0x%4.4x\n",__func__, BC_VERSION);
	return 0;
}

static inline int elan_ktf2k_ts_parse_xy(uint8_t *data,
			uint16_t *x, uint16_t *y)
{
	*x = *y = 0;

	*x = (data[0] & 0xf0);
	*x <<= 4;
	*x |= data[1];

	*y = (data[0] & 0x0f);
	*y <<= 8;
	*y |= data[2];

	return 0;
}

static int elan_ktf2k_ts_setup(struct i2c_client *client)
{
	int rc;
   
	rc = __hello_packet_handler(client);
	//printk("[elan] hellopacket's rc = %d\n",rc);

	if(rc == -1)
		return rc;

	mdelay(600);
	if (rc != 0x80){
	    rc = __fw_packet_handler(client);
	    if (rc < 0)
		    printk("[elan] %s, fw_packet_handler fail, rc = %d", __func__, rc);
		  else
	    	printk("[elan] %s: firmware checking done.\n", __func__);
			/* Check for FW_VERSION, if 0x0000 means FW update fail! */
	    if ( FW_VERSION == 0x00)
	    {
				rc = 0x80;
				printk("[elan] FW_VERSION = %d, last FW update fail\n", FW_VERSION);
	    }
	}
	return rc; /* Firmware need to be update if rc equal to 0x80(Recovery mode)   */
}

static int elan_ktf2k_ts_rough_calibrate(struct i2c_client *client){
      uint8_t cmd[] = {CMD_W_PKT, 0x29, 0x00, 0x01};

	//dev_info(&client->dev, "[elan] %s: enter\n", __func__);
	//printk("[elan] %s: enter\n", __func__);
	dev_info(&client->dev,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);

	if ((i2c_master_send(client, cmd, sizeof(cmd))) != sizeof(cmd)) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int elan_ktf2k_ts_set_power_state(struct i2c_client *client, int state)
{
	uint8_t cmd[] = {CMD_W_PKT, 0x50, 0x00, 0x01};

	dev_dbg(&client->dev, "[elan] %s: enter\n", __func__);

	cmd[1] |= (state << 3);

	dev_dbg(&client->dev,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);

	if ((i2c_master_send(client, cmd, sizeof(cmd))) != sizeof(cmd)) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int elan_ktf2k_ts_get_power_state(struct i2c_client *client)
{
	int rc = 0;
	uint8_t cmd[] = {CMD_R_PKT, 0x50, 0x00, 0x01};
	uint8_t buf[4], power_state;

	rc = elan_ktf2k_ts_get_data(client, cmd, buf, 4);
	if (rc)
		return rc;

	power_state = buf[1];
	dev_dbg(&client->dev, "[elan] dump repsponse: %0x\n", power_state);
	power_state = (power_state & PWR_STATE_MASK) >> 3;
	dev_dbg(&client->dev, "[elan] power state = %s\n",power_state == PWR_STATE_DEEP_SLEEP ? "Deep Sleep" : "Normal/Idle");

	return power_state;
}

static int elan_ktf2k_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
    int err;
    u8 beg = addr; 
    struct i2c_msg msgs[2] = {
        {
            .addr = client->addr,    .flags = 0,
            .len = 1,                .buf= &beg
        },
        {
            .addr = client->addr,    .flags = I2C_M_RD,
            .len = len,             .buf = data,
            .ext_flag = I2C_DMA_FLAG,
        }
    };
   
    if (!client)
        return -EINVAL;

    err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
    if (err != len) {
        printk("[elan] elan_ktf2k_read_block err=%d\n", err);
        err = -EIO;
    } else {
		//printk("[elan] elan_ktf2k_read_block ok\n");
        err = 0;    /*no error*/
    }
    return err;


}


static int elan_ktf2k_ts_recv_data(struct i2c_client *client, uint8_t *buf)
{
	int rc, bytes_to_recv=PACKET_SIZE;
	uint8_t *pReadData = 0;
	unsigned short addr = 0;

	if (buf == NULL)
		return -EINVAL;
	memset(buf, 0, bytes_to_recv);

//#ifdef ELAN_MTK6577
#ifdef MTK6589_DMA
     addr = client->addr ;
	 client->addr |= I2C_DMA_FLAG;
	 pReadData = gpDMABuf_va;
    // pa_addr   = gpDMABuf_pa;
    if(!pReadData){
		printk("[elan] dma_alloc_coherent failed!\n");
    }
	rc = i2c_master_recv(client, gpDMABuf_pa, bytes_to_recv);
	copy_to_user(buf, pReadData, bytes_to_recv);
	client->addr = addr;
	#ifdef ELAN_DEBUG
	//printk("[elan_debug] %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15],buf[16], buf[17]);
	#endif
#else	
	rc = i2c_master_recv(client, buf, 8);
	if (rc != 8)
		printk("[elan_debug] The first package error.\n");
	//printk("[elan_recv] %x %x %x %x %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
	mdelay(1);
	
	if (buf[0] == FIVE_FINGERS_PKT){    //for five finger
		rc = i2c_master_recv(client, buf+ 8, 8);	
		if (rc != 8)
			printk("[elan_debug] The second package error.\n");
		//printk("[elan_recv] %x %x %x %x %x %x %x %x\n", buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
		rc = i2c_master_recv(client, buf+ 16, 2);
		if (rc != 2)
			printk("[elan_debug] The third package error.\n");
		mdelay(1);
		//printk("[elan_recv] %x %x \n", buf[16], buf[17]);
	}
#endif	

#if defined (__CTP_ESD__)  // 
	if ( 
		    (buf[0] == 0x52) 
		    && 
		    ((buf[1] &0xf0) == 0xf0)
		)
	{
		esdflag=1;
		pr_tp(TPD_DEVICE "=============esd wakeup\n");
		wake_up_interruptible(&esd_wait_queue);
		return;
	}
#endif /* __CTP_ESD__ */
	
	return rc;
}

#ifdef SOFTKEY_AXIS_VER //SOFTKEY is reported via AXI
static void elan_ktf2k_ts_report_data(struct i2c_client *client, uint8_t *buf)
{
	//struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	struct input_dev *idev = tpd->dev;
	uint16_t x, y;
	uint16_t fbits=0;
	uint8_t i, num, reported = 0;
	uint8_t idx;
	int finger_num;
	int limitY = ELAN_Y_MAX -100; // limitY need define by Case!

// for 5 fingers	
	if ((buf[0] == MTK_FINGERS_PKT) || (buf[0] == FIVE_FINGERS_PKT)){
	    	finger_num = 5;
	    	num = buf[1] & 0x07; 
        	fbits = buf[1] >>3;
	    	idx=2;
	}else{
// for 2 fingers      
		finger_num = 2;
		num = buf[7] & 0x03; 
		fbits = buf[7] & 0x03;
		idx=1;
	}

	switch (buf[0]) {
		case MTK_FINGERS_PKT:
		case TWO_FINGERS_PKT:
		case FIVE_FINGERS_PKT:	
		case TEN_FINGERS_PKT:
			//input_report_key(idev, BTN_TOUCH, 1);
			if (num == 0)
			{
				//dev_dbg(&client->dev, "no press\n");
				if(key_pressed < 0){
					input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 0);
					input_report_abs(idev, ABS_MT_WIDTH_MAJOR, 0);
					input_mt_sync(idev);
				}
				else{
					//dev_err(&client->dev, "[elan] KEY_RELEASE: key_code:%d\n",OSD_mapping[key_pressed].key_event);
					input_report_key(idev, OSD_mapping[key_pressed].key_event, 0);
					key_pressed = -1;
				}
			}
			else 
			{			
				//dev_dbg(&client->dev, "[elan] %d fingers\n", num);                        
				//input_report_key(idev, BTN_TOUCH, 1);
				for (i = 0; i < finger_num; i++) 
				{	
					if ((fbits & 0x01)) 
					{
						elan_ktf2k_ts_parse_xy(&buf[idx], &x, &y);  
						//elan_ktf2k_ts_parse_xy(&buf[idx], &y, &x);
						//x = X_RESOLUTION-x;	 
						//y = Y_RESOLUTION-y; 
#if 1
	if(X_RESOLUTION > 0 && Y_RESOLUTION > 0)
	{
		x = ( x * LCM_X_MAX )/X_RESOLUTION;
		y = ( y * LCM_Y_MAX )/Y_RESOLUTION;
	}
	else
	{
		x = ( x * LCM_X_MAX )/ELAN_X_MAX;
		y = ( y * LCM_Y_MAX )/ELAN_Y_MAX;
	}
#endif 		 
						printk("[elan_debug SOFTKEY_AXIS_VER] %s, x=%d, y=%d\n",__func__, x , y);
									     
						if (!((x<=0) || (y<=0) || (x>=X_RESOLUTION) || (y>=Y_RESOLUTION))) 
						{   
							if ( y < limitY )
						     	{
			    					input_report_abs(idev, ABS_MT_TRACKING_ID, i);
								input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
								input_report_abs(idev, ABS_MT_POSITION_X, x);
								input_report_abs(idev, ABS_MT_POSITION_Y, y);
								input_mt_sync(idev);
							}
							else
							{
							    	int i=0;
							    	for(i=0;i<4;i++)
							    	{
							    		if((x > OSD_mapping[i].left_x) && (x < OSD_mapping[i].right_x))
							    		{
										//dev_err(&client->dev, "[elan] KEY_PRESS: key_code:%d\n",OSD_mapping[i].key_event);
										//printk("[elan] %d KEY_PRESS: key_code:%d\n", i, OSD_mapping[i].key_event);
							    			input_report_key(idev, OSD_mapping[i].key_event, 1);
							    			key_pressed = i;
							    		}
							    	}
							}
							reported++;
							
					  	} // end if border
					} // end if finger status
				  	fbits = fbits >> 1;
				  	idx += 3;
				} // end for
			}

			if (reported)
				input_sync(idev);
			else 
			{
				input_mt_sync(idev);
				input_sync(idev);
			}

			break;
	   	default:
				dev_err(&client->dev,
					"[elan] %s: unknown packet type: %0x\n", __func__, buf[0]);
				break;
	} // end switch
	return;
}
#else //SOFTKEY is reported via BTN bit
static void elan_ktf2k_ts_report_data(struct i2c_client *client, uint8_t *buf)
{
	/*struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);*/
	struct input_dev *idev = tpd->dev;
	uint16_t x, y;
	uint16_t fbits=0;
	uint8_t i, num, reported = 0;

	uint8_t idx, btn_idx;
	int finger_num;
/* for 10 fingers	*/
	if (buf[0] == TEN_FINGERS_PKT){
	    	finger_num = 10;
	    	num = buf[2] & 0x0f; 
	    	fbits = buf[2] & 0x30;	
		    fbits = (fbits << 4) | buf[1]; 
	    	idx=3;
		    btn_idx=33;
      }
// for 5 fingers	
	else if ((buf[0] == MTK_FINGERS_PKT) || (buf[0] == FIVE_FINGERS_PKT)){
	    	finger_num = 5;
	    	num = buf[1] & 0x07; 
        fbits = buf[1] >>3;
	    	idx=2;
	    	btn_idx=17;
	}else{
// for 2 fingers      
		finger_num = 2;
		num = buf[7] & 0x03; 
		fbits = buf[7] & 0x03;
		idx=1;
		btn_idx=7;
	}
		
	switch (buf[0]) {
		case MTK_FINGERS_PKT:
		case TWO_FINGERS_PKT:
		case FIVE_FINGERS_PKT:	
		case TEN_FINGERS_PKT:
			 
			if (num == 0)
			{
				//input_report_key(idev, BTN_TOUCH, 1);//
				dev_dbg(&client->dev, "no press\n");
				#ifdef ELAN_DEBUG
				printk("tp button_state0 = %x\n",button_state);
        		printk("tp buf[btn_idx] = %x KEY_MENU=%x KEY_HOME=%x KEY_BACK=%x KEY_SEARCH =%x\n",buf[btn_idx], KEY_MENU, KEY_HOME, KEY_BACK, KEY_SEARCH);
				#endif
				
				#ifdef ELAN_BUTTON
						
					switch (buf[btn_idx]) {
				    	case ELAN_KEY_BACK:
						printk("KEY back 1\n");
									#ifndef LCT_VIRTUAL_KEY
			                        input_report_key(idev, KEY_BACK, 1);
									#else
									input_report_key(idev, BTN_TOUCH, 1);
									input_report_abs(idev, ABS_MT_TRACKING_ID, 6);
									input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
									input_report_abs(idev, ABS_MT_POSITION_X, 180);
									input_report_abs(idev, ABS_MT_POSITION_Y, 1360);
									#endif
			                        button_state = KEY_BACK;
							tpd_down_flag = 0;
						break;
						
					case ELAN_KEY_HOME:
						printk("KEY home 1\n");
									#ifndef LCT_VIRTUAL_KEY
			                        input_report_key(idev, KEY_HOMEPAGE, 1);
									#else
									input_report_key(idev, BTN_TOUCH, 1);
									input_report_abs(idev, ABS_MT_TRACKING_ID, 7);
									input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
									input_report_abs(idev, ABS_MT_POSITION_X, 300);
									input_report_abs(idev, ABS_MT_POSITION_Y, 1360);
									#endif
			                        button_state = KEY_HOMEPAGE;
						tpd_down_flag = 0;
						break;
						
					case ELAN_KEY_MENU:
						printk("KEY menu 1\n");
						#ifndef LCT_VIRTUAL_KEY
                        			input_report_key(idev, KEY_MENU, 1);
						#else
						input_report_key(idev, BTN_TOUCH, 1);
						input_report_abs(idev, ABS_MT_TRACKING_ID, 8);
						input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
						input_report_abs(idev, ABS_MT_POSITION_X, 60);
						input_report_abs(idev, ABS_MT_POSITION_Y, 1360);
						#endif
			                        button_state = KEY_MENU;
						   tpd_down_flag = 0;
						break;
				
			     // TOUCH release
					default: 	
						if(tpd_down_flag == 0)
						{
							//key up
							if(button_state != 0)
							{
								input_report_key(idev, button_state, 0);
								button_state = 0;
							}
						}	
						else
						{
					    		printk("[ELAN ] test tpd up\n");
							input_report_key(idev, BTN_TOUCH, 0);
							input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 0);
							input_report_abs(idev, ABS_MT_WIDTH_MAJOR, 0);
							input_mt_sync(idev);
		            				tpd_down_flag = 0;
						}
               				break;
				    }
                //input_sync(idev);     
				#endif		      
		}
			else 
			{			
				//dev_dbg(&client->dev, "[elan] %d fingers\n", num);                        
				input_report_key(idev, BTN_TOUCH, 1);
				for (i = 0; i < finger_num; i++) 
				{	
					if ((fbits & 0x01)) 
					{
						elan_ktf2k_ts_parse_xy(&buf[idx], &x, &y);  
						//elan_ktf2k_ts_parse_xy(&buf[idx], &y, &x); 
						//printk("elan_ktf2k_ts_parse_xy (x = %d, y = %d)\n",x,y);
						#if 0
						if(X_RESOLUTION > 0 && Y_RESOLUTION > 0)
						{	
						    x = ( x * LCM_X_MAX )/X_RESOLUTION;
						    y = ( y * LCM_Y_MAX )/Y_RESOLUTION;
						}
						else
						{
						    x = ( x * LCM_X_MAX )/ELAN_X_MAX;
						    y = ( y * LCM_Y_MAX )/ELAN_Y_MAX;
						}
						#endif 		 

						x = ( x * (LCM_X_MAX-1) )/ELAN_X_MAX;
						y = ( y * (LCM_Y_MAX-1) )/ELAN_Y_MAX;
						#ifdef ELAN_DEBUG
						printk("[elan_debug  BTN bit] %s, x=%d, y=%d\n",__func__, x , y);
						#endif
						x = LCM_X_MAX-x;	 
						//y = Y_RESOLUTION-y;
									     
						if (!((x<0) || (y<0) || (y>=LCM_Y_MAX) || (x>=LCM_X_MAX))) //
						{   
							#if 0//
							{
								int tmp = x;
				 				x = y;
								y = tmp; //TPD_RES_X - tmp;
							}
							
							if(boot_mode == FACTORY_BOOT)
							{
								int tmp = x;
				 				x = LCM_Y_MAX - y -1;
								y = tmp; 
							}
							#endif
							input_report_key(idev, BTN_TOUCH, 1);
							input_report_abs(idev, ABS_MT_TRACKING_ID, i);
							input_report_abs(idev, ABS_MT_TOUCH_MAJOR, 8);
							input_report_abs(idev, ABS_MT_POSITION_X, x);
							input_report_abs(idev, ABS_MT_POSITION_Y, y);
							input_mt_sync(idev);
							reported++;
							tpd_down_flag=1;
					  	} // end if border
					} // end if finger status
				  	fbits = fbits >> 1;
				  	idx += 3;
				} // end for
			}
			if (reported)
				input_sync(idev);
			else 
			{
				input_mt_sync(idev);
				input_sync(idev);
			}
			break;
	   	default:
				#ifdef HALL_CHECK
					if(buf[0] == 0x66)
						prev_hall_state= -1;
				#endif //end of HALL_CHECK
				printk("[elan] %s: unknown packet type: %0x\n", __func__, buf[0]);
				break;
	} // end switch
	return;
}
#endif
static void elan_ktf2k_ts_work_func(struct work_struct *work)
{
	int rc;
	struct elan_ktf2k_ts_data *ts =
	container_of(work, struct elan_ktf2k_ts_data, work);
	uint8_t buf[PACKET_SIZE] = { 0 };

//		if (gpio_get_value(ts->intr_gpio))
		if (mt_get_gpio_in(GPIO_CTP_EINT_PIN))
		{
			//enable_irq(ts->client->irq);
			printk("[elan]: Detected Jitter at INT pin. \n");
			mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
			return;
		}
	
		rc = elan_ktf2k_ts_recv_data(ts->client, buf);
 
		if (rc < 0)
		{
			//enable_irq(ts->client->irq);
			printk("[elan] elan_ktf2k_ts_recv_data Error, Error code %d \n", rc);
			mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
			return;
		}

		//printk("[elan] %2x,%2x,%2x,%2x,%2x,%2x\n",buf[0],buf[1],buf[2],buf[3],buf[5],buf[6]);
		elan_ktf2k_ts_report_data(ts->client, buf);

		//enable_irq(ts->client->irq);
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	return;
}

static irqreturn_t elan_ktf2k_ts_irq_handler(int irq, void *dev_id)
{
	struct elan_ktf2k_ts_data *ts = dev_id;
	struct i2c_client *client = ts->client;

	dev_dbg(&client->dev, "[elan] %s\n", __func__);
	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
 	tpd_flag = 1;
 	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}

static int elan_ktf2k_ts_register_interrupt(struct i2c_client *client)
{
	struct elan_ktf2k_ts_data *ts = i2c_get_clientdata(client);
	int err = 0;

	err = request_irq(client->irq, elan_ktf2k_ts_irq_handler,
											IRQF_TRIGGER_LOW, client->name, ts);
	if (err)
		dev_err(&client->dev, "[elan] %s: request_irq %d failed\n",
				__func__, client->irq);

	return err;
}

static int touch_event_handler(void *unused)
{
	int rc;
	uint8_t buf[PACKET_SIZE] = { 0 };

	int touch_state = 3;
//	int button_state = 0;
	unsigned long time_eclapse;
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);
	int last_key = 0;
	int key;
	int index = 0;
	int i =0;

	do
	{
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		enable_irq(CUST_EINT_TOUCH_PANEL_NUM);
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);
		disable_irq(CUST_EINT_TOUCH_PANEL_NUM);
		mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		
#ifdef HALL_CHECK
		 /********start add hall check function ***********/
        if(curr_hall_state != prev_hall_state)
        {                       
            elan_hall_check_work_func();
            continue;
        }
        /******** end  add hall check function***********/
#endif //end of HALL_CHECK
		rc = elan_ktf2k_ts_recv_data(private_ts->client, buf);


		if (rc < 0)
		{
			printk("[elan] rc<0\n");
	
			continue;
		}

		elan_ktf2k_ts_report_data(/*ts*/private_ts->client, buf);

	}while(!kthread_should_stop());

	return 0;
}

static int tpd_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
    strcpy(info->type, TPD_DEVICE);
    return 0;
}

static void tpd_eint_interrupt_handler(void)
{
	#ifdef ELAN_DEBUG
    printk("[elan]TPD int\n");
	#endif
    tpd_flag = 1;
    wake_up_interruptible(&waiter);
}



#if defined (__CTP_ESD__)  // 
BOOL CTP_EsdCheck(struct i2c_client *client)
{

    BYTE reg_val[4] = {0x52,0x00,0x00,0x00};

	uint8_t id[] = {0x53, 0xf0, 0x00, 0x01};
	int len=0;
	#ifdef MTK6589_DMA
	len = elan_i2c_dma_send_data(client,id,4,TRUE);
	#else
	len = i2c_master_send(client, id, 4);
	#endif

	if (len != sizeof(id)/sizeof(id[0])) {
		 pr_tp(TPD_DEVICE "================ ERROR: read id command error len=%d  %s %x\n", len,client->name,client->addr);
			return true;
	}
	else
	{
        long ret = wait_event_interruptible_timeout(esd_wait_queue, 
                                            esdflag!=0,
                                            4*HZ);
		esdflag=0;
		if(ret==0)// timeout 
		{
			pr_tp(TPD_DEVICE "================ timeout\n");
			#ifdef MTK6589_DMA
			len = elan_i2c_dma_send_data(client,id,4,TRUE);
			#else
			len = i2c_master_send(client, id, 4);
			#endif
			 long ret1 = wait_event_interruptible_timeout(esd_wait_queue, esdflag!=0,2*HZ);
			 if(ret1) 
			 {
			 	 return false;
			 }
			 else
			 {
			 	return true;
			 }
			
		}
		else 
		{
			 pr_tp(TPD_DEVICE "================ ok\n");
		}
	}

	return 0;
}

BOOL CTP_EsdRecover(void)
{
	pr_tp(TPD_DEVICE "CTP_EsdRecover \n");

	if(is_suspended == TRUE)
	{
		printk("[elan] CTP_EsdRecover TP IN UPGRADE,WE do not need recover\n");
		return true;
	}
	

	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

    mt_set_gpio_mode(GPIO_CTP_RST_PIN, 0);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, 1);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, 0);
	msleep(10);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, 1);

    msleep(200);
    mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	return true;
}
static int ctp_esd_recovery_kthread(void *data)
{
	//struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	//sched_setscheduler(current, SCHED_RR, &param);

	struct i2c_client *client=data;
	
    pr_tp(TPD_DEVICE "enter esd_recovery_kthread()\n");
    for( ;; ) {

//		printk("======================%s", client->name);
		
        if (kthread_should_stop())
            break;

		if(is_inupgrade ==TRUE)
		{
			printk("[elan] ctp_esd_recovery_kthread TP IN UPGRADE,WE do not check esd\n");
			break;
		}

        pr_tp(TPD_DEVICE "sleep start in esd_recovery_kthread()\n");
        msleep(2000);       //2s
        pr_tp(TPD_DEVICE "sleep ends in esd_recovery_kthread()\n");

        if(!esd_kthread_pause)
        {
       
           ///execute ESD check and recover flow
           pr_tp(TPD_DEVICE "CTP_EsdCheck starts\n");
           if(CTP_EsdCheck(client))
           {    
                if(!esd_kthread_pause)
                {
                    pr_tp(TPD_DEVICE "CTP_EsdRecover starts\n");
					printk("CTP_EsdRecovery");
                    CTP_EsdRecover();
                    pr_tp(TPD_DEVICE "CTP_EsdRecover ends\n");
                }
                else
                {
                    /// this is error handling for some special case
                }
           }

           pr_tp(TPD_DEVICE "DISP_EsdCheck ends\n");

//           up(&sem_suspend);
       }
    }
    pr_tp(TPD_DEVICE "exit esd_recovery_kthread()\n");
    return 0;
}
#endif /* __CTP_ESD__ */

static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	int fw_err = 0;
	int New_FW_ID;	
	int New_FW_VER;	
	int i;
	int retval = TPD_OK;
	static struct elan_ktf2k_ts_data ts;

	client->addr |= I2C_ENEXT_FLAG;

	
	printk("[elan] %s:client addr is %x, TPD_DEVICE = %s\n",__func__,client->addr,TPD_DEVICE);
	printk("[elan] %s:I2C_WR_FLAG=%x,I2C_MASK_FLAG=%x,I2C_ENEXT_FLAG =%x\n",__func__,I2C_WR_FLAG,I2C_MASK_FLAG,I2C_ENEXT_FLAG);
	client->timing =  100;

	printk("[elan]%x=IOCTL_I2C_INT\n",IOCTL_I2C_INT);
	printk("[elan]%x=IOCTL_IAP_MODE_LOCK\n",IOCTL_IAP_MODE_LOCK);
	printk("[elan]%x=IOCTL_IAP_MODE_UNLOCK\n",IOCTL_IAP_MODE_UNLOCK);

#if 1
	//client->timing = 400;
	i2c_client = client;
	private_ts = &ts;
	private_ts->client = client;
	//private_ts->addr = 0x2a;
#endif
/*------------------------------------------------------------------------*/
hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
#ifdef TPD_POWER_SOURCE_CUSTOM
        hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
        hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif
#ifdef GPIO_CTP_EN_PIN
        mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
#endif

        mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
        msleep(5);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(200);


        mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
        mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

        mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
        mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
        //mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 0);
               
			mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, tpd_eint_interrupt_handler, 1);
 			mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	msleep(100);



/*------------------------------------------------------------------------*/
#if 0
	//hwPowerOn(MT65XX_POWER_LDO_VGP5, VOL_2800, "TP");
    	hwPowerOn(MT65XX_POWER_LDO_VGP5, VOL_1800, "TP");
	msleep(10);
#if 0
	/*LDO enable*/
	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
	msleep(50);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
#endif
	printk("[elan] ELAN enter tpd_probe_ ,the i2c addr=0x%x", client->addr);
	printk("GPIO43 =%d,GPIO_CTP_EINT_PIN =%d,GPIO_DIR_IN=%d,CUST_EINT_TOUCH_PANEL_NUM=%d",GPIO43,GPIO_CTP_EINT_PIN,GPIO_DIR_IN,CUST_EINT_TOUCH_PANEL_NUM);


 #if 1       
// Reset Touch Pannel
    mt_set_gpio_mode( GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO );
    mt_set_gpio_dir( GPIO_CTP_RST_PIN, GPIO_DIR_OUT );
    mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
    mdelay(10);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    mdelay(10);
    mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
// End Reset Touch Pannel	

    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
//    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
    msleep( 200 );
#endif
#endif
#ifdef HAVE_TOUCH_KEY
int retry;
	for(retry = 0; retry <3; retry++)
	{
		input_set_capability(tpd->dev,EV_KEY,tpd_keys_local[retry]);
	}
#endif

// Setup Interrupt Pin
	//mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	//mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	//mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_DISABLE);
	//mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

#if 0
	mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, tpd_eint_interrupt_handler, 1);
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	msleep(100);
// End Setup Interrupt Pin	
	#endif

#ifdef MTK6589_DMA    
    gpDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 4096, &gpDMABuf_pa, GFP_KERNEL);
    if(!gpDMABuf_va){
		printk(KERN_INFO "[elan] Allocate DMA I2C Buffer failed\n");
    }
#endif



	mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	fw_err = elan_ktf2k_ts_setup(client);
	if (fw_err < 0) {
		printk( "[elan] No Elan chip inside\n");
		#ifdef MTK6589_DMA    
		if(gpDMABuf_va){
			dma_free_coherent(NULL, 4096, gpDMABuf_va, gpDMABuf_pa);
			gpDMABuf_va = NULL;
			gpDMABuf_pa = NULL;
		 }
		#endif
		return -1;
	}
	mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	
	tpd_load_status = 1;

#if 0 /*RESET RESOLUTION: tmp use ELAN_X_MAX & ELAN_Y_MAX*/ 
	printk("[elan] RESET RESOLUTION\n");
	input_set_abs_params(tpd->dev, ABS_X, 0,  ELAN_X_MAX, 0, 0);
	input_set_abs_params(tpd->dev, ABS_Y, 0,  ELAN_Y_MAX, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_POSITION_X, 0, ELAN_X_MAX, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_POSITION_Y, 0, ELAN_Y_MAX, 0, 0);
#endif 

	#ifndef LCT_VIRTUAL_KEY
			set_bit( KEY_BACK,  tpd->dev->keybit );
    	set_bit( KEY_HOMEPAGE,  tpd->dev->keybit );
    	set_bit( KEY_MENU,  tpd->dev->keybit );
	#endif
	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);

	if(IS_ERR(thread))
	{
		retval = PTR_ERR(thread);
		printk(TPD_DEVICE "[elan]  failed to create kernel thread: %d\n", retval);
	}

	printk("[elan]  ELAN Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
// Firmware Update
	// MISC
  	ts.firmware.minor = MISC_DYNAMIC_MINOR;
  	ts.firmware.name = "elan-iap";
  	ts.firmware.fops = &elan_touch_fops;
  	ts.firmware.mode = S_IRWXUGO; 
   	
  	if (misc_register(&ts.firmware) < 0)
  		printk("[elan] misc_register failed!!");
  	else
  	  printk("[elan] misc_register finished!!"); 
// End Firmware Update	

#if IAP_PORTION
	if(0)
	{
		work_lock=1;
		disable_irq(CUST_EINT_TOUCH_PANEL_NUM);
		mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		power_lock = 1;
		
		 //Update_FW_One(client, RECOVERY);
		#if  1
		#if 0
		 /* for ektf2127 iap ekt file   */ 
	        //printk(" [elan]  [7bd0]=0x%02x,  [7bd1]=0x%02x, [7bd2]=0x%02x, [7bd3]=0x%02x\n",  file_fw_data[31696],file_fw_data[31697],file_fw_data[31698],file_fw_data[31699]);
	         New_FW_ID = file_fw_data[31699]<<8  | file_fw_data[31698] ;        
	         New_FW_VER = file_fw_data[31697]<<8  | file_fw_data[31696] ;
                #else   //for ektf3248 iap ekt file
	         printk(" [elan]  [7d64]=0x%02x,  [7d65]=0x%02x, [7d66]=0x%02x, [7d67]=0x%02x\n",  file_fw_data[32100],file_fw_data[32101],file_fw_data[32102],file_fw_data[32103]);
	         New_FW_ID = file_fw_data[32103]<<8  | file_fw_data[32102] ;		
	         New_FW_VER = 0x55 <<8	| file_fw_data[32100] ;
                  #endif
	#if 1//def ELAN_DEBUG	 
	    printk(" [elan]  FW_ID=0x%x,   New_FW_ID=0x%x \n",  FW_ID, New_FW_ID);           
	    printk(" [elan]  FW_VERSION=0x%x,   New_FW_VER=0x%x \n",  FW_VERSION  , New_FW_VER);  
	#endif	
		if(New_FW_ID == FW_ID)
		{
			if (New_FW_VER >FW_VERSION) 
			{
				printk("[elan_d3e] start fw update");
				Update_FW_One(client, RECOVERY);
			}
			else
			{
				printk("[elan] NEW_FW_VER <= FW_VERSION!");
			}	
		}
		else
		{
			 printk("[elan] FW_ID is different!");
			 //if(FW_ID == 0xabcd && New_FW_ID == 0x2300)
			// {
				printk("[elan_d3e] start fw update");
				Update_FW_One(client, RECOVERY);
			 //}
		}

		if(FW_ID==0){
			    RECOVERY=0x80;
			    printk("[elan] RECOVERY=%x",RECOVERY);
			     Update_FW_One(client, RECOVERY);
			}

           #endif
		work_lock=0;
		power_lock = 0;
		enable_irq(CUST_EINT_TOUCH_PANEL_NUM);
		mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	}
	
#ifdef HALL_CHECK
        /********start add hall check function ***********/
        elan_hall_check_init();
        /******** end  add hall check function ***********/
#endif //end of HALL_CHECK

#endif
    
#if defined (__CTP_ESD__)  
   ctp_esd_recovery_task = kthread_create(
    			   ctp_esd_recovery_kthread, client, "CTP esd_recovery_kthread");

    if (IS_ERR(ctp_esd_recovery_task)) {
        pr_tp(TPD_DEVICE "CTP ESD recovery task create fail\n");
    }
    else {
    	wake_up_process(ctp_esd_recovery_task);
    }
#endif /* __CTP_ESD__ */

    boot_mode = get_boot_mode();
    return 0;

}

static int tpd_remove(struct i2c_client *client)

{
    printk("[elan] TPD removed\n");

	#ifdef MTK6589_DMA    
    if(gpDMABuf_va){
        dma_free_coherent(NULL, 4096, gpDMABuf_va, gpDMABuf_pa);
        gpDMABuf_va = NULL;
        gpDMABuf_pa = NULL;
    }
	#endif

	

    return 0;
}


static int tpd_suspend(struct i2c_client *client, pm_message_t message)
{
    int retval = TPD_OK;
    static char data = 0x3;
    uint8_t cmd[] = {CMD_W_PKT, 0x50, 0x00, 0x01};
    
    printk("[elan] TP enter into sleep mode\n");
	
	#ifdef HALL_CHECK
        del_timer(&elan_hall_check_timer);
#endif //end of HALL_CHECK
#if 0
#if defined (__CTP_ESD__)  
    if (down_interruptible(&sem_suspend)) {
        pr_tp(TPD_DEVICE "can't get semaphore in tpd_suspend()\n");
        return;
    }
	
	if(is_suspended){
		is_suspended = TRUE;
		up(&sem_suspend);
		pr_tp(TPD_DEVICE " has been suspended\n");
		return;
	}
    is_suspended = TRUE;
#endif /* __CTP_ESD__ */
/* __CTP_ESD__ */
	if(is_inupgrade == TRUE)
	{
		printk("[elan] tpd_suspend TP IN UPGRADE,WE CAN NOT SLEEP\n");
		return TPD_OK;
	}
	
	#endif 
	 mt65xx_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	#ifdef TPD_CLOSE_POWER_IN_SLEEP
	 mt_set_gpio_mode( GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO );
     mt_set_gpio_dir( GPIO_CTP_RST_PIN, GPIO_DIR_OUT );
	 mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	 hwPowerDown(MT65XX_POWER_LDO_VGP4, "TP");
     hwPowerDown(MT65XX_POWER_LDO_VGP5, "TP_ENT");
	#else
	#ifdef MTK6589_DMA
		if ((elan_i2c_dma_send_data(private_ts->client, cmd, sizeof(cmd))) != sizeof(cmd)) 
		{
			printk("[elan] %s: i2c_master_send failed\n", __func__);
			return -retval;
		}
	#else
    if ((i2c_master_send(private_ts->client, cmd, sizeof(cmd))) != sizeof(cmd)) 
    {
			printk("[elan] %s: i2c_master_send failed\n", __func__);
			return -retval;
    }
	#endif
	#endif
	 is_suspended = TRUE;
  
#if 0
#if defined (__CTP_ESD__)  
	up(&sem_suspend);
#endif /* __CTP_ESD__ */
#endif
    return retval;
}


static int tpd_resume(struct i2c_client *client)
{
    int retval = TPD_OK;
    uint8_t cmd[] = {CMD_W_PKT, 0x58, 0x00, 0x01};
    printk("[elan] TPD wake up\n");
#if 0
  #if defined (__CTP_ESD__)  
    if (down_interruptible(&sem_suspend)) {
        pr_tp(TPD_DEVICE " can't get semaphore in mtkfb_late_resume()\n");
        return;
    }
   #endif /* __CTP_ESD__ */
#endif
	if(is_inupgrade == TRUE)
	{
		printk("[elan] tpd_resume TP IN UPGRADE,WE DO NOT NEED RESUME\n");
		return TPD_OK;
	}
 

#if 1	
	// Reset Touch Pannel
    mt_set_gpio_mode( GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO );
    mt_set_gpio_dir( GPIO_CTP_RST_PIN, GPIO_DIR_OUT );
    mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
    mdelay(10);
//#if !defined(EVB)
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
//#endif
    mdelay(10);
    mt_set_gpio_out( GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
    //msleep(200);
#else 
    if ((i2c_master_send(private_ts->client, cmd, sizeof(cmd))) != sizeof(cmd)) 
    {
		printk("[elan] %s: i2c_master_send failed\n", __func__);
		return -retval;
    }
    	msleep(200);//
#endif

    mt65xx_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
  //  is_suspended = FALSE;
  
  #ifdef HALL_CHECK
        elan_hall_check_timer.expires = jiffies + msecs_to_jiffies(HALL_TIMROUT_PERIOD);
        add_timer(&elan_hall_check_timer);
#endif //end of HALL_CHECK
#if 0
  #if defined (__CTP_ESD__) 
    is_suspended = FALSE;
    up(&sem_suspend);
  #endif /* __CTP_ESD__ */
#endif
    return retval;
}

static int tpd_local_init(void)
{
    printk("[elan]: I2C Touchscreen Driver init\n");
    if(i2c_add_driver(&tpd_i2c_driver) != 0)
    {
        printk("[elan]: unable to add i2c driver.\n");
        return -1;
    }

     if(tpd_load_status==0) //judge ic
    {
    	TPD_DMESG("[elan] add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return -1;
    }

#ifdef TPD_HAVE_BUTTON
#ifdef LCT_VIRTUAL_KEY
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

    printk("end %s, %d\n", __FUNCTION__, __LINE__);
    tpd_type_cap = 1;

 #if defined (__CTP_ESD__)  
    init_waitqueue_head(&esd_wait_queue);
#endif /* __CTP_ESD__ */

    return 0;
}


static struct tpd_driver_t tpd_device_driver =
{
    .tpd_device_name = "ektf2k_mtk",       
    .tpd_local_init = tpd_local_init,
    .suspend = tpd_suspend,
    .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif
};

static int __init tpd_driver_init(void)
{
    printk("[elan]: Driver Verison MTK0005 for MTK65xx serial\n");
	i2c_register_board_info(0, &i2c_tpd, 1);
    if(tpd_driver_add(&tpd_device_driver) < 0)
    {
        printk("[elan]: %s driver failed\n", __func__);
    }
    return 0;
}


static void __exit tpd_driver_exit(void)
{
    printk("[elan]: %s elan touch panel driver exit\n", __func__);
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);



