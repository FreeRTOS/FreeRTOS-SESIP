#include "FreeRTOS.h"
#include "task.h"


#include "fsl_debug_console.h"

/**
 * @brief Calls the port specific code to raise the privilege.
 *
 * @return pdFALSE if privilege was raised, pdTRUE otherwise.
 */
extern BaseType_t xPortRaisePrivilege( void );

/**
 * @brief If xRunningPrivileged is not pdTRUE, calls the port specific
 * code to reset the privilege, otherwise does nothing.
 */
extern void vPortResetPrivilege( BaseType_t xRunningPrivileged );


/* NXP PRINTF code resides somewhere in RAM that could be provided as accessible region, but it's simpler to
 * just run it as privileged */
#define MPU_PRINTF( ... ) 									   \
	{														   \
		BaseType_t xRunningPrivileged = xPortRaisePrivilege(); \
		PRINTF( __VA_ARGS__ ); 								   \
		vPortResetPrivilege( xRunningPrivileged );             \
	}

/* ------------------------------------------------------------------------------- */
extern uint32_t __privileged_functions_start__[];
extern uint32_t __privileged_functions_end__[];
extern uint32_t __FLASH_segment_start__[];
extern uint32_t __FLASH_segment_end__[];
extern uint32_t __privileged_data_start__[];
extern uint32_t __privileged_data_end__[];
extern uint32_t __syscalls_flash_start__[];
extern uint32_t __syscalls_flash_end__[];
extern uint32_t __SRAM_segment_start__[];
extern uint32_t __SRAM_segment_end__[];


/* ------------------------------------------------------------------------------- */

void printRegions()
{
    uint32_t * tmp = NULL;
    tmp = __privileged_functions_start__;
    tmp = __privileged_functions_end__;
    tmp = __FLASH_segment_start__;
    tmp = __FLASH_segment_end__;
    tmp = __privileged_data_start__;
    tmp = __privileged_data_end__;

    (void)tmp;

    PRINTF( "\r\n" );
    PRINTF( "privileged functions: %08x - %08x\r\n", __privileged_functions_start__, __privileged_functions_end__ );
    PRINTF( "privileged data:      %08x - %08x\r\n", __privileged_data_start__, __privileged_data_end__ );
    PRINTF( "system calls:         %08x - %08x\r\n", __syscalls_flash_start__, __syscalls_flash_end__ );
    PRINTF( "flash segment:        %08x - %08x\r\n", __FLASH_segment_start__, __FLASH_segment_end__ );
    PRINTF( "sram segment:         %08x - %08x\r\n", __SRAM_segment_start__, __SRAM_segment_end__ );
    PRINTF( "\r\n" );
}



#define SHARED_MEMORY_SIZE 32
#define MIN_REGION_SIZE 32
#define RESTRICTED_TASK_STACK_SIZE 128

/* We can use shared memory to pass off kernel data to user data */
static uint8_t ucSharedMemory[ SHARED_MEMORY_SIZE ] __attribute__( ( aligned( SHARED_MEMORY_SIZE ) ) );

static StackType_t xRWAccessTaskStack[ RESTRICTED_TASK_STACK_SIZE ] __attribute__( ( aligned( RESTRICTED_TASK_STACK_SIZE * sizeof( StackType_t ) ) ) );
static void prvRWAccessTask( void * pvParameters )
{
	/* Unused parameters. */
	( void ) pvParameters;

	ucSharedMemory[ 0 ] = 0;

	while(1)
	{
		ucSharedMemory[ 0 ] = 1;
		MPU_PRINTF( "Ran RW task\r\n" );

		vTaskDelay( pdMS_TO_TICKS( 8000 ) );
	}
}

/* This is how RO task communicates to handler that it intentionally memory faulted.
 * Note, handlers run priviliged thus will have access)
 * Also note, 32B is minimum valid size for region*/
static volatile uint8_t ucROTaskFaultTracker[ MIN_REGION_SIZE ] __attribute__( ( aligned( MIN_REGION_SIZE ) ) ) = { 0 };
static StackType_t xROAccessTaskStack[ RESTRICTED_TASK_STACK_SIZE ] __attribute__( ( aligned( RESTRICTED_TASK_STACK_SIZE * sizeof( StackType_t ) ) ) );
static void prvROAccessTask( void * pvParameters )
{
uint8_t ucVal;

	/* Unused parameters. */
	( void ) pvParameters;
	ucROTaskFaultTracker[ 0 ] = 0;

	for( ; ; )
	{
		/* This task has RO access to ucSharedMemory and therefore it can read
		 * it but cannot modify it. */
		ucVal = ucSharedMemory[ 0 ];

		/* Silent compiler warnings about unused variables. */
		( void ) ucVal;

//#define INJECT_TEST_MEMORY_FAULT
#ifdef INJECT_TEST_MEMORY_FAULT
		ucROTaskFaultTracker[ 0 ] = 1;

		MPU_PRINTF("Triggering memory violation...\r\n");

		/* Illegal access to generate Memory Fault. */
		ucSharedMemory[ 0 ] = 0;

		/* Ensure that the above line did generate MemFault and the fault
		 * handler did clear the  ucROTaskFaultTracker[ 0 ]. */
		if( ucROTaskFaultTracker[ 0 ] == 0 )
		{
			MPU_PRINTF("Access Violation handled.\r\n");
		}
		else
		{
			MPU_PRINTF("Error: Access violation should have triggered a fault\r\n");
		}
#endif
		MPU_PRINTF( "Ran RO task\r\n" );

		vTaskDelay( pdMS_TO_TICKS( 5000 ) );
	}
}


