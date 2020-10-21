/*
 * GENERATED FILE - DO NOT EDIT
 * Copyright (c) 2008-2013 Code Red Technologies Ltd,
 * Copyright 2015, 2018-2019 NXP
 * (c) NXP Semiconductors 2013-2020
 * Generated linker script file for LPC54018
 * Created from linkscript.ldt by FMCreateLinkLibraries
 * Using Freemarker v2.3.23
 * MCUXpresso IDE v11.2.0 [Build 4120] [2020-07-09] on Oct 21, 2020 9:58:02 AM
 */

INCLUDE "lpc54018iotmodule_freertos_hello_Debug_library.ld"
INCLUDE "lpc54018iotmodule_freertos_hello_Debug_memory.ld"

ENTRY(ResetISR)

SECTIONS
{
     /* MAIN TEXT SECTION */
    .text : ALIGN(4)
    {
        FILL(0xff)
        __vectors_start__ = ABSOLUTE(.) ;
        KEEP(*(.isr_vector))
        /* Global Section Table */
        . = ALIGN(4) ;
        __section_table_start = .;
        __data_section_table = .;
        LONG((LOADADDR(.data_RAM) - LOADADDR(.text)) + __base_SRAMX);
        LONG(    ADDR(.data_RAM));
        LONG(  SIZEOF(.data_RAM));
        LONG((LOADADDR(.data) - LOADADDR(.text)) + __base_SRAMX);
        LONG(    ADDR(.data));
        LONG(  SIZEOF(.data));
        LONG((LOADADDR(.data_RAM3) - LOADADDR(.text)) + __base_SRAMX);
        LONG(    ADDR(.data_RAM3));
        LONG(  SIZEOF(.data_RAM3));
        __data_section_table_end = .;
        __bss_section_table = .;
        LONG(    ADDR(.bss_RAM));
        LONG(  SIZEOF(.bss_RAM));
        LONG(    ADDR(.bss));
        LONG(  SIZEOF(.bss));
        LONG(    ADDR(.bss_RAM3));
        LONG(  SIZEOF(.bss_RAM3));
        __bss_section_table_end = .;
        __section_table_end = . ;
        /* End of Global Section Table */

        *(.after_vectors*)

    } > SRAMX AT> BOARD_FLASH

    .text : ALIGN(4)
    {
       *(.text*)
       KEEP(*freertos*/tasks.o(.rodata*)) /* FreeRTOS Debug Config */
       *(.rodata .rodata.* .constdata .constdata.*)
       . = ALIGN(4);
    } > SRAMX AT> BOARD_FLASH
    /*
     * for exception handling/unwind - some Newlib functions (in common
     * with C++ and STDC++) use this.
     */
    .ARM.extab : ALIGN(4)
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > SRAMX AT> BOARD_FLASH

    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > SRAMX AT> BOARD_FLASH
 
    _etext = .;
        
 
    /* USB_RAM */
    .m_usb_data (NOLOAD) :
    {
        *(m_usb_global)
    } > USB_RAM AT> USB_RAM
    /* DATA section for SRAMX */

    .data_RAM : ALIGN_WITH_INPUT
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM = .) ;
        PROVIDE(__start_data_SRAMX = .) ;
        *(.ramfunc.$RAM)
        *(.ramfunc.$SRAMX)
        *(.data.$RAM)
        *(.data.$SRAMX)
        *(.data.$RAM.*)
        *(.data.$SRAMX.*)
        . = ALIGN(4) ;
        PROVIDE(__end_data_RAM = .) ;
        PROVIDE(__end_data_SRAMX = .) ;
     } > SRAMX AT>BOARD_FLASH

    /* DATA section for USB_RAM */

    .data_RAM3 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM3 = .) ;
        PROVIDE(__start_data_USB_RAM = .) ;
        *(.ramfunc.$RAM3)
        *(.ramfunc.$USB_RAM)
        *(.data.$RAM3)
        *(.data.$USB_RAM)
        *(.data.$RAM3.*)
        *(.data.$USB_RAM.*)
        . = ALIGN(4) ;
        PROVIDE(__end_data_RAM3 = .) ;
        PROVIDE(__end_data_USB_RAM = .) ;
     } > USB_RAM AT>BOARD_FLASH

    /* MAIN DATA SECTION */
    .uninit_RESERVED (NOLOAD) : ALIGN(4)
    {
        _start_uninit_RESERVED = .;
        KEEP(*(.bss.$RESERVED*))
       . = ALIGN(4) ;
        _end_uninit_RESERVED = .;
    } > SRAM_0_1_2_3 AT> SRAM_0_1_2_3

    /* Main DATA section (SRAM_0_1_2_3) */
    .data : ALIGN(4)
    {
       FILL(0xff)
       _data = . ;
       PROVIDE(__start_data_RAM2 = .) ;
       PROVIDE(__start_data_SRAM_0_1_2_3 = .) ;
       *(vtable)
       *(.ramfunc*)
       KEEP(*(CodeQuickAccess))
       KEEP(*(DataQuickAccess))
       *(RamFunction)
       *(.data*)
       . = ALIGN(4) ;
       _edata = . ;
       PROVIDE(__end_data_RAM2 = .) ;
       PROVIDE(__end_data_SRAM_0_1_2_3 = .) ;
    } > SRAM_0_1_2_3 AT>BOARD_FLASH

    /* BSS section for SRAMX */
    .bss_RAM : ALIGN(4)
    {
       PROVIDE(__start_bss_RAM = .) ;
       PROVIDE(__start_bss_SRAMX = .) ;
       *(.bss.$RAM)
       *(.bss.$SRAMX)
       *(.bss.$RAM.*)
       *(.bss.$SRAMX.*)
       . = ALIGN (. != 0 ? 4 : 1) ; /* avoid empty segment */
       PROVIDE(__end_bss_RAM = .) ;
       PROVIDE(__end_bss_SRAMX = .) ;
    } > SRAMX AT> SRAMX

    /* BSS section for USB_RAM */
    .bss_RAM3 : ALIGN(4)
    {
       PROVIDE(__start_bss_RAM3 = .) ;
       PROVIDE(__start_bss_USB_RAM = .) ;
       *(.bss.$RAM3)
       *(.bss.$USB_RAM)
       *(.bss.$RAM3.*)
       *(.bss.$USB_RAM.*)
       . = ALIGN (. != 0 ? 4 : 1) ; /* avoid empty segment */
       PROVIDE(__end_bss_RAM3 = .) ;
       PROVIDE(__end_bss_USB_RAM = .) ;
    } > USB_RAM AT> USB_RAM

    /* MAIN BSS SECTION */
    .bss : ALIGN(4)
    {
        _bss = .;
        PROVIDE(__start_bss_RAM2 = .) ;
        PROVIDE(__start_bss_SRAM_0_1_2_3 = .) ;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4) ;
        _ebss = .;
        PROVIDE(__end_bss_RAM2 = .) ;
        PROVIDE(__end_bss_SRAM_0_1_2_3 = .) ;
        PROVIDE(end = .);
    } > SRAM_0_1_2_3 AT> SRAM_0_1_2_3

    /* NOINIT section for SRAMX */
    .noinit_RAM (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_noinit_RAM = .) ;
       PROVIDE(__start_noinit_SRAMX = .) ;
       *(.noinit.$RAM)
       *(.noinit.$SRAMX)
       *(.noinit.$RAM.*)
       *(.noinit.$SRAMX.*)
       . = ALIGN(4) ;
       PROVIDE(__end_noinit_RAM = .) ;
       PROVIDE(__end_noinit_SRAMX = .) ;
    } > SRAMX AT> SRAMX

    /* NOINIT section for USB_RAM */
    .noinit_RAM3 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_noinit_RAM3 = .) ;
       PROVIDE(__start_noinit_USB_RAM = .) ;
       *(.noinit.$RAM3)
       *(.noinit.$USB_RAM)
       *(.noinit.$RAM3.*)
       *(.noinit.$USB_RAM.*)
       . = ALIGN(4) ;
       PROVIDE(__end_noinit_RAM3 = .) ;
       PROVIDE(__end_noinit_USB_RAM = .) ;
    } > USB_RAM AT> USB_RAM

    /* DEFAULT NOINIT SECTION */
    .noinit (NOLOAD): ALIGN(4)
    {
        _noinit = .;
        PROVIDE(__start_noinit_RAM2 = .) ;
        PROVIDE(__start_noinit_SRAM_0_1_2_3 = .) ;
        *(.noinit*)
         . = ALIGN(4) ;
        _end_noinit = .;
       PROVIDE(__end_noinit_RAM2 = .) ;
       PROVIDE(__end_noinit_SRAM_0_1_2_3 = .) ;        
    } > SRAM_0_1_2_3 AT> SRAM_0_1_2_3

    /* Reserve and place Heap within memory map */
    _HeapSize = 0x1000;
    .heap :  ALIGN(4)
    {
        _pvHeapStart = .;
        . += _HeapSize;
        . = ALIGN(4);
        _pvHeapLimit = .;
    } > SRAMX

     _StackSize = 0x1000;
     /* Reserve space in memory for Stack */
    .heap2stackfill  :
    {
        . += _StackSize;
    } > SRAMX
    /* Locate actual Stack in memory map */
    .stack ORIGIN(SRAMX) + LENGTH(SRAMX) - _StackSize - 0:  ALIGN(4)
    {
        _vStackBase = .;
        . = ALIGN(4);
        _vStackTop = . + _StackSize;
    } > SRAMX

    /* ## Create checksum value (used in startup) ## */
    PROVIDE(__valid_user_code_checksum = 0 - 
                                         (_vStackTop 
                                         + (ResetISR + 1) 
                                         + (NMI_Handler + 1) 
                                         + (HardFault_Handler + 1) 
                                         + (( DEFINED(MemManage_Handler) ? MemManage_Handler : 0 ) + 1)   /* MemManage_Handler may not be defined */
                                         + (( DEFINED(BusFault_Handler) ? BusFault_Handler : 0 ) + 1)     /* BusFault_Handler may not be defined */
                                         + (( DEFINED(UsageFault_Handler) ? UsageFault_Handler : 0 ) + 1) /* UsageFault_Handler may not be defined */
                                         ) );

    /* Provide basic symbols giving location and size of main text
     * block, including initial values of RW data sections. Note that
     * these will need extending to give a complete picture with
     * complex images (e.g multiple Flash banks).
     */
    _image_start = LOADADDR(.text);
    _image_end = LOADADDR(.data) + SIZEOF(.data);
    _image_size = _image_end - _image_start;

    /* Provide symbols for LPC540xx parts for startup code to use
     * to set image to be plain load image or XIP.
     * Config : Plain load image = true
     */
    __imghdr_loadaddress = ADDR(.text);
    __imghdr_imagetype = 1;
}