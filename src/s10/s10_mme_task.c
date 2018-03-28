/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under 
 * the Apache License, Version 2.0  (the "License"); you may not use this file
 * except in compliance with the License.  
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
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

/*! \file s10_mme_task.c
* \brief
* \author Dincer Beken
* \company Blackned GmbH
* \email: dbeken@blackned.de
*
* \author Andreas Eberlein
* \company Blackned GmbH
* \email: aeberlein@blackned.de
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "assertions.h"
#include "hashtable.h"
#include "log.h"
#include "msc.h"
#include "mme_config.h"
#include "intertask_interface.h"
#include "timer.h"
#include "NwLog.h"
#include "NwGtpv2c.h"
#include "NwGtpv2cMsg.h"
#include "s10_mme.h"
#include "s10_mme_session_manager.h"

static NwGtpv2cStackHandleT             s10_mme_stack_handle = 0;
// Store the GTPv2-C teid handle
hash_table_ts_t                        *s10_mme_teid_2_gtv2c_teid_handle = NULL;
static void s10_exit(void);
//------------------------------------------------------------------------------
static NwRcT
s10_mme_log_wrapper (
  NwGtpv2cLogMgrHandleT hLogMgr,
  uint32_t logLevel,
  NwCharT * file,
  uint32_t line,
  NwCharT * logStr)
{
  OAILOG_DEBUG (LOG_S10, "%s\n", logStr);
  return NW_OK;
}

//------------------------------------------------------------------------------
static NwRcT
s10_mme_ulp_process_stack_req_cb (
  NwGtpv2cUlpHandleT hUlp,
  NwGtpv2cUlpApiT * pUlpApi)
{
  //     NwRcT rc = NW_OK;
  int                                     ret = 0;

  DevAssert (pUlpApi );

  switch (pUlpApi->apiType) {
  case NW_GTPV2C_ULP_API_INITIAL_REQ_IND:
    OAILOG_DEBUG (LOG_S10, "Received triggered response indication\n");

    switch (pUlpApi->apiInfo.initialReqIndInfo.msgType) {
    case NW_GTP_FORWARD_RELOCATION_REQ:
      ret = s10_mme_handle_forward_relocation_request(&s10_mme_stack_handle, pUlpApi);
      break;
    case NW_GTP_FORWARD_ACCESS_CONTEXT_NTF:
      ret = s10_mme_handle_forward_access_context_notification(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_FORWARD_RELOCATION_COMPLETE_NTF:
      ret = s10_mme_handle_forward_relocation_complete_notification(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_CONTEXT_REQ:
      ret = s10_mme_handle_context_request(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_CONTEXT_RSP:
      ret = s10_mme_handle_context_response(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_CONTEXT_ACK:
      ret = s10_mme_handle_context_acknowledgement(&s10_mme_stack_handle, pUlpApi);
      break;

    default:
      OAILOG_WARNING (LOG_S10, "Received unhandled message type %d\n", pUlpApi->apiInfo.triggeredRspIndInfo.msgType);
      break;
    }
    break;

  case NW_GTPV2C_ULP_API_TRIGGERED_RSP_IND:
    OAILOG_DEBUG (LOG_S10, "Received triggered response indication\n");

    switch (pUlpApi->apiInfo.triggeredRspIndInfo.msgType) {
    case NW_GTP_FORWARD_RELOCATION_RSP:
      ret = s10_mme_handle_forward_relocation_response(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_FORWARD_ACCESS_CONTEXT_NTF:
      ret = s10_mme_handle_forward_access_context_notification(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_FORWARD_ACCESS_CONTEXT_ACK:
      ret = s10_mme_handle_forward_access_context_acknowledge(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_FORWARD_RELOCATION_COMPLETE_ACK:
      ret = s10_mme_handle_forward_relocation_complete_acknowledge(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_CONTEXT_REQ:
      ret = s10_mme_handle_context_request(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_CONTEXT_RSP:
      ret = s10_mme_handle_context_response(&s10_mme_stack_handle, pUlpApi);
      break;

    case NW_GTP_CONTEXT_ACK:
      ret = s10_mme_handle_context_acknowledgement(&s10_mme_stack_handle, pUlpApi);
      break;

    default:
      OAILOG_WARNING (LOG_S10, "Received unhandled message type %d\n", pUlpApi->apiInfo.triggeredRspIndInfo.msgType);
      break;
    }

    break;

  /** Timeout Handler */
  case NW_GTPV2C_ULP_API_RSP_FAILURE_IND:
    ret = s10_mme_handle_ulp_error_indicatior(&s10_mme_stack_handle, pUlpApi);
    break;
  // todo: add initial reqs --> CBR / UBR / DBR !
  default:
    break;
  }

  return ret == 0 ? NW_OK : NW_FAILURE;
}

