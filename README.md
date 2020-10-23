

# Cloning
## CorePKCS11
After cloning and recursively initializing submodules, navigate to the corePKCS11 library and enter the following command
`git submodule update --init --recursive --checkout 3rdparty/pkcs11 3rdparty/mbedtls`

# Provisioning
In order to connect to AWS IoT Core, devices must be provisioned with a certificate and private key that authenticates it to IoT Core.

For a guide and script to provision your device, please read the [README](tools/README.md) in the tools directory.
