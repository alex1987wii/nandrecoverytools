一.編譯：
編譯Linux端的工具：修改Makefile, OBJECT = linux
編譯Windows端的工具和dll文件：修改Makefile, OBJECT = windows
命令行輸入make 即可完成編譯,生成的相關文件在output目錄下。

二.使用方法：

1.連接U3/U3 2nd 設備到PC（先嘗試PC telnet到target判斷設備連接是否正常）

2.保證libnetwork.dll  libupgrade.dll  linux_bb_recover_v0.1 windows_client_v0.1.exe 在同一路徑下。運行windows_client_v0.1.exe

3.等待一段時間後（20s左右),若出現以下界面則表示修復工具已經正常運行：

**********  Please choose the command  ***********
* a. CHECK_BB_ROOT_PATITION                      *
* b. CHECK_BB_USER_PATITON                       *
* c. CHECK_NETWORK_STATU                         *
* d. RECOVER_BB_ROOT_PATITION                    *
* e. RECOVER_BB_USER_PATITION                    *
* z. Reboot target & Quit                        *
**************************************************
Please input:

a爲檢測ROOT分區壞塊數量，b爲檢測USER DATA分區壞塊數量，c爲檢測PC與Target的連接狀態（for debug）
d爲修復ROOT分區的壞塊，e爲修復USER DATA分區的壞塊。
注：修復過程可能會導致對應分區的數據丟失，請慎重！另外修復完成後需要重啓系統才會重新被修復的壞塊。




三. nand_set_badblock_half_partition 僞壞塊制造工具:
nand_set_badblock_half_partition是運行在Target上的測試。用來制造僞壞塊。一般只要在user data 分區下制造僞壞塊即可。千萬不要動除了user data和rootfs分區，否則會丟失calibration分區的數據。如果在rootfs分區上制造了僞壞塊，那麼設備重啓之後會無法進入linxu系統，需要用spl模式重新燒寫linux（kernel）分區了。
[root@u3 /tmp]# ./nand_set_badblock_half_partition 
Caution! This tool use to make half of the given Nandflash partition as bad block, it will corrupt the partition's data! 
usage: nand_set_badblock_half_partition [OPTIONS] <device> e.g. nand_set_badblock_half_partition /dev/mtd5

./nand_set_badblock_half_partiton  /dev/mtd5     //user data 分區
./nand_set_badblock_half_partiton  /dev/mtd4     //rootfs 分區

對應分區設備號如下：
[root@u3 /tmp]# cat /proc/mtd 
dev:    size   erasesize  name
mtd0: 00020000 00020000 "Boot Agent"
mtd1: 003e0000 00020000 "bootloader"
mtd2: 00400000 00020000 "kernel"
mtd3: 00400000 00020000 "config data"
mtd4: 04000000 00020000 "Root file system"
mtd5: 03400000 00020000 "user data"
[root@u3 /tmp]# 

四. 提供給AP的relase包：
給AP的release包中需要包括以下文件：
h文件：頂層目錄下include中對應的error_code.h  network_library.h  UpgradeLib.h
dll文件：output下的libnetwork.dll libupgrade.dll
執行文件：reset_tg.sh linux_bb_recover_vX.x (X.x为版本號) nand_set_badblock_half_partition windows_client_vX.x.exe (X.x为版本號)
Bad_Block_recover_tool_vX.x.tar.bz2是一個release示例，通過替換裏面的文件即可。同時修改Bad_Block_recover_tool_vX.x.tar.bz2裏面readme.txt的內容。以及Bad_Block_recover_tool_vX.x.tar.bz2中Makefile 裏面的UTIL_VERSION。

