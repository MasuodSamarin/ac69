#include "sdk_cfg.h"
#include "sys_detect.h"
#include "key_drv/key_drv_ad.h"
#include "rtos/task_manage.h"
#include "file_operate/file_op.h"
#include "key_drv/key.h"
#include "ui/led/led.h"
#include "dac/dac.h"
#include "key_drv/key_voice.h"
#include "linein/dev_linein.h"
#include "rtc/rtc.h"
#include "uart.h"
#include "charge.h"

u8 soft_poweroff_cnt = 0;
u8 going_to_pwr_off = 0;
u8 bd_work_mode;
/*----------------------------------------------------------------------------*/
/**@brief
   @param  void
   @return void
   @note   void sys_init(void)
*/
/*----------------------------------------------------------------------------*/
void sys_init(void)
{

#if RTC_CLK_EN
    irtc_hw_init();
#endif // RTC_CLK_EN
#if UART_UPDATA_EN
   uart_update_init();
#endif

#ifndef USE_USB_DM_PRINTF  
	usb_2_io();
#endif

	key_init();
    //led_init();
	aux_init_api(); //AUX init
}

/*----------------------------------------------------------------------------*/
/**@brief  �����ؿ���
   @param  ctl :power_on or power_off
   @return null
   @note   void soft_power_ctl(u8 ctl)
*/
/*----------------------------------------------------------------------------*/
void soft_power_ctl(u8 ctl)
{
#if 0   //soft power ctl by mos transistor
    if(PWR_ON == ctl)
    {
        led_init();
        JL_PORTA->DIR &= ~BIT(14);
        JL_PORTA->OUT |= BIT(14);
    }
    else
    {
        LVD_CON &= ~BIT(2);
        //LED_INIT_DIS();
        JL_PORTA->DIR &= ~BIT(14);
        JL_PORTA->OUT &= ~BIT(14);
    }
#else   //soft power ctl by power off

    if(PWR_ON == ctl)
    {

    }
    else
    {
		printf("soft off\n");
		charge_mode_detect_ctl(0);
        soft_poweroff();
    }

#endif
}
/*----------------------------------------------------------------------------*/
/**@brief   power off �����ʱ�ػ�
   @param   null
   @return  null
   @note    void check_power_off(void)
*/
/*----------------------------------------------------------------------------*/
void check_power_off(void)
{
    if(soft_poweroff_cnt)
    {
        if(--soft_poweroff_cnt == 0)
        {
            soft_power_ctl(PWR_OFF);
        }
    }
}
/*----------------------------------------------------------------------------*/
/**@brief   �ر�IO������λ����
   @param   null
   @return  null
   @note    void clr_PINR_ctl(void)
*/
/*----------------------------------------------------------------------------*/
void clr_PINR_ctl(void)
{
    rtc_module_port_4s_reset(PORTR2 , 0 , 0 );
}


/*----------------------------------------------------------------------------*/
/**@brief  pwr config 
   @param  default_level=1:reset to default pwr output 
   		   default_level=0:reduce pwr output 
   @return void
   @note   
*/
/*----------------------------------------------------------------------------*/
/*3.3v-3.04v-2.76v-2.5v*/
extern void fm_ldo_level(u8 level);
/*3.53v-3.34v-3.18v-3.04v-2.87v-2.73v-2.62v-2.52v*/
extern void set_sys_ldo_level(u8 level);

volatile u8 low_power_set = 0;
u8 get_pwr_config_flag()
{
	return low_power_set;
}
void pwr_level_config(u8 default_level)
{
	//default config
	if(default_level && low_power_set)
	{
		puts(">>>>>>>>>Normal_power\n");
		low_power_set = 0;
		set_sys_ldo_level(SYS_LDO_NORMAL_LEVEL);	//level:0~7
		fm_ldo_level(FM_LDO_NORMAL_LEVEL);			//level:0~3
	}
	else if(!low_power_set && !default_level)
	{
		puts(">>>>>>>>>Lower_power\n");
		low_power_set = 1;	
		set_sys_ldo_level(SYS_LDO_REDUCE_LEVEL);	//level:0~7
		fm_ldo_level(FM_LDO_REDUCE_LEVEL);			//level:0~3
	}
}

/*----------------------------------------------------------------------------*/
/**@brief  ʵʱ�������
   @param  void
   @return void
   @note   void battery_check(void *)
*/
/*----------------------------------------------------------------------------*/
#if SYS_LVD_EN

