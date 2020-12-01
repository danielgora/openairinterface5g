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
#ifndef NR_NAS_ITTI_MESSAGING_H_
#define NR_NAS_ITTI_MESSAGING_H_

#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "assertions.h"
#include "intertask_interface.h"
#include "msc.h"


int nr_nas_itti_nas_establish_req(as_cause_t cause, as_call_type_t type, as_stmsi_t s_tmsi, plmn_t plmnID, Byte_t *data_pP, uint32_t lengthP, int user_id);

int nr_nas_itti_ul_data_req(const uint32_t ue_idP, void *const data_pP, const uint32_t lengthP, int user_id);

#endif /* NAS_ITTI_MESSAGING_H_ */
