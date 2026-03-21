# Project Name
TARGET = SynchedGranularLooper

# Sources
CPP_SOURCES = SynchedGranularLooper.cpp

# Library Locations
LIBDAISY_DIR = ../../../libDaisy/
DAISYSP_DIR = ../../../DaisySP/

# Enable FatFS for SD card support
USE_FATFS = 1

# Optimize for size (FatFS + app needs to fit in 128KB FLASH)
OPT = -Os

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
