/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
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

/* file: lte_sync_time.c
   purpose: coarse timing synchronization for LTE (using PSS)
   author: florian.kaltenberger@eurecom.fr, oscar.tonelli@yahoo.it
   date: 22.10.2009
*/

//#include <string.h>
#include "defs_NB_IoT.h"
#include "PHY/defs_NB_IoT.h"
#include "PHY/extern_NB_IoT.h"
// #include "SCHED/extern_NB_IoT.h"
#include <math.h>

#ifdef OPENAIR2
#include "LAYER2/MAC/defs.h"
#include "LAYER2/MAC/extern.h"
#include "RRC/LITE/extern.h"
#include "PHY_INTERFACE/extern.h"
#endif
//#define DEBUG_PHY

int* sync_corr_ue0 = NULL;
// int* sync_corr_ue1 = NULL;
// int* sync_corr_ue2 = NULL;
int sync_tmp[2048*4] __attribute__((aligned(32)));
short syncF_tmp[2048*2] __attribute__((aligned(32)));



int lte_sync_time_init_NB_IoT(NB_IoT_DL_FRAME_PARMS *frame_parms )   // LTE_UE_COMMON *common_vars
{

  int i,k,k2,l;

  sync_corr_ue0_NB_IoT = (int *)malloc16(LTE_NUMBER_OF_SUBFRAMES_PER_FRAME*sizeof(int)*frame_parms->samples_per_tti);
  // sync_corr_ue1 = (int *)malloc16(LTE_NUMBER_OF_SUBFRAMES_PER_FRAME*sizeof(int)*frame_parms->samples_per_tti);
  // sync_corr_ue2 = (int *)malloc16(LTE_NUMBER_OF_SUBFRAMES_PER_FRAME*sizeof(int)*frame_parms->samples_per_tti);

  if (sync_corr_ue0) {
#ifdef DEBUG_PHY
    msg("[openair][LTE_PHY][SYNC] sync_corr_ue allocated at %p\n", sync_corr_ue0);
#endif
    //common_vars->sync_corr = sync_corr;
  } else {
    msg("[openair][LTE_PHY][SYNC] sync_corr_ue0 not allocated\n");
    return(-1);
  }

//   if (sync_corr_ue1) {
// #ifdef DEBUG_PHY
//     msg("[openair][LTE_PHY][SYNC] sync_corr_ue allocated at %p\n", sync_corr_ue1);
// #endif
//     //common_vars->sync_corr = sync_corr;
//   } else {
//     msg("[openair][LTE_PHY][SYNC] sync_corr_ue1 not allocated\n");
//     return(-1);
//   }

//   if (sync_corr_ue2) {
// #ifdef DEBUG_PHY
//     msg("[openair][LTE_PHY][SYNC] sync_corr_ue allocated at %p\n", sync_corr_ue2);
// #endif
//     //common_vars->sync_corr = sync_corr;
//   } else {
//     msg("[openair][LTE_PHY][SYNC] sync_corr_ue2 not allocated\n");
//     return(-1);
//   }

  //  primary_synch0_time = (int *)malloc16((frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples)*sizeof(int)); 

  // Consider the CPs 10 normal length + 1 longer due to first symbol in slot
  primary_synch0_time_NB_IoT = (int16_t *)malloc16((frame_parms->ofdm_symbol_size*11 + frame_parms->nb_prefix_samples*10 + frame_parms->nb_prefix_samples0)*sizeof(int16_t)*2); // 11 symbols per subframe dedicated to primary synchro

  if (primary_synch0_time_NB_IoT) {
    //    bzero(primary_synch0_time,(frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples)*sizeof(int));
    bzero(primary_synch0_time_NB_IoT,(frame_parms->ofdm_symbol_size)*sizeof(int16_t)*2*11);
#ifdef DEBUG_PHY
    msg("[openair][LTE_PHY][SYNC] primary_synch0_time allocated at %p\n", primary_synch0_time_NB_IoT);
#endif
  } else {
    msg("[openair][LTE_PHY][SYNC] primary_synch0_time not allocated\n");
    return(-1);
  }

  //  primary_synch1_time = (int *)malloc16((frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples)*sizeof(int));
//   primary_synch1_time = (int16_t *)malloc16((frame_parms->ofdm_symbol_size)*sizeof(int16_t)*2);

//   if (primary_synch1_time) {
//     //    bzero(primary_synch1_time,(frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples)*sizeof(int));
//     bzero(primary_synch1_time,(frame_parms->ofdm_symbol_size)*sizeof(int16_t)*2);
// #ifdef DEBUG_PHY
//     msg("[openair][LTE_PHY][SYNC] primary_synch1_time allocated at %p\n", primary_synch1_time);
// #endif
//   } else {
//     msg("[openair][LTE_PHY][SYNC] primary_synch1_time not allocated\n");
//     return(-1);
//   }

//   //  primary_synch2_time = (int *)malloc16((frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples)*sizeof(int));
//   primary_synch2_time = (int16_t *)malloc16((frame_parms->ofdm_symbol_size)*sizeof(int16_t)*2);

//   if (primary_synch2_time) {
//     //    bzero(primary_synch2_time,(frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples)*sizeof(int));
//     bzero(primary_synch2_time,(frame_parms->ofdm_symbol_size)*sizeof(int16_t)*2);
// #ifdef DEBUG_PHY
//     msg("[openair][LTE_PHY][SYNC] primary_synch2_time allocated at %p\n", primary_synch2_time);
// #endif
//   } else {
//     msg("[openair][LTE_PHY][SYNC] primary_synch2_time not allocated\n");
//     return(-1);
//   }


  // generate oversampled sync_time sequences 

  if (frame_parms->NB_IoT_RB_ID <= (frame_parms->N_RB_DL>>1)) { // NB-IoT RB is in the first half 
    k = frame_parms->ofdm_symbol_size - frame_parms->N_RB_DL*6 + frame_parms->NB_IoT_RB_ID*12; 
  }else{// second half: DC carrier offset 
    k = 1 + 6*(2*frame_parms->NB_IoT_RB_ID - frame_parms->N_RB_DL); 
  }

  for (l=0; l<11 ; l++){
    k2 = k; 
    for (i=0; i<12; i++) { // 12 subcarriers in NB-IoT

        syncF_tmp[2*k2] = primary_synch_NB_IoT[12*l + 2*i]>>2;  //we need to shift input to avoid overflow in fft
        syncF_tmp[2*k2+1] = primary_synch_NB_IoT[12*l + 2*i+1]>>2;
        k2++;

    }

    switch (frame_parms->N_RB_DL) {
    case 6:
      idft128((short*)syncF_tmp,          /// complex input
  	   (short*)sync_tmp, /// complex output
  	   1);
      break;
    case 25:
      idft512((short*)syncF_tmp,          /// complex input
  	   (short*)sync_tmp, /// complex output
  	   1);
      break;
    case 50:
      idft1024((short*)syncF_tmp,          /// complex input
  	    (short*)sync_tmp, /// complex output
  	    1);
      break;
      
    case 75:
      idft1536((short*)syncF_tmp,          /// complex input
  	     (short*)sync_tmp,
  	     1); /// complex output
      break;
    case 100:
      idft2048((short*)syncF_tmp,          /// complex input
  	     (short*)sync_tmp, /// complex output
  	     1);
      break;
    default:
      LOG_E(PHY,"Unsupported N_RB_DL %d\n",frame_parms->N_RB_DL);
      break;
    }

    for (i=0; i<frame_parms->ofdm_symbol_size; i++)
      if (l < 4){ // Skip longest CP length
        ((int32_t*)primary_synch0_time_NB_IoT)[l*(frame_parms->nb_prefix_samples + frame_parms->ofdm_symbol_size) + i] = sync_tmp[i];
      }else{ // take into account the longest CP length is second slot of subframe
        ((int32_t*)primary_synch0_time_NB_IoT)[frame_parms->nb_prefix_samples0 + frame_parms->ofdm_symbol_size + 
                                      (l-1)*(frame_parms->nb_prefix_samples + frame_parms->ofdm_symbol_size) + i] = sync_tmp[i];
      }

  }

  // k=frame_parms->ofdm_symbol_size-36;

  // for (i=0; i<72; i++) {
  //   syncF_tmp[2*k] = primary_synch1[2*i]>>2;  //we need to shift input to avoid overflow in fft
  //   syncF_tmp[2*k+1] = primary_synch1[2*i+1]>>2;
  //   k++;

  //   if (k >= frame_parms->ofdm_symbol_size) {
  //     k++;  // skip DC carrier
  //     k-=frame_parms->ofdm_symbol_size;
  //   }
  // }

  // switch (frame_parms->N_RB_DL) {
  // case 6:
  //   idft128((short*)syncF_tmp,          /// complex input
	 //   (short*)sync_tmp, /// complex output
	 //   1);
  //   break;
  // case 25:
  //   idft512((short*)syncF_tmp,          /// complex input
	 //   (short*)sync_tmp, /// complex output
	 //   1);
  //   break;
  // case 50:
  //   idft1024((short*)syncF_tmp,          /// complex input
	 //    (short*)sync_tmp, /// complex output
	 //    1);
  //   break;
    
  // case 75:
  //   idft1536((short*)syncF_tmp,          /// complex input
	 //     (short*)sync_tmp, /// complex output
	 //     1);
  //   break;
  // case 100:
  //   idft2048((short*)syncF_tmp,          /// complex input
	 //    (short*)sync_tmp, /// complex output
	 //    1);
  //   break;
  // default:
  //   LOG_E(PHY,"Unsupported N_RB_DL %d\n",frame_parms->N_RB_DL);
  //   break;
  // }

  // for (i=0; i<frame_parms->ofdm_symbol_size; i++)
  //   ((int32_t*)primary_synch1_time)[i] = sync_tmp[i];

  // k=frame_parms->ofdm_symbol_size-36;

  // for (i=0; i<72; i++) {
  //   syncF_tmp[2*k] = primary_synch2[2*i]>>2;  //we need to shift input to avoid overflow in fft
  //   syncF_tmp[2*k+1] = primary_synch2[2*i+1]>>2;
  //   k++;

  //   if (k >= frame_parms->ofdm_symbol_size) {
  //     k++;  // skip DC carrier
  //     k-=frame_parms->ofdm_symbol_size;
  //   }
  // }

  // switch (frame_parms->N_RB_DL) {
  // case 6:
  //   idft128((short*)syncF_tmp,          /// complex input
	 //   (short*)sync_tmp, /// complex output
	 //   1);
  //   break;
  // case 25:
  //   idft512((short*)syncF_tmp,          /// complex input
	 //   (short*)sync_tmp, /// complex output
	 //   1);
  //   break;
  // case 50:
  //   idft1024((short*)syncF_tmp,          /// complex input
	 //    (short*)sync_tmp, /// complex output
	 //    1);
  //   break;
    
  // case 75:
  //   idft1536((short*)syncF_tmp,          /// complex input
	 //     (short*)sync_tmp, /// complex output
	 //     1);
  //   break;
  // case 100:
  //   idft2048((short*)syncF_tmp,          /// complex input
	 //    (short*)sync_tmp, /// complex output
	 //    1);
  //   break;
  // default:
  //   LOG_E(PHY,"Unsupported N_RB_DL %d\n",frame_parms->N_RB_DL);
  //   break;
  // }

  // for (i=0; i<frame_parms->ofdm_symbol_size; i++)
  //   ((int32_t*)primary_synch2_time)[i] = sync_tmp[i];




#ifdef DEBUG_PHY
  write_output("primary_sync0.m","psync0",primary_synch0_time,frame_parms->ofdm_symbol_size,1,1);
  // write_output("primary_sync1.m","psync1",primary_synch1_time,frame_parms->ofdm_symbol_size,1,1);
  // write_output("primary_sync2.m","psync2",primary_synch2_time,frame_parms->ofdm_symbol_size,1,1);
#endif
  return (1);
}


