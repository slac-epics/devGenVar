TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC    += devGenVar

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install devGenVar.dbd into <top>/dbd
DBD            += devGenVar.dbd
INC            += devGenVar.h

# specify all source files to be compiled and added to the library
devGenVar_SRCS += devGenVar.c test.c

devGenVar_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC       += genVarTest
DBD            += genVarTest.dbd

genVarTest_DBD += base.dbd
genVarTest_DBD += devGenVar.dbd


genVarTest_SRCS += genVarTest_registerRecordDeviceDriver.cpp
genVarTest_SRCS_DEFAULT += genVarTestMain.cpp
genVarTest_SRCS_RTEMS   += -nil-

genVarTest_LIBS += devGenVar
genVarTest_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

