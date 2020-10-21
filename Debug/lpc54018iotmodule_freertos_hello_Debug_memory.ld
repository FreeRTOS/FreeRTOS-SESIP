/*
 * GENERATED FILE - DO NOT EDIT
 * Copyright (c) 2008-2013 Code Red Technologies Ltd,
 * Copyright 2015, 2018-2019 NXP
 * (c) NXP Semiconductors 2013-2020
 * Generated linker script file for LPC54018
 * Created from memory.ldt by FMCreateLinkMemory
 * Using Freemarker v2.3.23
 * MCUXpresso IDE v11.2.0 [Build 4120] [2020-07-09] on Oct 21, 2020 9:58:02 AM
 */

MEMORY
{
  /* Define each memory region */
  BOARD_FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 0x1000000 /* 16M bytes (alias Flash) */  
  SRAMX (rwx) : ORIGIN = 0x0, LENGTH = 0x30000 /* 192K bytes (alias RAM) */  
  SRAM_0_1_2_3 (rwx) : ORIGIN = 0x20000000, LENGTH = 0x28000 /* 160K bytes (alias RAM2) */  
  USB_RAM (rwx) : ORIGIN = 0x40100000, LENGTH = 0x2000 /* 8K bytes (alias RAM3) */  
}

  /* Define a symbol for the top of each memory region */
  __base_BOARD_FLASH = 0x10000000  ; /* BOARD_FLASH */  
  __base_Flash = 0x10000000 ; /* Flash */  
  __top_BOARD_FLASH = 0x10000000 + 0x1000000 ; /* 16M bytes */  
  __top_Flash = 0x10000000 + 0x1000000 ; /* 16M bytes */  
  __base_SRAMX = 0x0  ; /* SRAMX */  
  __base_RAM = 0x0 ; /* RAM */  
  __top_SRAMX = 0x0 + 0x30000 ; /* 192K bytes */  
  __top_RAM = 0x0 + 0x30000 ; /* 192K bytes */  
  __base_SRAM_0_1_2_3 = 0x20000000  ; /* SRAM_0_1_2_3 */  
  __base_RAM2 = 0x20000000 ; /* RAM2 */  
  __top_SRAM_0_1_2_3 = 0x20000000 + 0x28000 ; /* 160K bytes */  
  __top_RAM2 = 0x20000000 + 0x28000 ; /* 160K bytes */  
  __base_USB_RAM = 0x40100000  ; /* USB_RAM */  
  __base_RAM3 = 0x40100000 ; /* RAM3 */  
  __top_USB_RAM = 0x40100000 + 0x2000 ; /* 8K bytes */  
  __top_RAM3 = 0x40100000 + 0x2000 ; /* 8K bytes */  
