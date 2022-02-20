/*
* Copyright (c) 2021 by picard(aka 3141card)
* This file is released under the GPLv2.
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ppu-lv2.h>


#define LV2_START    0x8000000000000000ULL
#define LV2_END      0x8000000000800000ULL
#define UFS2_MAGIC   0x19540119UL


/***********************************************************************
*	lv2_peek
***********************************************************************/
static uint64_t lv2_peek(uint64_t addr)
{
	lv2syscall1(6, addr);
	return_to_user_prog(uint64_t);
}
/***********************************************************************
*	lv2_poke
***********************************************************************/
static uint64_t lv2_poke(uint64_t addr, uint64_t value)
{
	lv2syscall2(7, addr, value);
	return_to_user_prog(uint64_t); 
}

/***********************************************************************
* lv2 peek 32 bit
***********************************************************************/
uint32_t lv2_peek_32(uint64_t addr)
{
	return (uint32_t)(lv2_peek(addr) >>32);
}

/***********************************************************************
* lv2 poke 32 bit
***********************************************************************/
void lv2_poke_32(uint64_t addr, uint32_t value)
{
	uint64_t value_org = lv2_peek(addr);
	lv2_poke(addr, (value_org & 0xFFFFFFFFULL) | (((uint64_t)value) <<32));
}

/***********************************************************************
* ring buzzer 
***********************************************************************/
static void buzzer(uint8_t mode)
{
	uint16_t param = 0;
	
	switch(mode){
		case 1: param = 0x0006; break;		// single beep
		case 2: param = 0x0036; break;		// double beep
		case 3: param = 0x01B6; break;		// triple beep
		case 4: param = 0x0FFF; break;		// continuous beep
	}
	
	lv2syscall3(392, 0x1007, 0xA, param);
}

/***********************************************************************
* search ufs superblock 
***********************************************************************/
static uint64_t get_ufs_sb_addr(void)
{
	uint64_t value = 0, addr = (LV2_END - 0xA8);
  
	while(addr > LV2_START) {
		if((uint32_t)(value = lv2_peek(addr)) == UFS2_MAGIC)
			return (uint64_t)(addr - 0x558);
		addr -= 0x100;
	}
	
	return -1;
}

/***********************************************************************
* main
***********************************************************************/
int32_t main()
{
  uint64_t sb_addr = get_ufs_sb_addr();
  uint32_t minfree = lv2_peek_32(sb_addr + 0x3C);
  uint32_t optim   = lv2_peek_32(sb_addr + 0x80);
  
  if(sb_addr == -1) {
		buzzer(3);  // fail
		return 0;
	}
  
  // toggle: original / new
  if((minfree == 8) && (optim == 0)) {
		minfree = 1;
		optim   = 1;
	}
  else {
		minfree = 8;
    optim   = 0;
	}
  
  // write patch
  lv2_poke_32(sb_addr + 0x3C, minfree);
  lv2_poke_32(sb_addr + 0x80, optim);
  
  buzzer(1);  // success
  
  return 0;
}

