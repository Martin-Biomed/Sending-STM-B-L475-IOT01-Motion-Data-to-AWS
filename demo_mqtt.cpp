/*
 * Copyright (c) 2020-2021 Arm Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#if !MBED_CONF_AWS_CLIENT_SHADOW

#include "mbed.h"
#include "mbed-trace/mbed_trace.h"
#include "rtos/ThisThread.h"
#include "rtos/Mutex.h"
#include "AWSClient/AWSClient.h"

#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"
#include "stm32l475e_iot01_magneto.h"
#include "stm32l475e_iot01_gyro.h"
#include "stm32l475e_iot01_accelero.h"

extern "C" {
#include "core_json.h"
}

#define TRACE_GROUP "Main"



#endif // !MBED_CONF_AWS_CLIENT_SHADOW