# Introduction
This folder contains a script that will retrieve a CSR from the device over the UART. Once that is done the script will:
* Share the device generated CSR with AWS IoT Core 
* Retrieve a certificate from AWS IoT Core based on said CSR
* Create a policy that allows for publishing to any topic
* Create a thing and attach the certificate.

You will need to have configured the source code with the proper MACRO (TBD) to specify the thing name in the device firmware. 

# Prerequisites
* Python 3.6 or greater
* boto3 
    * Install with `pip install boto3`
* pyserial 
    * Install with `pip install pyserial`
* aws cli
    * https://docs.aws.amazon.com/cli/latest/userguide/cli-chap-install.html
    * Be sure to have run `aws configure` before running the script! It relies on the default credentials created by aws cli.
* device connected over the debugger and detected by the computer
    * On OSX, the UART can generally be found with the command `ls /dev/cu*`
        * As an example, my NXP UART appears as **/dev/cu.usbmodemBSAZAQGQ2**
    * On Linux, it is the same as OSX, but the devices are named with a **tty** style, use `l /dev/tty*`
    * On Windows, generally the device will be labelled as a COM port. The available COM ports can be checked by opening the device manager application.

# Running the script
To provision the device, simple run the script with the following commands:
`python provision.py --thing-name {{desired_thing_name}} --uart-serial-port {{nxp_serial_port}}
You will get a series of prompts, you must only enter either 'y' or 'n', the device is configured to only read one character. 

The prompts will ask if you'd like to provision the device, destory existing credentials, etc.

