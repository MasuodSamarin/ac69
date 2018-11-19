#include "includes.h"
#include "iis.h"
#include "dac/dac.h"
#include "dac/dac_api.h"
#include "aec/aec_api.h"
#include "cpu/dac_param.h"
#include "dac/ladc.h"
#include "irq_api.h"

#define AUTO_LINK
extern void iis_isr_callback(void * iis_buf);

/*
CH0DAT      PB11/PA8
CH1DAT      PB12/PA9
CH2DAT      PB13/PA10
CH3DAT      PB14/PA11
SCLK        PB15/PA12
LRCK        PC11/PA13
MCLK        PC12/PA14
*/

#define IIS_USED_PORT           1   ///>0:PORTA   1:PORTB

#define IIS_CHANNEL0_EN    1
#define IIS_CHANNEL1_EN    0 
#define IIS_CHANNEL2_EN    0
#define IIS_CHANNEL3_EN    0


#ifdef AUTO_LINK

#define IIS_TEST    0 

#if IIS_TEST
const u8 sine_buf_32K_iis[] =
{
    0x00, 0x00, 0xFF, 0x7F, 0xF9, 0x18, 0x89, 0x7D, 0xFB, 0x30, 0x41, 0x76, 0x1C, 0x47, 0x6D, 0x6A,
    0x81, 0x5A, 0x81, 0x5A, 0x6D, 0x6A, 0x1D, 0x47, 0x41, 0x76, 0xFB, 0x30, 0x8A, 0x7D, 0xF8, 0x18,
    0xFF, 0x7F, 0x00, 0x00, 0x8A, 0x7D, 0x07, 0xE7, 0x41, 0x76, 0x05, 0xCF, 0x6C, 0x6A, 0xE4, 0xB8,
    0x81, 0x5A, 0x7E, 0xA5, 0x1D, 0x47, 0x93, 0x95, 0xFB, 0x30, 0xBF, 0x89, 0xF8, 0x18, 0x77, 0x82,
    0x00, 0x00, 0x01, 0x80, 0x07, 0xE7, 0x77, 0x82, 0x05, 0xCF, 0xBF, 0x89, 0xE3, 0xB8, 0x93, 0x95,
    0x7F, 0xA5, 0x7E, 0xA5, 0x94, 0x95, 0xE4, 0xB8, 0xBF, 0x89, 0x04, 0xCF, 0x76, 0x82, 0x07, 0xE7,
    0x01, 0x80, 0x00, 0x00, 0x76, 0x82, 0xF8, 0x18, 0xC0, 0x89, 0xFC, 0x30, 0x94, 0x95, 0x1C, 0x47,
    0x7E, 0xA5, 0x82, 0x5A, 0xE3, 0xB8, 0x6D, 0x6A, 0x05, 0xCF, 0x40, 0x76, 0x08, 0xE7, 0x89, 0x7D
};
#endif

//s16 * dual-buf * (2CHL * 32points)
static s16 alink_output_buf[2][DAC_DUAL_BUF_LEN] AT(.dac_buf_sec);

//static s16 alink16_buf[DAC_DUAL_BUF_LEN] AT(.);

void alink_isr(void)
{
    u16 reg;
    reg = JL_IIS->CON2;
    JL_IIS->CON2 |= 0x0f;//clr_pnd
    s16 *buf_addr0 = NULL ;
    s16 *buf_addr1 = NULL ;
    s16 *buf_addr2 = NULL ;
    s16 *buf_addr3 = NULL ;


    if(reg & BIT(4))  //channel 0
    {
       buf_addr0 = get_iis_addr(IIS_CHANNEL0_SEL);
       iis_isr_callback(buf_addr0);
#if 0
//        printf("%x ",*buf_addr0);
//        dac_write(sine_buf_32K,sizeof(sine_buf_32K));
        //cbuf_write_dac(buf_addr0,sizeof(alink_output_buf)/2);
		///*********录音用***************///
		//cbuf_write_dac(buf_addr0, DAC_BUF_LEN); //DAC 实时播放
        if (NULL != p_ladc)
        {
            if ((p_ladc->source_chl == ENC_MIC_CHANNEL) || (p_ladc->source_chl == ENC_LINE_LR_CHANNEL))
            {
				u32 i;
				for(i = 0; i < 2; i++) //左右声道
				{
					if(p_ladc->save_ladc_buf)
	                {
	                    p_ladc->save_ladc_buf((void *)p_ladc, buf_addr0, i, DAC_DUAL_BUF_LEN/*DAC_BUF_LEN DAC_DUAL_BUF_LEN*/);
	                }
				}
            }
        }
#endif
    }
    if(reg & BIT(5))  //channel 1
    {

       buf_addr1 = get_iis_addr(IIS_CHANNEL1_SEL);
	  // putchar('s');
#if IIS_TEST
	  memcpy(buf_addr1,sine_buf_32K_iis,128);
#else
     // iis_isr_callback(buf_addr1);
#endif
	}

    if(reg & BIT(6))  //channel 2
    {
       buf_addr2 = get_iis_addr(IIS_CHANNEL2_SEL);
	}

    if(reg & BIT(7))  //channel 3
    {
       buf_addr3 = get_iis_addr(IIS_CHANNEL3_SEL);
	}
}
IRQ_REGISTER(IRQ_ALINK_IDX, alink_isr);