void lte_sync_time_free_NB_IoT(void)
{


  if (sync_corr_ue0) {
    msg("Freeing sync_corr_ue (%p)...\n",sync_corr_ue0);
    free(sync_corr_ue0);
  }

  // if (sync_corr_ue1) {
  //   msg("Freeing sync_corr_ue (%p)...\n",sync_corr_ue1);
  //   free(sync_corr_ue1);
  // }

  // if (sync_corr_ue2) {
  //   msg("Freeing sync_corr_ue (%p)...\n",sync_corr_ue2);
  //   free(sync_corr_ue2);
  // }

  if (primary_synch0_time_NB_IoT) {
    msg("Freeing primary_sync0_time ...\n");
    free(primary_synch0_time_NB_IoT);
  }

  // if (primary_synch1_time) {
  //   msg("Freeing primary_sync1_time ...\n");
  //   free(primary_synch1_time);
  // }

  // if (primary_synch2_time) {
  //   msg("Freeing primary_sync2_time ...\n");
  //   free(primary_synch2_time);
  // }

  sync_corr_ue0_NB_IoT = NULL;
  // sync_corr_ue1 = NULL;
  // sync_corr_ue2 = NULL;
  primary_synch0_time_NB_IoT = NULL;
  // primary_synch1_time = NULL;
  // primary_synch2_time = NULL;
}

