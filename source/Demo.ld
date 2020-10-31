GROUP (
  "libgcc.a"
  "libc_nano.a"
  "libm.a"
  "libcr_newlib_nohost.a"
)

/*
 *   LPC54018 does not execute from Flash but from RAM (SRAMX). As a result, the
 *   MPU needs to be programmed to set the portion of SRAMX containing kernel
 *   code as privileged Read Only and the portion of SRAMX containing remaining
 *   of the code as Read Only. To facilitate this, SRAMX is divided into two
 *   parts:
 *       1. SRAMX_CODE - 128KB. Contains code.
 *       2. SRAMX_DATA - 64 KB. Contains data (only stack and heap as of now).
 *
 *   SRAM_0_1_2_3 is of size 160 KB which is not a power of 2. ARM v7 MPU requires
 *   the size of an MPU region to be a power of two. Since FreeRTOS Cortex M4 MPU
 *   port programs MPU to grant access to all SRAM (for tasks created using
 *   xTaskCreate), we need to ensure that the size of SRAM region is a power of
 *   two. This is why SRAM_0_1_2_3 is divided into two parts:
 *       1. SRAM_0_1_2_3 - 128 KB. Contains data. Since the size is now a power
 *          of two, an MPU region can be used to grant access to it.
 *       2. SRAM_0_1_2_3_UNUSED - 32 KB. Unused.
 */
MEMORY
{
    /* Define each memory region. */
    BOARD_FLASH (rx)            : ORIGIN = 0x10000000, LENGTH = 0x1000000 /* 16M bytes (alias Flash). */
    SRAMX (rwx)                 : ORIGIN = 0x0,        LENGTH = 0x30000   /* 128K bytes. */
    SRAM_0_1_2_3 (rwx)          : ORIGIN = 0x20000000, LENGTH = 0x20000   /* 128K bytes (alias RAM2). */
    SRAM_0_1_2_3_UNUSED (rwx)   : ORIGIN = 0x20020000, LENGTH = 0x8000    /* 32K bytes. */
    USB_RAM (rwx)               : ORIGIN = 0x40100000, LENGTH = 0x2000    /* 8K bytes (alias RAM3). */
}

/* Privilegd fuctions are stored are stored in FLash (VMA/LMA) and XIP. */
__privileged_functions_region_size__  = 64K;
__privileged_data_region_size__       = 128K;

/* Symbols needed by the MPU setup code. */
__FLASH_segment_start__ = ORIGIN( BOARD_FLASH );
__FLASH_segment_end__   = __FLASH_segment_start__ + LENGTH( BOARD_FLASH );

__SRAM_segment_start__  = ORIGIN( SRAM_0_1_2_3 );
__SRAM_segment_end__    = __SRAM_segment_start__ + LENGTH( SRAM_0_1_2_3 );

/* Entry point. */
ENTRY(ResetISR)

