#include "s3c2410_old.h"

int nand_read_ll(unsigned char *buf, unsigned long start_addr, int size);

#define NAND_SECTOR_SIZE    512
#define NAND_BLOCK_MASK     (NAND_SECTOR_SIZE - 1)
static inline void delay (unsigned long loops)
{
    __asm__ volatile ("1:\n"
      "subs %0, %1, #1\n"
      "bne 1b":"=r" (loops):"0" (loops));
}

#define __REGb(x) (*(volatile unsigned char *)(x))
#define __REGw(x) (*(volatile unsigned short *)(x))
#define __REGi(x) (*(volatile unsigned int *)(x))
#define NF_BASE 0x4e000000

#define NFCONF  __REGi(NF_BASE + 0x0)
#define NFCONT  __REGi(NF_BASE + 0x4)
#define NFCMD  __REGb(NF_BASE + 0x8)
#define NFADDR  __REGb(NF_BASE + 0xc)
#define NFDATA  __REGb(NF_BASE + 0x10)
#define NFDATA16 __REGw(NF_BASE + 0x10)
#define NFSTAT  __REGb(NF_BASE + 0x20)
#define NFSTAT_BUSY 1
#define nand_select() (NFCONT &= ~(1 << 1))
#define nand_deselect() (NFCONT |= (1 << 1))
#define nand_clear_RnB() (NFSTAT |= (1 << 2))

/* Standard NAND flash commands */
#define NAND_CMD_READ0      0
#define NAND_CMD_READ1      1
#define NAND_CMD_RNDOUT     5
#define NAND_CMD_PAGEPROG   0x10
#define NAND_CMD_READOOB    0x50
#define NAND_CMD_ERASE1     0x60
#define NAND_CMD_STATUS     0x70
#define NAND_CMD_STATUS_MULTI   0x71
#define NAND_CMD_SEQIN      0x80
#define NAND_CMD_RNDIN      0x85
#define NAND_CMD_READID     0x90
#define NAND_CMD_ERASE2     0xd0
#define NAND_CMD_PARAM      0xec
#define NAND_CMD_RESET      0xff

#define NAND_CMD_LOCK       0x2a
#define NAND_CMD_UNLOCK1    0x23
#define NAND_CMD_UNLOCK2    0x24

/* Extended commands for large page devices */
#define NAND_CMD_READSTART  0x30
#define NAND_CMD_RNDOUTSTART    0xE0
#define NAND_CMD_CACHEDPROG 0x15

struct boot_nand_t
{
	int page_size;
	int block_size;
	int bad_block_offset;
};

unsigned short nand_read_id(void)
{
	unsigned short res = 0;
	NFCMD = 0x90; 
	NFADDR = 0;
	res = NFDATA;
	res = (res << 8) | NFDATA;
	return res;
}

void nand_wait(void)
{
	 int i;
	 while (!(NFSTAT & NFSTAT_BUSY))
	 	for (i=0; i<10; i++);
}

static int is_bad_block(struct boot_nand_t * nand, unsigned long i)
{
	unsigned char data;
	unsigned long page_num;
	unsigned int count = 0;
	nand_clear_RnB();
	if (nand->page_size == 512)
	{
		NFCMD = NAND_CMD_READOOB; /* 0x50 */
		NFADDR = nand->bad_block_offset & 0xf;
		NFADDR = (i >> 9) & 0xff;
		NFADDR = (i >> 17) & 0xff;
		NFADDR = (i >> 25) & 0xff;
	}
       	else if (nand->page_size == 2048) 
	{
 		page_num = i >> 11; /* addr / 2048 */
 		NFCMD = NAND_CMD_READ0;
 		NFADDR = nand->bad_block_offset & 0xff;
 		NFADDR = (nand->bad_block_offset >> 8) & 0xff;
 		NFADDR = page_num & 0xff;
 		NFADDR = (page_num >> 8) & 0xff;
 		NFADDR = (page_num >> 16) & 0xff;
 		NFCMD = NAND_CMD_READSTART;
 	} 
	else 
	{
  		return -1;
 	}

	while (!(NFSTAT & NFSTAT_BUSY))
	 	for (count=0; count<10; count++);

 	data = (NFDATA & 0xff);
 	if (data != 0xff)
  		return 1;
 	return 0;
}

int nand_read_page_ll(struct boot_nand_t * nand, unsigned char *buf, unsigned long addr)
{
	unsigned short *ptr16 = (unsigned short *)buf;
	unsigned int i, page_num;

	nand_clear_RnB();
	NFCMD = NAND_CMD_READ0;
	if (nand->page_size == 512) 
	{
		/* Write Address */
		NFADDR = addr & 0xff;
		NFADDR = (addr >> 9) & 0xff;
		NFADDR = (addr >> 17) & 0xff;
		NFADDR = (addr >> 25) & 0xff;
	} 
	else if (nand->page_size == 2048) 
	{
		page_num = addr >> 11; /* addr / 2048 */
		/* Write Address */
		NFADDR = 0;
		NFADDR = 0;
		NFADDR = page_num & 0xff;
		NFADDR = (page_num >> 8) & 0xff;
		NFADDR = (page_num >> 16) & 0xff;
		NFCMD = NAND_CMD_READSTART;
	 } 
	else 
	{
  		return -1;
	 }
	while (!(NFSTAT & NFSTAT_BUSY))
	 	for (i=0; i<10; i++);
	
 	for (i = 0; i < (nand->page_size>>1); i++) 
	{
  		*ptr16 = NFDATA16;
  		ptr16++;
 	}

 	return nand->page_size;
}