static inline int abs32(int x)
{
  return (((int)((short*)&x)[0])*((int)((short*)&x)[0]) + ((int)((short*)&x)[1])*((int)((short*)&x)[1]));
}

#ifdef DEBUG_PHY
int debug_cnt=0;
#endif

#define SHIFT 17

int lte_sync_time_NB_IoT(int **rxdata, ///rx data in time domain
                  NB_IoT_DL_FRAME_PARMS *frame_parms,
                  int *eNB_id)
{



  // perform a time domain correlation using the oversampled sync sequence

  unsigned int n, ar, /*s,*/ peak_pos, peak_val/*, sync_source*/;
  int result;//result2;
  // int sync_out[3] = {0,0,0},sync_out2[3] = {0,0,0};
  // int tmp[3] = {0,0,0};
  int sync_out = 0;//sync_out2 = 0;
  int tmp = 0;
  // int length =   LTE_NUMBER_OF_SUBFRAMES_PER_FRAME*frame_parms->samples_per_tti>>1;
  int length =   LTE_NUMBER_OF_SUBFRAMES_PER_FRAME*frame_parms->samples_per_tti;

  //msg("[SYNC TIME] Calling sync_time.\n");
  if (sync_corr_ue0 == NULL) {
    msg("[SYNC TIME] sync_corr_ue0 not yet allocated! Exiting.\n");
    return(-1);
  }

  // if (sync_corr_ue1 == NULL) {
  //   msg("[SYNC TIME] sync_corr_ue1 not yet allocated! Exiting.\n");
  //   return(-1);
  // }

  // if (sync_corr_ue2 == NULL) {
  //   msg("[SYNC TIME] sync_corr_ue2 not yet allocated! Exiting.\n");
  //   return(-1);
  // }

  peak_val = 0;
  peak_pos = 0;
  // sync_source = 0;


  for (n=0; n<length; n+=4) {

#ifdef RTAI_ENABLED

    // This is necessary since the sync takes a long time and it seems to block all other threads thus screwing up RTAI. If we pause it for a little while during its execution we give RTAI a chance to catch up with its other tasks.
    if ((n%frame_parms->samples_per_tti == 0) && (n>0) && (openair_daq_vars.sync_state==0)) {
#ifdef DEBUG_PHY
      msg("[SYNC TIME] pausing for 1000ns, n=%d\n",n);
#endif
      rt_sleep(nano2count(1000));
    }

#endif

    sync_corr_ue0[n] = 0;
    // sync_corr_ue0[n+length] = 0;
    // sync_corr_ue1[n] = 0;
    // sync_corr_ue1[n+length] = 0;
    // sync_corr_ue2[n] = 0;
    // sync_corr_ue2[n+length] = 0;

    // for (s=0; s<3; s++) {
    //   sync_out[s]=0;
    //   sync_out2[s]=0;
    // }

    //    if (n<(length-frame_parms->ofdm_symbol_size-frame_parms->nb_prefix_samples)) {
    if (n<(length-frame_parms->ofdm_symbol_size)) {

      //calculate dot product of primary_synch0_time and rxdata[ar][n] (ar=0..nb_ant_rx) and store the sum in temp[n];
      // for (ar=0; ar<frame_parms->nb_antennas_rx; ar++) {
      for (ar=0; ar<1; ar++) {

        result  = dot_product((short*)primary_synch0_time_NB_IoT, (short*) &(rxdata[ar][n]), 11*frame_parms->ofdm_symbol_size, SHIFT);
        // result2 = dot_product((short*)primary_synch0_time, (short*) &(rxdata[ar][n+length]), 11*frame_parms->ofdm_symbol_size, SHIFT);

        ((short*)sync_corr_ue0_NB_IoT)[2*n] += ((short*) &result)[0];
        ((short*)sync_corr_ue0_NB_IoT)[2*n+1] += ((short*) &result)[1];
        // ((short*)sync_corr_ue0)[2*(length+n)] += ((short*) &result2)[0];
        // ((short*)sync_corr_ue0)[(2*(length+n))+1] += ((short*) &result2)[1];
        ((short*)sync_out)[0] += ((short*) &result)[0];
        ((short*)sync_out)[1] += ((short*) &result)[1];
        // ((short*)sync_out2)[0] += ((short*) &result2)[0];
        // ((short*)sync_out2)[1] += ((short*) &result2)[1];
      }

      // for (ar=0; ar<frame_parms->nb_antennas_rx; ar++) {
      //   result = dot_product((short*)primary_synch1_time, (short*) &(rxdata[ar][n]), frame_parms->ofdm_symbol_size, SHIFT);
      //   result2 = dot_product((short*)primary_synch1_time, (short*) &(rxdata[ar][n+length]), frame_parms->ofdm_symbol_size, SHIFT);
      //   ((short*)sync_corr_ue1)[2*n] += ((short*) &result)[0];
      //   ((short*)sync_corr_ue1)[2*n+1] += ((short*) &result)[1];
      //   ((short*)sync_corr_ue1)[2*(length+n)] += ((short*) &result2)[0];
      //   ((short*)sync_corr_ue1)[(2*(length+n))+1] += ((short*) &result2)[1];

      //   ((short*)sync_out)[2] += ((short*) &result)[0];
      //   ((short*)sync_out)[3] += ((short*) &result)[1];
      //   ((short*)sync_out2)[2] += ((short*) &result2)[0];
      //   ((short*)sync_out2)[3] += ((short*) &result2)[1];
      // }

      // for (ar=0; ar<frame_parms->nb_antennas_rx; ar++) {

      //   result = dot_product((short*)primary_synch2_time, (short*) &(rxdata[ar][n]), frame_parms->ofdm_symbol_size, SHIFT);
      //   result2 = dot_product((short*)primary_synch2_time, (short*) &(rxdata[ar][n+length]), frame_parms->ofdm_symbol_size, SHIFT);
      //   ((short*)sync_corr_ue2)[2*n] += ((short*) &result)[0];
      //   ((short*)sync_corr_ue2)[2*n+1] += ((short*) &result)[1];
      //   ((short*)sync_corr_ue2)[2*(length+n)] += ((short*) &result2)[0];
      //   ((short*)sync_corr_ue2)[(2*(length+n))+1] += ((short*) &result2)[1];
      //   ((short*)sync_out)[4] += ((short*) &result)[0];
      //   ((short*)sync_out)[5] += ((short*) &result)[1];
      //   ((short*)sync_out2)[4] += ((short*) &result2)[0];
      //   ((short*)sync_out2)[5] += ((short*) &result2)[1];
      // }

    }

    // calculate the absolute value of sync_corr[n]

    sync_corr_ue0_NB_IoT[n] = abs32(sync_corr_ue0_NB_IoT[n]);
    // sync_corr_ue0[n+length] = abs32(sync_corr_ue0[n+length]);
    // sync_corr_ue1[n] = abs32(sync_corr_ue1[n]);
    // sync_corr_ue1[n+length] = abs32(sync_corr_ue1[n+length]);
    // sync_corr_ue2[n] = abs32(sync_corr_ue2[n]);
    // sync_corr_ue2[n+length] = abs32(sync_corr_ue2[n+length]);

    // for (s=0; s<3; s++) {
    tmp = (abs32(sync_out)>>1); // + (abs32(sync_out2)>>1);

    if (tmp>peak_val) {
      peak_val = tmp;
      peak_pos = n;
      // sync_source = s;
      /*
      printf("s %d: n %d sync_out %d, sync_out2  %d (sync_corr %d,%d), (%d,%d) (%d,%d)\n",s,n,abs32(sync_out[s]),abs32(sync_out2[s]),sync_corr_ue0[n],
             sync_corr_ue0[n+length],((int16_t*)&sync_out[s])[0],((int16_t*)&sync_out[s])[1],((int16_t*)&sync_out2[s])[0],((int16_t*)&sync_out2[s])[1]);
      */
    }
    // }
  }

  // *eNB_id = sync_source;

  LOG_D(PHY,"[UE] lte_sync_time: Peak found at pos %d, val = %d (%d dB)\n",peak_pos,peak_val,dB_fixed(peak_val)/2);


#ifdef DEBUG_PHY
  if (debug_cnt == 0) {
    write_output("sync_corr0_ue.m","synccorr0",sync_corr_ue0,2*length,1,2);
    // write_output("sync_corr1_ue.m","synccorr1",sync_corr_ue1,2*length,1,2);
    // write_output("sync_corr2_ue.m","synccorr2",sync_corr_ue2,2*length,1,2);
    write_output("rxdata0.m","rxd0",rxdata[0],length<<1,1,1);
    //    exit(-1);
  } else {
    debug_cnt++;
  }


#endif


  return(peak_pos);

}

