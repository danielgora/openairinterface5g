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

/*! \file gNB_scheduler_uci.c
 * \brief MAC procedures related to UCI
 * \date 2020
 * \version 1.0
 * \company Eurecom
 */

#include "LAYER2/MAC/mac.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "NR_MAC_COMMON/nr_mac_extern.h"
#include "NR_MAC_gNB/mac_proto.h"
#include "common/ran_context.h"

extern RAN_CONTEXT_t RC;

#define MIN_RSRP_VALUE -141
#define MAX_NUM_SSB 128
#define MAX_SSB_SCHED 8
#define L1_RSRP_HYSTERIS 10 //considering 10 dBm as hysterisis for avoiding frequent SSB Beam Switching. !Fixme provide exact value if any
//#define L1_DIFF_RSRP_STEP_SIZE 2

int ssb_index_sorted[MAX_NUM_SSB] = {0};
int ssb_rsrp_sorted[MAX_NUM_SSB] = {0};
//Sorts ssb_index and ssb_rsrp array data and keeps in ssb_index_sorted and
//ssb_rsrp_sorted respectively
void ssb_rsrp_sort(int *ssb_index, int *ssb_rsrp) {
  int i, j;

  for(i = 0; *(ssb_index+i) != 0; i++) {
    for(j = i; *(ssb_index+j) != 0; j++) {
      if(*(ssb_rsrp+j) >= *(ssb_rsrp+i)) {
        ssb_index_sorted[i] = *(ssb_index+j);
        ssb_rsrp_sorted[i] = *(ssb_rsrp+j);
      }
    }
  }
}

//Measured RSRP Values Table 10.1.16.1-1 from 36.133
//Stored all the upper limits[Max RSRP Value of corresponding index]
//stored -1 for invalid values
int L1_SSB_CSI_RSRP_measReport_mapping_38133_10_1_6_1_1[128] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //0 - 9
    -1, -1, -1, -1, -1, -1, -140, -139, -138, -137, //10 - 19
    -136, -135, -134, -133, -132, -131, -130, -129, -128, -127, //20 - 29
    -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, //30 - 39
    -116, -115, -114, -113, -112, -111, -110, -109, -108, -107, //40 - 49
    -106, -105, -104, -103, -102, -101, -100, -99, -98, -97, //50 - 59
    -96, -95, -94, -93, -92, -91, -90, -89, -88, -87, //60 - 69
    -86, -85, -84, -83, -82, -81, -80, -79, -78, -77, //70 - 79
    -76, -75, -74, -73, -72, -71, -70, -69, -68, -67, //80 - 89
    -66, -65, -64, -63, -62, -61, -60, -59, -58, -57, //90 - 99
    -56, -55, -54, -53, -52, -51, -50, -49, -48, -47, //100 - 109
    -46, -45, -44, -44, -1, -1, -1, -1, -1, -1, //110 - 119
    -1, -1, -1, -1, -1, -1, -1, -1//120 - 127
  };

//Differential RSRP values Table 10.1.6.1-2 from 36.133
//Stored the upper limits[MAX RSRP Value]
int diff_rsrp_ssb_csi_meas_10_1_6_1_2[16] = {
  0, -2, -4, -6, -8, -10, -12, -14, -16, -18, //0 - 9
  -20, -22, -24, -26, -28, -30 //10 - 15
};


void nr_schedule_pucch(int Mod_idP,
                       int UE_id,
                       int nr_ulmix_slots,
                       frame_t frameP,
                       sub_frame_t slotP) {

  uint16_t O_csi, O_ack, O_uci;
  uint8_t O_sr = 0; // no SR in PUCCH implemented for now
  NR_ServingCellConfigCommon_t *scc = RC.nrmac[Mod_idP]->common_channels->ServingCellConfigCommon;
  NR_UE_info_t *UE_info = &RC.nrmac[Mod_idP]->UE_info;
  AssertFatal(UE_info->active[UE_id],"Cannot find UE_id %d is not active\n",UE_id);

  NR_CellGroupConfig_t *secondaryCellGroup = UE_info->secondaryCellGroup[UE_id];
  int bwp_id=1;
  NR_BWP_Uplink_t *ubwp=secondaryCellGroup->spCellConfig->spCellConfigDedicated->uplinkConfig->uplinkBWP_ToAddModList->list.array[bwp_id-1];
  nfapi_nr_ul_tti_request_t *UL_tti_req = &RC.nrmac[Mod_idP]->UL_tti_req[0];
  NR_sched_pucch *curr_pucch;

  for (int k=0; k<nr_ulmix_slots; k++) {
    for (int l=0; l<2; l++) {
      curr_pucch = &UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][l];
      O_ack = curr_pucch->dai_c;
      O_csi = curr_pucch->csi_bits;
      O_uci = O_ack + O_csi + O_sr;
      if ((O_uci>0) && (frameP == curr_pucch->frame) && (slotP == curr_pucch->ul_slot)) {
        UL_tti_req->SFN = curr_pucch->frame;
        UL_tti_req->Slot = curr_pucch->ul_slot;
        UL_tti_req->pdus_list[UL_tti_req->n_pdus].pdu_type = NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE;
        UL_tti_req->pdus_list[UL_tti_req->n_pdus].pdu_size = sizeof(nfapi_nr_pucch_pdu_t);
        nfapi_nr_pucch_pdu_t  *pucch_pdu = &UL_tti_req->pdus_list[UL_tti_req->n_pdus].pucch_pdu;
        memset(pucch_pdu,0,sizeof(nfapi_nr_pucch_pdu_t));
        UL_tti_req->n_pdus+=1;

        LOG_I(MAC,"Scheduling pucch reception for frame %d slot %d with (%d, %d, %d) (SR ACK, CSI) bits\n",
              frameP,slotP,O_sr,O_ack,curr_pucch->csi_bits);

        nr_configure_pucch(pucch_pdu,
                           scc,
                           ubwp,
                           UE_info->rnti[UE_id],
                           curr_pucch->resource_indicator,
                           O_csi,
                           O_ack,
                           O_sr);

        memset((void *) &UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][l],
               0,
               sizeof(NR_sched_pucch));
      }
    }
  }
}


//! Calculating number of bits set
uint8_t number_of_bits_set (uint8_t buf,uint8_t * max_ri){
  uint8_t nb_of_bits_set = 0;
  uint8_t mask = 0xff;
  uint8_t index = 0;

  for (index=7; (buf & mask) && (index>=0)  ; index--){
    if (buf & (1<<index))
      nb_of_bits_set++;

    mask>>=1;
  }
  *max_ri = 8-index;
  return nb_of_bits_set;
}


