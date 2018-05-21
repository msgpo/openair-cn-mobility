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

/*! \file sgw_config.h
* \brief
* \author Lionel Gauthier
* \company Eurecom
* \email: lionel.gauthier@eurecom.fr
*/

#ifndef FILE_SGW_CONFIG_SEEN
#define FILE_SGW_CONFIG_SEEN
#include <stdint.h>
#include <stdbool.h>
#include "log.h"
#include "bstrlib.h"
#include "common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SGW_CONFIG_STRING_SGW_CONFIG                            "S-GW"
#define SGW_CONFIG_STRING_NETWORK_INTERFACES_CONFIG             "NETWORK_INTERFACES"
#define SGW_CONFIG_STRING_OVS_CONFIG                            "OVS"
#define SGW_CONFIG_STRING_SGW_INTERFACE_NAME_FOR_S1U_S12_S4_UP  "SGW_INTERFACE_NAME_FOR_S1U_S12_S4_UP"
#define SGW_CONFIG_STRING_SGW_IPV4_ADDRESS_FOR_S1U_S12_S4_UP    "SGW_IPV4_ADDRESS_FOR_S1U_S12_S4_UP"
#define SGW_CONFIG_STRING_SGW_UDP_PORT_FOR_S1U_S12_S4_UP        "SGW_UDP_PORT_FOR_S1U_S12_S4_UP"
#define SGW_CONFIG_STRING_SGW_INTERFACE_NAME_FOR_S5_S8_UP       "SGW_INTERFACE_NAME_FOR_S5_S8_UP"
#define SGW_CONFIG_STRING_SGW_IPV4_ADDRESS_FOR_S5_S8_UP         "SGW_IPV4_ADDRESS_FOR_S5_S8_UP"
#define SGW_CONFIG_STRING_SGW_INTERFACE_NAME_FOR_S11            "SGW_INTERFACE_NAME_FOR_S11"
#define SGW_CONFIG_STRING_SGW_IPV4_ADDRESS_FOR_S11              "SGW_IPV4_ADDRESS_FOR_S11"
#define SGW_CONFIG_STRING_SGW_UDP_PORT_FOR_S11                  "SGW_UDP_PORT_FOR_S11"

#define SGW_CONFIG_STRING_OVS_BRIDGE_NAME                       "BRIDGE_NAME"
#define SGW_CONFIG_STRING_OVS_EGRESS_PORT_NUM                   "EGRESS_PORT_NUM"
#define SGW_CONFIG_STRING_OVS_INGRESS_PORT_NUM                  "INGRESS_PORT_NUM"
#define SGW_CONFIG_STRING_OVS_L2_EGRESS_PORT                    "L2_EGRESS_PORT"
#define SGW_CONFIG_STRING_OVS_L2_INGRESS_PORT                   "L2_INGRESS_PORT"
#define SGW_CONFIG_STRING_OVS_UPLINK_MAC                        "UPLINK_MAC"
#define SGW_CONFIG_STRING_OVS_UDP_PORT_FOR_S1U                  "UDP_PORT_FOR_S1U"
#define SGW_CONFIG_STRING_OVS_ARP_DAEMON_EGRESS                 "ARP_DAEMON_EGRESS"
#define SGW_CONFIG_STRING_OVS_ARP_DAEMON_INGRESS                "ARP_DAEMON_INGRESS"

#define SPGW_ABORT_ON_ERROR true
#define SPGW_WARN_ON_ERROR false

typedef struct spgw_ovs_config_s {
  int      gtpu_udp_port_num;
  bstring  bridge_name;
  int      egress_port_num;
  bstring  l2_egress_port;
  int      ingress_port_num;
  bstring  l2_ingress_port;
  bstring  uplink_mac; // next (first) hop
  bool     arp_daemon_egress;
  bool     arp_daemon_ingress;
} spgw_ovs_config_t;

typedef struct sgw_config_s {
  /* Reader/writer lock for this configuration */
  pthread_rwlock_t rw_lock;

  struct {
    uint32_t  queue_size;
    bstring   log_file;
  } itti_config;

  struct {
    bstring        if_name_S1u_S12_S4_up;
    struct in_addr S1u_S12_S4_up;
    int            netmask_S1u_S12_S4_up;

    bstring        if_name_S5_S8_up;
    struct in_addr S5_S8_up;
    int            netmask_S5_S8_up;

    bstring        if_name_S11;
    struct in_addr S11;
    int            netmask_S11;
  } ipv4;
  uint16_t     udp_port_S1u_S12_S4_up;
  uint16_t     udp_port_S5_S8_up;
  uint16_t     udp_port_S5_S8_cp;
  uint16_t     udp_port_S11;

  bool         local_to_eNB;
#if (!EMBEDDED_SGW)
  log_config_t log_config;
#endif

  bstring      config_file;

  spgw_ovs_config_t ovs_config;
} sgw_config_t;

void sgw_config_init (sgw_config_t * config_pP);
int sgw_config_process (sgw_config_t * config_pP);
int sgw_config_parse_file (sgw_config_t * config_pP);
void sgw_config_display (sgw_config_t * config_p);

#define sgw_config_read_lock(sGWcONFIG)  do { pthread_rwlock_rdlock(&(sGWcONFIG)->rw_lock);} while(0)
#define sgw_config_write_lock(sGWcONFIG) do { pthread_rwlock_wrlock(&(sGWcONFIG)->rw_lock);} while(0)
#define sgw_config_unlock(sGWcONFIG)     do { pthread_rwlock_unlock(&(sGWcONFIG)->rw_lock);} while(0)

#ifdef __cplusplus
}
#endif

#endif /* FILE_SGW_CONFIG_SEEN */
