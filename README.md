## Introduction
This project is an example of a IoT application using FreeRTOS.

## Getting started

### Step1 Clone the repository to your local machine
To clone using HTTPS:
```
git clone https://github.com/FuschiaPlatinumFoxTerrier/SESIP_Demo --recurse-submodules
```
Using SSH:
```
git clone git@github.com:FuschiaPlatinumFoxTerrier/SESIP_Demo --recurse-submodules
```
### Step2 Download and install MCUExpresso

You can set up your build environment following the instructions at https://docs.aws.amazon.com/freertos/latest/userguide/getting_started_nxp.html for this board.

Under the section "Build and run the FreeRTOS Demo project" you will want to import this project instead of the one in that guide. The project files for this project are located in the root, so you should be able to just import from the root folder which should be called SESIP_Demo


Additionally, for FreeRTOS kernel feature information refer to the [Developer Documentation](https://www.freertos.org/features.html), and [API Reference](https://www.freertos.org/a00106.html).

### Getting help
If you have any questions or need assistance troubleshooting your FreeRTOS project, we have an active community that can help on the [FreeRTOS Community Support Forum](https://forums.freertos.org). Please also refer to [FAQ](http://www.freertos.org/FAQHelp.html) for frequently asked questions.


# Provisioning
In order to connect to AWS IoT Core, devices must be provisioned with a certificate and private key that authenticates it to IoT Core.

For a guide and script to provision your device, please read the [README](tools/README.md) in the tools directory.