//!TODO : same function can be written to handle csi_resources
void compute_csi_bitlen(NR_CSI_MeasConfig_t *csi_MeasConfig, NR_UE_info_t *UE_info, int UE_id, module_id_t Mod_idP){
  uint8_t csi_report_id = 0;
  uint8_t csi_resourceidx =0;
  uint8_t csi_ssb_idx =0;
  NR_CSI_ReportConfig__reportQuantity_PR reportQuantity_type;
  NR_CSI_ResourceConfigId_t csi_ResourceConfigId;

  for (csi_report_id=0; csi_report_id < csi_MeasConfig->csi_ReportConfigToAddModList->list.count; csi_report_id++){
    struct NR_CSI_ReportConfig *csi_reportconfig = csi_MeasConfig->csi_ReportConfigToAddModList->list.array[csi_report_id];
    csi_ResourceConfigId=csi_reportconfig->resourcesForChannelMeasurement;
    reportQuantity_type = csi_reportconfig->reportQuantity.present;
    UE_info->csi_report_template[UE_id][csi_report_id].reportQuantity_type = reportQuantity_type;

    for ( csi_resourceidx = 0; csi_resourceidx < csi_MeasConfig->csi_ResourceConfigToAddModList->list.count; csi_resourceidx++) {
      struct NR_CSI_ResourceConfig *csi_resourceconfig = csi_MeasConfig->csi_ResourceConfigToAddModList->list.array[csi_resourceidx];
      if ( csi_resourceconfig->csi_ResourceConfigId != csi_ResourceConfigId)
        continue;
      else {
        uint8_t nb_ssb_resources =0;
        //Finding the CSI_RS or SSB Resources
        if (NR_CSI_ReportConfig__reportQuantity_PR_cri_RSRP == reportQuantity_type || 
            NR_CSI_ReportConfig__reportQuantity_PR_ssb_Index_RSRP == reportQuantity_type) {

          if (NR_CSI_ReportConfig__groupBasedBeamReporting_PR_disabled == csi_reportconfig->groupBasedBeamReporting.present) {
            if (NULL != csi_reportconfig->groupBasedBeamReporting.choice.disabled->nrofReportedRS)
              UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.nb_ssbri_cri = *(csi_reportconfig->groupBasedBeamReporting.choice.disabled->nrofReportedRS)+1;
            else
		/*! From Spec 38.331
		 * nrofReportedRS
		 * The number (N) of measured RS resources to be reported per report setting in a non-group-based report. N <= N_max, where N_max is either 2 or 4 depending on UE
		 * capability. FFS: The signaling mechanism for the gNB to select a subset of N beams for the UE to measure and report.  
		 * When the field is absent the UE applies the value 1
		 */
              UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.nb_ssbri_cri= 1;
          }else 
	    UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.nb_ssbri_cri= 2;

          if (NR_CSI_ReportConfig__reportQuantity_PR_ssb_Index_RSRP == UE_info->csi_report_template[UE_id][csi_report_id].reportQuantity_type) {
            for ( csi_ssb_idx = 0; csi_ssb_idx < csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.count; csi_ssb_idx++) {
              if (csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.array[csi_ssb_idx]->csi_SSB_ResourceSetId ==
                  *(csi_resourceconfig->csi_RS_ResourceSetList.choice.nzp_CSI_RS_SSB->csi_SSB_ResourceSetList->list.array[0])){
 
                ///We can configure only one SSB resource set from spec 38.331 IE CSI-ResourceConfig
                nb_ssb_resources=  csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.array[csi_ssb_idx]->csi_SSB_ResourceList.list.count;
                UE_info->csi_report_template[UE_id][csi_report_id].SSB_Index_list = csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.array[csi_ssb_idx]->csi_SSB_ResourceList.list.array;
                UE_info->csi_report_template[UE_id][csi_report_id].CSI_Index_list = NULL;
              }
              break;
            }
          } else /*if (NR_CSI_ReportConfig__reportQuantity_PR_cri_RSRP == UE_info->csi_report_template[UE_id][csi_report_id].reportQuantity_type)*/{
            for ( csi_ssb_idx = 0; csi_ssb_idx < csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.count; csi_ssb_idx++) {
              if (csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.array[csi_ssb_idx]->nzp_CSI_ResourceSetId ==
                  *(csi_resourceconfig->csi_RS_ResourceSetList.choice.nzp_CSI_RS_SSB->nzp_CSI_RS_ResourceSetList->list.array[0])) {

                ///For periodic and semi-persistent CSI Resource Settings, the number of CSI-RS Resource Sets configured is limited to S=1 for spec 38.212
                nb_ssb_resources=  csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.array[csi_ssb_idx]->nzp_CSI_RS_Resources.list.count;
                UE_info->csi_report_template[UE_id][csi_report_id].CSI_Index_list = csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.array[csi_ssb_idx]->nzp_CSI_RS_Resources.list.array;
                UE_info->csi_report_template[UE_id][csi_report_id].SSB_Index_list = NULL;
              }
              break;
            }
         }

         if (nb_ssb_resources) {
           UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.cri_ssbri_bitlen =ceil(log2 (nb_ssb_resources));
           UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.rsrp_bitlen = 7; //From spec 38.212 Table 6.3.1.1.2-6: CRI, SSBRI, and RSRP 
           UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.diff_rsrp_bitlen =4; //From spec 38.212 Table 6.3.1.1.2-6: CRI, SSBRI, and RSRP
         } else { 
            UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.cri_ssbri_bitlen =0;
            UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.rsrp_bitlen = 0;  
            UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.diff_rsrp_bitlen =0; 
         }

        LOG_I (MAC, "UCI: CSI_bit len : ssbri %d, rsrp: %d, diff_rsrp: %d\n",
               UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.cri_ssbri_bitlen,
               UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.rsrp_bitlen,
               UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.diff_rsrp_bitlen);
        }

        uint8_t ri_restriction;
        uint8_t ri_bitlen;
        uint8_t nb_allowed_ri;
        uint8_t max_ri;

        if (NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_PMI_CQI == reportQuantity_type ||
            NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_LI_PMI_CQI==reportQuantity_type ||
            NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_CQI==reportQuantity_type ||
            NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_i1_CQI==reportQuantity_type||
            NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_i1==reportQuantity_type){

          for ( csi_ssb_idx = 0; csi_ssb_idx < csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.count; csi_ssb_idx++) {
            if (csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.array[csi_ssb_idx]->nzp_CSI_ResourceSetId ==
                *(csi_resourceconfig->csi_RS_ResourceSetList.choice.nzp_CSI_RS_SSB->nzp_CSI_RS_ResourceSetList->list.array[0])) {
              ///For periodic and semi-persistent CSI Resource Settings, the number of CSI-RS Resource Sets configured is limited to S=1 for spec 38.212
              nb_ssb_resources=  csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.array[csi_ssb_idx]->nzp_CSI_RS_Resources.list.count;
              UE_info->csi_report_template[UE_id][csi_report_id].CSI_Index_list = csi_MeasConfig->nzp_CSI_RS_ResourceSetToAddModList->list.array[csi_ssb_idx]->nzp_CSI_RS_Resources.list.array;
              UE_info->csi_report_template[UE_id][csi_report_id].SSB_Index_list = NULL;
            }
            break;
          }
          UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.cri_bitlen=ceil(log2 (nb_ssb_resources));

          if (NR_CodebookConfig__codebookType__type1__subType_PR_typeI_SinglePanel==csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.present){

            switch (RC.nrrrc[Mod_idP]->carrier.pdsch_AntennaPorts) {
              case 1:;
                UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.ri_bitlen=0;
                break;
              case 2:
		/*  From Spec 38.212 
		 *  If the higher layer parameter nrofCQIsPerReport=1, nRI in Table 6.3.1.1.2-3 is the number of allowed rank indicator
		 *  values in the 4 LSBs of the higher layer parameter typeI-SinglePanel-ri-Restriction according to Subclause 5.2.2.2.1 [6,
		 *  TS 38.214]; otherwise nRI in Table 6.3.1.1.2-3 is the number of allowed rank indicator values according to Subclause
		 *  5.2.2.2.1 [6, TS 38.214].
		 *
		 *  But from Current RRC ASN structures nrofCQIsPerReport is not present. Present a dummy variable is present so using it to
		 *  calculate RI for antennas equal or more than two.
		 * */
                 AssertFatal (NULL!=csi_reportconfig->dummy, "nrofCQIsPerReport is not present");

                 ri_restriction = csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.choice.typeI_SinglePanel->typeI_SinglePanel_ri_Restriction.buf[0];

                 /* Replace dummy with the nrofCQIsPerReport from the CSIreport 
                 config when equalent ASN structure present */
                if (0==*(csi_reportconfig->dummy)){
                  nb_allowed_ri = number_of_bits_set((ri_restriction & 0xf0), &max_ri);
                  ri_bitlen = ceil(log2(nb_allowed_ri));
                }
                else{
                  nb_allowed_ri = number_of_bits_set(ri_restriction, &max_ri);
                  ri_bitlen = ceil(log2(nb_allowed_ri));
                }
                ri_bitlen = ri_bitlen<1?ri_bitlen:1; //from the spec 38.212 and table  6.3.1.1.2-3: RI, LI, CQI, and CRI of codebookType=typeI-SinglePanel 
                UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.ri_bitlen=ri_bitlen;
                break;
              case 4:
                AssertFatal (NULL!=csi_reportconfig->dummy, "nrofCQIsPerReport is not present");

                ri_restriction = csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.choice.typeI_SinglePanel->typeI_SinglePanel_ri_Restriction.buf[0];

                /* Replace dummy with the nrofCQIsPerReport from the CSIreport 
                config when equalent ASN structure present */
                if (0==*(csi_reportconfig->dummy)){
                  nb_allowed_ri = number_of_bits_set((ri_restriction & 0xf0), &max_ri);
                  ri_bitlen = ceil(log2(nb_allowed_ri));
                }
                else{
                  nb_allowed_ri = number_of_bits_set(ri_restriction,&max_ri);
                  ri_bitlen = ceil(log2(nb_allowed_ri));
                }
                ri_bitlen = ri_bitlen<2?ri_bitlen:2; //from the spec 38.212 and table  6.3.1.1.2-3: RI, LI, CQI, and CRI of codebookType=typeI-SinglePanel 
                UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.ri_bitlen=ri_bitlen;
                break;
              case 6:
              case 8:
                AssertFatal (NULL!=csi_reportconfig->dummy, "nrofCQIsPerReport is not present");
		 
                ri_restriction = csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.choice.typeI_SinglePanel->typeI_SinglePanel_ri_Restriction.buf[0];

                /* Replace dummy with the nrofCQIsPerReport from the CSIreport 
                config when equalent ASN structure present */
                if (0==*(csi_reportconfig->dummy)){
                  nb_allowed_ri = number_of_bits_set((ri_restriction & 0xf0),&max_ri);
                  ri_bitlen = ceil(log2(nb_allowed_ri));
                }
                else{
                  nb_allowed_ri = number_of_bits_set(ri_restriction, &max_ri);
                  ri_bitlen = ceil(log2(nb_allowed_ri));
                }
                //ri_bitlen = ri_bitlen<1?ri_bitlen:1; //from the spec 38.212 and table  6.3.1.1.2-3: RI, LI, CQI, and CRI of codebookType=typeI-SinglePanel 
                UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.ri_bitlen=ri_bitlen;
                break;
              default:
                //UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.ri_bitlen=0;
                AssertFatal(RC.nrrrc[Mod_idP]->carrier.pdsch_AntennaPorts>8,"Number of antennas %d are out of range", RC.nrrrc[Mod_idP]->carrier.pdsch_AntennaPorts);
            }
          }
          UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.li_bitlen=0;
          UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.cqi_bitlen=0;
          UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.pmi_x1_bitlen=0;
          UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.pmi_x2_bitlen=0;
        }

        if( NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_LI_PMI_CQI==reportQuantity_type ){
          if (NR_CodebookConfig__codebookType__type1__subType_PR_typeI_SinglePanel==csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.present){

            switch (RC.nrrrc[Mod_idP]->carrier.pdsch_AntennaPorts) {
              case 1:;
                UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.li_bitlen=0;
                break;
              case 2:
              case 4:
              case 6:
              case 8:
		/*  From Spec 38.212 
		 *  If the higher layer parameter nrofCQIsPerReport=1, nRI in Table 6.3.1.1.2-3 is the number of allowed rank indicator
		 *  values in the 4 LSBs of the higher layer parameter typeI-SinglePanel-ri-Restriction according to Subclause 5.2.2.2.1 [6,
		 *  TS 38.214]; otherwise nRI in Table 6.3.1.1.2-3 is the number of allowed rank indicator values according to Subclause
		 *  5.2.2.2.1 [6, TS 38.214].
		 *
		 *  But from Current RRC ASN structures nrofCQIsPerReport is not present. Present a dummy variable is present so using it to
		 *  calculate RI for antennas equal or more than two.
		 * */
		 //! TODO: The bit length of LI is as follows LI = log2(RI), Need to confirm wheather we should consider maximum RI can be reported from ri_restricted
		 //        or we should consider reported RI. If we need to consider reported RI for calculating LI bit length then we need to modify the code.
                UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.li_bitlen=ceil(log2(max_ri))<2?ceil(log2(max_ri)):2;
                break;
              default:
                AssertFatal(RC.nrrrc[Mod_idP]->carrier.pdsch_AntennaPorts>8,"Number of antennas %d are out of range", RC.nrrrc[Mod_idP]->carrier.pdsch_AntennaPorts);
            }
          }
        }

        if (NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_PMI_CQI == reportQuantity_type ||
            NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_LI_PMI_CQI==reportQuantity_type ||
            NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_CQI==reportQuantity_type ||
            NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_i1_CQI==reportQuantity_type){

          switch (RC.nrrrc[Mod_idP]->carrier.pdsch_AntennaPorts){
            case 1:
            case 2:
            case 4:
            case 6:
            case 8:
	        /*  From Spec 38.212 
		 *  If the higher layer parameter nrofCQIsPerReport=1, nRI in Table 6.3.1.1.2-3 is the number of allowed rank indicator
		 *  values in the 4 LSBs of the higher layer parameter typeI-SinglePanel-ri-Restriction according to Subclause 5.2.2.2.1 [6,
		 *  TS 38.214]; otherwise nRI in Table 6.3.1.1.2-3 is the number of allowed rank indicator values according to Subclause
		 *  5.2.2.2.1 [6, TS 38.214].
		 *
		 *  But from Current RRC ASN structures nrofCQIsPerReport is not present. Present a dummy variable is present so using it to
		 *  calculate RI for antennas equal or more than two.
		 * */
		 
              if (max_ri > 4 && max_ri < 8){
                if (NR_CodebookConfig__codebookType__type1__subType_PR_typeI_SinglePanel==csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.present){
                  if (NR_CSI_ReportConfig__reportFreqConfiguration__cqi_FormatIndicator_widebandCQI==csi_reportconfig->reportFreqConfiguration->cqi_FormatIndicator)
                    UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.cqi_bitlen = 8;
                  else 
                    UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.cqi_bitlen = 4;
                }
              }else{ //This condition will work even for type1-multipanel.
                if (NR_CSI_ReportConfig__reportFreqConfiguration__cqi_FormatIndicator_widebandCQI==csi_reportconfig->reportFreqConfiguration->cqi_FormatIndicator)
                  UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.cqi_bitlen = 4;
                else 
                  UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.cqi_bitlen = 2;
              }
              break;
            default:
              AssertFatal(RC.nrrrc[Mod_idP]->carrier.pdsch_AntennaPorts>8,"Number of antennas %d are out of range", RC.nrrrc[Mod_idP]->carrier.pdsch_AntennaPorts);
          }
        }
        if (NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_PMI_CQI == reportQuantity_type ||
            NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_LI_PMI_CQI==reportQuantity_type){
          if (NR_CodebookConfig__codebookType__type1__subType_PR_typeI_SinglePanel==csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.present){
            switch (csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.choice.typeI_SinglePanel->nrOfAntennaPorts.present){
              case NR_CodebookConfig__codebookType__type1__subType__typeI_SinglePanel__nrOfAntennaPorts_PR_two:
                if (max_ri ==1)
                  UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.pmi_x1_bitlen = 2;
                else if (max_ri ==2)
                  UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen.pmi_x1_bitlen = 1;
                break;
              default:
                AssertFatal(csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.choice.typeI_SinglePanel->nrOfAntennaPorts.present!=
                            NR_CodebookConfig__codebookType__type1__subType__typeI_SinglePanel__nrOfAntennaPorts_PR_two,
                            "Not handled Yet %d", csi_reportconfig->codebookConfig->codebookType.choice.type1->subType.choice.typeI_SinglePanel->nrOfAntennaPorts.present);
		break;
            }
          }
        }
        break;
      }
    }
  }
}