//#define DEBUG_PHY

// int lte_sync_time_eNB(int32_t **rxdata, ///rx data in time domain
//                       LTE_DL_FRAME_PARMS *frame_parms,
//                       uint32_t length,
//                       uint32_t *peak_val_out,
//                       uint32_t *sync_corr_eNB)
// {

//   // perform a time domain correlation using the oversampled sync sequence

//   unsigned int n, ar, peak_val, peak_pos;
//   uint64_t mean_val;
//   int result;
//   short *primary_synch_time;
//   int eNB_id = frame_parms->Nid_cell%3;

//   // msg("[SYNC TIME] Calling sync_time_eNB(%p,%p,%d,%d)\n",rxdata,frame_parms,eNB_id,length);
//   if (sync_corr_eNB == NULL) {
//     LOG_E(PHY,"[SYNC TIME] sync_corr_eNB not yet allocated! Exiting.\n");
//     return(-1);
//   }

//   switch (eNB_id) {
//   case 0:
//     primary_synch_time = (short*)primary_synch0_time;
//     break;

//   case 1:
//     primary_synch_time = (short*)primary_synch1_time;
//     break;

//   case 2:
//     primary_synch_time = (short*)primary_synch2_time;
//     break;

//   default:
//     LOG_E(PHY,"[SYNC TIME] Illegal eNB_id!\n");
//     return (-1);
//   }

