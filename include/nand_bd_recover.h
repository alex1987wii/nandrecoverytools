/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * Author: Jarick Huang
 *
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef __NADN_BD_RECOVER_H__ #define __NADN_BD_RECOVER_H__ 


//#define NAND_DEBUG                                     
                                                           
#ifdef NAND_DEBUG                                        
    #define NAND_DEBUG(fmt, args...)  printf(fmt, ## args)   
#else                                                      
    #define NAND_DEBUG(fmt, args...)                         
#endif

extern pthread_mutex_t mutex_bb_recover_num;
extern uint32_t g_bb_recover_num;
/*
 * get_bd_number
 * Get the number of badblock in a specify partition.
 * @partiton_name: name by given partiton, such as "rootfs" "user data"
 * use cammand "cat /proc/mtd" to get avaliable partition name 
 */

int get_bd_number(unsigned char *partition_name);



/*
 * nand_bad_recover
 * Find out all badblock in a specify partition, trying to recover it.
 * @partiton_name: name by given partiton, such as "rootfs" "user data"
 * use cammand "cat /proc/mtd" to get avaliable partition name 
 */
int nand_bd_recover(unsigned char *partition_name);


#ifdef __cplusplus
}
#endif

#endif /* __NADN_BD_RECOVER_H__ */