//------------------------------------------------------------------------------
static NwRcT
s10_mme_send_udp_msg (
  NwGtpv2cUdpHandleT udpHandle,
  uint8_t * buffer,
  uint32_t buffer_len,
  uint32_t peerIpAddr,
  uint32_t peerPort)
{
  // Create and alloc new message
  MessageDef                             *message_p;
  udp_data_req_t                         *udp_data_req_p;
  int                                     ret = 0;

  message_p = itti_alloc_new_message (TASK_S10, UDP_DATA_REQ);
  udp_data_req_p = &message_p->ittiMsg.udp_data_req;
  udp_data_req_p->peer_address = peerIpAddr;
  udp_data_req_p->peer_port = peerPort;
  udp_data_req_p->buffer = buffer;
  udp_data_req_p->buffer_length = buffer_len;
  ret = itti_send_msg_to_task (TASK_UDP, INSTANCE_DEFAULT, message_p);
  return ((ret == 0) ? NW_OK : NW_FAILURE);
}

//------------------------------------------------------------------------------
static NwRcT
s10_mme_start_timer_wrapper (
  NwGtpv2cTimerMgrHandleT tmrMgrHandle,
  uint32_t timeoutSec,
  uint32_t timeoutUsec,
  uint32_t tmrType,
  void *timeoutArg,
  NwGtpv2cTimerHandleT * hTmr)
{
  long                                    timer_id;
  int                                     ret = 0;

  if (tmrType == NW_GTPV2C_TMR_TYPE_REPETITIVE) {
    ret = timer_setup (timeoutSec, timeoutUsec, TASK_S10, INSTANCE_DEFAULT, TIMER_PERIODIC, timeoutArg, &timer_id);
  } else {
    ret = timer_setup (timeoutSec, timeoutUsec, TASK_S10, INSTANCE_DEFAULT, TIMER_ONE_SHOT, timeoutArg, &timer_id);
  }

  *hTmr = (NwGtpv2cTimerHandleT) timer_id;
  return ((ret == 0) ? NW_OK : NW_FAILURE);
}

//------------------------------------------------------------------------------
static NwRcT
s10_mme_stop_timer_wrapper (
  NwGtpv2cTimerMgrHandleT tmrMgrHandle,
  NwGtpv2cTimerHandleT tmrHandle)
{
  long                                    timer_id;

  timer_id = (long)tmrHandle;
  return ((timer_remove (timer_id) == 0) ? NW_OK : NW_FAILURE);
}