#define POWER_CHECK_CNT   100 //POWER_CHECK_CNT*10ms
u16 unit_cnt = 0;             //������λʱ��POWER_CHECK_CNT*10ms
u16 low_warn_cnt = 0;         //��λʱ���ڼ�⵽������ѹ����
u16 low_off_cnt = 0;          //��λʱ���ڼ�⵽�ػ���ѹ����
u16 low_pwr_cnt = 0;
u16 normal_pwr_cnt = 0;
extern u8 is_pwrtimer_en();
static u8 low_voice_cnt = 0;
volatile u8 low_power_flag = 0;
void battery_check(void *prt)
{
    u16 val;
	u8 cnt;

    val = get_battery_level();

	charge_disconnect_bt_check();

    unit_cnt++;

    if(val < 31)
        low_off_cnt++;
    if(val < 33)
        low_warn_cnt++;
	if(val < 35)
		low_pwr_cnt++;

    if(unit_cnt >= POWER_CHECK_CNT)
    {
        //printf("val:%d,low_off_cnt:%d,low_warn_cnt:%d\n",val,low_off_cnt,low_warn_cnt);
		if(is_sniff_mode())
			cnt = 4;
		else if(is_pwrtimer_en())
			cnt = 4;
		else
			cnt = 10;

        if(low_off_cnt > POWER_CHECK_CNT/2)//�͵�ػ�
        {
            puts("\n*******Low Power********\n");
			low_power_flag = 1;
            os_taskq_post(MAIN_TASK_NAME,1,MSG_LOW_POWER);
        }
        else if(low_warn_cnt > POWER_CHECK_CNT/2)//�͵�����
        {
			low_power_flag = 1;
			if(!going_to_pwr_off)
			{
				R_LED_ON();
				B_LED_OFF();
			}

			low_voice_cnt ++;
			if(low_voice_cnt > cnt)
			{
				puts("\n**Low Power,Please Charge Soon!!!**\n");
				low_voice_cnt = 0;
            	os_taskq_post(MAIN_TASK_NAME,1,MSG_LOW_POWER_VOICE);
			}
        }
		else
		{
			if(low_power_flag)
			{
				R_LED_OFF();
				B_LED_OFF();
			}
			low_power_flag = 0;
		}

#if SWITCH_PWR_CONFIG
		if(low_pwr_cnt > POWER_CHECK_CNT/2) {
			pwr_level_config(0);
			normal_pwr_cnt = 0;
		}
		else
		{
			if(val > 35)
			{
				if(normal_pwr_cnt++ > 5)//make sure battery full enough
				{
					normal_pwr_cnt = 0;
					pwr_level_config(1);
				}
			}
			else {
				normal_pwr_cnt = 0;
			}
		}
#endif

        unit_cnt = 0;
        low_off_cnt = 0;
        low_warn_cnt = 0;
    }
}
#endif

/*----------------------------------------------------------------------------*/
/**@brief  �ָ�USB��Ϊ��ͨIO��
   @param  USB_IO_CON
   BIT(15)-BIT(8):  --   --   --   --  DMIN DPIN  --    --
   BIT(7) -BIT(0): DMPU DPPU DMPD DPPD DMIE DPIE DMOUT DPOUT
   @return void
   @note   void usb_2_io(void)

*/
/*----------------------------------------------------------------------------*/
void usb_2_io(void)
{
//    sys_det_puts("USB_2_IO\n");
    JL_USB->CON0 |= BIT(0)|BIT(1);//PHY_ON(enable)  IO_MODE(enable)
    JL_USB->IO_CON = (BIT(3)|BIT(2));//IO����
}

/*----------------------------------------------------------------------------*/
/**@brief   watch dog timer config
   @param   void
   @return  void
   @note
 */
/*----------------------------------------------------------------------------*/
/*
cfg:1010:1s  1011:2s    1100:4s
    1101:8s  1110:16s   1111:32s
*/
void open_wdt(u8 cfg)//�����Ź�
{
    CPU_SR_ALLOC();
    OS_ENTER_CRITICAL();
    //puts("open_wdt\n");
    JL_CRC->REG  = 0X6EA5;
    JL_SYSTEM->WDT_CON  = cfg&0x0f;
    JL_SYSTEM->WDT_CON &= ~BIT(5);
    JL_SYSTEM->WDT_CON |=  BIT(4);
    JL_CRC->REG  = 0XAAAA;
    OS_EXIT_CRITICAL();
}

void close_wdt(void)//�ؿ��Ź�
{
    CPU_SR_ALLOC();
    OS_ENTER_CRITICAL();
    //puts("close_wdt\");
    JL_CRC->REG = 0x6EA5;
    JL_SYSTEM->WDT_CON &= ~BIT(4);
    JL_CRC->REG = 0;
    OS_EXIT_CRITICAL();
}

