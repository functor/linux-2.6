#
# Makefile for CKRM
#

ifeq ($(CONFIG_CKRM),y)
    obj-y = ckrm_events.o ckrm.o ckrmutils.o ckrm_numtasks_stub.o rbce/
endif	
obj-$(CONFIG_CKRM_TYPE_TASKCLASS)  += ckrm_tc.o
obj-$(CONFIG_CKRM_TYPE_SOCKETCLASS)  += ckrm_sockc.o
obj-$(CONFIG_CKRM_RES_NUMTASKS)     += ckrm_numtasks.o
obj-$(CONFIG_CKRM_RES_LISTENAQ)     += ckrm_listenaq.o