uint16_t nr_get_csi_bitlen(int Mod_idP,
                           int UE_id,
                           uint8_t csi_report_id) {

  uint16_t csi_bitlen =0;
  NR_UE_info_t *UE_info = &RC.nrmac[Mod_idP]->UE_info;
  L1_RSRP_bitlen_t * CSI_report_bitlen = NULL;
  CSI_Meas_bitlen_t * csi_meas_bitlen = NULL;

  if (NR_CSI_ReportConfig__reportQuantity_PR_ssb_Index_RSRP==UE_info->csi_report_template[UE_id][csi_report_id].reportQuantity_type||
      NR_CSI_ReportConfig__reportQuantity_PR_cri_RSRP==UE_info->csi_report_template[UE_id][csi_report_id].reportQuantity_type){
    CSI_report_bitlen = &(UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen); //This might need to be moodif for Aperiodic CSI-RS measurements
    csi_bitlen+= ((CSI_report_bitlen->cri_ssbri_bitlen * CSI_report_bitlen->nb_ssbri_cri) +
                  CSI_report_bitlen->rsrp_bitlen +(CSI_report_bitlen->diff_rsrp_bitlen * 
                  (CSI_report_bitlen->nb_ssbri_cri -1 )));//*UE_info->csi_report_template[UE_id][csi_report_id].nb_of_csi_ssb_report);
  } else{
   csi_meas_bitlen = &(UE_info->csi_report_template[UE_id][csi_report_id].csi_meas_bitlen); //This might need to be moodif for Aperiodic CSI-RS measurements
   csi_bitlen+= (csi_meas_bitlen->cri_bitlen +csi_meas_bitlen->ri_bitlen+csi_meas_bitlen->li_bitlen+csi_meas_bitlen->cqi_bitlen+csi_meas_bitlen->pmi_x1_bitlen+csi_meas_bitlen->pmi_x2_bitlen);
 }

  return csi_bitlen;
}