void clear_wdt(void)//�忴�Ź�
{
    //puts(" clr_wdt ");
    JL_SYSTEM->WDT_CON |= BIT(6);
}

/*----------------------------------------------------------------------------*/
/**@brief  	sd data multiplex 
   @param   void
   @return  void
   @note
 */
/*----------------------------------------------------------------------------*/
#if SD_DADA_MULT
u16 adc_value_PA1;
u8 g_aux_online_flag;
extern u32 get_sd0_dev_busy_status_api(void);
extern void close_sd0_controller(void);
extern void release_sd0_controller(void);
void sd_data_multiplex(void)
{
    u16 res = 0;
    static u8 dev_online_cnt,dev_offline_cnt,adc_timeout_cnt,adc_err_cnt;

    //if(task_start_complete == 0)
        //return;

    if(!get_sd0_dev_busy_status_api())
    {
		adc_busy_flag = 1;

		//CPU_SR_ALLOC();
        //OS_ENTER_CRITICAL();

        //�ر� sd controller
        close_sd0_controller();

        //����PA1 IO
        JL_PORTA->DIR |= BIT(1);
        JL_PORTA->HD &= ~BIT(1);
        JL_PORTA->PU |= BIT(1);
        JL_PORTA->PD &= ~BIT(1);
        JL_PORTA->DIE &= ~BIT(1);

        //����ADC
		//adc_ch_sel(AD_KEY_PA1);
		res = adc_res_api(AD_KEY_PA1);
		res = adc_res_api(get_next_adc_ch());

        //�ָ� sd io
        JL_PORTA->DIR &= ~BIT(1);
        JL_PORTA->HD |= BIT(1);
        JL_PORTA->PU |= BIT(1);
        JL_PORTA->DIE |= BIT(1); 

        //�ָ� sd controller
        release_sd0_controller();
        //OS_EXIT_CRITICAL();
		adc_busy_flag = 0;
    }
    else {
        putchar('B');
        return;
    }

    adc_value_PA1 = (res*33)/0x3ffL;
    if(++adc_timeout_cnt > 20)
    {
        //printf("online_cnt:%d     offline_cnt:%d       err_cnt:%d\n"   ,dev_online_cnt,dev_offline_cnt,adc_err_cnt);
        if(dev_online_cnt > 13) {
            //puts("aux_online\n");
            g_aux_online_flag = 1;
        }
        else if(dev_offline_cnt > 13) {
            //puts("*************aux_offline*************\n");
            g_aux_online_flag = 0;
        }
        dev_online_cnt = 0;
        adc_timeout_cnt = 0;
        dev_offline_cnt = 0;
        adc_err_cnt = 0;
    }

    if((adc_value_PA1>27)&&(adc_value_PA1<31)) {
        dev_online_cnt++;
    }
    else if(adc_value_PA1 >= 31) {
        dev_offline_cnt++;
    }
    else {
        adc_err_cnt++;
        printf("\n========err======= %d   %d  \n",res ,adc_value_PA1);
    }
    //printf("adc_val:%d,online_cnt:%d,offline_cnt:%d\n",adc_value_PA1,dev_online_cnt,dev_offline_cnt);
    printf("[%d]",adc_value_PA1);
}
#endif //SD_DADA_MULT

/*----------------------------------------------------------------------------*/
/**@brief  	pwm4_cfg 
   @param	toggle:switch
  			pre_div:0-2-4-8-16-32-64-128(div = 0~7) 
			duty:duty/16(dety = 0~15)
   @return  void
   @note    pwm4 clk = lsb_clk/16div(default:48M/16 = 3M)
 */
/*----------------------------------------------------------------------------*/
void pwm4_cfg(u8 toggle,u8 pre_div,u8 duty)
{
	u8 pwm4_scaler;
	u8 pwm4_duty;
	if(toggle) {
		pwm4_scaler = pre_div & 0x7;	
		pwm4_duty = duty & 0xF;
    	JL_PWM4->CON = (pwm4_scaler << 4) | pwm4_duty;
		JL_IOMAP->CON1 |= BIT(11)|BIT(12)|BIT(13);
    	//JL_PORTA->DIR &= ~BIT(2);JL_PORTA->PU |= BIT(2);JL_PORTA->PD |= BIT(2);
	} else {
    	JL_PWM4->CON = 0;
		JL_IOMAP->CON1 &= ~(BIT(11)|BIT(12)|BIT(13));
	}
}


