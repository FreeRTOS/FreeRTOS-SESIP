# Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# Licensed under the Apache License, Version 2.0 (the "License").
# You may not use this file except in compliance with the License.
# A copy of the License is located at
#     http://www.apache.org/licenses/LICENSE-2.0
# or in the "license" file accompanying this file. This file is distributed 
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either 
# express or implied. See the License for the specific language governing 
# permissions and limitations under the License.
#
# AWS IoT OTA Update Script
# Important Note: Requires Python 3

import pathlib
import re
from pathlib import Path
from shutil import copyfile
import random
import boto3
from botocore.exceptions import ClientError
import sys, argparse
import subprocess
import json

parser = argparse.ArgumentParser(description='Script to start OTA update')
parser.add_argument("--thing-name", help="Name of thing",required=True)
parser.add_argument("--s3bucket", help="S3 bucket to store firmware updates", required=True)
parser.add_argument("--otasigningprofile", help="Signing profile to be created or used", required=True)
parser.add_argument("--signingcertificateid", help="certificate id (not arn) to be used", required=True)
parser.add_argument("--codelocation", help="base folder location (can be relative)",default="../", required=False)
args=parser.parse_args()


ota_update_role_name = "FreeRTOSOTAUpdate-2"

ota_update_role_trust_policy = {
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "",
      "Effect": "Allow",
      "Principal": {
        "Service": [
          "iot.amazonaws.com"
        ]
      },
      "Action": "sts:AssumeRole"
    }
  ]
}

ota_update_role_iam_pass_policy = {
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "iam:GetRole",
                "iam:PassRole"
            ],
            "Resource": "arn:aws:iam::*:role/*"
        }
    ]
}

ota_update_role_ota_update_policy = {

    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "s3:GetObjectVersion",
                "s3:PutObject",
                "s3:GetObject",
                "s3:ListBucketVersions"
            ],
            "Resource": "arn:aws:s3:::*"
        },
        {
            "Effect": "Allow",
            "Action": [
                "signer:StartSigningJob",
                "signer:DescribeSigningJob",
                "signer:GetSigningProfile",
                "signer:PutSigningProfile"
            ],
            "Resource": "*"
        },
        {
            "Effect": "Allow",
            "Action": [
                "s3:ListBucket",
                "s3:ListAllMyBuckets",
                "s3:GetBucketLocation"
            ],
            "Resource": "*"
        },
        {
            "Effect": "Allow",
            "Action": [
                "iot:DeleteJob"
            ],
            "Resource": "arn:aws:iot:*:*:job/AFR_OTA*"
        },
        {
            "Effect": "Allow",
            "Action": [
                "iot:DeleteStream"
            ],
            "Resource": "arn:aws:iot:*:*:stream/AFR_OTA*"
        },
        {
            "Effect": "Allow",
            "Action": [
                "iot:CreateStream",
                "iot:CreateJob"
            ],
            "Resource": "*"
        }
    ]
}

