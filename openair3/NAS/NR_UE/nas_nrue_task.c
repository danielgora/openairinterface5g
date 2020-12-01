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

#define NAS_BUILT_IN_UE 1 //QUES: #undef
// #define __LITTLE_ENDIAN_BITFIELD 1

#include "utils.h"
# include "assertions.h"
# include "intertask_interface.h"
# include "nas_nrue_task.h"
# include "common/utils/LOG/log.h"

# include "user_defs.h"
# include "user_api.h"
# include "nas_parser.h"
# include "nas_proc.h"
# include "msc.h"
# include "memory.h"

#include "nas_user.h"

// FIXME make command line option for NAS_UE_AUTOSTART
# define NAS_UE_AUTOSTART 1

// FIXME review these externs
extern unsigned char NB_eNB_INST;
extern uint16_t NB_UE_INST;

static int _nas_mm_msg_decode_header(mm_msg_header_t *header, const uint8_t *buffer, uint32_t len);

void *nas_nrue_task(void *args_p)
{
  int                   nb_events;
  struct epoll_event   *events;
  MessageDef           *msg_p;
  instance_t            instance;
  unsigned int          Mod_id;
  int                   result;
  nas_user_container_t *users=args_p;

  itti_mark_task_ready (TASK_NAS_NRUE);
  MSC_START_USE();
  
  while(1) {
    // Wait for a message or an event
    itti_receive_msg (TASK_NAS_NRUE, &msg_p);

    if (msg_p != NULL) {
      instance = ITTI_MSG_INSTANCE (msg_p);
      Mod_id = instance - NB_eNB_INST;
      if (instance == INSTANCE_DEFAULT) {
        printf("%s:%d: FATAL: instance is INSTANCE_DEFAULT, should not happen.\n",
               __FILE__, __LINE__);
        exit_fun("exit... \n");
      }

      switch (ITTI_MSG_ID(msg_p)) {
      case INITIALIZE_MESSAGE:
        LOG_I(NAS, "[UE %d] Received %s\n", Mod_id,  ITTI_MSG_NAME (msg_p));

        break;

      case TERMINATE_MESSAGE:
        itti_exit_task ();
        break;

      case MESSAGE_TEST:
        LOG_I(NAS, "[UE %d] Received %s\n", Mod_id,  ITTI_MSG_NAME (msg_p));
        break;

      case NAS_CELL_SELECTION_CNF:  //CUC：NAS_CELL_SELECTION_CNF √
        LOG_I(NAS, "[UE %d] Received %s: errCode %u, cellID %u, tac %u\n", Mod_id,  ITTI_MSG_NAME (msg_p),
              NAS_CELL_SELECTION_CNF (msg_p).errCode, NAS_CELL_SELECTION_CNF (msg_p).cellID, NAS_CELL_SELECTION_CNF (msg_p).tac);
        as_stmsi_t s_tmsi={0, 0};
        as_nas_info_t nas_info;
        plmn_t plmnID={0, 0, 0, 0};
        generateRegistrationRequest(&nas_info);
        nr_nas_itti_nas_establish_req(0, AS_TYPE_ORIGINATING_SIGNAL, s_tmsi, plmnID, nas_info.data, nas_info.length, 0);
        break;

      case NAS_CELL_SELECTION_IND:
        LOG_I(NAS, "[UE %d] Received %s: cellID %u, tac %u\n", Mod_id,  ITTI_MSG_NAME (msg_p),
              NAS_CELL_SELECTION_IND (msg_p).cellID, NAS_CELL_SELECTION_IND (msg_p).tac);

        /* TODO not processed by NAS currently */
        break;

      case NAS_PAGING_IND:
        LOG_I(NAS, "[UE %d] Received %s: cause %u\n", Mod_id,  ITTI_MSG_NAME (msg_p),
              NAS_PAGING_IND (msg_p).cause);

        /* TODO not processed by NAS currently */
        break;

      case NAS_CONN_ESTABLI_CNF:
        LOG_I(NAS, "[UE %d] Received %s: errCode %u, length %u\n", Mod_id,  ITTI_MSG_NAME (msg_p),
              NAS_CONN_ESTABLI_CNF (msg_p).errCode, NAS_CONN_ESTABLI_CNF (msg_p).nasMsg.length);

        break;

      case NAS_CONN_RELEASE_IND:
        LOG_I(NAS, "[UE %d] Received %s: cause %u\n", Mod_id,  ITTI_MSG_NAME (msg_p),
              NAS_CONN_RELEASE_IND (msg_p).cause);

        break;

      case NAS_UPLINK_DATA_CNF:
        LOG_I(NAS, "[UE %d] Received %s: UEid %u, errCode %u\n", Mod_id,  ITTI_MSG_NAME (msg_p),
              NAS_UPLINK_DATA_CNF (msg_p).UEid, NAS_UPLINK_DATA_CNF (msg_p).errCode);

        break;

      case NAS_DOWNLINK_DATA_IND: //CUC：NAS_DOWNLINK_DATA_IND √
        LOG_I(NAS, "[UE %d] Received %s: UEid %u, length %u\n", Mod_id,  ITTI_MSG_NAME (msg_p),
              NAS_DOWNLINK_DATA_IND (msg_p).UEid, NAS_DOWNLINK_DATA_IND (msg_p).nasMsg.length);
        nr_nas_proc_dl_transfer_ind (NAS_DOWNLINK_DATA_IND(msg_p).nasMsg.data, NAS_DOWNLINK_DATA_IND(msg_p).nasMsg.length); //handle dl info NAS mesaages.
        break;

      default:
        LOG_E(NAS, "[UE %d] Received unexpected message %s\n", Mod_id,  ITTI_MSG_NAME (msg_p));
        break;
      }

      result = itti_free (ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
      AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
      msg_p = NULL;
    }


  }

  free(users);
  return NULL;
}


void nr_nas_proc_dl_transfer_ind (Byte_t *data, uint32_t len) { 
  as_nas_info_t nas_info;
  MM_msg msg;
  decodeNasMsg(&msg,data,len);
  switch (msg.header.message_type) {

    case FGS_IDENTITY_REQUEST: { 
      generateIdentityResponse(&nas_info, FGS_MOBILE_IDENTITY_SUCI);
      nr_nas_itti_ul_data_req(0, nas_info.data, nas_info.length, 0);
      break;
      }

    case FGS_AUTHENTICATION_REQUEST: { 
      uint8_t buf = 0;
      generateAuthenticationResp(&nas_info, &buf);
      nr_nas_itti_ul_data_req(0, nas_info.data, nas_info.length, 0);
      break;
      }

    case FGS_SECURITY_MODE_COMMAND: { 
      generateSecurityModeComplete(&nas_info);
      nr_nas_itti_ul_data_req(0, nas_info.data, nas_info.length, 0);
      break;
      }
    
    case REGISTRATION_ACCEPT: { 
      generateRegistrationComplete(&nas_info, 0);
      nr_nas_itti_ul_data_req(0, nas_info.data, nas_info.length, 0);
      break;
      }

  }

//   //****************************** //CUC:test
//   printf("decodeaaadecode:");
//   for (int i = 0; i < len; i++)
//   {
//     printf("%02x ",*(data+i));

//   }
//   printf("decodeaaadecode \n ");
//   printf("encodeaaaencode:");
//   for (int i = 0; i < nas_info.length; i++)
//   {
//     printf("%02x ",*(nas_info.data+i));

//   }
//   printf("encodeaaaencode \n ");
//   //******************************

}

#define CHAR_TO_UINT8(input) ((input & 0xf) + 9*(input>>6))
//function to convert string to byte array
int string2ByteArray(char* input,uint8_t* output)
{
    int loop;
    int i;
    
    loop = 0;
    i = 0;
    
    while(input[loop] != '\0')
    {
        output[i++] = (CHAR_TO_UINT8(input[loop]))<<4 |  CHAR_TO_UINT8(input[loop+1]);
        loop += 2;
    }
    return i;
}

void tesths(void) //CUC:test
{
  printf("Authentication: \n ");
  char Authenticationrequest[] = "7e005601020000217d003b4a2e3bb80403de19020f57b16a2010583f0d352eb89001539b2cb2cbf1da5c";
  uint32_t len1=84;
  Byte_t *data1= (uint8_t *)malloc(sizeof(uint8_t)*len1);
  string2ByteArray(Authenticationrequest, data1);
  nr_nas_proc_dl_transfer_ind(data1,len1);
  
  printf("Security mode: \n ");
  char Securitymodecommand[] = "7e005d0201028020e1360102";
  uint32_t len2=24;
  Byte_t *data2= (uint8_t *)malloc(sizeof(uint8_t)*len2);
  string2ByteArray(Securitymodecommand, data2);
  nr_nas_proc_dl_transfer_ind(data2,len2);

  printf("Registration: \n ");
  char Registrationrequest[] = "7e0042010177000bf202f8398000410000000154070002f83900000115020101210200005e01be";
  uint32_t len3=94;
  Byte_t *data3= (uint8_t *)malloc(sizeof(uint8_t)*len3);
  string2ByteArray(Registrationrequest, data3);
  nr_nas_proc_dl_transfer_ind(data3,len3);

  printf("Registration request: \n ");
  as_nas_info_t nas_info;
  generateRegistrationRequest(&nas_info);
  printf("length:%02x\n",nas_info.length);
  printf("encodeaaaencode:");
  for (int i = 0; i < nas_info.length; i++)
  {
    printf("%02x ",*(nas_info.data+i));

  }
  printf("encodeaaaencode \n ");
  //******************************

  
}
int decodeNasMsg(MM_msg *msg, uint8_t *buffer, uint32_t len) {
  int header_result;
  int decode_result=0;

  /* First decode the EMM message header */
  header_result = _nas_mm_msg_decode_header(&msg->header, buffer, len);

  if (header_result < 0) {
    LOG_TRACE(ERROR, "NR_UE   - Failed to decode EMM message header "
              "(%d)", header_result);
    LOG_FUNC_RETURN(header_result);
  }

  buffer += header_result;
  len -= header_result;
  LOG_TRACE(INFO, "NR_UE   - Message Type 0x%02x", msg->header.message_type);

  switch(msg->header.message_type) { 

    case FGS_IDENTITY_REQUEST: { 

      break;
      }

    case FGS_AUTHENTICATION_REQUEST: { 

      break;
      }

    case FGS_SECURITY_MODE_COMMAND: { 

      break;
      }
    
    case REGISTRATION_ACCEPT: { 

      break;
      }

    default:
      LOG_TRACE(ERROR, "NR_UE   - Unexpected message type: 0x%x",
    		  msg->header.message_type);
      decode_result = TLV_ENCODE_WRONG_MESSAGE_TYPE;
      break;

  }

  LOG_FUNC_RETURN (header_result + decode_result);
}


static int _nas_mm_msg_decode_header(mm_msg_header_t *header, const uint8_t *buffer, uint32_t len) {  //QUES: 静态函数在哪声明？
  int size = 0;

  /* Check the buffer length */
  if (len < sizeof(mm_msg_header_t)) {
    return (TLV_ENCODE_BUFFER_TOO_SHORT);
  }

  /* Encode the extendedprotocol discriminator */
  DECODE_U8(buffer + size, header->ex_protocol_discriminator, size);
  /* Encode the security header type */
  DECODE_U8(buffer + size, header->security_header_type, size);

    /* Encode the message type */
  if (header->security_header_type == 0x00)
  {
    DECODE_U8(buffer + size, header->message_type, size);
  }
  if (header->security_header_type == 0x03)
  {
    size += 7;
    DECODE_U8(buffer + size, header->message_type, size);      
  }
  /* Check the protocol discriminator */
  if (header->ex_protocol_discriminator != FGS_MOBILITY_MANAGEMENT_MESSAGE) {
    LOG_TRACE(ERROR, "ESM-MSG   - Unexpected extened protocol discriminator: 0x%x",
              header->ex_protocol_discriminator);
    return (TLV_ENCODE_PROTOCOL_NOT_SUPPORTED);
  }

  return (size);
}
