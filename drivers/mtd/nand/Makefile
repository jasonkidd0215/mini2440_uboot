# SPDX-License-Identifier: GPL-2.0+

ifeq ($(CONFIG_SPL_BUILD)$(CONFIG_TPL_BUILD),)
nandcore-objs := core.o bbt.o
obj-$(CONFIG_MTD_NAND_CORE) += nandcore.o
obj-$(CONFIG_MTD_RAW_NAND) += raw/
obj-$(CONFIG_MTD_SPI_NAND) += spi/
obj-$(CONFIG_MTD_NAND_S3C2410_NAND) += s3c2410_nand.o
else
obj-$(CONFIG_$(SPL_TPL_)NAND_SUPPORT) += raw/
endif
