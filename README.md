# Sending-STM-B-L475-IOT01-Motion-Data-to-AWS

This code was developed an compiled in the Mbed Online Keil Studio.

The outcome of this code is for the B-L475-IOT01 board to be able to recognize four different types of motion based on the built-in accelerometer data pushed through an embedded ML model. The classified result is then publishes the data to an AWS server using MQTT.

The types of motion it can classify are:

  - Idle
  - Snake
  - Wave
  - Up-Down

The mbed library (modified by me for this code) used to send data to AWS can be found here: https://github.com/ARMmbed/mbed-os-example-for-aws

The embedded ML was based on the code from: https://docs.edgeimpulse.com/docs/edge-impulse-cli/cli-data-forwarder#classifying-data-mbed-os

The dataset used for the Motion Recognition is originally from this source (I erased all the data from the original tutorial and recorded it all using the data acqusition feature in Edge Impulse): 
https://studio.edgeimpulse.com/public/84984/latest

To convert the AWS credential files downloaded when creating a "Thing" on AWS to the right format (aws_credentials.h), use the "Export_Create_Device_IoT_Credentials.py" script, and use the instructions in the file itself.

Enter your wi-fi connectivity details in "mbed_app.json".

Don't forget to extract the contents of "compile_commands.zip" and include that extracted JSON file in your code :) 

Other ZIP files which must be extracted (and their contents added to the code) include "BUILD.zip", "edge-impulse-sdk.zip" and "mbed-client-for-aws.zip".

 .
 .

Useful links in creating your AWS "Thing":

  - Setting IAM Accounts Access Policies: https://docs.aws.amazon.com/IAM/latest/UserGuide/getting-started_create-admin-group.html

  - Getting Started with STM32L4 Discovery Kit: https://docs.aws.amazon.com/freertos/latest/userguide/getting_started_st.html
  
  I have found that the way most tutorials tell us to set up policies often leads to connectivity issues, so for my hobby purposes, I leave the policies with the wild card symbol: 
  
  ![image](https://user-images.githubusercontent.com/50542181/179390283-c112d8ab-74aa-42ab-9ab8-1539d09fb54f.png)

.

Output file (not to be imported to the Online Keil IDE): RTOS_Sending_Motion_ML_Data_AWS_Latest.DISCO_L475VG_IOT01A.bin
