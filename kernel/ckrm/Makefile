#
# Makefile for CKRM
#

ifeq ($(CONFIG_CKRM),y)
    obj-y = ckrm.o ckrmutils.o ckrm_tasks_stub.o rbce/
endif	
    obj-$(CONFIG_CKRM_TYPE_TASKCLASS) 	+= ckrm_tc.o
    obj-$(CONFIG_CKRM_RES_NUMTASKS) 	+= ckrm_tasks.o
    obj-$(CONFIG_CKRM_TYPE_SOCKETCLASS) += ckrm_sockc.o
    obj-$(CONFIG_CKRM_RES_LISTENAQ) 	+= ckrm_listenaq.o
    obj-$(CONFIG_CKRM_CPU_SCHEDULE) 	+= ckrm_cpu_class.o ckrm_cpu_monitor.o