static void                            *
s10_mme_thread (
  void *args)
{
  itti_mark_task_ready (TASK_S10);
  OAILOG_START_USE ();
  MSC_START_USE ();

  while (1) {
    MessageDef                             *received_message_p = NULL;

    itti_receive_msg (TASK_S10, &received_message_p);
    assert (received_message_p );

    switch (ITTI_MSG_ID (received_message_p)) {
    /** Only the signals to send. */
    case S10_FORWARD_RELOCATION_REQUEST:{
        s10_mme_forward_relocation_request(&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_forward_relocation_request);
      }
      break;

    case S10_FORWARD_RELOCATION_RESPONSE:{
        s10_mme_forward_relocation_response(&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_forward_relocation_response);
      }
      break;

    case S10_FORWARD_ACCESS_CONTEXT_NOTIFICATION:{
        s10_mme_forward_access_context_notification(&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_forward_access_context_notification);
      }
      break;

    case S10_FORWARD_ACCESS_CONTEXT_ACKNOWLEDGE:{
        s10_mme_forward_access_context_acknowledge(&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_forward_access_context_acknowledge);
      }
      break;

    case S10_FORWARD_RELOCATION_COMPLETE_NOTIFICATION:{
        s10_mme_forward_relocation_complete_notification(&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_forward_relocation_complete_notification);
      }
      break;

    case S10_FORWARD_RELOCATION_COMPLETE_ACKNOWLEDGE:{
        s10_mme_forward_relocation_complete_acknowledge(&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_forward_relocation_complete_acknowledge);
      }
      break;

    case S10_CONTEXT_REQUEST:{
        s10_mme_context_request (&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_context_request);
      }
      break;

    case S10_CONTEXT_RESPONSE:{
        s10_mme_context_response (&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_context_response);
      }
      break;

    case S10_CONTEXT_ACKNOWLEDGE:{
        s10_mme_context_acknowledge (&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_context_acknowledge);
      }
      break;

    /** New Internal Messages. */

    /**
     * Use this message in case of an error to remove the S10 Local Tunnel endpoints.
     * No response to MME_APP is sent/expected.
     */
    case S10_REMOVE_UE_TUNNEL:{
        s10_mme_remove_ue_tunnel(&s10_mme_stack_handle, &received_message_p->ittiMsg.s10_remove_ue_tunnel);
      }
      break;

    case UDP_DATA_IND:{
        /*
         * We received new data to handle from the UDP layer
         */
        NwRcT                                   rc;
        udp_data_ind_t                         *udp_data_ind;

        udp_data_ind = &received_message_p->ittiMsg.udp_data_ind;
        rc = nwGtpv2cProcessUdpReq (s10_mme_stack_handle, udp_data_ind->buffer, udp_data_ind->buffer_length, udp_data_ind->peer_port, udp_data_ind->peer_address);
        DevAssert (rc == NW_OK);
      }
      break;

    case TIMER_HAS_EXPIRED:{
        OAILOG_DEBUG (LOG_S10, "Processing timeout for timer_id 0x%lx and arg %p\n", received_message_p->ittiMsg.timer_has_expired.timer_id, received_message_p->ittiMsg.timer_has_expired.arg);
        DevAssert (nwGtpv2cProcessTimeout (received_message_p->ittiMsg.timer_has_expired.arg) == NW_OK);
      }
      break;
    case TERMINATE_MESSAGE: {
      s10_exit();
      itti_exit_task ();
      break;
    }

    default:{
        OAILOG_ERROR (LOG_S10, "Unkwnon message ID %d:%s\n", ITTI_MSG_ID (received_message_p), ITTI_MSG_NAME (received_message_p));
      }
      break;
    }

    itti_free (ITTI_MSG_ORIGIN_ID (received_message_p), received_message_p);
    received_message_p = NULL;
  }

  return NULL;
}

//------------------------------------------------------------------------------
static int
s10_send_init_udp (
  char *address,
  uint16_t port_number)
{
  MessageDef                             *message_p = itti_alloc_new_message (TASK_S10, UDP_INIT);
  if (message_p == NULL) {
    return RETURNerror;
  }
  message_p->ittiMsg.udp_init.port = port_number;
  message_p->ittiMsg.udp_init.address = address;
  OAILOG_DEBUG (LOG_S10, "Tx UDP_INIT IP addr %s:%d\n", message_p->ittiMsg.udp_init.address, message_p->ittiMsg.udp_init.port);
  return itti_send_msg_to_task (TASK_UDP, INSTANCE_DEFAULT, message_p);
}

