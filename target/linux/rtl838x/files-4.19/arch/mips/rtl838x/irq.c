// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTL838X architecture specific IRQ handling
 *
 * Copyright  (C) 2020 B. Koblitz
 * based on the original BSP
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 * 
 */
 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <mach-rtl838x.h>
#include "prom.h"

extern struct rtl838x_soc_info soc_info;

static DEFINE_RAW_SPINLOCK(irq_lock);

extern irqreturn_t c0_compare_interrupt(int irq, void *dev_id);
unsigned int rtl838x_ictl_irq_dispatch1(void);
unsigned int rtl838x_ictl_irq_dispatch2(void);
unsigned int rtl838x_ictl_irq_dispatch3(void);
unsigned int rtl838x_ictl_irq_dispatch4(void);
unsigned int rtl838x_ictl_irq_dispatch5(void);


static struct irqaction irq_cascade1 = {        
	.handler = no_action,
	.name = "RTL838X IRQ cascade1",
};

static struct irqaction irq_cascade2 = {        
	.handler = no_action,
	.name = "RTL838X IRQ cascade2",
};

static struct irqaction irq_cascade3 = {        
	.handler = no_action,           
	.name = "RTL838X IRQ cascade3",
};

static struct irqaction irq_cascade4 = {        
	.handler = no_action,
	.name = "RTL838X IRQ cascade4",
};

static struct irqaction irq_cascade5 = {       
	.handler = no_action,
	.name = "RTL838X IRQ cascade5",
};

static void rtl838x_ictl_enable_irq(struct irq_data *i)
{   
	unsigned long flags;   

	raw_spin_lock_irqsave(&irq_lock, flags);
	rtl838x_w32(rtl838x_r32(GIMR) | (1 << ICTL_OFFSET(i->irq)), GIMR);
	raw_spin_unlock_irqrestore(&irq_lock, flags);
}

static unsigned int rtl838x_ictl_startup_irq(struct irq_data *i)
{   
	rtl838x_ictl_enable_irq(i);   
	return 0;
}

static void rtl838x_ictl_disable_irq(struct irq_data *i)
{   
	unsigned long flags;   

	raw_spin_lock_irqsave(&irq_lock, flags);   
	rtl838x_w32(rtl838x_r32(GIMR) & (~(1 << ICTL_OFFSET(i->irq))), GIMR);   
	raw_spin_unlock_irqrestore(&irq_lock, flags);
}


static void rtl838x_ictl_eoi_irq(struct irq_data *i)
{   
	unsigned long flags;   

	raw_spin_lock_irqsave(&irq_lock, flags); 
	rtl838x_w32(rtl838x_r32(GIMR) | (1 << ICTL_OFFSET(i->irq)), GIMR);
	raw_spin_unlock_irqrestore(&irq_lock, flags);
}

static struct irq_chip rtl838x_ictl_irq = { 
	.name = "RTL838X ICTL",
	.irq_startup = rtl838x_ictl_startup_irq,
	.irq_shutdown = rtl838x_ictl_disable_irq,
	.irq_enable = rtl838x_ictl_enable_irq,
	.irq_disable = rtl838x_ictl_disable_irq,
	.irq_ack = rtl838x_ictl_disable_irq,
	.irq_mask = rtl838x_ictl_disable_irq,
	.irq_unmask = rtl838x_ictl_enable_irq,
	.irq_eoi = rtl838x_ictl_eoi_irq,
};

static void rtl8380_wdt_phase1(void)
{
	rtl838x_w32(0x80000000, (volatile void *)(0xb8003154)); /*WDT PH1 IP clear*/
}


/* 
*   RTL8390/80/28 Interrupt Scheme
* 
*   Source       IRQ     EXT_IRQ    CPU INT 
*   --------   -------   ------   ------- 
*   UART0          31        39       IP3 
*   UART1          30        38       IP2 
*   TIMER0         29        37       IP6 
*   TIMER1         28        36       IP2 
*   OCPTO          27        35       IP2
*   HLXTO          26        34       IP2
*   SLXTO          25        33       IP2
*   NIC            24        32       IP5
*   GPIO_ABCD      23        31       IP5
*   GPIO_EFGH      22        30       IP5  ==> RTL8328 only
*   RTC            21        29       IP5  ==> RTL8328 only
*   SWCORE         20        28       IP4 
*/

