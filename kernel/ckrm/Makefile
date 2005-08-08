#
# Makefile for CKRM
#

ifeq ($(CONFIG_CKRM),y)
    obj-y = ckrm_events.o ckrm.o ckrmutils.o ckrm_numtasks_stub.o rbce/
endif	

obj-$(CONFIG_CKRM_TYPE_TASKCLASS) += ckrm_tc.o
obj-$(CONFIG_CKRM_TYPE_SOCKETCLASS) += ckrm_sockc.o
obj-$(CONFIG_CKRM_RES_NUMTASKS) += ckrm_numtasks.o
obj-$(CONFIG_CKRM_RES_LISTENAQ) += ckrm_listenaq.o
obj-$(CONFIG_CKRM_CPU_SCHEDULE) += ckrm_cpu_class.o ckrm_cpu_monitor.o
obj-$(CONFIG_CKRM_RES_MEM) += ckrm_memcore.o ckrm_memctlr.o
obj-$(CONFIG_CKRM_RES_NULL) += ckrm_null_class.o