void nr_csi_meas_reporting(int Mod_idP,
                           int UE_id,
                           frame_t frame,
                           sub_frame_t slot,
                           int slots_per_tdd,
                           int ul_slots,
                           int n_slots_frame) {

  NR_UE_info_t *UE_info = &RC.nrmac[Mod_idP]->UE_info;
  NR_sched_pucch *curr_pucch;
  NR_PUCCH_ResourceSet_t *pucchresset;
  NR_CSI_ReportConfig_t *csirep;
  NR_CellGroupConfig_t *secondaryCellGroup = UE_info->secondaryCellGroup[UE_id];
  NR_CSI_MeasConfig_t *csi_measconfig = secondaryCellGroup->spCellConfig->spCellConfigDedicated->csi_MeasConfig->choice.setup;
  NR_BWP_Uplink_t *ubwp=secondaryCellGroup->spCellConfig->spCellConfigDedicated->uplinkConfig->uplinkBWP_ToAddModList->list.array[0];
  NR_PUCCH_Config_t *pucch_Config = ubwp->bwp_Dedicated->pucch_Config->choice.setup;

  AssertFatal(csi_measconfig->csi_ReportConfigToAddModList->list.count>0,"NO CSI report configuration available");

  for (int csi_report_id = 0; csi_report_id < csi_measconfig->csi_ReportConfigToAddModList->list.count; csi_report_id++){

    csirep = csi_measconfig->csi_ReportConfigToAddModList->list.array[csi_report_id];

    AssertFatal(csirep->reportConfigType.choice.periodic!=NULL,"Only periodic CSI reporting is implemented currently");
    int period, offset, sched_slot;
    csi_period_offset(csirep,&period,&offset);
    sched_slot = (period+offset)%n_slots_frame;
    // prepare to schedule csi measurement reception according to 5.2.1.4 in 38.214
    // preparation is done in first slot of tdd period
    if ( (frame%(period/n_slots_frame)==(offset/n_slots_frame)) && (slot==((sched_slot/slots_per_tdd)*slots_per_tdd))) {

      // we are scheduling pucch for csi in the first pucch occasion (this comes before ack/nack)
      curr_pucch = &UE_info->UE_sched_ctrl[UE_id].sched_pucch[sched_slot-slots_per_tdd+ul_slots][0];

      NR_PUCCH_CSI_Resource_t *pucchcsires = csirep->reportConfigType.choice.periodic->pucch_CSI_ResourceList.list.array[0];

      int found = -1;
      pucchresset = pucch_Config->resourceSetToAddModList->list.array[1]; // set with formats >1
      int n_list = pucchresset->resourceList.list.count;
      for (int i=0; i<n_list; i++) {
        if (*pucchresset->resourceList.list.array[i] == pucchcsires->pucch_Resource)
          found = i;
      }
      AssertFatal(found>-1,"CSI resource not found among PUCCH resources");

      curr_pucch->resource_indicator = found;

      n_list = pucch_Config->resourceToAddModList->list.count;

      // going through the list of PUCCH resources to find the one indexed by resource_id
      for (int i=0; i<n_list; i++) {
        NR_PUCCH_Resource_t *pucchres = pucch_Config->resourceToAddModList->list.array[i];
        if (pucchres->pucch_ResourceId == *pucchresset->resourceList.list.array[found]) {
          switch(pucchres->format.present){
            case NR_PUCCH_Resource__format_PR_format2:
              if (pucch_Config->format2->choice.setup->simultaneousHARQ_ACK_CSI == NULL)
                curr_pucch->simultaneous_harqcsi = false;
              else
                curr_pucch->simultaneous_harqcsi = true;
              break;
            case NR_PUCCH_Resource__format_PR_format3:
              if (pucch_Config->format3->choice.setup->simultaneousHARQ_ACK_CSI == NULL)
                curr_pucch->simultaneous_harqcsi = false;
              else
                curr_pucch->simultaneous_harqcsi = true;
              break;
            case NR_PUCCH_Resource__format_PR_format4:
              if (pucch_Config->format4->choice.setup->simultaneousHARQ_ACK_CSI == NULL)
                curr_pucch->simultaneous_harqcsi = false;
              else
                curr_pucch->simultaneous_harqcsi = true;
              break;
          default:
            AssertFatal(1==0,"Invalid PUCCH format type");
          }
        }
      }
      curr_pucch->csi_bits += nr_get_csi_bitlen(Mod_idP,UE_id,csi_report_id); // TODO function to compute CSI meas report bit size
      curr_pucch->frame = frame;
      curr_pucch->ul_slot = sched_slot;
    }
  }
}


void nr_rx_acknack(nfapi_nr_uci_pusch_pdu_t *uci_pusch,
                   nfapi_nr_uci_pucch_pdu_format_0_1_t *uci_01,
                   nfapi_nr_uci_pucch_pdu_format_2_3_4_t *uci_234,
                   NR_UL_IND_t *UL_info, NR_UE_sched_ctrl_t *sched_ctrl, NR_mac_stats_t *stats) {

  // TODO
  int max_harq_rounds = 4; // TODO define macro

  if (uci_01 != NULL) {
    // handle harq
    int harq_idx_s = 0;

    // iterate over received harq bits
    for (int harq_bit = 0; harq_bit < uci_01->harq->num_harq; harq_bit++) {
      // search for the right harq process
      for (int harq_idx = harq_idx_s; harq_idx < NR_MAX_NB_HARQ_PROCESSES; harq_idx++) {
        // if the gNB received ack with a good confidence
        if ((UL_info->slot-1) == sched_ctrl->harq_processes[harq_idx].feedback_slot) {
          if ((uci_01->harq->harq_list[harq_bit].harq_value == 1) &&
              (uci_01->harq->harq_confidence_level == 0)) {
            // toggle NDI and reset round
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
          }
          else
            sched_ctrl->harq_processes[harq_idx].round++;
          sched_ctrl->harq_processes[harq_idx].is_waiting = 0;
          harq_idx_s = harq_idx + 1;
          // if the max harq rounds was reached
          if (sched_ctrl->harq_processes[harq_idx].round == max_harq_rounds) {
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
            stats->dlsch_errors++;
          }
          break;
        }
        // if feedback slot processing is aborted
        else if (((UL_info->slot-1) > sched_ctrl->harq_processes[harq_idx].feedback_slot) &&
                 (sched_ctrl->harq_processes[harq_idx].is_waiting)) {
          sched_ctrl->harq_processes[harq_idx].round++;
          if (sched_ctrl->harq_processes[harq_idx].round == max_harq_rounds) {
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
          }
          sched_ctrl->harq_processes[harq_idx].is_waiting = 0;
        }
      }
    }
  }

  if (uci_234 != NULL) {
    int harq_idx_s = 0;
    int acknack;

    // iterate over received harq bits
    for (int harq_bit = 0; harq_bit < uci_234->harq.harq_bit_len; harq_bit++) {
      acknack = ((uci_234->harq.harq_payload[harq_bit>>3])>>harq_bit)&0x01;
      for (int harq_idx = harq_idx_s; harq_idx < NR_MAX_NB_HARQ_PROCESSES-1; harq_idx++) {
        // if the gNB received ack with a good confidence or if the max harq rounds was reached
        if ((UL_info->slot-1) == sched_ctrl->harq_processes[harq_idx].feedback_slot) {
          // TODO add some confidence level for when there is no CRC
          if ((uci_234->harq.harq_crc != 1) && acknack) {
            // toggle NDI and reset round
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
          }
          else
            sched_ctrl->harq_processes[harq_idx].round++;
          sched_ctrl->harq_processes[harq_idx].is_waiting = 0;
          harq_idx_s = harq_idx + 1;
          // if the max harq rounds was reached
          if (sched_ctrl->harq_processes[harq_idx].round == max_harq_rounds) {
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
            stats->dlsch_errors++;
          }
          break;
        }
        // if feedback slot processing is aborted
        else if (((UL_info->slot-1) > sched_ctrl->harq_processes[harq_idx].feedback_slot) &&
                 (sched_ctrl->harq_processes[harq_idx].is_waiting)) {
          sched_ctrl->harq_processes[harq_idx].round++;
          if (sched_ctrl->harq_processes[harq_idx].round == max_harq_rounds) {
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
          }
          sched_ctrl->harq_processes[harq_idx].is_waiting = 0;
        }
      }
    }
  }
}