//   peak_val = 0;
//   peak_pos = 0;
//   mean_val = 0;

//   for (n=0; n<length; n+=4) {

//     sync_corr_eNB[n] = 0;

//     if (n<(length-frame_parms->ofdm_symbol_size-frame_parms->nb_prefix_samples)) {

//       //calculate dot product of primary_synch0_time and rxdata[ar][n] (ar=0..nb_ant_rx) and store the sum in temp[n];
//       for (ar=0; ar<frame_parms->nb_antennas_rx; ar++)  {

//         result = dot_product((short*)primary_synch_time, (short*) &(rxdata[ar][n]), frame_parms->ofdm_symbol_size, SHIFT);
//         //((short*)sync_corr)[2*n]   += ((short*) &result)[0];
//         //((short*)sync_corr)[2*n+1] += ((short*) &result)[1];
//         sync_corr_eNB[n] += abs32(result);

//       }

//     }

//     /*
//     if (eNB_id == 2) {
//       printf("sync_time_eNB %d : %d,%d (%d)\n",n,sync_corr_eNB[n],mean_val,
//        peak_val);
//     }
//     */
//     mean_val += sync_corr_eNB[n];

//     if (sync_corr_eNB[n]>peak_val) {
//       peak_val = sync_corr_eNB[n];
//       peak_pos = n;
//     }
//   }

