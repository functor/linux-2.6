################################################################################
#
# Makefile for building Linux Broadcom Gigabit ethernet driver as a module.
#
################################################################################

obj-$(CONFIG_BCM5700) = bcm5700.o

bcm5700-objs    := b57um.o b57proc.o tigon3.o autoneg.o 5701rls.o tcp_seg.o b57diag.o

EXTRA_CFLAGS = -DDBG=0 -DT3_JUMBO_RCV_RCB_ENTRY_COUNT=256 -DNICE_SUPPORT -DPCIX_TARGET_WORKAROUND=1 -DINCLUDE_TBI_SUPPORT -DINCLUDE_5701_AX_FIX=1 $(NETDUMP_CFLAG)
ifeq ($(ARCH),x86_64)
  CFLAGS+=-mno-red-zone -mcmodel=kernel -pipe -finline-limit=2000
endif

ifeq ($(ARCH),ia64)
  CFLAGS+=-pipe -ffixed-r13 -mfixed-range=f10-f15,f32-f127 -falign-functions=32
endif

ifeq ($(ARCH),ppc64)
  CFLAGS+=-m64 
  CFLAGS+=-fno-strict-aliasing -fno-common -fomit-frame-pointer -msoft-float -pipe -mminimal-toc -fno-builtin 
endif

ifdef DBG
  CFLAGS += -DTIGON3_DEBUG=2
endif

ifdef SMALL
  CFLAGS += -Os -DBCM_SMALL_DRV=1
else
  CFLAGS += -O2
endif
