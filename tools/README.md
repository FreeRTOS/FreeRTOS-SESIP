# Introduction
This folder contains a script that will orchestrate the provisioning of a new device. The script will bridge the connection between the device and AWS IoT Core.
The communication occurs over the UART serial port, so it is important to make sure that the serial port is properly connected to the device executing the script, and that the 
device is writing to the correct serial port.

At a high level the script will receive a CSR from the device over the UART. Once that is done the script will:
* Share the device generated CSR with AWS IoT Core 
* Retrieve a certificate from AWS IoT Core based on said CSR
* Create a policy that allows for publishing to any topic
* Create a thing and attach the certificate.

# Warning
This script assumes you WANT to provisin the device, and will reprovision every time. If you DO NOT want to provision the device, use a different serial program and ignore the 
prompt to reprovision, it will time out after a few seconds.

# Prerequisites
* Python 3.6 or greater
* boto3 
    * Install with `pip install boto3`
* pyserial 
    * Install with `pip install pyserial`
* aws cli
    * https://docs.aws.amazon.com/cli/latest/userguide/cli-chap-install.html
    * Be sure to have run `aws configure` before running the script! The script will read the default account information, as well as region and output configuration created by this command.
* device connected over the debugger and detected by the computer
    * On OSX, the UART can generally be found with the command `ls /dev/cu*`
        * As an example, my NXP UART appears as **/dev/cu.usbmodemBSAZAQGQ2**
    * On Linux, it is the same as OSX, but the devices are named with a **tty** style, use `ls /dev/tty*`
    * On Windows, generally the device will be labelled as a COM port. The available COM ports can be checked by opening the device manager application.
* OpenSSL
    * OpenSSL is required to create a self-signed certificate and private key to be used by OTA. OpenSSL is commonly distributed with most operating systems, but can be found here https://www.openssl.org/.

# Running the script
To provision the device, simple run the script with the following commands:
`python provision.py --thing-name {{desired_thing_name}} --uart-serial-port {{nxp_serial_port}}`
Once finished provisioning, the script will output `Provisioning script has ended.` to the terminal. It is safe to end the program and use a different serial program, but the script will continue 
reading from the serial port for convenience.