unsigned int rtl838x_ictl_irq_dispatch1(void)
{       
	/* Identify shared IRQ  */     
	unsigned int extint_ip = rtl838x_r32(GIMR) & rtl838x_r32(GISR);  

	if (extint_ip & TC1_IP)         
		do_IRQ(RTL838X_TC1_EXT_IRQ);        
	else if (extint_ip & UART1_IP)          
		do_IRQ(RTL838X_UART1_EXT_IRQ);      
	else            
		spurious_interrupt();

	return IRQ_HANDLED;
}

unsigned int rtl838x_ictl_irq_dispatch2(void)
{                 
	do_IRQ(RTL838X_UART0_EXT_IRQ);      
	return IRQ_HANDLED;
}

unsigned int rtl838x_ictl_irq_dispatch3(void)
{       
	do_IRQ(RTL838X_SWCORE_EXT_IRQ);     
	return IRQ_HANDLED;
}

unsigned int rtl838x_ictl_irq_dispatch4(void)
{       
	/* Identify shared IRQ */
	unsigned int extint_ip = rtl838x_r32(GIMR) & rtl838x_r32(GISR);             

	if (extint_ip & NIC_IP)              
		do_IRQ(RTL838X_NIC_EXT_IRQ);        
	else if (extint_ip & GPIO_ABCD_IP)
		do_IRQ(RTL838X_GPIO_ABCD_EXT_IRQ);  
        else if ((extint_ip & GPIO_EFGH_IP) && (soc_info.family == RTL8328_FAMILY_ID))                
		do_IRQ(RTL838X_GPIO_EFGH_EXT_IRQ);
	else if (((soc_info.family == RTL8380_FAMILY_ID) 
		|| (soc_info.family == RTL8330_FAMILY_ID)) && (extint_ip & WDT_IP1_IP))
		{
			rtl8380_wdt_phase1();
			/* WDT Phase-1 */
			do_IRQ(RTL838X_WDT_IP1_IRQ);
		}
	else
		spurious_interrupt();

	return IRQ_HANDLED;
}

unsigned int rtl838x_ictl_irq_dispatch5(void)
{
	do_IRQ(RTL838X_TC0_EXT_IRQ);        
	return IRQ_HANDLED;
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending;   

	pending =  read_c0_cause() & read_c0_status() & ST0_IM;  

	if (pending & CAUSEF_IP7)
		c0_compare_interrupt(7, NULL);
	else if (pending & CAUSEF_IP6)               
		rtl838x_ictl_irq_dispatch5();       
	else if (pending & CAUSEF_IP5)          
		rtl838x_ictl_irq_dispatch4();       
	else if (pending & CAUSEF_IP4)          
		rtl838x_ictl_irq_dispatch3();       
	else if (pending & CAUSEF_IP3)          
		rtl838x_ictl_irq_dispatch2();       
	else if (pending & CAUSEF_IP2)          
		rtl838x_ictl_irq_dispatch1();
	else            
		spurious_interrupt();
}

static void __init rtl838x_ictl_irq_init(unsigned int irq_base)
{       
	int i;  

	for (i=0; i < RTL838X_IRQ_ICTL_NUM; i++) 
		irq_set_chip_and_handler(irq_base + i, &rtl838x_ictl_irq, handle_level_irq);

	setup_irq(RTL838X_ICTL1_IRQ, &irq_cascade1);        
	setup_irq(RTL838X_ICTL2_IRQ, &irq_cascade2);        
	setup_irq(RTL838X_ICTL3_IRQ, &irq_cascade3);        
	setup_irq(RTL838X_ICTL4_IRQ, &irq_cascade4);        
	setup_irq(RTL838X_ICTL5_IRQ, &irq_cascade5);
        
	/* Set GIMR, IRR */     
	rtl838x_w32(TC0_IE | UART0_IE, GIMR);                
	rtl838x_w32(IRR0_SETTING, IRR0);
	rtl838x_w32(IRR1_SETTING, IRR1);     
	if(soc_info.family  == RTL8328_FAMILY_ID)
		rtl838x_w32(IRR1_8328_SETTING, IRR1);
	else
		rtl838x_w32(IRR1_SETTING, IRR1);
	rtl838x_w32(IRR2_SETTING, IRR2);     
	rtl838x_w32(IRR3_SETTING, IRR3);
}
 
void __init arch_init_irq(void)
{
	/* do board-specific irq initialization */
	printk("In arch_init_irq, status register %x\n", read_c0_status());

	mips_cpu_irq_init();

	rtl838x_ictl_irq_init(RTL838X_IRQ_ICTL_BASE);

	printk("Done setting up IRQ: %x\n", read_c0_status());
}
