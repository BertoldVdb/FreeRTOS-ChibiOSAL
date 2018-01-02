/*
    ChibiOS - Copyright (C) 2006..2016 Giovanni Di Sirio

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

#include "hal.h"
#include "FreeRTOS.h" /* Must come first. */
#include "task.h"     /* RTOS task related API prototypes. */
#include "queue.h"    /* RTOS queue related API prototypes. */
#include "timers.h"   /* Software timer related API prototypes. */
#include "semphr.h"

void vTaskLed( void * pvParameters )
{
    (void)pvParameters;

    palSetPadMode(GPIOB, 3, PAL_MODE_OUTPUT_PUSHPULL);
    for( ;; )
    {
        palSetPad(GPIOB, 3);
        osalThreadSleepMilliseconds(250);
        palClearPad(GPIOB, 3);
        osalThreadSleepMilliseconds(250);
    }
}


int main(void) {

	/*
	 * System initializations.
	 * - HAL initialization, this also initializes the configured device drivers
	 *   and performs the board-specific initializations.
	 */
	halInit();
	
    TaskHandle_t xHandle;
	xTaskCreate(vTaskLed, "LED", 512, NULL, 2, &xHandle );

	/*
	 * Enabling interrupts, initialization done.
	 */
	osalSysEnable();

	return 0;
}

void errorAssertCalled(const char* file, unsigned long line, const char* reason){
    (void)file;
    (void)line;
    (void)reason;
	while(1);
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName ){
    (void)xTask;
    (void)pcTaskName;
	while(1);
}