void xCreateRestrictedTasks( BaseType_t xPriority )
{
    /* Create restricted tasks */
    TaskParameters_t xRWAccessTaskParameters =
    {
    	.pvTaskCode		= prvRWAccessTask,
    	.pcName			= "RWAccess",
    	.usStackDepth	= RESTRICTED_TASK_STACK_SIZE,
    	.pvParameters	= NULL,
    	.uxPriority		= xPriority,
    	.puxStackBuffer	= xRWAccessTaskStack,
    	.xRegions		=	{
    							{ ucSharedMemory,	SHARED_MEMORY_SIZE,	portMPU_REGION_READ_WRITE | portMPU_REGION_EXECUTE_NEVER},
    							{ 0, 0, 0 },
    							{ 0, 0, 0 },
    						}
    };
	xTaskCreateRestricted( &( xRWAccessTaskParameters ), NULL );

	TaskParameters_t xROAccessTaskParameters =
	{
		.pvTaskCode		= prvROAccessTask,
		.pcName			= "ROAccess",
		.usStackDepth	= RESTRICTED_TASK_STACK_SIZE,
		.pvParameters	= NULL,
		.uxPriority		= xPriority,
		.puxStackBuffer	= xROAccessTaskStack,
		.xRegions		=	{
								{ ucSharedMemory, SHARED_MEMORY_SIZE, portMPU_REGION_PRIVILEGED_READ_WRITE_UNPRIV_READ_ONLY | portMPU_REGION_EXECUTE_NEVER	},
								{ ( void * ) ucROTaskFaultTracker,	SHARED_MEMORY_SIZE,	portMPU_REGION_READ_WRITE | portMPU_REGION_EXECUTE_NEVER },
								{0,0,0}
								//{ 0x20000500, 0x100, portMPU_REGION_READ_WRITE },
							}
	};
	xTaskCreateRestricted( &( xROAccessTaskParameters ), NULL );
}


/* ------------------------------------------------------------------------------- */


/* For readability. Read about Hardfault Entry in ARMv7 docs for more details */
typedef struct
{
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;
	uint32_t return_address;
	uint32_t xPSR;
} HardFaultStack_t;

/* User Memory Fault Handler */
portDONT_DISCARD void vHandleMemoryFault( uint32_t * pulFaultStackAddress )
{
	uint32_t ulPC;
	uint16_t usOffendingInstruction;

	HardFaultStack_t * const xFaultStack = ( HardFaultStack_t * ) pulFaultStackAddress;


	if( ucROTaskFaultTracker[ 0 ] == 1 )
	{
		/* Read program counter. */

		ulPC = xFaultStack->return_address;

		/* Read the offending instruction. */
		usOffendingInstruction = *( uint16_t * )ulPC;

		/* From ARM docs:
		 * If the value of bits[15:11] of the halfword being decoded is one of
		 * the following, the halfword is the first halfword of a 32-bit
		 * instruction:
		 * - 0b11101.
		 * - 0b11110.
		 * - 0b11111.
		 * Otherwise, the halfword is a 16-bit instruction.
		 */

		/* Extract bits[15:11] of the offending instruction. */
		usOffendingInstruction = usOffendingInstruction & 0xF800;
		usOffendingInstruction = ( usOffendingInstruction >> 11 );

		/* Increment to next instruction, depending on current instruction size (32-bit or 16-bit) */
		if( usOffendingInstruction == 0x001F ||
			usOffendingInstruction == 0x001E ||
			usOffendingInstruction == 0x001D )
		{
			ulPC += 4;
		}
		else
		{
			ulPC += 2;
		}

		/* Indicate to RO task its expected fault was handled */
		ucROTaskFaultTracker[ 0 ] = 0;

		/* Resume execution after offending instruction from RO task */
		xFaultStack->return_address = ulPC;

		PRINTF("Expected memory violation caught by handler...\r\n", ulPC);

	}
	else
	{
		PRINTF("Memory Access Violation. Inst @ %x\r\n", ulPC);

		while(1);
	}
}
