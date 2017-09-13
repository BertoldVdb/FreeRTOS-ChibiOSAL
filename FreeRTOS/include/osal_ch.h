/*
    OSAL_CH v0.1.0 - Copyright (C) 2017 Bertold Van den Bergh
    All rights reserved

    This file implements the ChibiHAL OS abstraction layer on FreeRTOS. 
    >>>IT IS NOT PART OF THE STANDARD FREERTOS<<<
  
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ READ THIS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    WARNING: By default on the Cortex-M3 port it is not legal to yield in a
    critical section. This makes the behaviour of S-type functions wrong. If
    you do it anyway: a PendSV interrupt is scheduled, but the interupt is 
    masked so nothing happens :(. It does (I suppose) work in FreeRTOS ports
    that do a synchronous context switch.

    I have made a modification to the Cortex-M3 port to boost the PendSV
    interrupt priority when yielding in a critical section. This does not impact
    yielding from interrupts, since this is by definition not possible when a 
    critical section is executing.
    However: it is forbidden to enter a critical section in an ISR and then call
    something that yields. This will corrupt the TCB and crash the system. It is
    not well defined what the desired behaviour whould be when trying to sleep
    in an ISR anyway...
   
    To avoid corruption, I also save the criticalNestingCount and basepri on the
    stack. This (hopefully) makes the behaviour correct.
    
    I have only tested this on an STM32F103 CPU.
   
    WARNING2: The OSAL defines functions for signalling events, but these can
    seemlingly not be extracted. The only two defined functions are: 
    osalEventBroadcastFlagsI and osalEventBroadcastFlags, which both insert an
    event. We have implemented a simple listener like API that works with this
    OSAL, but it is of course not standard compliant.
   
    (Note that the FreeRTOS event API is not used since it doesn't match well
    with how ChibiOS HAL uses it. FreeRTOS uses a dedicated service thread.)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ READ THIS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/
#ifndef _OSAL_CH_H_
#define _OSAL_CH_H_

#include <stdint.h>
#include <limits.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "portmacro.h"

#define FALSE false
#define TRUE true
#define OSAL_SUCCESS false
#define OSAL_FAILED true

/* FreeRTOS can work with both 16 and 32-bit ticks */
#if configUSE_16_BIT_TICKS == 0
#define OSAL_ST_RESOLUTION 32
#else
#define OSAL_ST_RESOLUTION 16
#endif

/* Standard time constants */
#define TIME_IMMEDIATE ((systime_t)0)
#define TIME_INFINITE  ((systime_t)portMAX_DELAY)

/* ChibiOS HAL can setup SysTick for you, but FreeRTOS already did it */
#define OSAL_ST_MODE_NONE 0
#define OSAL_ST_MODE      OSAL_ST_MODE_NONE

/* Standard message types used by the HAL */
#define MSG_OK      (msg_t)0
#define MSG_TIMEOUT (msg_t)-1
#define MSG_RESET   (msg_t)-2
#define MSG_EVENT_W (msg_t)-3

/* Systick rate set by FreeRTOS */
#define OSAL_ST_FREQUENCY configTICK_RATE_HZ

/* A lot of typedefs, matching FreeRTOS types with ChibiOS */
/* Thread related */
typedef TaskHandle_t thread_t;
typedef thread_t * thread_reference_t;
typedef struct {
    thread_t head;
} threads_queue_t;

/* Stores the system state before enterring critical section */
typedef UBaseType_t syssts_t;

/* SysTick and cycle time */
typedef TickType_t systime_t;
typedef unsigned long rtcnt_t;

/* Message ID */
typedef int32_t msg_t;

/* Event/Poll related */
typedef UBaseType_t eventflags_t;
typedef struct event_repeater event_repeater_t;

typedef struct {
    event_repeater_t* firstRepeater;
    eventflags_t setEvents;
    thread_reference_t waitThread;
} event_source_t;

typedef struct event_repeater {
    event_repeater_t* nextRepeater;
    event_repeater_t* prevRepeater;
    event_source_t* source;
    event_source_t* target;
    eventflags_t triggerEvents;
    eventflags_t setEvents;
    eventflags_t myEvent;
} event_repeater_t;

/* Mutex type */
typedef struct {
    SemaphoreHandle_t handle;
    StaticSemaphore_t staticData;
} mutex_t;

