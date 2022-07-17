/*
 * Copyright (c) 2020-2021 Arm Limited
 * SPDX-License-Identifier: Apache-2.0
 */

 // Original code based on: https://os.mbed.com/teams/mbed-os-examples/code/mbed-client-for-aws/
 
 // ML Inferencing Code: https://github.com/edgeimpulse/example-standalone-inferencing-mbed
 // Additional Inferencing Support: https://docs.edgeimpulse.com/docs/tutorials/running-your-impulse-locally/running-your-impulse-mbed

// Guidance on an AWS Connectivity Issue and Resolution I had: https://github.com/ARMmbed/mbed-os-example-for-aws/issues

// This code is only set up to publish to AWS, it has not been developed to receive messages as it does not have a shadow

// Moved most of the contents of the original version of demo_mqtt.cpp to the main.cpp file

#include "mbed.h"
#include "mbed-trace/mbed_trace.h"
#include "rtos/Mutex.h"
#include "rtos.h"
#include "AWSClient/AWSClient.h"
#include "aws_credentials.h"

#include <string.h> 

// Sensors drivers present in the BSP library
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"
#include "stm32l475e_iot01_magneto.h"
#include "stm32l475e_iot01_gyro.h"
#include "stm32l475e_iot01_accelero.h"

//////////////// Edge Impulse Libraries //////////////////

#include "ei_run_classifier.h"
#include "numpy.hpp"


extern "C" {
#include "core_json.h"
}

#define TRACE_GROUP "Main"

// Implemented by the two demos
void on_message_callback(
    const char *topic,
    uint16_t topic_length,
    const void *payload,
    size_t payload_length
);

int buf_size = 8;
int final_message_buf_size = 50;

static char base_message[8] = {0}; //Used to not be a pointer
static char prediction[8] = {0};
static char topic[8] = {0};
static char load[50] = {0};

int connected = 0; // Once the first iteration of the main() runs, this will "lock"
int connection_success = 0; // Once the first stage of connecting is completed successfully, this wil "lock"
int message_success = 0;

Mutex connection_mutex;
Thread thread1;
Thread thread2;
EventFlags event_flags; //Used to syncrhonize threads

//https://forums.mbed.com/t/difference-between-thisthread-sleep-and-wait/13988/2

#define INFERENCE_FLAG    (1UL << 0)  // keeps thread1 waiting until the zigbee is initialized
#define AWS_FLAG   (1UL << 1)

static bool reply_received = false;

////////////////// Edge Impulse Section ///////////////////////

static int64_t sampling_freq = EI_CLASSIFIER_FREQUENCY; // in Hz.
static int64_t time_between_samples_us = (1000000 / (sampling_freq - 1));

// to classify 1 frame of data you need EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE values
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

Timer t;

int16_t pDataXYZ[3] = {0};
int64_t next_tick = 0;
float biggest_value = 0; // Needed to declare variables here so I could jump between labels
int num_chars = 0;  // sprintf: If successful, it returns the total number of chars (stored in num_chars) written excluding null-character
size_t ix = 0;
int responses_processed = 0; // Lets us know if MQTT response is working
static int first_connect; //The first iteration of the send_data super loop


///////////////////////////////////////////////////////////////

void on_message_callback(
    const char *topic,
    uint16_t topic_length,
    const void *payload,
    size_t payload_length)
{
    char *json_value;
    size_t value_length;
    
    int timer;

    connection_mutex.lock();
    tr_info("Messaging Function Called");
    
    auto ret = JSON_Search((char *)payload, payload_length, "sender", strlen("sender"), &json_value, &value_length);
    if (ret == JSONSuccess && (strncmp(json_value, "device", strlen("device")) == 0)) {
        tr_info("Message sent successfully");
        message_success = 1;
    } else {
        ret = JSON_Search((char *)payload, payload_length, "message", strlen("message"), &json_value, &value_length);
        if (ret == JSONSuccess) {
            reply_received = true;
            tr_info("Message received from the cloud: \"%.*s\"", value_length, json_value);
        } else {
            tr_error("Failed to extract message from the payload: \"%.*s\"", payload_length, (const char *) payload);
        }
        message_success = 0;
    }

    connection_mutex.unlock();
}

// Task to process MQTT responses at a regular interval
static int process_responses()
{
    AWSClient &client = AWSClient::getInstance();

    if (!client.isConnected()) {
        responses_processed = 0;
    }
    else{
        responses_processed = 1; 
    } 
    
    if (client.processResponses() != MBED_SUCCESS) {
        tr_error("AWSClient::processResponses() failed");
    }    
    return responses_processed;
}

void demo()
{
    while(1){

        event_flags.wait_all(INFERENCE_FLAG); //Works

        AWSClient &client = AWSClient::getInstance();

        // Subscribe to the topic
        char topic[] = MBED_CONF_APP_AWS_MQTT_TOPIC;
        int ret = client.subscribe(topic, strlen(topic));
        if (ret != MBED_SUCCESS) {
            tr_error("AWSClient::subscribe() failed");
        } 
                
        sprintf(load, "{\n"
            "    \"sender\": \"device\",\n"
            "    \"message\": \"%s\",\n"
            "}",
            base_message);
            
        tr_info("Publishing: \"%s\"", base_message);
        
        ret = client.publish(
                    topic,
                    strlen(topic),
                    load,
                    strlen(load)
        );

        if (ret != MBED_SUCCESS) {
            tr_error("AWSClient::publish() failed");
        }
        
        tr_info("Length of array (MQTT): \"%d\"\n", strlen(load));
        
        memset(load, 0, final_message_buf_size);

        ThisThread::sleep_for(500ms);
    }
}

