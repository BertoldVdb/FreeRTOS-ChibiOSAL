/*
    OSAL_CH v0.1.0 - Copyright (C) 2017 Bertold Van den Bergh
    All rights reserved

    This file implements the ChibiHAL OS abstraction layer on FreeRTOS. 
    >>>IT IS NOT PART OF THE STANDARD FREERTOS<<<
    
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

#include "osal_ch.h"

/* In this file there are slightly more complicated functions */

msg_t osalThreadEnqueueTimeoutS(threads_queue_t* thread_queue, systime_t timeout)
{
    if(!timeout) {
        return MSG_TIMEOUT;
    }

    osalDbgCheck(thread_queue != NULL);
    osalDbgCheckClassS();

    thread_t currentTask = xGetCurrentTaskHandle();

    vTaskSetUserData(currentTask, thread_queue->head);
    thread_queue->head = currentTask;

    return osalThreadSuspendTimeoutS(NULL, timeout);
}

void osalThreadDequeueAllI(threads_queue_t* thread_queue, msg_t msg)
{
    osalDbgCheck(thread_queue != NULL);
    osalDbgCheckClassI();

    thread_reference_t toWakeUp;
    while((toWakeUp = thread_queue->head)) {
        osalThreadResumeI(&toWakeUp, msg);

        thread_queue->head = pxTaskGetUserData(toWakeUp);
    }
}

void osalThreadDequeueNextI(threads_queue_t* thread_queue, msg_t msg)
{
    osalDbgCheck(thread_queue != NULL);
    osalDbgCheckClassI();

    thread_reference_t toWakeUp = thread_queue->head;
    if(toWakeUp) {
        osalThreadResumeI(&toWakeUp, msg);

        thread_queue->head=pxTaskGetUserData(toWakeUp);
    }
}

msg_t osalThreadSuspendS(thread_reference_t* thread_reference)
{
    msg_t ulInterruptStatus;
    osalDbgCheckClassS();

    if(thread_reference) {
        *thread_reference = xGetCurrentTaskHandle();
    }

    xTaskNotifyWait(ULONG_MAX, ULONG_MAX, (uint32_t*)&ulInterruptStatus, portMAX_DELAY );

    return ulInterruptStatus;
}

msg_t osalThreadSuspendTimeoutS(thread_reference_t* thread_reference, systime_t timeout)
{
    msg_t ulInterruptStatus;
    osalDbgCheckClassS();

    if(!timeout) {
        return MSG_TIMEOUT;
    }

    if(thread_reference) {
        *thread_reference = xGetCurrentTaskHandle();
    }

    if(!xTaskNotifyWait(ULONG_MAX, ULONG_MAX, (uint32_t*)&ulInterruptStatus, timeout )) {
        return MSG_TIMEOUT;
    }

    return ulInterruptStatus;
}

eventflags_t osalEventWaitTimeoutS(event_source_t* event_source, systime_t timeout){
    eventflags_t result = 0;
    osalDbgCheck(event_source != NULL);
    osalDbgCheckClassS();

    osalThreadSuspendTimeoutS(&event_source->waitThread, timeout);

    result = event_source->setEvents;
    event_source->setEvents = 0;
    
    return result;
}

void osalEventBroadcastFlagsI(event_source_t* event_source, eventflags_t set)
{
    osalDbgCheck(event_source != NULL);
    osalDbgCheckClassI();

    event_source->setEvents |= set;
    eventflags_t localEvents = event_source->setEvents;

    /* Any repeaters? */
    event_repeater_t* repeater = event_source->firstRepeater;
    while(repeater){
        if(localEvents & repeater->triggerEvents){
            repeater->setEvents |= localEvents;
            osalEventBroadcastFlagsI(repeater->target, repeater->myEvent);
        }
        event_source->setEvents &=~ repeater->triggerEvents;
        repeater = repeater->nextRepeater;
    }
   
    /* Wake up any waiting threads that may be waiting on remaining events */
    if(event_source->setEvents){
        osalThreadResumeI(&event_source->waitThread, MSG_EVENT_W);
    }
}

void osalEventRepeaterRegister(event_repeater_t* event_repeater,
                               event_source_t* event_source,
                               eventflags_t trigger_events,
                               event_source_t* target_event_sink,
                               eventflags_t my_event){
    osalDbgCheck(event_source != NULL);
    osalDbgCheck(event_repeater != NULL);
    osalDbgCheck(target_event_sink != NULL);
    osalDbgCheckClassS();

    /* Initialize the structure */
    event_repeater->triggerEvents = trigger_events;
    event_repeater->myEvent = my_event;
    event_repeater->setEvents = 0;
    event_repeater->target = target_event_sink;
    event_repeater->source = event_source;

    /* Insert at the beginning */
    event_repeater_t* oldFirstRepeater = event_source->firstRepeater;
    event_source->firstRepeater = event_repeater;
    event_repeater->nextRepeater = oldFirstRepeater;
    event_repeater->prevRepeater = NULL;
    if(oldFirstRepeater){
        oldFirstRepeater->prevRepeater = event_repeater;
    }

    /* Broadcast an empty event, to trigger the system should one already be waiting */
    osalEventBroadcastFlagsI(event_source, 0);
}

void osalEventRepeaterUnregister(event_repeater_t* event_repeater){
    /* Are we the first? */
    if(!event_repeater->prevRepeater){
        event_repeater->source->firstRepeater = event_repeater->nextRepeater;
        return;
    }

    /* The next ones previous is my previous */
    if(event_repeater->nextRepeater){
        event_repeater->nextRepeater->prevRepeater = event_repeater->prevRepeater;
    }

    /* The previous next one is my next */
    event_repeater->prevRepeater->nextRepeater = event_repeater->nextRepeater;
}

UBaseType_t uxSavedInterruptStatus;