// function to update pucch scheduling parameters in UE list when a USS DL is scheduled
void nr_acknack_scheduling(int Mod_idP,
                           int UE_id,
                           frame_t frameP,
                           sub_frame_t slotP,
                           int slots_per_tdd,
                           int *pucch_id,
                           int *pucch_occ) {

  NR_ServingCellConfigCommon_t *scc = RC.nrmac[Mod_idP]->common_channels->ServingCellConfigCommon;
  NR_UE_info_t *UE_info = &RC.nrmac[Mod_idP]->UE_info;
  NR_sched_pucch *curr_pucch;
  int max_acknacks,pucch_res,first_ul_slot_tdd,k,i,l;
  uint8_t pdsch_to_harq_feedback[8];
  int found = 0;
  int nr_ulmix_slots = scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofUplinkSlots;
  if (scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofUplinkSymbols!=0)
    nr_ulmix_slots++;

  bool csi_pres=false;
  for (k=0; k<nr_ulmix_slots; k++) {
    if(UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][0].csi_bits>0)
      csi_pres=true;
  }

  // As a preference always schedule ack nacks in PUCCH0 (max 2 per slots)
  // Unless there is CSI meas reporting scheduled in the period to avoid conflicts in the same slot
  if (csi_pres)
    max_acknacks=10;
  else
    max_acknacks=2;

  // this is hardcoded for now as ue specific
  NR_SearchSpace__searchSpaceType_PR ss_type = NR_SearchSpace__searchSpaceType_PR_ue_Specific;
  get_pdsch_to_harq_feedback(Mod_idP,UE_id,ss_type,pdsch_to_harq_feedback);

  // for each possible ul or mixed slot
  for (k=0; k<nr_ulmix_slots; k++) {
    for (l=0; l<1; l++) { // scheduling 2 PUCCH in a single slot does not work with the phone, currently
      curr_pucch = &UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][l];
      //if it is possible to schedule acknack in current pucch (no exclusive csi pucch)
      if ((curr_pucch->csi_bits == 0) || (curr_pucch->simultaneous_harqcsi==true)) {
        // if there is free room in current pucch structure
        if (curr_pucch->dai_c<max_acknacks) {
          pucch_res = get_pucch_resource(UE_info,UE_id,k,l);
          if (pucch_res>-1){
            curr_pucch->resource_indicator = pucch_res;
            curr_pucch->frame = frameP;
            // first pucch occasion in first UL or MIXED slot
            first_ul_slot_tdd = scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofDownlinkSlots;
            i = 0;
            while (i<8 && found == 0)  {  // look if timing indicator is among allowed values
              if (pdsch_to_harq_feedback[i]==(first_ul_slot_tdd+k)-(slotP % slots_per_tdd))
                found = 1;
              if (found == 0) i++;
            }
            if (found == 1) {
              // computing slot in which pucch is scheduled
              curr_pucch->dai_c++;
              curr_pucch->ul_slot = first_ul_slot_tdd + k + (slotP - (slotP % slots_per_tdd));
              curr_pucch->timing_indicator = i; // index in the list of timing indicators
              *pucch_id = k;
              *pucch_occ = l;
              return;
            }
          }
        }
      }
    }
  }
  AssertFatal(1==0,"No Uplink slot available in accordance to allowed timing indicator\n");
}


void csi_period_offset(NR_CSI_ReportConfig_t *csirep,
                       int *period, int *offset) {

  NR_CSI_ReportPeriodicityAndOffset_PR p_and_o = csirep->reportConfigType.choice.periodic->reportSlotConfig.present;

  switch(p_and_o){
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots4:
      *period = 4;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots4;
      break;
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots5:
      *period = 5;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots5;
      break;
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots8:
      *period = 8;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots8;
      break;
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots10:
      *period = 10;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots10;
      break;
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots16:
      *period = 16;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots16;
      break;
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots20:
      *period = 20;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots20;
      break;
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots40:
      *period = 40;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots40;
      break;
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots80:
      *period = 80;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots80;
      break;
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots160:
      *period = 160;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots160;
      break;
    case NR_CSI_ReportPeriodicityAndOffset_PR_slots320:
      *period = 320;
      *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots320;
      break;
    default:
      AssertFatal(1==0,"No periodicity and offset resource found in CSI report");
  }
}


int get_pucch_resource(NR_UE_info_t *UE_info,int UE_id,int k,int l) {

  // to be updated later, for now simple implementation
  // use the second allocation just in case there is csi in the first
  // in that case use second resource (for a different symbol) see 9.2 in 38.213
  if (l==1) {
    if (UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][0].csi_bits==0)
      return -1;
    else
      return 1;
  }
  else
    return 0;

}


uint16_t compute_pucch_prb_size(uint8_t format,
                                uint8_t nr_prbs,
                                uint16_t O_tot,
                                uint16_t O_csi,
                                NR_PUCCH_MaxCodeRate_t *maxCodeRate,
                                uint8_t Qm,
                                uint8_t n_symb,
                                uint8_t n_re_ctrl) {

  uint16_t O_crc;

  if (O_tot<12)
    O_crc = 0;
  else{
    if (O_tot<20)
      O_crc = 6;
    else {
      if (O_tot<360)
        O_crc = 11;
      else
        AssertFatal(1==0,"Case for segmented PUCCH not yet implemented");
    }
  }

  int rtimes100;
  switch(*maxCodeRate){
    case NR_PUCCH_MaxCodeRate_zeroDot08 :
      rtimes100 = 8;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot15 :
      rtimes100 = 15;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot25 :
      rtimes100 = 25;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot35 :
      rtimes100 = 35;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot45 :
      rtimes100 = 45;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot60 :
      rtimes100 = 60;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot80 :
      rtimes100 = 80;
      break;
  default :
    AssertFatal(1==0,"Invalid MaxCodeRate");
  }

  float r = (float)rtimes100/100;

  if (O_csi == O_tot) {
    if ((O_tot+O_csi)>(nr_prbs*n_re_ctrl*n_symb*Qm*r))
      AssertFatal(1==0,"MaxCodeRate %.2f can't support %d UCI bits and %d CRC bits with %d PRBs",
                  r,O_tot,O_crc,nr_prbs);
    else
      return nr_prbs;
  }

  if (format==2){
    // TODO fix this for multiple CSI reports
    for (int i=1; i<=nr_prbs; i++){
      if((O_tot+O_crc)<=(i*n_symb*Qm*n_re_ctrl*r) &&
         (O_tot+O_crc)>((i-1)*n_symb*Qm*n_re_ctrl*r))
        return i;
    }
    AssertFatal(1==0,"MaxCodeRate %.2f can't support %d UCI bits and %d CRC bits with at most %d PRBs",
                r,O_tot,O_crc,nr_prbs);
  }
  else{
    AssertFatal(1==0,"Not yet implemented");
  }
}


