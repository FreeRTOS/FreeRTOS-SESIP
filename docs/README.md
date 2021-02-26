## Getting started

### Step 1 Clone the repository to your local machine
To clone using HTTPS:
```
git clone https://github.com/FreeRTOS/Lab-Project-FreeRTOS-SESIP.git --recurse-submodules
```
Using SSH:
```
git clone https://github.com/FreeRTOS/Lab-Project-FreeRTOS-SESIP.git --recurse-submodules
```

If you have downloaded the repo without using the --recurse-submodules argument, or you just want to update the submodules you need to run:

git submodule update --init --recursive

### Step 2 Download and install MCUExpresso

You can set up your build environment following the instructions at https://docs.aws.amazon.com/freertos/latest/userguide/getting_started_nxp.html for this board.

Under the section "Build and run the FreeRTOS Demo project" you will want to import this project instead of the one in that guide. The project files for this project are located in the root, so you should be able to just import from the root folder which should be called SESIP_Demo

Additionally, for FreeRTOS kernel feature information refer to the [Developer Documentation](https://www.freertos.org/features.html), and [API Reference](https://www.freertos.org/a00106.html).

### Step 3 Provision

Go to the [tools](https://github.com/FreeRTOS/Lab-Project-FreeRTOS-SESIP/tree/main/tools) folder and execute the provisioning script to prepare your hardware for AWS connectivity.

### Getting help

If you have any questions or need assistance troubleshooting your FreeRTOS project, we have an active community that can help on the [FreeRTOS Community Support Forum](https://forums.freertos.org). Please also refer to [FAQ](http://www.freertos.org/FAQHelp.html) for frequently asked questions.

## Repository structure

The repository structure is as follows:

Lab-Project-FreeRTOS-SESIP
├── docs                                       *Documentation*
│   └── wolfSSL-migration-guide                *How to convert from MBED-TLS to wolfSSL for pkcs11*
├── lib                                        *Libraries used to build this project*
│   ├── FreeRTOS                               *Libraries provided by FreeRTOS*
│   │   ├── FreeRTOS-Kernel                    *FreeRTOS Kernel*
│   │   ├── FreeRTOS-Plus-TCP                  *FreeRTOS TCP/IP stack*
│   │   ├── FreeRTOS-Plus-Trace                *FreeRTOS Trace interface*
│   │   ├── Logging                            *Simple UART Logging library*
│   │   ├── coreMQTT                           *MQTT*
│   │   ├── corePKCS11                         *PKCS11 interface*
│   │   ├── ota-for-aws-iot-embedded-sdk       *AWS OTA Library*
│   │   ├── platform                           *Platform Interfaces for TLS & PKCS11*
│   │   └── provision                          *Interface to AWS provisioning scripts*
│   ├── bootloader                             *NXP Bootloader*
│   ├── mbedtls                                *MBEDTLS*
│   ├── nxp                                    *NXP HAL's and platform interfaces*
│   └── pkcs11                                 *PKCS11*
├── linkscripts                                *linker scripts for this application including MPU partitions*
├── source                                     *The application*
│   └── user                                   *MPU User level code for the application*
└── tools                                      *Provisioning scripts for AWS & Bootloading

### FreeRTOS sources
All of the FreeRTOS components are included the lib/FreeRTOS directory of this repo.  Most of these components are submoduled into this project to facilitate an applicate first project organization.  The links below are to the root of each library.  The version submoduled will be a released version for the SESIP certification.

#### Kernel
The FreeRTOS Kernel Source is in [FreeRTOS/FreeRTOS-Kernel repository](https://github.com/FreeRTOS/FreeRTOS-Kernel), and it is consumed as a submodule in this repository.

#### FreeRTOS-Plus-TCP
The FreeRTOS-Plus-TCP source is in [FreeRTOS/FreeRTOS-Plus-TCP repository](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP), and it is consumed as a submodule in this repository.

#### coreMQTT
The coreMQTT source is in [FreeRTOS/coreMQTT](https://github.com/FreeRTOS/coreMQTT), and it is consumed as a submodule in this repository.

#### corePKCS11
The corePKCS11 source is in [FreeRTOS/corePKCS11](https://github.com/FreeRTOS/corePKCS11), and it is consumed as a submodule in this repository.

#### ota-for-aws-iot-embedded
This library is part of the AWS collection of repositories that covers libraries specific to AWS technologies.  The ota-for-aws source is in [aws/ota-for-aws-iot-embedded-sdk](https://github.com/aws/ota-for-aws-iot-embedded-sdk), and it is consumed as a submodule in this repository. 

### Supplementary library sources

#### MBEDTLS
This project uses ARM's MBEDTLS.  The source for this library is in [ARMmbed/mbedtls](https://github.com/ARMmbed/mbedtls).  Note, a specific version was used for the submodule found in the lib/mbedtls directory.

#### pkcs11
This project uses a fork of the OASIS PKCS11 TC Repo.  The OASIS repo can be found [here](https://github.com/oasis-tcs/pkcs11).  The specific fork submoduled into this project is in [amazon-freertos/pkcs11](https://github.com/amazon-freertos/pkcs11).  Note, a specific version was used for the submodule found in the lib/pkcs11 directory.

#### NXP Supporting Files
All of the NXP files were extracted from the SDK provided by the [NXP MCUXpresso Softwre Development Kit](https://www.nxp.com/design/software/development-software/mcuxpresso-software-and-tools-/mcuxpresso-software-development-kit-sdk:MCUXpresso-SDK).

## Previous releases

This is the first release of this SESIP project.  There are no previous releases to this project.
