#
_XDCBUILDCOUNT = 
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = /home/spendice/ti/simplelink_cc2640r2_sdk_4_40_00_10/source;/home/spendice/ti/simplelink_cc2640r2_sdk_4_40_00_10/kernel/tirtos/packages;/home/spendice/ti/simplelink_cc2640r2_sdk_4_40_00_10/source/ti/blestack
override XDCROOT = /home/spendice/ti/ccs1281/xdctools_3_62_01_16_core
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = /home/spendice/ti/simplelink_cc2640r2_sdk_4_40_00_10/source;/home/spendice/ti/simplelink_cc2640r2_sdk_4_40_00_10/kernel/tirtos/packages;/home/spendice/ti/simplelink_cc2640r2_sdk_4_40_00_10/source/ti/blestack;/home/spendice/ti/ccs1281/xdctools_3_62_01_16_core/packages;..
HOSTOS = Linux
endif