void iis_set_sr(u32 rate)
{
    static u32 iis_sr_old = 0;

	if(iis_sr_old == rate)
		return;

	iis_sr_old = rate;
	switch(rate)
	{
		case 48000:
			/*12.288Mhz 256fs*/
			iis_puts("IIS_RATE = 48K\n");
			PLL_ALINK_SEL(MCLK_12M288K);
			MCKD_1DIV();
			LRCLK_256FS();
			break ;

		case 44100:
			/*11.289Mhz 256fs*/
			iis_puts("IIS_RATE = 44.1K\n");
			PLL_ALINK_SEL(MCLK_11M2896K);
			MCKD_1DIV();
			LRCLK_256FS();
			break ;

		case 32000:
			/*12.288Mhz 384fs*/
			iis_puts("IIS_RATE = 32K\n");
			PLL_ALINK_SEL(MCLK_12M288K);
			MCKD_1DIV();
			LRCLK_384FS();
			break ;

		case 24000:
			/*12.288Mhz 512fs*/
			iis_puts("IIS_RATE = 24K\n");
			PLL_ALINK_SEL(MCLK_12M288K);
			MCKD_1DIV();
			LRCLK_512FS();
			break ;

		case 22050:
			/*11.289Mhz 512fs*/
			iis_puts("IIS_RATE = 22.05K\n");
			PLL_ALINK_SEL(MCLK_11M2896K);
			MCKD_1DIV();
			LRCLK_512FS();
			break ;

		case 16000:
			/*12.288/2Mhz 384fs*/
			iis_puts("IIS_RATE = 16K\n");
			PLL_ALINK_SEL(MCLK_12M288K);
			MCKD_2DIV();
			LRCLK_384FS();
			break ;

		case 12000:
			/*12.288/2Mhz 512fs*/
			iis_puts("IIS_RATE = 12K\n");
			PLL_ALINK_SEL(MCLK_12M288K);
			MCKD_2DIV();
			LRCLK_512FS();
			break ;

		case 11025:
			/*11.289/2Mhz 512fs*/
			iis_puts("IIS_RATE = 11.025K\n");
			PLL_ALINK_SEL(MCLK_11M2896K);
			MCKD_2DIV();
			LRCLK_512FS();
			break ;

		case 8000:
			/*12.288/4Mhz 384fs*/
			iis_puts("IIS_RATE = 8K\n");
			PLL_ALINK_SEL(MCLK_12M288K);
			MCKD_4DIV();
			LRCLK_384FS();
			break ;

		default:
			iis_puts("IIS_RATE err\n");
			break;
    }
}

static void iis_channel_en(u8 en, u32 channel, u8 mode)
{
    u16 chl;
    u16 dir;//数据方向0：发送，1：接受

    chl = BIT(channel);//IIS_MODE,16bit
    dir = mode<<(channel +3);

    printf("channel = %x\n chl = %x \ndir = %x\n",channel, chl, dir);

    JL_IIS->CON1 &= ~((0x0f)<<(channel));
    if(en)
    {
        JL_IIS->CON1 |= (chl|dir);
    }
    printf("JL_IIS->CON1 = %x\n",JL_IIS->CON1);
}