void inference_thread(){

    while (1){

        biggest_value = 0;

        for (ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
                next_tick = t.read_us() + time_between_samples_us;
                BSP_ACCELERO_AccGetXYZ(pDataXYZ);

                // copy accelerometer data into the features array 
                features[ix + 0] = (float)pDataXYZ[0]/100;  
                features[ix + 1] = (float)pDataXYZ[1]/100;
                features[ix + 2] = (float)pDataXYZ[2]/100;

                while (t.read_us() < next_tick) {
                    /* busy loop */
                }
        }

        // frame full? then classify
        ei_impulse_result_t result = { 0 };

        // create signal from features frame
        signal_t signal;
        numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

        // run classifier 
        EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false); 
        tr_info("run_classifier returned: %d", res); //Used to be ei_printf
        //if (res != 0) return 1;

        // print predictions (Used to be ei_printf)
        tr_info("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): ",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);

        // print the predictions
        for (ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            printf("%s:\t%.5f\n", result.classification[ix].label, result.classification[ix].value); //Used to be ei_printf
            
            if (result.classification[ix].value >= biggest_value){
                memset(base_message, 0, buf_size);
                biggest_value = result.classification[ix].value;
                num_chars = sprintf(prediction, result.classification[ix].label);  //num_chars stores the number of chars after the prediction is filled             
            }
            strcat(base_message, prediction); //Concatenates the base_message format string with the prediction string
            memset(prediction, 0, buf_size);            
        }

        tr_info("Predicted Action: %s ", base_message);

        event_flags.set(INFERENCE_FLAG);

    }
}


int main()
{

    if (connected == 0){
    
        BSP_ACCELERO_Init();         
        connected = 1;
        t.start();
        goto connect;
    }


connect:
    
    // "goto" requires early initialization of variables
    AWSClient &client = AWSClient::getInstance();
    //rtos::Thread process_thread;
    AWSClient::TLSCredentials_t credentials;
    int ret;
    
    mbed_trace_init();
    tr_info("Connecting to the network...");
    auto network = NetworkInterface::get_default_instance();
    if (network == NULL) {
        tr_error("No network interface found");
        goto disconnect;
    }
    ret = network->connect();
    if (ret != 0) {
        tr_error("Connection error: %x", ret);
        goto disconnect;
    }
    
    tr_info("MAC: %s", network->get_mac_address());
    tr_info("Connection Success");

    // Set credentials
    credentials.clientCrt = aws::credentials::clientCrt;
    credentials.clientCrtLen = sizeof(aws::credentials::clientCrt);
    credentials.clientKey = aws::credentials::clientKey;
    credentials.clientKeyLen = sizeof(aws::credentials::clientKey);
    credentials.rootCrtMain = aws::credentials::rootCA;
    credentials.rootCrtMainLen = sizeof(aws::credentials::rootCA);

    // Initialize client
    ret = client.init(
              on_message_callback,
              credentials
          );
          
    if (ret != MBED_SUCCESS) {
        tr_error("AWSClient::init() failed");
        goto disconnect;
    }

    // Connect to AWS IoT Core
    ret = client.connect(
              network,
              credentials,
              MBED_CONF_APP_AWS_ENDPOINT,
              MBED_CONF_APP_AWS_CLIENT_IDENTIFIER
          );
    if (ret != MBED_SUCCESS) {
        tr_error("AWSClient::connect() failed");
        goto disconnect;
    }

    // Start a background thread to process MQTT
    //ret = process_thread.start(process_responses);
    
    ret = process_responses();
    
    if (ret != 1) {
        tr_error("Failed to start thread to process MQTT");
        goto disconnect;
    }
    else{
        connection_success = 1;
        first_connect = 1;
        tr_info("Entering send_data section");
        goto send_data;
    }
    

disconnect:
    if (connection_success == 0){
        if (client.isConnected()) {
            ret = client.disconnect();
            
            if (ret != MBED_SUCCESS) {
                tr_error("AWS::disconnect() failed");
            }
        }
    
        ret = network->disconnect();
        if (ret != MBED_SUCCESS) {
            tr_error("NetworkInterface::disconnect() failed");
        }
        
    }


    // Run a demo depending on the configuration in mbed_app.json:
    // * demo_mqtt.cpp if aws-client.shadow is unset or false
    // * demo_shadow.cpp if aws-client.shadow is true
    
send_data: 
    
    // fill the features array
    
        if (first_connect == 1){ // To be run only once in the send_data super loop
        
            strcat(topic, MBED_CONF_APP_AWS_MQTT_TOPIC);
            tr_info("Subscribed to topic: %s", topic);
            ret = client.subscribe(topic, strlen(topic));
            
            if (ret != MBED_SUCCESS) {
                tr_error("AWSClient::subscribe() failed");
            }
            
            first_connect = 0;
        }
        
        thread1.start(callback(inference_thread));

        thread2.start(callback(demo));

}