# FreeRTOS with ChibiOS HAL

This repository contains all code needed to build the ChibiOS HAL together with the FreeRTOS real-time operating system. An example project is included in the folder example.

First, you should run make in the FreeRTOS directory. This will create a shared library containing FreeRTOS. You can edit the settings in FreeRTOSConfig.h to match your project.

Then, run make in the example folder to create the binary.
