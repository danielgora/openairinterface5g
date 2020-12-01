/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#ifndef NAS_NRUE_TASK_H_
#define NAS_NRUE_TASK_H_

#include "openairinterface5g_limits.h"

//CUC：add nas_nrue_task.h √
#include "platform_types.h"
#include "nr_nas_itti_messaging.h"
#include "nas_log.h"
#include "TLVDecoder.h"
#include "TLVEncoder.h"
#include "nr_nas_msg_sim.h"


void *nas_nrue_task(void *args_p);
void nr_nas_proc_dl_transfer_ind (Byte_t *data, uint32_t len);
int decodeNasMsg(MM_msg *msg, uint8_t *buffer, uint32_t len);

int string2ByteArray(char* input,uint8_t* output); //CUC:test
void tesths(void);

#endif /* NAS_TASK_H_ */