//   mean_val/=length;

//   *peak_val_out = peak_val;

//   if (peak_val <= (40*(uint32_t)mean_val)) {
//     LOG_D(PHY,"[SYNC TIME] No peak found (%u,%u,%"PRIu64",%"PRIu64")\n",peak_pos,peak_val,mean_val,40*mean_val);
//     return(-1);
//   } else {
//     LOG_D(PHY,"[SYNC TIME] Peak found at pos %u, val = %u, mean_val = %"PRIu64"\n",peak_pos,peak_val,mean_val);
//     return(peak_pos);
//   }

// }

// #ifdef PHY_ABSTRACTION
// #include "SIMULATION/TOOLS/defs.h"
// #include "SIMULATION/RF/defs.h"
// //extern channel_desc_t *UE2eNB[NUMBER_OF_UE_MAX][NUMBER_OF_eNB_MAX];

// int lte_sync_time_eNB_emul(PHY_VARS_eNB *phy_vars_eNB,
//                            uint8_t sect_id,
//                            int32_t *sync_val)
// {

//   uint8_t UE_id;
//   uint8_t CC_id = phy_vars_eNB->CC_id;

//   msg("[PHY] EMUL lte_sync_time_eNB_emul eNB %d, sect_id %d\n",phy_vars_eNB->Mod_id,sect_id);
//   *sync_val = 0;

//   for (UE_id=0; UE_id<NB_UE_INST; UE_id++) {
//     //msg("[PHY] EMUL : eNB %d checking UE %d (PRACH %d) PL %d dB\n",phy_vars_eNB->Mod_id,UE_id,PHY_vars_UE_g[UE_id]->generate_prach,UE2eNB[UE_id][phy_vars_eNB->Mod_id]->path_loss_dB);
//     if ((PHY_vars_UE_g[UE_id][CC_id]->generate_prach == 1) && (phy_vars_eNB->Mod_id == (UE_id % NB_eNB_INST))) {
//       *sync_val = 1;
//       return(0);
//     }
//   }

//   return(-1);
// }
// #endif