//identifies the target SSB Beam index
//keeps the required date for PDCCH and PDSCH TCI state activation/deactivation CE consutruction globally
//handles triggering of PDCCH and PDSCH MAC CEs
void tci_handling(module_id_t Mod_idP, int UE_id, int CC_id, NR_UE_sched_ctrl_t *sched_ctrl, frame_t frame, slot_t slot) {

  int strongest_ssb_rsrp = 0;
  int cqi_idx = 0;
  int curr_ssb_beam_index = 0; //ToDo: yet to know how to identify the serving ssb beam index
  uint8_t target_ssb_beam_index = curr_ssb_beam_index;
  //uint8_t max_reported_RSRP = 16;
  //int serving_SSB_Beam_RSRP;
  uint8_t is_triggering_ssb_beam_switch =0;
  uint8_t ssb_idx = 0;
  int pdsch_bwp_id =0;
  int ssb_index[MAX_NUM_SSB] = {0};
  int ssb_rsrp[MAX_NUM_SSB] = {0};
  uint8_t idx = 0;
  int bwp_id  = 1;
  NR_UE_info_t *UE_info = &RC.nrmac[Mod_idP]->UE_info;
  //NR_COMMON_channels_t *cc = RC.nrmac[Mod_idP]->common_channels;
  NR_CellGroupConfig_t *secondaryCellGroup = UE_info->secondaryCellGroup[UE_id];
  NR_BWP_Downlink_t *bwp = secondaryCellGroup->spCellConfig->spCellConfigDedicated->downlinkBWP_ToAddModList->list.array[bwp_id-1];
  //NR_CSI_MeasConfig_t *csi_MeasConfig = UE_info->secondaryCellGroup[UE_id]->spCellConfig->spCellConfigDedicated->csi_MeasConfig->choice.setup;
  //bwp indicator
  int n_dl_bwp = secondaryCellGroup->spCellConfig->spCellConfigDedicated->downlinkBWP_ToAddModList->list.count;
  uint8_t nr_ssbri_cri = 0;
  uint8_t nb_of_csi_ssb_report = UE_info->csi_report_template[UE_id][cqi_idx].nb_of_csi_ssb_report;
  //uint8_t bitlen_ssbri = log (nb_of_csi_ssb_report)/log (2);
  //uint8_t max_rsrp_reported = -1;
  int better_rsrp_reported = -140-(-0); /*minimum_measured_RSRP_value - minimum_differntail_RSRP_value*///considering the minimum RSRP value as better RSRP initially
  uint8_t diff_rsrp_idx = 0;
  uint8_t i, j;

  if (n_dl_bwp < 4)
    pdsch_bwp_id = bwp_id;
  else
    pdsch_bwp_id = bwp_id - 1; // as per table 7.3.1.1.2-1 in 38.212

  /*Example:
  CRI_SSBRI: 1 2 3 4| 5 6 7 8| 9 10 1 2|
  nb_of_csi_ssb_report = 3 //3 sets as above
  nr_ssbri_cri = 4 //each set has 4 elements
  storing ssb indexes in ssb_index array as ssb_index[0] = 1 .. ssb_index[4] = 5
  ssb_rsrp[0] = strongest rsrp in first set, ssb_rsrp[4] = strongest rsrp in second set, ..
  idx: resource set index
  */

  //for all reported SSB
  for (idx = 0; idx < nb_of_csi_ssb_report; idx++) {
    nr_ssbri_cri = sched_ctrl->CSI_report[idx].choice.ssb_cri_report.nr_ssbri_cri;
    //if group based beam Reporting is disabled
    /*if(NR_CSI_ReportConfig__groupBasedBeamReporting_PR_disabled ==
        csi_MeasConfig->csi_ReportConfigToAddModList->list.array[0]->groupBasedBeamReporting.present ) {*/
      //extracting the ssb indexes
      for (ssb_idx = 0; ssb_idx < nr_ssbri_cri; ssb_idx++) {
        ssb_index[idx * nb_of_csi_ssb_report + ssb_idx] = sched_ctrl->CSI_report[idx].choice.ssb_cri_report.CRI_SSBRI[ssb_idx];
      }

      //if strongest measured RSRP is configured
      strongest_ssb_rsrp = get_measured_rsrp(sched_ctrl->CSI_report[idx].choice.ssb_cri_report.RSRP);
      ssb_rsrp[idx * nb_of_csi_ssb_report] = strongest_ssb_rsrp;
      LOG_I(MAC,"ssb_rsrp = %d\n",strongest_ssb_rsrp);

      //if current ssb rsrp is greater than better rsrp
      if(ssb_rsrp[idx * nb_of_csi_ssb_report] > better_rsrp_reported) {
        better_rsrp_reported = ssb_rsrp[idx * nb_of_csi_ssb_report];
        target_ssb_beam_index = idx * nb_of_csi_ssb_report;
      }

      for(diff_rsrp_idx =1; diff_rsrp_idx < nr_ssbri_cri; diff_rsrp_idx++) {
        ssb_rsrp[idx * nb_of_csi_ssb_report + diff_rsrp_idx] = get_diff_rsrp(sched_ctrl->CSI_report[idx].choice.ssb_cri_report.diff_RSRP[diff_rsrp_idx-1], strongest_ssb_rsrp);

        //if current reported rsrp is greater than better rsrp
        if(ssb_rsrp[idx * nb_of_csi_ssb_report + diff_rsrp_idx] > better_rsrp_reported) {
          better_rsrp_reported = ssb_rsrp[idx * nb_of_csi_ssb_report + diff_rsrp_idx];
          target_ssb_beam_index = idx * nb_of_csi_ssb_report + diff_rsrp_idx;
        }
      }
#if 0
      //}
    //if group based beam reporting is enabled
    else if (NR_CSI_ReportConfig__groupBasedBeamReporting_PR_disabled !=
             csi_MeasConfig->csi_ReportConfigToAddModList->list.array[0]->groupBasedBeamReporting.present ) {
      //extracting the ssb indexes
      //for group based reporting only 2 SSB RS are reported, 38.331
      for (ssb_idx = 0; ssb_idx < 2; ssb_idx++) {
        ssb_index[idx * nb_of_csi_ssb_report + ssb_idx] = sched_ctrl->CSI_report[UE_id][idx].choice.ssb_cri_report.CRI_SSBRI[ssb_idx];
      }

      strongest_ssb_rsrp = get_measured_rsrp(sched_ctrl->CSI_report[UE_id][idx].choice.ssb_cri_report.RSRP);
      ssb_rsrp[idx * nb_of_csi_ssb_report] = strongest_ssb_rsrp;

      if(ssb_rsrp[idx * nb_of_csi_ssb_report] > better_rsrp_reported) {
        better_rsrp_reported = ssb_rsrp[idx * nb_of_csi_ssb_report];
        target_ssb_beam_index = idx * nb_of_csi_ssb_report;
      }

      ssb_rsrp[idx * nb_of_csi_ssb_report + 1] = get_diff_rsrp(sched_ctrl->CSI_report[UE_id][idx].choice.ssb_cri_report.diff_RSRP[diff_rsrp_idx], strongest_ssb_rsrp);

      if(ssb_rsrp[idx * nb_of_csi_ssb_report + 1] > better_rsrp_reported) {
        better_rsrp_reported = ssb_rsrp[idx * nb_of_csi_ssb_report + 1];
        target_ssb_beam_index = idx * nb_of_csi_ssb_report + 1;
      }
    }
#endif
  }


  if(ssb_index[target_ssb_beam_index] != ssb_index[curr_ssb_beam_index] && ssb_rsrp[target_ssb_beam_index] > ssb_rsrp[curr_ssb_beam_index]) {
    if( ssb_rsrp[target_ssb_beam_index] - ssb_rsrp[curr_ssb_beam_index] > L1_RSRP_HYSTERIS) {
      is_triggering_ssb_beam_switch = 1;
      LOG_I(MAC, "Triggering ssb beam switching using tci\n");
    }
  }

  if(is_triggering_ssb_beam_switch) {
    //filling pdcch tci state activativation mac ce structure fields
    sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.is_scheduled = 1;
    //OAI currently focusing on Non CA usecase hence 0 is considered as serving
    //cell id
    sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.servingCellId = 0; //0 for PCell as 38.331 v15.9.0 page 353 //serving cell id for which this MAC CE applies
    sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.coresetId = 0; //coreset id for which the TCI State id is being indicated

    /* 38.321 v15.8.0 page 66
    TCI State ID: This field indicates the TCI state identified by TCI-StateId as specified in TS 38.331 [5] applicable
    to the Control Resource Set identified by CORESET ID field.
    If the field of CORESET ID is set to 0,
      this field indicates a TCI-StateId for a TCI state of the first 64 TCI-states configured by tci-States-ToAddModList and tciStates-ToReleaseList in the PDSCH-Config in the active BWP.
    If the field of CORESET ID is set to the other value than 0,
     this field indicates a TCI-StateId configured by tci-StatesPDCCH-ToAddList and tciStatesPDCCH-ToReleaseList in the controlResourceSet identified by the indicated CORESET ID.
    The length of the field is 7 bits
     */
    if(sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.coresetId == 0) {
      int tci_state_id = checkTargetSSBInFirst64TCIStates_pdschConfig(ssb_index[target_ssb_beam_index], Mod_idP, UE_id);

      if( tci_state_id != -1)
        sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.tciStateId = tci_state_id;
      else {
        //identify the best beam within first 64 TCI States of PDSCH
        //Config TCI-states-to-addModList
        int flag = 0;

        for(i =0; ssb_index_sorted[i]!=0; i++) {
          tci_state_id = checkTargetSSBInFirst64TCIStates_pdschConfig(ssb_index_sorted[i], Mod_idP, UE_id) ;

          if(tci_state_id != -1 && ssb_rsrp_sorted[i] > ssb_rsrp[curr_ssb_beam_index] && ssb_rsrp_sorted[i] - ssb_rsrp[curr_ssb_beam_index] > L1_RSRP_HYSTERIS) {
            sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.tciStateId = tci_state_id;
            flag = 1;
            break;
          }
        }

        if(flag == 0 || ssb_rsrp_sorted[i] < ssb_rsrp[curr_ssb_beam_index] || ssb_rsrp_sorted[i] - ssb_rsrp[curr_ssb_beam_index] < L1_RSRP_HYSTERIS) {
          sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.is_scheduled = 0;
        }
      }
    } else {
      int tci_state_id = checkTargetSSBInTCIStates_pdcchConfig(ssb_index[target_ssb_beam_index], Mod_idP, UE_id);

      if (tci_state_id !=-1)
        sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.tciStateId = tci_state_id;
      else {
        //identify the best beam within CORESET/PDCCH
        ////Config TCI-states-to-addModList
        int flag = 0;

        for(i =0; ssb_index_sorted[i]!=0; i++) {
          tci_state_id = checkTargetSSBInTCIStates_pdcchConfig(ssb_index_sorted[i], Mod_idP, UE_id);

          if( tci_state_id != -1 && ssb_rsrp_sorted[i] > ssb_rsrp[curr_ssb_beam_index] && ssb_rsrp_sorted[i] - ssb_rsrp[curr_ssb_beam_index] > L1_RSRP_HYSTERIS) {
            sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.tciStateId = tci_state_id;
            flag = 1;
            break;
          }
        }

        if(flag == 0 || ssb_rsrp_sorted[i] < ssb_rsrp[curr_ssb_beam_index] || ssb_rsrp_sorted[i] - ssb_rsrp[curr_ssb_beam_index] < L1_RSRP_HYSTERIS) {
          sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.is_scheduled = 0;
        }
      }
    }

    sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.tci_present_inDCI = bwp->bwp_Dedicated->pdcch_Config->choice.setup->controlResourceSetToAddModList->list.array[bwp_id-1]->tci_PresentInDCI;

    //filling pdsch tci state activation deactivation mac ce structure fields
    if(sched_ctrl->UE_mac_ce_ctrl.pdcch_state_ind.tci_present_inDCI) {
      sched_ctrl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.is_scheduled = 1;
      /*
      Serving Cell ID: This field indicates the identity of the Serving Cell for which the MAC CE applies
      Considering only PCell exists. Serving cell index of PCell is always 0, hence configuring 0
      */
      sched_ctrl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.servingCellId = 0;
      /*
      BWP ID: This field indicates a DL BWP for which the MAC CE applies as the codepoint of the DCI bandwidth
      part indicator field as specified in TS 38.212
      */
      sched_ctrl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.bwpId = pdsch_bwp_id;

      /*
       * TODO ssb_rsrp_sort() API yet to code to find 8 best beams, rrc configuration
       * is required
       */
      for(i = 0; i<8; i++) {
        sched_ctrl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.tciStateActDeact[i] = i;
      }

      sched_ctrl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.highestTciStateActivated = 8;

      for(i = 0, j =0; i<MAX_TCI_STATES; i++) {
        if(sched_ctrl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.tciStateActDeact[i]) {
          sched_ctrl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.codepoint[j] = i;
          j++;
        }
      }
    }//tci_presentInDCI
  }//is-triggering_beam_switch
}//tci handling


