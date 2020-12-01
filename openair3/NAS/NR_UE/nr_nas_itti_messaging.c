
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

#include <string.h>

#include "intertask_interface.h"
#include "nr_nas_itti_messaging.h"
#include "msc.h"

int nr_nas_itti_nas_establish_req(as_cause_t cause, as_call_type_t type, as_stmsi_t s_tmsi, plmn_t plmnID, Byte_t *data, uint32_t length, int user_id) {
  MessageDef *message_p;
  message_p = itti_alloc_new_message(TASK_NAS_NRUE, NAS_CONN_ESTABLI_REQ);
  NAS_CONN_ESTABLI_REQ(message_p).cause                       = cause;
  NAS_CONN_ESTABLI_REQ(message_p).type                        = type;
  NAS_CONN_ESTABLI_REQ(message_p).s_tmsi                      = s_tmsi;
  NAS_CONN_ESTABLI_REQ(message_p).plmnID                      = plmnID;
  NAS_CONN_ESTABLI_REQ(message_p).initialNasMsg.data          = data;
  NAS_CONN_ESTABLI_REQ(message_p).initialNasMsg.length        = length;
  MSC_LOG_TX_MESSAGE(
    MSC_NAS_UE,
    MSC_RRC_UE,
    NULL,0,
    "0 NAS_CONN_ESTABLI_REQ MME code %u m-TMSI %u PLMN %X%X%X.%X%X%X",
    s_tmsi.MMEcode, s_tmsi.m_tmsi,
    plmnID.MCCdigit1, plmnID.MCCdigit2, plmnID.MCCdigit3,
    plmnID.MNCdigit1, plmnID.MNCdigit2, plmnID.MNCdigit3);
  return itti_send_msg_to_task(TASK_RRC_NRUE, user_id, message_p);
}

int nr_nas_itti_ul_data_req(const uint32_t ue_id, void *const data, const uint32_t length, int user_id) {
  MessageDef *message_p;
  message_p = itti_alloc_new_message(TASK_NAS_NRUE, NAS_UPLINK_DATA_REQ);
  NAS_UPLINK_DATA_REQ(message_p).UEid          = ue_id;
  NAS_UPLINK_DATA_REQ(message_p).nasMsg.data   = data;
  NAS_UPLINK_DATA_REQ(message_p).nasMsg.length = length;
  return itti_send_msg_to_task(TASK_RRC_NRUE, user_id, message_p);
}