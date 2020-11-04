# Provisioning Script

This folder contains a script that will orchestrate the provisioning of a new device. The script will bridge the connection between the device and AWS IoT Core.
The communication occurs over the UART serial port, so it is important to make sure that the serial port is properly connected to the device executing the script, and that the 
device is writing to the correct serial port.

At a high level the script will receive a CSR from the device over the UART. Once that is done the script will:
* Share the device generated CSR with AWS IoT Core 
* Retrieve a certificate from AWS IoT Core based on said CSR
* Create a policy that allows for publishing to any topic
* Create a thing and attach the certificate.

## Warning
This script assumes you WANT to provisin the device, and will reprovision every time. If you DO NOT want to provision the device, use a different serial program and ignore the 
prompt to reprovision, it will time out after a few seconds.

## Prerequisites
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

## Running the script
To provision the device, simple run the script with the following commands:
`python provision.py --thing-name {{desired_thing_name}} --uart-serial-port {{nxp_serial_port}}`
Once finished provisioning, the script will output `Provisioning script has ended.` to the terminal. It is safe to end the program and use a different serial program, but the script will continue 
reading from the serial port for convenience.


# OTA Update Script

There is an ota update script in python provided for easy creation of OTA jobs in AWS. The script takes in as input a thing name, s3 bucket name to store the image, signing profile name and certificate ID for the OTA code signing certificate. The certificate ID can be obtained by checking the logs from the provisioning script, when the OTA code signing certificate is provisioned.

The script assumes the thing name and associated credentials are already provisioned using the provisioning script. S3 bucket name should be a globally unique name, the script tries to create the bucket in the region if not already created. Script also creates a signing profile with the name provided if not already present. It converts the binary firmware image in ".axf" to ".bin" format, uploads the image to the specified s3 bucket and creates an OTA job.

## Prerequisites
* Python 3.6 or greater
* boto3 
    * Install with `pip install boto3`
* aws cli
    * https://docs.aws.amazon.com/cli/latest/userguide/cli-chap-install.html
    * Be sure to have run `aws configure` before running the script! The script will read the default account information, as well as region and output configuration created by this command.
* arm-none-eabi-objcopy
    * Used to convert the image to binary format.
    * Arm tools needs to be installed and the path needs to be added to the environment variable.
    * Follow the steps [here](https://mynewt.apache.org/latest/get_started/native_install/cross_tools.html#installing-the-arm-cross-toolchain) to install arm toolchain and link the path to the environment variable.

## Running the script
You can run the ota update script to start the ota update as follows:
`python ota_update.py --thing-name <thing name> --s3bucket <s3 bucket name> --otasigningprofile <signing profile name> --signingcertificateid <signing certificate ID>`
Signing certificate ID can be obtained from the logs of provisioning script when ota code signing key is provisioned to the device.

Once the script completes successfully following logs should be printed:
```
######################################################
OTA Update Job Status: CREATE_PENDING
OTA Update Job ID: <ota job id>
OTA Update Job ARN: <ota job arn>
########################################################
```

You can retrieve the status of the job created using the following command:
`aws iot describe-job --job-id <ota job id>`

The "status" field should be "QUEUED" or "IN_PROGRESS" as the OTA is queued or starts execution on the device.

Once the OTA update completes (succeeded or Failed on the device), status should change to "COMPLETED"

To cancel ongoing job, execute the following command:
`aws iot cancel-job --job-id <ota job id>`

To delete a job execute the following command. To delete the job, cancel the job first.
`aws iot delete-job --job-id <ota job id>`