//returns the measured RSRP value (upper limit)
int get_measured_rsrp(uint8_t index) {
  //if index is invalid returning minimum rsrp -140
  if((index >= 0 && index <= 15) || index >= 114)
    return MIN_RSRP_VALUE;

  return L1_SSB_CSI_RSRP_measReport_mapping_38133_10_1_6_1_1[index];
}


//returns the differential RSRP value (upper limit)
int get_diff_rsrp(uint8_t index, int strongest_rsrp) {
  if(strongest_rsrp != -1) {
    return strongest_rsrp + diff_rsrp_ssb_csi_meas_10_1_6_1_2[index];
  } else
    return MIN_RSRP_VALUE;
}


void reverse_n_bits(uint8_t *value, uint16_t bitlen) {
  uint16_t j;
  uint8_t i;
  for(j = bitlen - 1,i = 0; j > i; j--, i++) {
    if(((*value>>j)&1) != ((*value>>i)&1)) {
      *value ^= (1<<j);
      *value ^= (1<<i);
    }
  }
}

void extract_pucch_csi_report (NR_CSI_MeasConfig_t *csi_MeasConfig,
                               nfapi_nr_uci_pucch_pdu_format_2_3_4_t *uci_pdu,
                               NR_UE_sched_ctrl_t *sched_ctrl,
                               frame_t frame,
                               slot_t slot,
                               NR_SubcarrierSpacing_t scs, int UE_id,
                               module_id_t Mod_idP) {

  /** From Table 6.3.1.1.2-3: RI, LI, CQI, and CRI of codebookType=typeI-SinglePanel */
  uint8_t idx = 0;
  uint8_t payload_size = ceil(((double)uci_pdu->csi_part1.csi_part1_bit_len)/8);
  uint8_t *payload = calloc (payload_size, sizeof(uint8_t));
  NR_CSI_ReportConfig__reportQuantity_PR reportQuantity_type = NR_CSI_ReportConfig__reportQuantity_PR_NOTHING;
  NR_UE_info_t *UE_info = &(RC.nrmac[Mod_idP]->UE_info);
  uint8_t csi_report_id = 0;

  memcpy ( payload, uci_pdu->csi_part1.csi_part1_payload, payload_size);
  

  UE_info->csi_report_template[UE_id][csi_report_id].nb_of_csi_ssb_report = 0;
  for ( csi_report_id =0; csi_report_id < csi_MeasConfig->csi_ReportConfigToAddModList->list.count; csi_report_id++ ) {
    //Assuming in periodic reporting for one slot can be configured with only one CSI-ReportConfig
    //   if (csi_MeasConfig->csi_ReportConfigToAddModList->list.array[csi_report_id]->reportConfigType.present == NR_CSI_ReportConfig__reportConfigType_PR_periodic) {
    //Has to implement according to reportSlotConfig type
    /*reportQuantity must be considered according to the current scheduled
      CSI-ReportConfig if multiple CSI-ReportConfigs present*/
    reportQuantity_type = UE_info->csi_report_template[UE_id][csi_report_id].reportQuantity_type;
    LOG_I(PHY,"SFN/SF:%d%d reportQuantity type = %d\n",frame,slot,reportQuantity_type);

    if (NR_CSI_ReportConfig__reportQuantity_PR_ssb_Index_RSRP == reportQuantity_type || 
        NR_CSI_ReportConfig__reportQuantity_PR_cri_RSRP == reportQuantity_type) {
      uint8_t csi_ssb_idx = 0;
      uint8_t diff_rsrp_idx = 0;
      uint8_t cri_ssbri_bitlen = UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.cri_ssbri_bitlen;

    /*! As per the spec 38.212 and table:  6.3.1.1.2-12 in a single UCI sequence we can have multiple CSI_report
     * the number of CSI_report will depend on number of CSI resource sets that are configured in CSI-ResourceConfig RRC IE
     * From spec 38.331 from the IE CSI-ResourceConfig for SSB RSRP reporting we can configure only one resource set
     * From spec 38.214 section 5.2.1.2 For periodic and semi-persistent CSI Resource Settings, the number of CSI-RS Resource Sets configured is limited to S=1
     */

      /** from 38.214 sec 5.2.1.4.2
      - if the UE is configured with the higher layer parameter groupBasedBeamReporting set to 'disabled', the UE is
        not required to update measurements for more than 64 CSI-RS and/or SSB resources, and the UE shall report in
        a single report nrofReportedRS (higher layer configured) different CRI or SSBRI for each report setting

      - if the UE is configured with the higher layer parameter groupBasedBeamReporting set to 'enabled', the UE is not
      required to update measurements for more than 64 CSI-RS and/or SSB resources, and the UE shall report in a
      single reporting instance two different CRI or SSBRI for each report setting, where CSI-RS and/or SSB
      resources can be received simultaneously by the UE either with a single spatial domain receive filter, or with
      multiple simultaneous spatial domain receive filter
      */

      idx = 0; //Since for SSB RSRP reporting in RRC can configure only one ssb resource set per one report config
      sched_ctrl->CSI_report[idx].choice.ssb_cri_report.nr_ssbri_cri = UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen.nb_ssbri_cri;

      for (csi_ssb_idx = 0; csi_ssb_idx < sched_ctrl->CSI_report[idx].choice.ssb_cri_report.nr_ssbri_cri ; csi_ssb_idx++) {
        if(cri_ssbri_bitlen > 1)
          reverse_n_bits(payload, cri_ssbri_bitlen);

        if (NR_CSI_ReportConfig__reportQuantity_PR_ssb_Index_RSRP == reportQuantity_type)
          sched_ctrl->CSI_report[idx].choice.ssb_cri_report.CRI_SSBRI [csi_ssb_idx] = 
            *(UE_info->csi_report_template[UE_id][csi_report_id].SSB_Index_list[cri_ssbri_bitlen>0?((*payload)&~(~1<<(cri_ssbri_bitlen-1))):cri_ssbri_bitlen]);
        else
          sched_ctrl->CSI_report[idx].choice.ssb_cri_report.CRI_SSBRI [csi_ssb_idx] = 
            *(UE_info->csi_report_template[UE_id][csi_report_id].CSI_Index_list[cri_ssbri_bitlen>0?((*payload)&~(~1<<(cri_ssbri_bitlen-1))):cri_ssbri_bitlen]);
	  
        *payload >>= cri_ssbri_bitlen;
        LOG_I(PHY,"SSB_index = %d\n",sched_ctrl->CSI_report[idx].choice.ssb_cri_report.CRI_SSBRI [csi_ssb_idx]);
      }

      reverse_n_bits(payload, 7);
      sched_ctrl->CSI_report[idx].choice.ssb_cri_report.RSRP = (*payload) & 0x7f;
      *payload >>= 7;

      for ( diff_rsrp_idx =0; diff_rsrp_idx < sched_ctrl->CSI_report[idx].choice.ssb_cri_report.nr_ssbri_cri - 1; diff_rsrp_idx++ ) {
        reverse_n_bits(payload,4);
        sched_ctrl->CSI_report[idx].choice.ssb_cri_report.diff_RSRP[diff_rsrp_idx] = (*payload) & 0x0f;
        *payload >>= 4;
      }
      UE_info->csi_report_template[UE_id][csi_report_id].nb_of_csi_ssb_report++;
      LOG_I(MAC,"csi_payload size = %d, rsrp_id = %d\n",payload_size, sched_ctrl->CSI_report[idx].choice.ssb_cri_report.RSRP);
    }
  }

  if ( !(reportQuantity_type)) 
    AssertFatal(reportQuantity_type, "reportQuantity is not configured");


#if 0

  if ( NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_PMI_CQI == reportQuantity_type ||
       NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_i1 == reportQuantity_type ||
       NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_i1_CQI == reportQuantity_type ||
       NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_CQI == reportQuantity_type ||
       NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_LI_PMI_CQI== reportQuantity_type) {
    // Handling of extracting cri
    sched_ctrl->CSI_report[UE_id][cqi_idx].choice.cri_ri_li_pmi_cqi_report.cri = calloc ( 1, ceil(bitlen_cri/8));
    *(sched_ctrl->CSI_report[UE_id][cqi_idx].choice.cri_ri_li_pmi_cqi_report.cri) = *((uint32_t *)payload) & ~(~1<<(bitlen_cri-1));
    *payload >>= bitlen_cri;

    if ( 1 == RC.nrrrc[gnb_mod_idP]->carrier.pdsch_AntennaPorts ) {
      /** From Table 6.3.1.1.2-3: RI, LI, CQI, and CRI of codebookType=typeI-SinglePanel */
      sched_ctrl->CSI_report[UE_id][cqi_idx].choice.cri_ri_li_pmi_cqi_report->ri = NULL;
    } else {
      //Handling for the ri for multiple csi ports
    }
  }

  if (NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_LI_PMI_CQI== reportQuantity_type) {
    if ( 1 == RC.nrrrc[gnb_mod_idP]->carrier.pdsch_AntennaPorts )
      /** From Table 6.3.1.1.2-3: RI, LI, CQI, and CRI of codebookType=typeI-SinglePanel */
      sched_ctrl->CSI_report[UE_id][cqi_idx].choice.cri_ri_li_pmi_cqi_report->li = NULL;
    else {
      //Handle for li for multiple CSI ports
    }
  }

  //TODO: check for zero padding if available shift payload to the number of zero padding bits

  if ( NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_PMI_CQI == reportQuantity_type ||
       NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_LI_PMI_CQI== reportQuantity_type) {
    if ( 1 == RC.nrrrc[gnb_mod_idP]->carrier.pdsch_AntennaPorts ) {
      /** From Table 6.3.1.1.2-3: RI, LI, CQI, and CRI of codebookType=typeI-SinglePanel */
      sched_ctrl->CSI_report[UE_id][cqi_idx].choice.cri_ri_li_pmi_cqi_report->pmi_x1 = NULL;
      sched_ctrl->CSI_report[UE_id][cqi_idx].choice.cri_ri_li_pmi_cqi_report->pmi_x2 = NULL;
    }
  }

  if ( NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_PMI_CQI == reportQuantity_type ||
       NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_i1_CQI == reportQuantity_type ||
       NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_CQI == reportQuantity_type ||
       NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_LI_PMI_CQI== reportQuantity_type) {
    /** From Table 6.3.1.1.2-3: RI, LI, CQI, and CRI of codebookType=typeI-SinglePanel */
    *(sched_ctrl->CSI_report[UE_id][cqi_idx].choice.cri_ri_li_pmi_cqi_report->cqi) = *(payload) & 0x0f;
    *(payload) >>= 4;
  }

#endif
}

