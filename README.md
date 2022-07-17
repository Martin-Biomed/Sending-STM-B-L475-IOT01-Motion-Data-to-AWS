# Sending-STM-B-L475-IOT01-Motion-Data-to-AWS

The following code is heavily based off of: 

- https://docs.edgeimpulse.com/docs/edge-impulse-cli/cli-data-forwarder#classifying-data-mbed-os

The dataset for this project can be found in:

- https://studio.edgeimpulse.com/public/84984/latest

This dataset originates from the original Edge Impulse example project, but I deleted and created my own custom sample database.

This code is written such that you can physically move the IoT Discovery Board with certain motions and be able to distinguish between four different classes:

- Idle
- Snake
- Wave
- Up-Down

To generate the ''aws_credential.h'' file in the right format, use the ''Export_Create_Device_Credentials.py'' script (contains instruction comments).

Modify your local Wi-Fi settings in "mbed_app.json".