/* Sections. */
SECTIONS
{
    /* The startup code and FreeRTOS kernel code are placed at the beginning
     * of SRAMX_CODE. */
    .privileged_functions : ALIGN(4)
    {
        FILL(0xff)
        __vectors_start__ = .;
        __FLASH_segment_start__ = __vectors_start__;
        __privileged_functions_start__ = __vectors_start__;
        KEEP(*(.isr_vector))

        /* Global Section Table. */
        . = ALIGN(4);
        __section_table_start = .;

		/* User for copying initialized data*/
        __data_section_table = .;
      	/* SRAMX */
        LONG(LOADADDR(.data));
        LONG(     ADDR(.data));
        LONG(   SIZEOF(.data));

		/* SRAM_0_1_2_3 */
        LONG(LOADADDR(.data_RAM2));
        LONG(     ADDR(.data_RAM2));
        LONG(   SIZEOF(.data_RAM2));

		/* USB RAM*/
        LONG(LOADADDR(.data_RAM3));
        LONG(     ADDR(.data_RAM3));
        LONG(   SIZEOF(.data_RAM3));
        __data_section_table_end = .;

        __bss_section_table = .;
        LONG(    ADDR(.bss));
        LONG(  SIZEOF(.bss));
        
        LONG(    ADDR(.bss_RAM2));
        LONG(  SIZEOF(.bss_RAM2));

        LONG(    ADDR(.bss_RAM3));
        LONG(  SIZEOF(.bss_RAM3));
        __bss_section_table_end = .;

        __section_table_end = .;
        /* End of Global Section Table. */

        /* Functions placed after vector table. */
        *(.after_vectors*)

        /* Kernel code. */
        *(privileged_functions)

        FILL(0xDEAD);
        /* Ensure that non-privileged code is placed after the region reserved for
         * privileged kernel code. */
        /* Note that dot (.) actually refers to the byte offset from the start of
         * the current section (.privileged_functions in this case). As a result,
         * setting dot (.) to a value sets the size of the section. */
        . = __privileged_functions_region_size__;
        __privileged_functions_end__ = .;
    } > BOARD_FLASH

    /* Text Section. */
    .text : ALIGN(4)
    {        
        /* Place the FreeRTOS System Calls first in the unprivileged region. */
        __syscalls_flash_start__ = .;
        *(freertos_system_calls)
        __syscalls_flash_end__ = .;

        /* For XIP, some objects need to be in .data. Otherwise place all other .text here + RO data */
        *(EXCLUDE_FILE(*/mflash_drv.o */fsl_spifi.o */bignum.o */fsl_enet.o) .text*)
        KEEP(*freertos*/tasks.o(.rodata*)) /* FreeRTOS Debug Config. */
        *(.rodata .rodata.* .constdata .constdata.*)
        . = ALIGN(4);
    } > BOARD_FLASH

    /* For exception handling/unwind - some Newlib functions (in common
     * with C++ and STDC++) use this. */
    .ARM.extab : ALIGN(4)
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > BOARD_FLASH


    .ARM.exidx : ALIGN(4)
    {
    	__exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > BOARD_FLASH

    
    /* End of text section. */
    _etext = .;

    /* USB_RAM. */
    .m_usb_data (NOLOAD) :
    {
        *(m_usb_global)
    } > USB_RAM AT> USB_RAM

    .data_RAM2 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM2 = .) ;
        PROVIDE(__start_data_SRAM_0_1_2_3 = .) ;
        *(.ramfunc.$RAM2)
        *(.ramfunc.$SRAM_0_1_2_3)
        *(.data.$RAM2*)
        *(.data.$SRAM_0_1_2_3*)
        . = ALIGN(4) ;
        PROVIDE(__end_data_RAM2 = .) ;
        PROVIDE(__end_data_SRAM_0_1_2_3 = .) ;
     } > SRAM_0_1_2_3 AT>BOARD_FLASH

    /* Data section for USB_RAM. */
    .data_RAM3 : ALIGN(4)
    {
        FILL(0xff)
        PROVIDE(__start_data_RAM3 = .);
        PROVIDE(__start_data_USB_RAM = .);
        *(.ramfunc.$RAM3)
        *(.ramfunc.$USB_RAM)
        *(.data.$RAM3*)
        *(.data.$USB_RAM*)
        . = ALIGN(4);
        PROVIDE(__end_data_RAM3 = .);
        PROVIDE(__end_data_USB_RAM = .);
     } > USB_RAM AT> BOARD_FLASH

    /* Main Data Section - Reserved. */
    .uninit_RESERVED (NOLOAD) : ALIGN(4)
    {
        _start_uninit_RESERVED = .;
        __privileged_data_start__ = _start_uninit_RESERVED;
        KEEP(*(.bss.$RESERVED*))
       . = ALIGN(4);

        _end_uninit_RESERVED = .;
    } > SRAMX AT> SRAMX

    /* Main DATA section (SRAMX). */
    .data : ALIGN(4)
    {
    	FILL(0xff)
        _data = .;
        PROVIDE(__start_data_RAM = .);
        PROVIDE(__start_data_SRAMX = .);

        /* FreeRTOS kernel data. */
        *(privileged_data)
        FILL(0xDEAD);
        /* Ensure that non-privileged data is placed after the region reserved for
         * privileged kernel data. */
        /* Note that dot (.) actually refers to the byte offset from the start of
         * the current section (.data in this case). As a result, setting
         * dot (.) to a value extends the size of the section. */
        . = __privileged_data_region_size__;
        __privileged_data_end__ = .;

        FILL(0xff)
        *(vtable)
        *(.ramfunc*)
        KEEP(*(CodeQuickAccess))
        KEEP(*(DataQuickAccess))
        *(RamFunction)
        */mflash_drv.o(.text .text* .rodata .rodata*)
        */fsl_spifi.o(.text .text* .rodata .rodata*)
        */bignum.o(.text .text* .rodata .rodata*)
        */fsl_enet.o(.text .text* .rodata .rodata*)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
        PROVIDE(__end_data_RAM = .);
        PROVIDE(__end_data_SRAMX = .);
    } > SRAMX AT> BOARD_FLASH

    /* BSS section for SRAM_0_1_2_3 */
    .bss_RAM2 : ALIGN(4)
    {
       PROVIDE(__start_bss_RAM2 = .) ;
       PROVIDE(__start_bss_SRAM_0_1_2_3 = .) ;
        *(.bss*)
       . = ALIGN (. != 0 ? 4 : 1) ; /* avoid empty segment */
       PROVIDE(__end_bss_RAM2 = .) ;
       PROVIDE(__end_bss_SRAM_0_1_2_3 = .) ;
    } > SRAM_0_1_2_3 AT> SRAM_0_1_2_3

    /* BSS section for USB_RAM. */
    .bss_RAM3 : ALIGN(4)
    {
        PROVIDE(__start_bss_RAM3 = .);
        PROVIDE(__start_bss_USB_RAM = .);
        . = ALIGN (. != 0 ? 4 : 1); /* Avoid empty segment. */
        PROVIDE(__end_bss_RAM3 = .);
        PROVIDE(__end_bss_USB_RAM = .);
    } > USB_RAM AT> USB_RAM

    /* Main BSS Section. */
    .bss : ALIGN(4)
    {
        _bss = .;
        PROVIDE(__start_bss_RAM = .);
        PROVIDE(__start_bss_SRAMX = .);
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        PROVIDE(__end_bss_RAM = .);
        PROVIDE(__end_bss_SRAMX = .);
        PROVIDE(end = .);
    } > SRAMX AT> SRAMX

    /* NOINIT section for SRAM_0_1_2_3 */
    .noinit_RAM2 (NOLOAD) : ALIGN(4)
    {
       PROVIDE(__start_noinit_RAM2 = .) ;
       PROVIDE(__start_noinit_SRAM_0_1_2_3 = .) ;
       *(.noinit.$RAM2)
       *(.noinit.$SRAM_0_1_2_3)
       *(.noinit.$RAM2.*)
       *(.noinit.$SRAM_0_1_2_3.*)
       . = ALIGN(4) ;
       PROVIDE(__end_noinit_RAM2 = .) ;
       PROVIDE(__end_noinit_SRAM_0_1_2_3 = .) ;
    } > SRAM_0_1_2_3 AT> SRAM_0_1_2_3

    /* NOINIT section for USB_RAM. */
    .noinit_RAM3 (NOLOAD) : ALIGN(4)
    {
        PROVIDE(__start_noinit_RAM3 = .);
        PROVIDE(__start_noinit_USB_RAM = .);
        *(.noinit.$RAM3)
        *(.noinit.$USB_RAM)
        *(.noinit.$RAM3.*)
        *(.noinit.$USB_RAM.*)
        . = ALIGN(4);
        PROVIDE(__end_noinit_RAM3 = .);
        PROVIDE(__end_noinit_USB_RAM = .);
    } > USB_RAM AT> USB_RAM

    /* Default NOINIT Section. */
    .noinit (NOLOAD): ALIGN(4)
    {
        _noinit = .;
        PROVIDE(__start_noinit_RAM = .);
        PROVIDE(__start_noinit_SRAMX = .);
        *(.noinit*)
         . = ALIGN(4);
        _end_noinit = .;
       PROVIDE(__end_noinit_RAM = .);
       PROVIDE(__end_noinit_SRAMX = .);
    } > SRAMX AT> SRAMX

    /* Reserve and place Heap within memory map. */
    _HeapSize = 0x1000;
    .heap : ALIGN(4)
    {
        _pvHeapStart = .;
        . += _HeapSize;
        . = ALIGN(4);
        _pvHeapLimit = .;
    } > SRAMX

     /* Reserve space in memory for Stack. */
     _StackSize = 0x1000;
    .heap2stackfill  :
    {
        . += _StackSize;
    } > SRAMX

    /* Locate actual Stack in memory map. */
    .stack ORIGIN(SRAMX) + LENGTH(SRAMX) - _StackSize - 0:  ALIGN(4)
    {
        _vStackBase = .;
        . = ALIGN(4);
        _vStackTop = . + _StackSize;
    } > SRAMX

    /* ## Create checksum value (used in startup). ## */
    PROVIDE(__valid_user_code_checksum = 0 -
                                         (_vStackTop
                                         + (ResetISR + 1)
                                         + (NMI_Handler + 1)
                                         + (HardFault_Handler + 1)
                                         + (( DEFINED(MemManage_Handler) ? MemManage_Handler : 0 ) + 1)   /* MemManage_Handler may not be defined. */
                                         + (( DEFINED(BusFault_Handler) ? BusFault_Handler : 0 ) + 1)     /* BusFault_Handler may not be defined. */
                                         + (( DEFINED(UsageFault_Handler) ? UsageFault_Handler : 0 ) + 1) /* UsageFault_Handler may not be defined. */
                                         ) );

    /* Provide basic symbols giving location and size of main text
     * block, including initial values of RW data sections. Note that
     * these will need extending to give a complete picture with
     * complex images (e.g multiple Flash banks). */
    _image_start = LOADADDR(.privileged_functions);
    _image_end = LOADADDR(.data) + SIZEOF(.data);
    _image_size = _image_end - _image_start;

    /* Provide symbols for LPC540xx parts for startup code to use
     * to set image to be plain load image or XIP.
     * Config : Plain load image = true. */
    __imghdr_loadaddress = LOADADDR(.privileged_functions);
    __imghdr_imagetype = 3;
}