//int nand_read_page_ll(struct boot_nand_t * nand, unsigned char *buf, unsigned long addr);
int nand_read_ll(unsigned char *buf, unsigned long start_addr, int size)
{
	unsigned int i,j;
	unsigned short nand_id;
	struct boot_nand_t nand;
    	int *con_addr = 0x56000010;
    	int *data_addr = 0x56000014;
    	int con = 0x00015400;
    	int data = 0x000001C0;
	unsigned short *ptr16 = (unsigned short *)buf;
	unsigned int z,y,page_num;

	nand_select();
	nand_clear_RnB();

	for(i = 0; i < 10; i++);

//	nand_id = nand_read_id();

	nand.page_size = 2048;
	nand.block_size = 128 * 1024;
	nand.bad_block_offset = nand.page_size;
	/* Samsung K91208 or Hynix HY27US08121A */
	/*if(nand_id == 0xec76 || nand_id == 0xad76)
	{
		nand.page_size = 512;
		//nand.block_size = 16 * 1024;
		nand.block_size = 0x3e00;
		nand.bad_block_offset = 5;
	}
	else if (nand_id == 0xecf1 || nand_id == 0xecda || nand_id == 0xecd3)
	{
		nand.page_size = 2048;
		nand.block_size = 128 * 1024;
		nand.bad_block_offset = nand.page_size;
	}
	else
	{
		return -1;
	}

	if((start_addr & (nand.block_size - 1)) || (size & ((nand.block_size - 1))))
		return -1;*/

	//*con_addr = con;
	//*data_addr = 0x00000160;

	//for(i = start_addr; i < (start_addr + size);)
	for(i = 0; i < 0x60000;)
	{
		#if 0
		if((i & (nand.block_size - 1)) == 0)
		{
			if(is_bad_block(&nand, i) || is_bad_block(&nand, i+nand.page_size))
			{
				i += nand.block_size;
				size += nand.block_size;
				continue;
			}
		}
		#endif	
		//j = nand_read_page_ll(&nand, buf, i);
		nand_clear_RnB();
		NFCMD = NAND_CMD_READ0;
	 
		page_num = i >> 11; /* addr / 2048 */
		/* Write Address */
		NFADDR = 0;
		NFADDR = 0;
		NFADDR = page_num & 0xff;
		NFADDR = (page_num >> 8) & 0xff;
		NFADDR = (page_num >> 16) & 0xff;
		NFCMD = NAND_CMD_READSTART;
	 
		while (!(NFSTAT & NFSTAT_BUSY))
	 		for (y=0; y<10; y++);
	
 		for (z = 0; z < (nand.page_size>>1); z++) 
		{
  			*ptr16 = NFDATA16;
  			ptr16++;
 		}

 		j = nand.page_size;

		i += j;
		buf += j;
		ptr16 = (unsigned short *) buf;
	}

	nand_deselect();

	return 0;
}

/* S3C2440: Mpll = (2*m * Fin) / (p * 2^s), UPLL = (m * Fin) / (p * 2^s)
 * m = M (the value for divider M)+ 8, p = P (the value for divider P) + 2
 */
#define S3C2440_MPLL_400MHZ     ((0x5c<<12)|(0x01<<4)|(0x01))
//#define S3C2440_MPLL_400MHZ     ((0x7f<<12)|(0x02<<4)|(0x01))
#define S3C2440_MPLL_200MHZ     ((0x5c<<12)|(0x01<<4)|(0x02))
#define S3C2440_MPLL_100MHZ     ((0x5c<<12)|(0x01<<4)|(0x03))
#define S3C2440_UPLL_96MHZ      ((0x38<<12)|(0x02<<4)|(0x01))
#define S3C2440_UPLL_48MHZ      ((0x38<<12)|(0x02<<4)|(0x02))
#define S3C2440_CLKDIV          (0x05) // | (1<<3))    /* FCLK:HCLK:PCLK = 1:4:8, UCLK = UPLL/2 */
#define S3C2440_CLKDIV188       0x04    /* FCLK:HCLK:PCLK = 1:8:8 */
#define S3C2440_CAMDIVN188      ((0<<8)|(1<<9)) /* FCLK:HCLK:PCLK = 1:8:8 */

/* S3C2410: Mpll,Upll = (m * Fin) / (p * 2^s)
 * m = M (the value for divider M)+ 8, p = P (the value for divider P) + 2
 */
#define S3C2410_MPLL_200MHZ     ((0x5c<<12)|(0x04<<4)|(0x00))
#define S3C2410_UPLL_48MHZ      ((0x28<<12)|(0x01<<4)|(0x02))
#define S3C2410_CLKDIV          0x03    /* FCLK:HCLK:PCLK = 1:2:4 */
void clock_init(void)
{
	S3C24X0_CLOCK_POWER *clk_power = (S3C24X0_CLOCK_POWER *)0x4C000000;

        /* FCLK:HCLK:PCLK = 1:4:8 */
        clk_power->CLKDIVN = S3C2440_CLKDIV;

        /* change to asynchronous bus mod */
        __asm__(    "mrc    p15, 0, r1, c1, c0, 0\n"    /* read ctrl register   */  
                    "orr    r1, r1, #0xc0000000\n"      /* Asynchronous         */  
                    "mcr    p15, 0, r1, c1, c0, 0\n"    /* write ctrl register  */  
                    :::"r1"
                    );

        /* to reduce PLL lock time, adjust the LOCKTIME register */
        clk_power->LOCKTIME = 0xFFFFFFFF;
        //clk_power->LOCKTIME = 0x00FFFFFF;

        /* configure UPLL */
        clk_power->UPLLCON = S3C2440_UPLL_48MHZ;

        /* some delay between MPLL and UPLL */
        delay (4000);

        /* configure MPLL */
        clk_power->MPLLCON = S3C2440_MPLL_400MHZ;

        /* some delay between MPLL and UPLL */
        delay (8000);
}
