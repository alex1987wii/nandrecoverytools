# ===============================================================
#            Environment variables definition
# ===============================================================
#
# The global definition of our project.
#
# OBJECT:  The execute environment of this making, valid 
#          value can be: 
#            @linux: ad6900 ARM, 
#            @windows: PC with windows system
#
#
# UTIL_VERSION: Build version, such as "v0.1"
#

#OBJECT=windows
OBJECT = linux
UTIL_VERSION = v0.1


ifeq ($(OBJECT),windows) 
    # compile flag and link flag definitions
	CC=gcc
	RC=windres
    CFLAGS  = -DFT_WINDOW -DNT_WINDOW -O2 -I ./include -DSF_VERSION=\"$(UTIL_VERSION)\"
    LDFLAGS = -lcomctl32 -lsetupapi -lWs2_32 -lshlwapi

endif


ifeq ($(OBJECT),linux) 
    ifdef CROSS_COMPILE
            CC=$(CROSS_COMPILE)gcc -static 
            CFLAGS=-O2 -Wall -DNT_LINUX -DVERSION=$(UTIL_VERSION) -I ./include
            LDFLAGS=-pthread
    else
            CC=/opt/ad6900/arm-compiler/bin/arm-linux-gcc -static 
            CFLAGS=-O2 -Wall -DNT_LINUX -DVERSION=$(UTIL_VERSION) -I ./include
            LDFLAGS=-pthread
    endif

endif


####### Files

LINUX_COMMON_OBJS += ./lib/libmtd.o
LINUX_COMMON_OBJS += ./lib/libmtd_legacy.o

LINUX_TEST_DUMP_OBJS += ./tools/nanddump.o
LINUX_TEST_CHECK_BB_OBJS = ./tools/nand_check_bad_block.o
LINUX_TEST_SET_BB_OBJS = ./tools/nand_set_bad_block.o
LINUX_TEST_SET_BB_HALF_PARTITION_OBJS = ./tools/nand_set_badblock_half_partition.o
LINUX_TEST_MANUL_RECOVER_BB_OBJS = ./tools/nand_manul_recover_fake_badblock.o
LINUX_TEST_RECOVER_BB_OBJS = ./tools/nand_recover_fake_badblock.o
LINUX_TEST_RECOVER_BB_ROOTFS_OBJS = ./tools/nand_recover_fake_badblock_rootfs.o

SHARE_OBJS += ./lib/network_library.o

LINUX_TOOL_BB_OBJS += linux_bb_recover.o
LINUX_TOOL_BB_OBJS += ./lib/nand_bd_recover.o

#add source files for linux tools here

#****Debug & test only************#
#LINUX_TOOLS += nanddump
#LINUX_TOOLS += nand_set_bad_block
#LINUX_TOOLS += nand_manul_recover_fake_badblock
#LINUX_TOOLS += nand_check_bad_block
#LINUX_TOOLS += nand_recover_fake_badblock 
#LINUX_TOOLS += nand_recover_fake_badblock_rootfs 
LINUX_TOOLS += nand_set_badblock_half_partition
#********************************************#

LINUX_TOOLS  += linux_bb_recover_$(UTIL_VERSION)


#add source files for windows tool here
WINDOWS_TOOLS= windows_client_$(UTIL_VERSION).exe
WINDOWS_GUI_TOOLS=NandflashRecoveryTool_$(UTIL_VERSION).exe
WIN_LIB_NET= libnetwork.dll
Win_OBJ= windows/windows_client.o 
WIN_GUI_OBJ=windows/NandflashRecoveryTool.o windows/NandRecoveryToolrc.o
WIN_LIB_NET_OBJS= ./lib/network_library.o
#WINDOWS_OBJ+= windows_tool_obj

ifeq ($(OBJECT),linux) 

    all: clean linux_objs $(LINUX_TOOLS) install

else

    all: clean windows_tool_obj win_lib_net_obj $(WIN_LIB_NET) $(WINDOWS_TOOLS) $(WINDOWS_GUI_TOOLS) install

endif

linux_objs: $(LINUX_COMMON_OBJS) \
	$(LINUX_TEST_DUMP_OBJS) $(LINUX_TEST_CHECK_BB_OBJS) \
	$(LINUX_TEST_SET_BB_OBJS) $(LINUX_TEST_SET_BB_HALF_PARTITION_OBJS) \
    $(LINUX_TOOL_BB_OBJS) $(SHARE_OBJS)

nanddump:
	$(CC) $(LINUX_TEST_DUMP_OBJS) $(LINUX_COMMON_OBJS) $(LDFLAGS) -o nanddump 

nand_set_bad_block:
	$(CC) $(LINUX_TEST_SET_BB_OBJS) $(LINUX_COMMON_OBJS) $(LDFLAGS) -o  nand_set_bad_block

nand_set_badblock_half_partition :
	$(CC) $(LINUX_TEST_SET_BB_HALF_PARTITION_OBJS) $(LINUX_COMMON_OBJS) $(LDFLAGS) -o nand_set_badblock_half_partition

linux_bb_recover_$(UTIL_VERSION) :
	$(CC) $(LINUX_TOOL_BB_OBJS) $(SHARE_OBJS) $(LINUX_COMMON_OBJS) $(LDFLAGS) -o linux_bb_recover_$(UTIL_VERSION)

$(WIN_LIB_NET):
	$(CC) -shared $(WIN_LIB_NET_OBJS) -lWs2_32 -o ./lib/$(WIN_LIB_NET) 

$(WINDOWS_TOOLS):
	$(CC) $(Win_OBJ) $(LDFLAGS) -L ./lib -lnetwork -lupgrade -o  $(WINDOWS_TOOLS) 

$(WINDOWS_GUI_TOOLS):
	$(CC) $(WIN_GUI_OBJ) $(LDFLAGS) -mwindows -L ./lib -lnetwork -lupgrade -o $(WINDOWS_GUI_TOOLS)



windows_tool_obj: $(Win_OBJ) $(WIN_GUI_OBJ)
win_lib_net_obj: ./lib/network_library.o

windows/NandflashRecoveryTool.o:windows/NandRecoveryTool.c ./include/*.h
	$(CC) -c -o $@ $< $(CFLAGS)
windows/NandRecoveryToolrc.o:./windows/NandRecoveryTool.rc ./include/NandRecoveryTool.h
	$(RC) -o  $@ $< -I./include
	
%.o: %.c ./include/* ./include/mtd/* ./lib/* ./tools/* ./windows/*

install:
	mkdir ./output/ -p
ifeq ($(OBJECT),linux) 
	mv $(LINUX_TOOLS) ./output
else
	mv $(WINDOWS_TOOLS) $(WINDOWS_GUI_TOOLS) ./lib/$(WIN_LIB_NET) ./output
	cp ./lib/libupgrade.dll ./reset_tg.sh ./output
endif

clean:
ifeq ($(OBJECT),linux) 
	rm -f $(LINUX_TOOLS) *.o ./lib/*.o ./tools/*.o
else
	rm -f $(WINDOWS_TOOLS)  $(WINDOWS_GUI_TOOLS) *.o ./lib/*.o ./windows/*.o ./lib/$(WIN_LIB_NET) 
endif

