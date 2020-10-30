import serial
import argparse
import uuid
from time import sleep
import re
import boto3
import json

class IoTAgent:
    def __init__(self, client):
        self.client = client
        self.policy = {
            "Version": "2012-10-17",
            "Statement": [{"Effect": "Allow", "Action": "iot:*", "Resource": "*"}],
        }

    def upload_csr(self, csr):
        response = self.client.create_certificate_from_csr(
            certificateSigningRequest=csr, setAsActive=True
        )
        return response

    def create_policy(self, policy_name, policy_doc):
        try:
            response = self.client.create_policy(
                policyName=policy_name,
                policyDocument=policy_doc,
                tags=[
                    {"Key": "CreatedBy", "Value": "AutoGen"},
                ],
            )
            return response
        except Exception as e:
            print(e)
        return None

    def create_thing(self, thing_name):
        response = self.client.create_thing(
            thingName=thing_name,
        )
        return response

    def attach_policies(self, policy_name, cert_arn):
        self.client.attach_policy(policyName=policy_name, target=cert_arn)

    def attach_cert_to_thing(self, thing_name, cert_arn):
        response = self.client.attach_thing_principal(
            thingName=thing_name, principal=cert_arn
        )
        
    def get_endpoint(self):
        response = self.client.describe_endpoint(
                        endpointType='iot:Data-ATS'
                    )
        return response['endpointAddress']

class UartInterface():
    def __init__(self, serial):
        self.serial = serial
        self.terminate_string = ">>>>>>"

    def write(self, message):
        """
        Write data buffer over any kind of stream, eg UART, SPI, WiFi.
        Returns total data written
        """
        total_written = 0
        for char in message:
            self.serial.flush()
            # This is necessary or we will start dropping packets.
            sleep(0.01)
            written = self.serial.write(char.encode("ascii"))
            print(f"{char}", end='')
            total_written += written
        print()

        self.serial.flush()
        written = self.serial.write(self.terminate_string.encode("ascii"))
        total_written += written

        assert total_written == len(message) + len(self.terminate_string)
        return total_written

    def read(self, stopper, length=-1):
        """
        Read data over any kind of stream, eg UART, SPI, WiFi.
        The read will stop if the end_car is read, or the length is exceeded.
        Timeouts are configured in the implementation.
        Returns the read string encoded in utf-8
        """
        read_string = ""
        read_length = -1
        read_line = ""
        decoded_char = ""

        while True:
            if (stopper in read_string) or (length > 0 and read_length >= length):
                break

            c = self.serial.read(size=1)
            try:
                decoded_character = c.decode("ascii")
            except UnicodeDecodeError as e:
                decoded_character = hex(int(str(ord(c))))

            read_string += decoded_character
            read_line += decoded_character            

            read_length += 1
            if c == b"\n":
                print(read_line)
                read_line = ""

        # We still want to see the message we are terminating the read on.
        print(read_line)
        return read_string


def initialize_uart(port):
    """
    Initialize the serial port used to communicate with the device.
    Returns the serial port object.
    """
    print("Initializing host machine UART.")
    term = serial.Serial(
        port=port,
        baudrate=115200,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS,
        timeout=20,
    )
    uart = UartInterface(term)
    return uart


def provision(stream_interface, thing_name):
    """
    Coordinate the provisioning process given a streaming interface and 
    a thing name.
    """
    print("Beginning provisioning script...")
    client = boto3.client('iot')
    agent = IoTAgent(client)

    device_output = stream_interface.read("y/n", -1)
    if "Device was already provisioned" in device_output:
        user_input = input()
        stream_interface.write(user_input)
        device_output = stream_interface.read("y/n", -1)
    if "Do you want to provision the device" in device_output:
        user_input = input()
        stream_interface.write(user_input)
        device_output = stream_interface.read("read thing name", -1)
        stream_interface.write(thing_name)
        device_output = stream_interface.read("read thing endpoint", -1)
        stream_interface.write(agent.get_endpoint())
        device_output = stream_interface.read("Finished outputting CSR", -1)
        csr = ""
        if "Finished outputting CSR" in device_output:
            csr = re.search(r"(-----BEGIN CERTIFICATE REQUEST-----((?:.*\n)+)-----END CERTIFICATE REQUEST-----)", device_output).group(0)
            device_output = stream_interface.read("Ready to read device certificate", -1)
            if "Ready to read device certificate" in device_output and csr != "":
                pem = provision_to_iot_core(csr, thing_name)
                print("Writing x509 device certificate PEM to device.")
                sleep(1)
                stream_interface.write(pem.strip())
                print("Finished writing certificate to device.")
    print("======================\nProvisioning script has ended. The script will continue to read the serial port, but you can now end the python program by entering `ctrl+c`")
    # Just sit here and read until the user exits
    device_output = stream_interface.read("!!!!!!!!!!!!!!", -1)

def provision_to_iot_core(csr, thing_name):
    """
    Given CSR and thing name, create a fully authenticated thing in AWS IoT Core.
    """
    client = boto3.client('iot')
    agent = IoTAgent(client)

    policy_name = "DemoPolicy"
    policiy_info = agent.create_policy(policy_name, json.dumps(agent.policy))
    
    agent.create_thing(thing_name)

    data = agent.upload_csr(csr)
    print(json.dumps(data, indent=4))

    agent.attach_policies(policy_name, data["certificateArn"])
    agent.attach_cert_to_thing(thing_name, data["certificateArn"])

    return data['certificatePem']

def main(args):
    uart = initialize_uart(args.uart_serial_port)
    provision(uart, args.thing_name)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="""
        This script provisions devices over various interfaces. Once provisioned,
        the device will be able to authenticate to AWS IoT Core.
        """
    )
    parser.add_argument(
        "--thing-name",
        type=str,
        help="Name of the IoT thing to create.",
        default=f"generated-thing-{uuid.uuid4()}",
    )
    parser.add_argument(
        "--teardown",
        type=bool,
        help="Destroy previously generated thing anmes.",
        default=False,
    )
    parser.add_argument(
        "--uart-serial-port",
        type=str,
        help="Name of the UART serial port to write over. If defined will attempt to write credentials over the UART interface.",
    )

    args = parser.parse_args()
    main(args)
