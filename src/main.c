#include <arch/antares.h> 
#include <generic/initcall.h>
#include <lib/printk.h>
#include <lib/earlycon.h>
#include <lib/printk.h>
#include <lib/earlycon.h>
#include <lib/spisd.h>
#include <lib/panic.h>
#include <lib/stlinky.h>

#include "stm32f10x_gpio.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_spi.h"
#include <stdio.h>
#include <inttypes.h>


 
static GPIO_InitTypeDef gpc = {
        .GPIO_Pin = GPIO_Pin_9|GPIO_Pin_8,
        .GPIO_Speed = GPIO_Speed_50MHz,
        .GPIO_Mode = GPIO_Mode_Out_PP
};


ANTARES_INIT_LOW(io_init) 
{ 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOC, ENABLE);
        GPIO_Init(GPIOC, &gpc);
	GPIO_SetBits(GPIOC, GPIO_Pin_9);
	printk("\n\n\n\n\n\n\n\n\n\n\n");
	printk("Necromant's SD rescue device\n");
	printk("Powered by Antares " CONFIG_VERSION_STRING " git commit " CONFIG_VERSION_GIT "\n");
	printk("System running at %u\n", SystemCoreClock)
}


static uint8_t spi_xfer(uint8_t data)
{
	uint8_t ret;
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = data;
	while (!(SPI1->SR & SPI_SR_RXNE));
        ret = SPI1->DR;
	return ret;

}

static void delay_us(uint32_t delay_us)
{
	uint32_t nb_loop;

	nb_loop = (((SystemCoreClock / 1000000)/4) * delay_us)+1; /* uS (divide by 4 because each loop take about 4 cycles including nop +1 is here to avoid delay of 0 */
	asm volatile(
		"1: " "\n\t"
		" nop " "\n\t"
		" subs.w %0, %0, #1 " "\n\t"
		" bne 1b " "\n\t"
		: "=r" (nb_loop)
		: "0"(nb_loop)
		: "r3"
		);
}

static void spi_cs(int cs) 
{
	if (cs)
		GPIO_ResetBits(GPIOA, GPIO_Pin_4);
	else
		GPIO_SetBits(GPIOA, GPIO_Pin_4);
	delay_us(1000);
};


void spi_init_for_sd(uint32_t prescaler)
{
	SPI_InitTypeDef SPI_InitStructure;
	SPI_Cmd(SPI1, DISABLE);  
	SPI_StructInit(&SPI_InitStructure);
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b; 
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft; 
	SPI_InitStructure.SPI_BaudRatePrescaler = prescaler;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB; 
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_Init(SPI1, &SPI_InitStructure); 
	SPI_Cmd(SPI1, ENABLE); 
}


static void spi_set_speed(int speed)
{
	if (speed == 400) 
		spi_init_for_sd(SPI_BaudRatePrescaler_128);
	else
		spi_init_for_sd(SPI_BaudRatePrescaler_2);
}

struct sd_card sd = {
	.cs = spi_cs,
	.xfer = spi_xfer,
	.set_speed = spi_set_speed
};


ANTARES_INITCALL_LOW(spi)
{
	printk("spi: initializing hardware\n");
	GPIO_InitTypeDef GPIO_InitStructure;
 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC , ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	/* SCK , MOSI */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7 ;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 ;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	spi_init_for_sd(SPI_BaudRatePrescaler_64);

}

void sd_dump_info(struct sd_info* i)
{
	printk("sd: manufacturer_id  0x%hhx\n", i->manufacturer);
	printk("sd: oem              %c%c%c\n", i->oem[0], i->oem[1], i->oem[2]);
	printk("sd: revision         %d.%d \n", ((i->revision & 0xf0)>>4) , (i->revision & 0xf));
	printk("sd: serial           0x%x\n", i->serial);
	printk("sd: manufactured     %hhd/%d\n", i->manufacturing_month, 2000 + (int) i->manufacturing_year);
	printk("sd: capacity         %llu bytes (%llu MiBs)\n", i->capacity, i->capacity / 1024 / 1024 );
	printk("sd: content          %s\n", i->flag_copy ? "copied" : "original");
	printk("sd: write_prot       %s\n", i->flag_write_protect ? "on" : "off");
	printk("sd: tmp write_prot   %s\n", i->flag_write_protect_temp ? "on" : "off");

}

static unsigned long totalblocks = 0;
ANTARES_INITCALL_LOW(sdcard)
{
	DEPENDS(spi);
	printk("sd: initialising card\n");
	char ret = sd_init(&sd);
	BUG_ON(ret != 0);
	printk("sd: Card ready. type: %d version: %d SHDC: %s\n",
	       sd.card_type, 
	       sd.version,
	       sd_is_shdc(&sd) ? "yes" : "no" );
	struct sd_info info;
	ret = sd_read_info(&sd, &info);
	sd_dump_info(&info);
	if (ret) {
		printk("sd: sd_read_info returned non-null value!\n");
		printk("sd: Information may be incomplete!");
	}
	if (info.capacity == 0) {
		printk("sd: Card reports zero capacity, asuming 2GiB\n");
		info.capacity = 2 * 1024 * 1024 * 1024;
	}
	totalblocks = info.capacity / 512;
	
}

const char msg[] = "|||||";

ANTARES_INITCALL_LOW(stlink)
{
	DEPENDS(sdcard);
	printk("stlink: Waiting for st-link terminal\n");
	stlinky_tx(&g_stlinky_term, msg, ARRAY_SIZE(msg)-1); /* Don't send terminating NULL byte */
	printk("stlink: terminal ready, initiating data dump\n");
}

static unsigned long cur_block;
#define BLKLEN 512


ANTARES_APP(dumper) 
{ 
	char tmp[BLKLEN];
	int ptr = 0;
	printk("Dumping block %lu/%lu \r", cur_block, totalblocks);
	int ret = sd_read(&sd,cur_block, tmp);
	if (ret) {
		printk("\n problem with block %lu: %d\n", cur_block, ret);
	}
	while (ptr != BLKLEN) {
		ptr += stlinky_tx(&g_stlinky_term, &tmp[ptr], BLKLEN - ptr);
	}

	GPIO_WriteBit(GPIOC, GPIO_Pin_8, (cur_block & 0x1));
	if (cur_block == totalblocks) 
		panic("All done, have fun");

	cur_block++;
}