class AWS_IoT_OTA:

    def PrepareBinFile(self):
        # Create a copy f the object converting from ".axf" to ".bin"
        image_source_path = self.BUILD_PATH / Path("lpc54018iotmodule_freertos_sesip.axf") 
        command = "arm-none-eabi-objcopy --output-target=binary " + str(image_source_path) + " " + str(self.IMAGE_PATH)

        print("Command: " + str(command))

        try: 
             subprocess.run(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                shell=True,
                timeout=30,
                cwd=self.BUILD_PATH.absolute(),
            )

        except Exception as e:
            print("Error preparing bin file %s" % str(self.IMAGE_PATH))
            print(str(e))
            sys.exit()
        
        print("Prepared BIN file at %s" % str(self.IMAGE_PATH))


    # Copy the file to the s3 bucket
    def CopyFirmwareFileToS3(self):
        self.s3 = boto3.resource('s3')
        try:
            
            self.s3.create_bucket(Bucket=args.s3bucket, CreateBucketConfiguration={'LocationConstraint': args.region})
        except ClientError as error:
            if error.response['Error']['Code'] == 'BucketAlreadyOwnedByYou':
                print("Bucket %s already exists." % args.s3bucket )
            else:
                print("Error in creating s3 bucket %s" % error)

        try:
            self.s3.meta.client.put_bucket_versioning( Bucket=args.s3bucket,
                VersioningConfiguration={
                    'MFADelete': 'Disabled',
                    'Status': 'Enabled'})
            self.s3.meta.client.upload_file(str(self.IMAGE_PATH), args.s3bucket, self.IMAGE_NAME)
        except Exception as e:
            print("Error uploading file to s3: %s", e)
            sys.exit()


    # Get the latest version
    def GetLatestS3FileVersion(self):
        try: 
            versions=self.s3.meta.client.list_object_versions(Bucket=args.s3bucket, Prefix=self.IMAGE_NAME)['Versions']
            latestversion = [x for x in versions if x['IsLatest']==True]
            self.latestVersionId=latestversion[0]['VersionId']
            #print("Using version %s" % self.latestVersionId)
        except Exception as e:
            print("Error getting versions: %s" % e)
            sys.exit()
    
    def AttachPolicy(self, role_name, policy_arn):
        try:
           policy_attach_res = self.iam_client.attach_role_policy(
               RoleName=role_name,
               PolicyArn=policy_arn)
        except ClientError as error:
            print('Unexpected error occurred... hence cleaning up')
            iam_client.delete_role(RoleName= role_name)
            sys.exit()
    
    def CreateAndAttachPolicy(self, role_name, policy_name, policy_json):

        policy_arn = ''
        try:
            policy_res = self.iam_client.create_policy(
            PolicyName=policy_name,
            PolicyDocument=json.dumps(policy_json))
            policy_arn = policy_res['Policy']['Arn']
        except ClientError as error:
            if error.response['Error']['Code'] == 'EntityAlreadyExists':
                print("Policy %s already exists... hence using the same policy" % policy_name )
                policy_arn = 'arn:aws:iam::' + args.account + ':policy/' + policy_name
            else:
                print("Error creating policy %s" % error)
                iam_client.delete_role(RoleName= role_name)
                sys.exit()
        self.AttachPolicy(role_name, policy_arn)

    def CreateRole(self):
        role_exists = False
        self.iam_client = boto3.client('iam')
        try:
            create_role_res = self.iam_client.create_role(
                RoleName=ota_update_role_name,
                AssumeRolePolicyDocument=json.dumps(ota_update_role_trust_policy),
                Description='This is an OTA update role')
            self.role_arn = create_role_res['Role']['Arn']
        except ClientError as error:
            if error.response['Error']['Code'] == 'EntityAlreadyExists':
                print("Role %s already exists" % ota_update_role_name )
                role_exists = True
                self.role_arn = 'arn:aws:iam::' + args.account + ':role/' + ota_update_role_name
            else:
                print("Error creating role %s" % error)
                sys.exit()
        
        if role_exists is False:
            self.CreateAndAttachPolicy(ota_update_role_name, ota_update_role_name + '_iam_pass_policy', ota_update_role_iam_pass_policy)
            self.CreateAndAttachPolicy(ota_update_role_name, ota_update_role_name + '_ota_update_policy', ota_update_role_ota_update_policy)
            self.AttachPolicy(ota_update_role_name, 'arn:aws:iam::aws:policy/service-role/AWSIoTThingsRegistration' )
            self.AttachPolicy(ota_update_role_name, 'arn:aws:iam::aws:policy/service-role/AWSIoTLogging' )
            self.AttachPolicy(ota_update_role_name, 'arn:aws:iam::aws:policy/service-role/AWSIoTRuleActions' )

    # Create signing profile if it does not exist
    def CreateSigningProfile(self):
        try:
            signer = boto3.client('signer')
            profiles = signer.list_signing_profiles()['profiles']

            foundProfile=False
            afrProfile=None
            print("Searching for profile %s" % args.otasigningprofile)

            if len(profiles) > 0:
              for profile in profiles:
                if profile['profileName'] == args.otasigningprofile:
                    foundProfile = True
                    afrProfile = profile
            
            if (afrProfile != None):
                foundProfile=True
                print("Found Profile %s in account" % args.otasigningprofile)

            if (foundProfile == False):
                # Create profile
                newProfile = signer.put_signing_profile(
                    signingParameters={
                        'certname':'Code Verify Key'
                    },
                    profileName=args.otasigningprofile,
                    signingMaterial={
                        'certificateArn':self.SIGNINGCERTIFICATEARN   
                    },
                platformId='AmazonFreeRTOS-Default'
                )
                print("Created new signing profile: %s" % newProfile)
        except Exception as e:
            print("Error creating signing profile: %s" % e)
            sys.exit()


    def CreateOTAJob(self):
        
        # Create OTA job
        try:
            iot = boto3.client('iot')
            randomValue=random.randint(1, 65535)
            #Initialize the template to use
            files=[{
                'fileName': self.IMAGE_NAME,
                    'fileVersion': '1',
                    'fileLocation': {
                        's3Location': {
                            'bucket': args.s3bucket,
                            'key': self.IMAGE_NAME,
                            'version': self.latestVersionId
                        }
                    },
                    'codeSigning':{
                        'startSigningJobParameter':{
                            'signingProfileName': args.otasigningprofile,
                            'destination': {
                                's3Destination': {
                                    'bucket': args.s3bucket
                                }
                            }
                        }
                    }    
                }] 

            target="arn:aws:iot:"+args.region+":"+args.account+":"+args.devicetype+"/"+args.thing_name
            updateId="nxp-"+str(randomValue)

            print ("Files for update: %s" % files)
            
            ota_update=iot.create_ota_update(
                otaUpdateId=updateId,
                targetSelection='SNAPSHOT',
                files=files,
                protocols=['MQTT', 'HTTP'],
                targets=[target],
                roleArn=self.role_arn
            )

            print ("######################################################")
            print("OTA Update Job Status: %s" % ota_update['otaUpdateStatus'])
            print("OTA Update Job ID: %s" % "AFR_OTA-"['otaUpdateId'])
            print("OTA Update Job ARN: %s" % ota_update['otaUpdateArn'])
            print("########################################################")


        except Exception as e:
            print("Error creating OTA Job: %s" % e)
            sys.exit()


    def __init__(self):
        boto3.setup_default_session()
        self.Session = boto3.session.Session()
        
        args.region = self.Session.region_name
        
        # Get account Id
        args.account=boto3.client('sts').get_caller_identity().get('Account')

        args.devicetype = "thing"
        
        self.SIGNINGCERTIFICATEARN="arn:aws:acm:"+args.region+":"+args.account+":certificate/"+args.signingcertificateid

        print("Certificate ARN: %s" % self.SIGNINGCERTIFICATEARN)

        self.BUILD_PATH = Path(args.codelocation) / Path("Debug/")
        self.IMAGE_NAME = "lpc54018iotmodule_freertos_sesip.bin"
        self.IMAGE_PATH = self.BUILD_PATH / Path(self.IMAGE_NAME)
    

    def DoUpdate(self):
        self.PrepareBinFile()
        self.CopyFirmwareFileToS3()
        self.GetLatestS3FileVersion()
        self.CreateRole()
        self.CreateSigningProfile()
        self.CreateOTAJob()


def main(argv):
    ota = AWS_IoT_OTA()
    ota.DoUpdate()
   

if __name__ == "__main__":
    main(sys.argv[1:])