/* Some more complex functions are defined in osal_ch.c */
#ifdef __cplusplus
extern "C" {
#endif
msg_t osalThreadEnqueueTimeoutS(threads_queue_t* thread_queue, systime_t timeout);
void osalThreadDequeueAllI(threads_queue_t* thread_queue, msg_t msg);
void osalThreadDequeueNextI(threads_queue_t* thread_queue, msg_t msg);

msg_t osalThreadSuspendS(thread_reference_t* thread_reference);
msg_t osalThreadSuspendTimeoutS(thread_reference_t* thread_reference, systime_t timeout);

eventflags_t osalEventWaitTimeoutS(event_source_t* event_source, systime_t timeout);
void osalEventBroadcastFlagsI(event_source_t* event_source, eventflags_t set);
void osalEventRepeaterRegister(event_repeater_t* event_repeater,
                               event_source_t* event_source,
                               eventflags_t trigger_events,
                               event_source_t* target_event_sink,
                               eventflags_t my_event);
void osalEventRepeaterUnregister(event_repeater_t* event_repeater);
#ifdef __cplusplus
}
#endif

/* FreeRTOS thread function template */
#define THD_FUNCTION(func, param) void func(void* param)

/* Debug functions */
#if( configASSERT_DEFINED == 1 )
#define osalDbgAssert(mustBeTrue, msg) do {  \
    if (!(mustBeTrue)) {                     \
        osalSysHalt(msg);                    \
    }                                        \
} while(false);

#define osalSysHalt(text) do {                                      \
    osalSysDisable();                                               \
    errorAssertCalled( __FILE__, __LINE__, text );                  \
    while(1);                                                       \
} while(0)

#else
#define osalDbgAssert(mustBeTrue, msg) do {                         \
    osalSysDisable();                                               \
    while(1);                                                       \
} while(0)


#define osalSysHalt(text) do {                                      \
    osalSysDisable();                                               \
    while(1);                                                       \
} while(0)
#endif

#define osalDbgCheck(mustBeTrue) osalDbgAssert(mustBeTrue, __func__)

/* Many HAL functions use these for safety checking */
#if( configASSERT_DEFINED == 1 )
#define osalDbgCheckClassI() do {                                       \
    osalDbgAssert(xPortIsCriticalSection(), "not in critical section"); \
}while(0);

#define osalDbgCheckClassS() do {                                   \
    osalDbgCheckClassI();                                           \
    osalDbgAssert(!xPortIsInsideInterrupt(), "in interrupt");       \
}while(0);

#else

#define osalDbgCheckClassI()
#define osalDbgCheckClassS()
#endif

/* Proxy functions to FreeRTOS */
#define osalSysDisable vTaskEndScheduler
#define osalSysEnable vTaskStartScheduler
#define osalSysLock taskENTER_CRITICAL
#define osalSysGetStatusAndLockX taskENTER_CRITICAL_FROM_ISR
#define osalSysRestoreStatusX(state) taskEXIT_CRITICAL_FROM_ISR(state)
#define osalOsGetSystemTimeX xTaskGetTickCountFromISR
#define osalThreadSleepS(time) vTaskDelay(time)
#define osalThreadSleep(time) vTaskDelay(time)

/* More proxying using simple inline functions */
extern UBaseType_t uxSavedInterruptStatus;

static inline void osalOsRescheduleS(void)
{
    osalDbgCheckClassS();
    if(uxYieldPending()) taskYIELD();
}

static inline void osalSysLockFromISR(void)
{
    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
}

static inline void osalSysUnlockFromISR(void)
{
    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}

static inline void osalSysUnlock(void){
    osalOsRescheduleS();
    taskEXIT_CRITICAL();
}

static inline void osalEventBroadcastFlags(event_source_t* event_source, eventflags_t set)
{
    osalDbgCheck(event_source != NULL);

    osalSysLock();
    osalEventBroadcastFlagsI(event_source, set);
    osalSysUnlock();
}

static inline eventflags_t osalEventRepeaterGetS(event_repeater_t* event_repeater){
    eventflags_t flags = event_repeater->setEvents;
    event_repeater->setEvents = 0;
    return flags;
}