static void iis_channel_init(void)
{
    my_memset(alink_output_buf,0x00,sizeof(alink_output_buf));

#if IIS_USED_PORT
///    PB6(MCLK) PB0(SCLK) PB1(LRCLK) PB2(CHL0) PB3(CHL1) PB4(CHL2) PB5(CHL3)
    JL_PORTB->DIR &= ~(BIT(0)|BIT(1)|BIT(6));
    JL_IOMAP->CON0 |= BIT(11);//ALINK_IO_SEL_PORTC 12-6
#else
///    PA15(MCLK) PA9(SCLK) PA10(LRCLK) PA11(CHL0) PA12(CHL1) PA13(CHL2) PA14(CHL3)
    JL_PORTA->DIR &= ~(BIT(9)|BIT(10)|BIT(15));
    JL_IOMAP->CON0 &= ~BIT(11);//ALINK_IO_SEL_PORTA 14-6
#endif



#if IIS_CHANNEL0_EN
#if IIS_USED_PORT
    JL_PORTB->DIR &= ~BIT(2);//in
#else
    JL_PORTA->DIR &= ~BIT(11);//in
#endif
    JL_IIS->ADR0 = (volatile unsigned int)alink_output_buf;
    iis_channel_en(1, IIS_CHANNEL0, IIS_SEND_MODE);
#endif

#if IIS_CHANNEL1_EN
#if IIS_USED_PORT
    JL_PORTB->DIR &= ~BIT(3);//out
#else
    JL_PORTA->DIR &= ~BIT(12);//out
#endif
    JL_IIS->ADR1 = (volatile unsigned int)alink_output_buf;
    iis_channel_en(1, IIS_CHANNEL1, IIS_SEND_MODE);
#endif

#if IIS_CHANNEL2_EN
#if IIS_USED_PORT
    JL_PORTB->DIR &= ~BIT(4);//out
#else
    JL_PORTA->DIR &= ~BIT(13);//out
#endif
    JL_IIS->ADR2 = alink_output_buf;
    iis_channel_en(1, IIS_CHANNEL2, IIS_SEND_MODE);
#endif

#if IIS_CHANNEL3_EN
#if IIS_USED_PORT
	JL_PORTB->DIR &= ~BIT(5);//out
#else
	JL_PORTA->DIR &= ~BIT(14);//out
#endif
    JL_IIS->ADR3 = alink_output_buf;
    iis_channel_en(1, IIS_CHANNEL3, IIS_SEND_MODE);
#endif

}

void * get_iis_addr(u8 channel_sel)
{
    void* buf_used_addr=NULL;//can be used

	if(channel_sel==IIS_CHANNEL0_SEL)
	{
#if IIS_CHANNEL0_EN
        buf_used_addr = (JL_IIS->CON0 & BIT(12))? alink_output_buf[0]:alink_output_buf[1];
#endif
			
	}
	else if(channel_sel==IIS_CHANNEL1_SEL)
	{
#if IIS_CHANNEL1_EN
         buf_used_addr = (JL_IIS->CON0 & BIT(13))? alink_output_buf[0]:alink_output_buf[1];
#endif
			
	}
	else if(channel_sel==IIS_CHANNEL2_SEL)
	{
#if IIS_CHANNEL2_EN
        buf_used_addr = (JL_IIS->CON0 & BIT(14))? alink_output_buf[0]:alink_output_buf[1];
#endif
			
	}
	else if(channel_sel==IIS_CHANNEL3_SEL)
	{
#if IIS_CHANNEL3_EN
         buf_used_addr = (JL_IIS->CON0 & BIT(15))? alink_output_buf[0]:alink_output_buf[1];
#endif
			
	}

    return buf_used_addr;
}

void audio_link_init(void)
{
    IRQ_REQUEST(IRQ_ALINK_IDX, alink_isr, ALINK_ISR_PRIORITY) ;

	MCKD_PLL_ALNK_CLK();
    /* MCKD_OS_CLK(); */
    //MCKD_SYSCLK();
    SCLK_OUT_EN(1);
    MCLK_OUT_EN(1);

    iis_channel_init();
    JL_IIS->LEN = 32;

    /* iis_set_sr(8000); */
    iis_set_sr(44100);

    ALINK_EN(1);
    puts("IIS_INIT_OVER\n");
    put_u32hex(JL_IIS->CON0);
}


void audio_link_close(void)
{
    ALINK_EN(0);
}

void iis_outclk_auto_close(bool mute)
{
    static bool last_mute = true;
    if(last_mute != mute){
        last_mute = mute;
        //JL_PORTA->DIR &= ~BIT(0);
        if(last_mute){
           //JL_PORTA->OUT &= ~BIT(0);
           JL_IIS->CON0 &= ~(BIT(7)|BIT(8)); //Close iis MCLK,SCLK,LRCLK 
        }else{
           //JL_PORTA->OUT |= BIT(0);
           JL_IIS->CON0 |= (BIT(7)|BIT(8)); //Open iis MCLK,SCLK,LRCLK
        }
        //printf("!!!mute = %d\n",last_mute);
    }
}

#endif