//------------------------------------------------------------------------------
int
s10_mme_init (
  const mme_config_t * mme_config_p)
{
  int                                     ret = 0;
  NwGtpv2cUlpEntityT                      ulp;
  NwGtpv2cUdpEntityT                      udp;
  NwGtpv2cTimerMgrEntityT                 tmrMgr;
  NwGtpv2cLogMgrEntityT                   logMgr;
  struct in_addr                          addr;
  char                                   *s10_address_str = NULL;

  OAILOG_DEBUG (LOG_S10, "Initializing S10 interface\n");

  if (nwGtpv2cInitialize (&s10_mme_stack_handle) != NW_OK) {
    OAILOG_ERROR (LOG_S10, "Failed to initialize gtpv2-c stack\n");
    goto fail;
  }

  /*
   * Set ULP entity
   */
  ulp.hUlp = (NwGtpv2cUlpHandleT) NULL;
  ulp.ulpReqCallback = s10_mme_ulp_process_stack_req_cb;
  DevAssert (NW_OK == nwGtpv2cSetUlpEntity (s10_mme_stack_handle, &ulp));
  /*
   * Set UDP entity
   */
  udp.hUdp = (NwGtpv2cUdpHandleT) NULL;
  udp.udpDataReqCallback = s10_mme_send_udp_msg;
  DevAssert (NW_OK == nwGtpv2cSetUdpEntity (s10_mme_stack_handle, &udp));
  /*
   * Set Timer entity
   */
  tmrMgr.tmrMgrHandle = (NwGtpv2cTimerMgrHandleT) NULL;
  tmrMgr.tmrStartCallback = s10_mme_start_timer_wrapper;
  tmrMgr.tmrStopCallback = s10_mme_stop_timer_wrapper;
  DevAssert (NW_OK == nwGtpv2cSetTimerMgrEntity (s10_mme_stack_handle, &tmrMgr));
  logMgr.logMgrHandle = 0;
  logMgr.logReqCallback = s10_mme_log_wrapper;
  DevAssert (NW_OK == nwGtpv2cSetLogMgrEntity (s10_mme_stack_handle, &logMgr));

  if (itti_create_task (TASK_S10, &s10_mme_thread, NULL) < 0) {
    OAILOG_ERROR (LOG_S10, "gtpv1u phtread_create: %s\n", strerror (errno));
    goto fail;
  }

  DevAssert (NW_OK == nwGtpv2cSetLogLevel (s10_mme_stack_handle, NW_LOG_LEVEL_DEBG));
  mme_config_read_lock (&mme_config);
  addr.s_addr = mme_config.ipv4.s10;
  s10_address_str = inet_ntoa (addr);
  DevAssert (s10_address_str );
  s10_send_init_udp (s10_address_str, mme_config.ipv4.port_s10);
  mme_config_unlock (&mme_config);

  bstring b = bfromcstr("s10_mme_teid_2_gtv2c_teid_handle");
  s10_mme_teid_2_gtv2c_teid_handle = hashtable_ts_create(mme_config_p->max_ues, HASH_TABLE_DEFAULT_HASH_FUNC, hash_free_int_func, b);
  bdestroy(b);

  OAILOG_DEBUG (LOG_S10, "Initializing S10 interface: DONE\n");
  return ret;
fail:
  OAILOG_DEBUG (LOG_S10, "Initializing S10 interface: FAILURE\n");
  return RETURNerror;
}


static void s10_exit(void)
{
  if (nwGtpv2cFinalize(s10_mme_stack_handle) != NW_OK) {
    OAI_FPRINTF_ERR ("An error occurred during tear down of nwGtp s10 stack.\n");
  }
  if (hashtable_ts_destroy(s10_mme_teid_2_gtv2c_teid_handle) != HASH_TABLE_OK) {
    OAI_FPRINTF_ERR("An error occured while destroying s10 teid hash table");
  }
}