static inline void osalMutexLock(mutex_t* mutex)
{
    osalDbgCheck(mutex != NULL);
    xSemaphoreTake(mutex->handle, portMAX_DELAY);
}

static inline void osalMutexUnlock(mutex_t* mutex)
{
    osalDbgCheck(mutex != NULL);
    xSemaphoreGive(mutex->handle);
}

static inline void osalThreadResumeI(thread_reference_t* thread_reference, msg_t msg)
{
    osalDbgCheckClassI();
    if(*thread_reference) xTaskNotifyFromISR( *thread_reference, msg, eSetValueWithOverwrite, NULL );
}

static inline void osalThreadResumeS(thread_reference_t* thread_reference, msg_t msg)
{
    osalDbgCheckClassS();
    if(*thread_reference) xTaskNotify( *thread_reference, msg, eSetValueWithOverwrite );
}

static inline void osalSysPolledDelayX(rtcnt_t cycles)
{
    vPortBusyDelay(cycles);
}

/* Init objects */
static inline void osalThreadQueueObjectInit(threads_queue_t* thread_queue)
{
    osalDbgCheck(thread_queue != NULL);
    thread_queue->head = NULL;
}

static inline void osalMutexObjectInit(mutex_t* mutex)
{
    osalDbgCheck(mutex != NULL);
    mutex->handle = xSemaphoreCreateMutexStatic(&mutex->staticData);
}

static inline void osalEventObjectInit(event_source_t* event_source)
{
    osalDbgCheck(event_source != NULL);
    event_source->setEvents = 0;
    event_source->firstRepeater = NULL;
    event_source->waitThread = NULL;
}

#define OSAL_IRQ_PROLOGUE()
/* At the end of the interrupt handler we need to check if we need
 * to yield */
#define OSAL_IRQ_EPILOGUE() {                  \
    portYIELD_FROM_ISR(uxYieldPending());      \
}
#define OSAL_IRQ_HANDLER(handleName) void handleName(void)

/* Macros converting real time to systicks */
typedef uint32_t conv_t;
#define OSAL_T2ST(r, t, hz, div) ((r)(((conv_t)(hz) * (conv_t)(t) + (conv_t)(div-1)) / (conv_t)(div)))

#define OSAL_US2ST(t) OSAL_T2ST(systime_t, t, configTICK_RATE_HZ, 1000000)
#define OSAL_MS2ST(t) OSAL_T2ST(systime_t, t, configTICK_RATE_HZ, 1000)
#define OSAL_S2ST(t)  OSAL_T2ST(systime_t, t, configTICK_RATE_HZ, 1)

/* And easy-to-use sleep macros */
#define osalThreadSleepMicroseconds(t) osalThreadSleep(OSAL_US2ST(t))
#define osalThreadSleepMilliseconds(t) osalThreadSleep(OSAL_MS2ST(t))
#define osalThreadSleepSeconds(t)      osalThreadSleep(OSAL_S2ST(t))

/* Why is this in the OSAL? It is always the same? */
static inline bool osalOsIsTimeWithinX(systime_t now, systime_t begin, systime_t end)
{
    systime_t duration = end - begin;
    systime_t past = now - begin;

    if(past < duration) return true;
    
    return false;
}

/* FreeRTOS has a runtime check for invalid priorities, making this check redudant */
#define OSAL_IRQ_IS_VALID_PRIORITY(n) true

/* No initialization needed */
static inline void osalInit(void)
{
}

/* Some code in ChibiOS seems to call the kernel directly instead of using the osal */
#define chSysLock osalSysLock
#define chSysUnock osalSysUnlock
#define chEvtObjectInit osalEventObjectInit
#define chVTGetSystemTime osalOsGetSystemTimeX
#define chCoreGetStatusX xPortGetFreeHeapSize
static inline void chThdExitS(msg_t msg){
    (void)msg;

    vTaskDelete(NULL);
}
static inline void chEvtBroadcast(event_source_t* event_source){
    osalEventBroadcastFlags(event_source, 1);
}
static inline void chEvtBroadcastI(event_source_t* event_source){
    osalEventBroadcastFlagsI(event_source, 1);
}
#define CH_KERNEL_VERSION "FreeRTOS v9"
#define CH_CFG_USE_MEMCORE TRUE
#define CH_CFG_USE_HEAP TRUE
#endif
