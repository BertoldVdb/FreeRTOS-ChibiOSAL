/*
    ChibiOS - Copyright (C) 2006..2016 Giovanni Di Sirio
    	      Copyright (C) 2017 Bertold Van den Bergh

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/


#include "osal.h"

/* Forward IRQs to FreeRTOS */
void __attribute__((naked)) PendSV_Handler(void){
        xPortPendSVHandler();
}

void __attribute__((naked)) SysTick_Handler(void){
        xPortSysTickHandler();
}

void __attribute__((naked)) SVC_Handler(void) {
        vPortSVCHandler();
}

