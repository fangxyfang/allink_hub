#include "rmci.h"
//#include "Alink_Types.h"
#include "cJSON.h"
#include "debug.h"
#include "uart_api.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

extern int g_serial_fd;
//extern int g_baudrates[4];
int g_baudrates[4] = {0,LORA_BAUDRATE, GTW_BAUDRATE, OTHER_BAUDRATE}; //[0]:shot right baudrate
// static module_region_t s_lora_region[] = {
//     {"868EU",
//      0x0001,
//      0,
//      "LOW",
//      {0x0C, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53,
//      0x53}},
//     {"915NA",
//      0x07FE,
//      6,
//      "HIGH",
//      {0x0A, 0x04, 0x06, 0x08, 0x0a, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16,
//      0x18}},
//     {"China",
//      0x07FF,
//      1,
//      "HIGH",
//      {0x00, 0x01, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12,
//      0x14}},
//     {"Australia",
//      0x07C0,
//      6,
//      "HIGH",
//      {0x0A, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x12, 0x14, 0x16,
//      0x18}},
//     {"Brazil",
//      0x07C2,
//      6,
//      "HIGH",
//      {0x0A, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x10, 0x12, 0x14, 0x16,
//      0x18}},
//     {"India",
//      0x0006,
//      1,
//      "LOW",
//      {0x0C, 0x38, 0x38, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
//      0x40}},
//     {"Russia",
//      0x0002,
//      1,
//      "LOW",
//      {0x0C, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
//      0x5A}},
//     {"Korea",
//      0x07FE,
//      6,
//      "HIGH",
//      {0x0A, 0xD1, 0xD3, 0xD5, 0xD7, 0xD9, 0xDB, 0xDD, 0xDF, 0xE1, 0xE3,
//      0xE5}},
//     {"Japan",
//      0x0780,
//      8,
//      "HIGH",
//      {0x0A, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x16,
//      0x18}},
//     {"ROK",
//      0x0006,
//      1,
//      "LOW",
//      {0x0C, 0x58, 0x58, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59, 0x59,
//      0x59}}};

void rmci_cmd_form(uint8 cmd, uint8 *cmd_payload, int cmd_payload_len,
                   uint8 *cmd_buf, int *p_cmd_buf_len) {
  switch (cmd) {
  case CMD_GET_MODULE_TYPE:
  case CMD_GET_APPTYPE:
  case CMD_GET_FW_VERSION:
  case CMD_GET_NODE_INFO:
  case CMD_GET_APPNODE_INFO:

  case CMD_QUERY_SYS_CTRL_INFO:
  case CMD_QUERY_RECEIVER_STATUS:
  case CMD_SET_MODULE_TYPE_GTW:
  case CMD_GET_ENA_MODULE_TYPE:
  case CMD_GET_CHNMAPPING:
  case CMD_QUERY_REGION:
    cmd_buf[0] = 0x5B;
    cmd_buf[1] = 0x24;
    cmd_buf[2] = cmd;
    cmd_buf[3] = 0x5D;
    *p_cmd_buf_len = 4;
    break;

  case CMD_SET_CMD_BATCH:
  case CMD_SET_CHANNEL:
  case CMD_SET_TX_POWER:
  case CMD_SET_PAN_ID:        // 2
  case CMD_SET_SYS_CTRL_INFO: // 4
  case CMD_SET_REGION:
  case CMD_SET_CHNMAPPING: // 12
    cmd_buf[0] = 0x5B;
    cmd_buf[1] = 0x20 + 4 + cmd_payload_len;
    cmd_buf[2] = cmd;
    memcpy(&cmd_buf[3], cmd_payload, cmd_payload_len);
    cmd_buf[2 + cmd_payload_len + 1] = 0x5D;
    *p_cmd_buf_len = 4 + cmd_payload_len;
    break;
  default:
    break;
  }
}

void rmci_cmd_send(int fd, uint8 cmd, uint8 *cmd_payload, int cmd_payload_len) {
  uint8 cmd_buf[RMCI_CMD_SIZE];
  char str_buf[2 * RMCI_CMD_SIZE + 2] = {0};
  int i = 0, offset = 0,write_len=0;
  int cmd_buf_len = 0;
  rmci_cmd_form(cmd, cmd_payload, cmd_payload_len, cmd_buf, &cmd_buf_len);
  write_len=write(fd, cmd_buf, cmd_buf_len);
  if(write_len<=0){
    debug_info_l5("cmd write err\n");
  }
  for (i = 0; i < cmd_buf_len; i++) {
    offset += snprintf(str_buf + offset, sizeof(str_buf) - offset, "%02x",
                       cmd_buf[i]);
  }
  debug_info_l5("cmd <<<(hex) %s\n", str_buf);
}

int rmci_cmd_rsp(int fd, uint8 cmd, char *p_payload, int *p_out_len,
                 int timeout) {
  uint8 rsp[RMCI_RSP_SIZE];
  int recv_len = 0;
  int rsp_len = 0;
  time_t tsf = time(NULL) + timeout;

  while (time(NULL) < tsf) {
    memset(rsp, 0, RMCI_RSP_SIZE);
    // uart_recv_line(fd, rsp, len);
    // uart_recv_line(fd, rsp, RMCI_BUFSIZE,timeout);
    uart_recv_uint8_line(fd, rsp, RMCI_RSP_SIZE, &recv_len);

    if (rsp[0] == 0x5B) {
      rsp_len = rsp[1] - 0x20;
	  if(rsp_len < 4 || rsp_len > 100)
	  {
	  	debug("err resp_len \n");
	  	return -1 ;
	  }
      if (rsp[2] == cmd + 1) { //&& rsp_len==recv_len){
        if (rsp[rsp_len - 1] == 0x5D) {
          if ((p_payload!=NULL)&&(p_out_len!=NULL)&&((rsp_len - 4) < RMCI_RSP_SIZE)){
            memcpy(p_payload, &rsp[3], rsp_len - 4);
            *p_out_len = rsp_len - 4;
            return 0; // success
          }
        }
      }
    }
  }

  return -1; // no resp;
}

int rmci_cmd_query(int fd, uint8 cmd, uint8 *p_rsp_payload, int *p_out_len) {
  int ret = -1, i = 0;
  while ((ret != 0) && i < 3) {
    rmci_cmd_send(fd, cmd, NULL, 0);
    ret = rmci_cmd_rsp(fd, cmd, p_rsp_payload, p_out_len, 1);
    i++;
  }
  return ret;
}
int rmci_cmd_set(int fd, uint8 cmd, uint8 *p_cmd_payload, int cmd_payload_len,
                 uint8 *p_rsp_payload, int *p_out_len) {
  int ret = -1, i = 0;
  while ((ret != 0) && i < 3) {
    rmci_cmd_send(fd, cmd, p_cmd_payload, cmd_payload_len);
    ret = rmci_cmd_rsp(fd, cmd, p_rsp_payload, p_out_len, 1); // add 0625
    i++;
  }
  return ret;
}

void restart_prog() { exec_syscmd("central_hub_start.sh restart", NULL, 0); }
#if 0
int set_nvram_from_update_json(unsigned char *cfg_str) {
  cJSON *sub_item = NULL, *str_json = NULL;
  module_config_t module_config = {0};
  char set_cmd[128];
  uint8 chg = 0;
  debug_info_l5("%s,%s, cfg_str %s.\n", __FILE__, __FUNCTION__, cfg_str);
  str_json = cJSON_Parse(cfg_str);
  if (str_json != NULL) {
    debug_info_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
    sub_item = cJSON_GetObjectItem(str_json, "channelCfg");
    if (sub_item != NULL) {
      module_config.channel = (uint8)(sub_item->valueint);
      snprintf(set_cmd,sizeof(set_cmd), "/bin/nvram set HN_CHANNEL=%lld", sub_item->valueint);
      debug_info_l5("%s,%s, channelCfg %d.\n", __FILE__, __FUNCTION__,
                    sub_item->valueint);
      debug_info_l5("%s,%s, set_cmd %s.\n", __FILE__, __FUNCTION__, set_cmd);
      //exec_syscmd(set_cmd, NULL, 0);
      chg |= 1;
    }
    sub_item = cJSON_GetObjectItem(str_json, "panidCfg");
    if (sub_item != NULL) {
      module_config.panid = (uint8)sub_item->valueint;
      snprintf(set_cmd,sizeof(set_cmd), "/bin/nvram set HN_PANID=%lld", sub_item->valueint);
      debug_info_l5("%s,%s, channelCfg %d.\n", __FILE__, __FUNCTION__,
                    module_config.channel);
      debug_info_l5("%s,%s, set_cmd %s.\n", __FILE__, __FUNCTION__, set_cmd);
      //exec_syscmd(set_cmd, NULL, 0);
      chg |= 2;
    }
    sub_item = cJSON_GetObjectItem(str_json, "alarmOfflineNumCfg");
    if (sub_item != NULL) {
      module_config.alarmOfflineNumCfg = sub_item->valueint;
      snprintf(set_cmd,sizeof(set_cmd), "/bin/nvram set HN_OFFLINE_NUM=%d",
              module_config.alarmOfflineNumCfg);
      exec_syscmd(set_cmd, NULL, 0);
      chg |= 4;
    }
    sub_item = cJSON_GetObjectItem(str_json, "trigAlarmEnableCfg");
    if (sub_item != NULL) {
      module_config.trigAlarmEnableCfg = sub_item->valueint;
      snprintf(set_cmd,sizeof(set_cmd), "/bin/nvram set HN_TRIG_ALARM=%d",
              module_config.trigAlarmEnableCfg);
      exec_syscmd(set_cmd, NULL, 0);
      chg |= 8;
    }
    sub_item = cJSON_GetObjectItem(str_json, "trigRelayEnableCfg");
    if (sub_item != NULL) {
      module_config.trigRelayEnableCfg = sub_item->valueint;
      snprintf(set_cmd,sizeof(set_cmd), "/bin/nvram set HN_TRIG_RELAY=%d",
              module_config.trigRelayEnableCfg);
      exec_syscmd(set_cmd, NULL, 0);
      chg |= 16;
    }
    sub_item = cJSON_GetObjectItem(str_json, "locationCfg");
    if (sub_item != NULL) {
      strlcpy(module_config.locationCfg, sub_item->valuestring,
              sizeof(module_config.locationCfg));
      snprintf(set_cmd,sizeof(set_cmd), "/bin/nvram set HN_GW_LOCATION=%s",
              module_config.locationCfg);
      exec_syscmd(set_cmd, NULL, 0);
      chg |= 32;
    }
    cJSON_Delete(str_json);
  }
  if (chg) {
    exec_syscmd("/bin/nvram commit", NULL, 0);
  }
  return 0;
}
#endif
void set_nvram_or_fact(uint8 *p_nvram, uint8 *cmd_para,module_config_t *p_module_config) {
  //int ret = 0;
  uint8 cmd_buf[128] = {0}, cmd_result[32] = {0};
  // char *set_hn_chanmapping_cmd_ok =
  //     "/bin/nvram set HN_CHANMAPPING_SET_OK=1";                   //-g 2
  //char *cmd_commit = "/bin/nvram commit";                         //-g 3

  snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s set HN_CHANNEL=%d", p_nvram,
           p_module_config->channel);
  debug_l5("%s cmd_buf %s\n", __FUNCTION__, cmd_buf);
  exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
  snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s set HN_PANID=%d", p_nvram,
           p_module_config->panid);
  debug_l5("%s cmd_buf %s\n", __FUNCTION__, cmd_buf);
  exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
  snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s set HN_REGION_NAME=%s", p_nvram,
           p_module_config->region_name);
  exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
  snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s set HN_TXPOWER=%s", p_nvram,
           p_module_config->power_str);
  exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
  snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s set HN_CHANNEL_MASK=%s", p_nvram,
           p_module_config->channel_mask);
  exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
  snprintf(cmd_buf, sizeof(cmd_buf),
           "/bin/%s set HN_SET_CHAMAPPING_CMD=%s", p_nvram,
           p_module_config->set_chnmapping_cmd_str);
  exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));

  // snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s set NOW_REG_CONF=%s", p_nvram,
  //          cmd_para);
  // exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
  //fxy 
  //=(g_baudrates[1] [2] [3] valid then save g_baudrates[0] 
  if((g_baudrates[0]==g_baudrates[1])||(g_baudrates[0]==g_baudrates[2])||(g_baudrates[0]==g_baudrates[3])){
    snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s set HN_BAUD_RATE=%d", p_nvram,
            g_baudrates[0]);
    exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
  }

  snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s set HN_CHANMAPPING_SET_OK=1", p_nvram);
  exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
  snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s set HN_CHANNEL_SET_OK=1", p_nvram); //fxy 20211223
  exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));  //fxy 20211223
  snprintf(cmd_buf, sizeof(cmd_buf), "/bin/%s commit", p_nvram);
  exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));

  //exec_syscmd(set_hn_chanmapping_cmd_ok, NULL, 0);
  //exec_syscmd(cmd_commit, NULL, 0);
}
void setconfig_ok(uint8 *cmd_function_flag, uint8 *cmd_para,
                  module_config_t *p_module_config) {
  int ret = 0;
  uint8 cmd_buf[128] = {0}, cmd_result[32] = {0};
  char *set_hn_chanmapping_cmd_ok =
      "/bin/nvram set HN_CHANMAPPING_SET_OK=1";                   //-g 2
  char *set_hn_channel_ok = "/bin/nvram set HN_CHANNEL_SET_OK=1"; //-g 3
  char *cmd_commit = "/bin/nvram commit";                         //-g 3
   
  if (!strncmp(cmd_function_flag, CMD_FUNCTION_SET_CHNMAPPING_CMD, 1)) {
    set_nvram_or_fact("nvram", cmd_para,p_module_config);
    set_nvram_or_fact("fact_nvram",cmd_para,p_module_config);

    debug_notice_l5("%s %s\n",__FUNCTION__,cmd_para);
  } else if (!strncmp(cmd_function_flag, CMD_FUNCTION_SET_CHANNEL, 1)) {
    snprintf(cmd_buf, sizeof(cmd_buf), "/bin/nvram set HN_CHANNEL=%d",
             p_module_config->channel);
    exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
    snprintf(cmd_buf, sizeof(cmd_buf), "/bin/nvram set HN_PANID=%d",
             p_module_config->panid);
    exec_syscmd(cmd_buf, cmd_result, sizeof(cmd_result));
    exec_syscmd(set_hn_channel_ok, NULL, 0);
    exec_syscmd(cmd_commit, NULL, 0);
    debug_notice_l5("%s chan %d panid %d\n",__FUNCTION__,p_module_config->channel,p_module_config->panid);
  }
}

int get_baud_rate_config(void) {
  unsigned char para[32];
  int ret = 0;
  char *get_hn_baud_rate_cmd = "/bin/nvram get HN_BAUD_RATE";
  memset(para, 0, sizeof(para));
  get_nvram_para(get_hn_baud_rate_cmd, para, sizeof(para));
  if((strlen(para) > 0)&&(strlen(para) <=8)) {
    ret = atoi(para);
    if((ret==g_baudrates[1])||(ret==g_baudrates[2])||(ret==g_baudrates[3])){
      //valid
    }else{
      ret = 0;
    }
  } 
  debug_info_l5("get HN_BAUD_RATE %d\n", ret);
  return ret;
 

}

int getconfig(uint8 *cmd_function_flag, module_config_t *p_module_config) {
  unsigned char para[32];
  int ret = 0;
  int i = 0;
  char *get_hn_channel_cmd = "/bin/nvram get HN_CHANNEL";
  char *get_hn_power_cmd = "/bin/nvram get HN_TXPOWER";
  char *get_hn_panid_cmd = "/bin/nvram get HN_PANID";
  char *get_hn_region_name_cmd = "/bin/nvram get HN_REGION_NAME";
  char *get_hn_set_chnmapping_cmd = "/bin/nvram get HN_SET_CHAMAPPING_CMD";
  char *get_hn_pre_shared_secret_cmd = "/bin/nvram get HN_PRE_SHARED_SECRET";

  memset(p_module_config, 0, sizeof(module_config_t));
  p_module_config->panid = 0xffff;
  p_module_config->channel = 0xff;
  p_module_config->power = 0xee; // invalid value
  // p_module_config->region_idx = 0xff; // invalid value

  memset(p_module_config->pre_shared_secret, 0,
         sizeof(p_module_config->pre_shared_secret));
  get_nvram_para(get_hn_pre_shared_secret_cmd,
                 p_module_config->pre_shared_secret,
                 sizeof(p_module_config->pre_shared_secret));
  if (strlen(p_module_config->pre_shared_secret) <= 32) {
    // ret = str_to_hex(p_module_config->pre_shared_secret, para);
  } else {
    p_module_config->ng |= GET_CFG_ERR_PSS;
  }

  memset(para, 0, sizeof(para));
  get_nvram_para(get_hn_set_chnmapping_cmd, para, sizeof(para));
  if (strlen(para) == 12 * 2) {
    ret = str_to_hex(p_module_config->set_chnmapping_cmd, para);
    if (ret == -1) {
      p_module_config->ng |= GET_CFG_ERR_SET_CHNMAPPING;
    } else {
      debug_info_l5("get nvram para chnmaping\n");
      alink_printf_string_byhexs(p_module_config->set_chnmapping_cmd, 12);
    }
  } else {
    p_module_config->ng |= GET_CFG_ERR_SET_CHNMAPPING;
  }

  // if ((cmd_function_flag !=
  // NULL)&&!strncmp(cmd_function_flag,CMD_FUNCTION_SET_CHNMAPPING_CMD,1)){
  //   return p_module_config->ng;
  // }

  memset(para, 0, sizeof(para));
  get_nvram_para(get_hn_region_name_cmd, para, sizeof(para));
  if (strlen(para) != 0) {
    memcpy(p_module_config->region_name, para,
           sizeof(p_module_config->region_name));
  } else {
    p_module_config->ng |= GET_CFG_ERR_REGION;
  }

  memset(para, 0, sizeof(para));
  get_nvram_para(get_hn_panid_cmd, para, sizeof(para));
  if (strlen(para) != 0) {
    p_module_config->panid = atoi(para);
  } else {
    p_module_config->ng |= GET_CFG_ERR_PANID;
  }
  debug_info_l5("p_module_config->ng %d\n", p_module_config->ng);

  memset(para, 0, sizeof(para));
  get_nvram_para(get_hn_channel_cmd, para, sizeof(para));
  if (strlen(para) != 0) {
    p_module_config->channel = atoi(para);
  } else {
    p_module_config->ng |= GET_CFG_ERR_CHANNEL;
  }
  // check channel valid, Lora: 0 - 10, Mesh: 0 -26

  // 0xFF for maximum(High), 0x80 for Middle, and 0x00 for minimum(Low)
  memset(para, 0, sizeof(para));
  get_nvram_para(get_hn_power_cmd, para, sizeof(para));
  if (!strncmp(para, TX_POWER_LOW, strlen(TX_POWER_LOW))) {
    p_module_config->power = 0x00;
  } else if (!strncmp(para, TX_POWER_MIDDLE, strlen(TX_POWER_MIDDLE))) {
    p_module_config->power = 0x80;
  } else if (!strncmp(para, TX_POWER_HIGH, strlen(TX_POWER_HIGH))) {
    p_module_config->power = 0xFF;
  } else {
    p_module_config->ng |= GET_CFG_ERR_TX_POWER;
  }

  // memset(para, 0, sizeof(para));
  // get_nvram_para(get_hn_set_region_enable_cmd, para, sizeof(para));
  // if (strlen(para) != 0) {
  //   p_module_config->set_region_enable = atoi(para);
  // } else {
  //   // p_module_config->ng |=4;
  // }

  return p_module_config->ng;
}

int getconfig_from_chg(uint8 *cmd_function_flag, uint8 *chg_para,
                       module_config_t *p_module_config) {
  // uint8 cmd_result[128]={0};
  uint8 module_type[16] = {0}, channel_mask[16] = {0}, region[16] = {0};
  uint8 panid[16] = {0}, channel[8] = {0}, txpower[8] = {0},
        channel_mapping[26] = {0};

  int ret = 0;
  int i = 0;
  if((cmd_function_flag==NULL)||(chg_para==NULL)||(p_module_config==NULL)){
    ret =-1;
    return ret;
  }
  // HN_REG_CONF_INPUT:	LORA,0x000007FE,915NA,6,High,0A,0406080A0C0E1012141618
  // char *get_hn_region_chg_cmd = "/bin/nvram get HN_REG_CONF_INPUT";
  memset(p_module_config, 0, sizeof(module_config_t));
  p_module_config->panid = 0xffff; // invalid value
  p_module_config->channel = 0xff; // invalid value
  p_module_config->power = 0xee;   // invalid value

  // get_nvram_para(get_hn_region_chg_cmd, cmd_result, sizeof(para));
  if (!strncmp(cmd_function_flag, CMD_FUNCTION_SET_CHNMAPPING_CMD, 1)) {
    if (strlen(chg_para) > 0) {
      ret = sscanf(chg_para, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%s",
                   module_type, channel_mask, region, channel, txpower,
                   channel_mapping, channel_mapping + 2);
      if (7 != ret) {
        debug_err_l5("ERR,sscanf return value:%d.\n", ret);
        ret = -1;
        p_module_config->ng |= GET_CFG_ERR_PARA;
      } else {
        debug_info_l5("module_type %s txpower %s channel_mapping %s\n",
                      module_type, txpower, channel_mapping);
        if (strlen(channel_mapping) == 12 * 2) {
          ret =
              str_to_hex(p_module_config->set_chnmapping_cmd, channel_mapping);
          if (ret == -1) {
            p_module_config->ng |= GET_CFG_ERR_SET_CHNMAPPING;
          } else {
            debug_info_l5("chg chnmaping1\n");
            alink_printf_string_byhexs(p_module_config->set_chnmapping_cmd, 12);
            strlcpy(p_module_config->set_chnmapping_cmd_str, channel_mapping,
                    sizeof(p_module_config->set_chnmapping_cmd_str));
          }
          if (!strncmp(region,"2400ISM", 7)) { // 20210927,special for 2400ISM
            debug_info_l5("chg chnmaping2 2400ISM no need set\n");
          } 
        } else {
          p_module_config->ng |= GET_CFG_ERR_SET_CHNMAPPING;
        }
        if (strlen(region) != 0) {
          strlcpy(p_module_config->region_name, region,
                  sizeof(p_module_config->region_name));
        } else {
          p_module_config->ng |= GET_CFG_ERR_REGION;
        }
        if (strlen(channel) != 0) {
          p_module_config->channel = atoi(channel);
        } else {
          p_module_config->ng |= GET_CFG_ERR_CHANNEL;
        }
        // 0xFF for maximum(High), 0x80 for Middle, and 0x00 for minimum(Low)
        if (!strncmp(txpower, TX_POWER_LOW, strlen(TX_POWER_LOW))) {
          p_module_config->power = 0x00;

        } else if (!strncmp(txpower, TX_POWER_MIDDLE,
                            strlen(TX_POWER_MIDDLE))) {
          p_module_config->power = 0x80;
        } else if (!strncmp(txpower, TX_POWER_HIGH, strlen(TX_POWER_HIGH))) {
          p_module_config->power = 0xFF;
        } else {
          p_module_config->ng |= GET_CFG_ERR_TX_POWER;
        }
        strlcpy(p_module_config->power_str, txpower,
                sizeof(p_module_config->power_str));
                
        p_module_config->panid = DEFAULT_PANID_999;
        #if 0        
        if (!strncmp(strtoupper(module_type), "LORA", 4)) {
          p_module_config->panid =
              DEFAULT_PANID_100; // lora 100; mesh 999 //default panid
        } else if (!strncmp(strtoupper(module_type), "MESH", 4)) {
          p_module_config->panid = DEFAULT_PANID_999;
        }
        #endif

        strlcpy(p_module_config->channel_mask, channel_mask,
                sizeof(p_module_config->channel_mask));
      }
    }
  } else if (!strncmp(cmd_function_flag, CMD_FUNCTION_SET_CHANNEL, 1)) {
    if (strlen(chg_para) > 0) {
      ret = sscanf(chg_para, "%[^,],%s", panid, channel);
      if (2 != ret) {
        debug_err_l5("ERR,sscanf return value:%d.\n", ret);
        ret = -1;
      } else {
        debug_info_l5("panid %s channel %s\n",panid, channel);
        if (strlen(channel) != 0) {
          p_module_config->channel = atoi(channel);
        } else {
          p_module_config->ng |= GET_CFG_ERR_CHANNEL;
        }
        if (strlen(panid) != 0) {
          p_module_config->panid = atoi(panid);
        } else {
          p_module_config->ng |= GET_CFG_ERR_PANID;
        }
      }
    }
  }

  // check channel valid, Lora: 0 - 10, Mesh: 0 -26
  return p_module_config->ng;
}

void reset_modem() {
  debug_info_l5("step1 reset modem\n");
  // exec_syscmd("echo 0 > /sys/class/gpio/gpio64/value",NULL,0);
  // usleep(500*1000);
  exec_syscmd("echo 1 > /sys/class/gpio/gpio64/value",NULL,0); // real 0
  usleep(500 * 1000);
  exec_syscmd("echo 0 > /sys/class/gpio/gpio64/value",NULL,0); // real 1
  usleep(3000 * 1000);
}

int init_config_modem(uint8 *cmd_function_flag, uint8 *cmd_para,
                      module_config_t *p_module_config) {

  time_t last_sns_req_tsf = 0;
  int ret = 0, cfg_chg = 0;
  int err_count = 0;
  int i = 0, j = 0;
  int uart_err_count = 0;
  uint8 rsp_payload[RMCI_RSP_SIZE]={0}, cmd_payload[64]={0};
  int out_len = 0;
  int cfg_ret = 0;
  // int fd;
  uint8 query_module_type_cnt = 0;
  module_state_t module;
  memset(&module, 0, sizeof(module_state_t));
  debug_info_l5("module.cfg_chg_cnt %d\n", module.cfg_chg_cnt);
  module.module_lora1_mesh0 = 0xff;
  p_module_config->module_lora1_mesh0 = 0xff;
  module.cfg_chg_cnt = 0;
#if 0 
  test_generate_req_or_rsp_json_msg();
  // for test
  write_csv_file("time,a1,a2,a3,a4\n","test.csv");
  write_csv_file("202108170901,5,6,7,8\n","test.csv");
  write_csv_file("202108170901,5,6,7,8\n","test.csv");
#endif
  debug_info_l5("init_config_modem \n");

  if ((cmd_function_flag != NULL) &&
      !strncmp(cmd_function_flag, CMD_FUNCTION_GET_MODEM_TYPE, 1)) {
    debug_info_l5("no need getconfig \n");
  } else {
    if (cmd_function_flag == NULL) {
      cfg_ret = getconfig(cmd_function_flag, p_module_config);
    } else if (!strncmp(cmd_function_flag, CMD_FUNCTION_SET_CHNMAPPING_CMD,
                        1)) {
      cfg_ret =
          getconfig_from_chg(cmd_function_flag, cmd_para, p_module_config);
    } else if (!strncmp(cmd_function_flag, CMD_FUNCTION_SET_CHANNEL, 1)) {
      cfg_ret =
          getconfig_from_chg(cmd_function_flag, cmd_para, p_module_config);
    }

    if ((cfg_ret & GET_CFG_ERR_CHANNEL) == GET_CFG_ERR_CHANNEL) {
      debug_err_l5("channel not config, exit\n");
      ret = GET_CFG_ERR_CHANNEL_ERR;
    } else if ((cfg_ret & GET_CFG_ERR_TX_POWER) == GET_CFG_ERR_TX_POWER) {
      debug_err_l5("tx power not config, exit\n");
      ret = GET_CFG_ERR_TX_POWER_ERR;
    } else if ((cfg_ret & GET_CFG_ERR_PANID) == GET_CFG_ERR_PANID) {
      debug_err_l5("panid not config, exit\n");
      ret = GET_CFG_ERR_PANID_ERR;
    } else if ((cfg_ret & GET_CFG_ERR_SET_CHNMAPPING) ==
               GET_CFG_ERR_SET_CHNMAPPING) {
      debug_err_l5("chnmapping not config, exit\n");
      ret = GET_CFG_ERR_SET_CHNMAPPING_ERR;
    } else if ((cfg_ret & GET_CFG_ERR_REGION) == GET_CFG_ERR_REGION) {
      debug_err_l5("region not config, exit\n");
      ret = GET_CFG_ERR_REGION_ERR;
    } else if ((cfg_ret & GET_CFG_ERR_PSS) == GET_CFG_ERR_PSS) {
      debug_err_l5("pre set secret err, exit\n");
      ret = GET_CFG_ERR_PSS_ERR;
    } else if ((cfg_ret & GET_CFG_ERR_PARA) == GET_CFG_ERR_PARA) {
      debug_err_l5("centrlhub web page call para err, exit\n");
      ret = GET_CFG_ERR_PARA;
    }

    // } else if ((cfg_ret & GET_CFG_ERR_PSS) == GET_CFG_ERR_PSS) {
    //   debug_info_l5("pre share secret not config, exit\n");
    //   ret = GET_CFG_ERR_PSS_ERR;
    // }

    if (ret != 0) {
      return ret;
    }
  }

  /* init gpio */
  // wakeup pin
  reset_modem();
  // get_config(p_zigbee_status);
  g_baudrates[0]=get_baud_rate_config();

  while (1) {
  start:
    module.module_type_got = 0;
    p_module_config->module_type_got = 0;   
    // module.cfg_chg_cnt = 0;
    while (query_module_type_cnt < 10) {
      for (i = 0; i < 4; i++) {
        if (g_baudrates[i] == 0) {
          continue;
        }
        g_serial_fd = uart_open(ATPORT, g_baudrates[i]);
        if (g_serial_fd == -1) {
          debug_err_l5("open uart error.\n");
          sleep(1);
          reset_modem();
          continue;
        }
        module.baudrate = g_baudrates[i]; // MODULE_GTW;
        debug_info_l5("step2 detect modem (get module type) now baudrate %d\n",
                      module.baudrate);
        ret = rmci_cmd_query(g_serial_fd, CMD_GET_MODULE_TYPE, rsp_payload,
                             &out_len);
        if (ret) {
          debug_info_l5("no resp retry next baudrate.\n");
          if (g_serial_fd >= 0) {
            close(g_serial_fd);
            g_serial_fd = -1;
          }
          continue;
        } else {

          module.module_type = rsp_payload[0];
          debug_info_l5("get module.module_type %d\n", module.module_type);
          if (module.module_type == MODULE_TYPE_MESH_9) {
            module.module_lora1_mesh0 = 0;
            module.module_type_got = 1;
            p_module_config->module_lora1_mesh0 = 0;
            p_module_config->module_type_got = 1;
            exec_syscmd("/bin/nvram set HN_LORA1_MESH0=0", NULL, 0);
            exec_syscmd("/bin/nvram set HN_MODEM_TYPE=RM900A-1KEE", NULL, 0);
            exec_syscmd("/bin/nvram commit", NULL, 0);
          } else if (module.module_type == MODULE_TYPE_MESH_10) {
            module.module_lora1_mesh0 = 0;
            module.module_type_got = 1;
            p_module_config->module_lora1_mesh0 = 0;
            p_module_config->module_type_got = 1;            
            exec_syscmd("/bin/nvram set HN_LORA1_MESH0=0", NULL, 0);
            exec_syscmd("/bin/nvram set HN_MODEM_TYPE=RM2400A", NULL, 0);
            exec_syscmd("/bin/nvram commit", NULL, 0);
          } else if (module.module_type == MODULE_TYPE_LORA_42) {
            module.module_lora1_mesh0 = 1;
            module.module_type_got = 1;
            p_module_config->module_lora1_mesh0 = 1;
            p_module_config->module_type_got = 1;            
            exec_syscmd("/bin/nvram set HN_LORA1_MESH0=1", NULL, 0);
            exec_syscmd("/bin/nvram set HN_MODEM_TYPE=RMLORAB/C", NULL, 0);
            exec_syscmd("/bin/nvram commit", NULL, 0);
          } else { // err module_type
            module.module_type_got = -1;
            p_module_config->module_type_got = -1;            
            // goto err;
          }
          break;
        }
      }
      if (module.module_type_got) {
        break;
      }
      query_module_type_cnt++;
    }
    query_module_type_cnt = 0;
    if (module.module_type_got == 1) {
      if (cmd_function_flag != NULL) {
        if (!strncmp(cmd_function_flag, CMD_FUNCTION_GET_MODEM_TYPE, 1)) {
          debug_info_l5("get modem type then return\n");
          return 0; //!!!
        }
      }
      debug_info_l5("step3 get modem application type (GTW?)\n");
      memset(rsp_payload,0,sizeof(rsp_payload));
      ret = rmci_cmd_query(g_serial_fd, CMD_GET_APPTYPE, rsp_payload, &out_len);
      if (ret) {
        ret = 41;
        goto err;
      } else {

        if (rsp_payload[0] != GTW) {
          if (rsp_payload[0] == RTR) {
            exec_syscmd("/bin/nvram set HN_APP_TYPE=RTR", NULL, 0);
            //exec_syscmd("/bin/nvram commit", NULL, 0);
          }
          if (rsp_payload[0] == STD) {
            exec_syscmd("/bin/nvram set HN_APP_TYPE=STD", NULL, 0);
            //exec_syscmd("/bin/nvram commit", NULL, 0);
          }
          debug_info_l5("application type!=GTW then set GTW\n");
          ret = rmci_cmd_set(g_serial_fd, CMD_SET_MODULE_TYPE_GTW, NULL, 0,
                             rsp_payload, &out_len);
          if (ret) {
            ret = 42;
            goto err;
          } else {
            goto start;
          }
        } else {
          exec_syscmd("/bin/nvram set HN_APP_TYPE=GTW", NULL, 0);
          //exec_syscmd("/bin/nvram commit", NULL, 0);
          debug_info_l5("application type==GTW, step4 get config\n");
          // ret = rmci_cmd_query(g_serial_fd, CMD_GET_FW_VERSION, rsp_payload,
          //                      &out_len);
          if (module.module_type == MODULE_TYPE_MESH_9 ||
              module.module_type == MODULE_TYPE_MESH_10) {
            debug_info_l5("CMD_GET_NODE_INFO\n");    
            ret = rmci_cmd_query(g_serial_fd, CMD_GET_NODE_INFO, rsp_payload,
                                 &out_len);
          } else if (module.module_type == MODULE_TYPE_LORA_42) {
            debug_info_l5("CMD_GET_APPNODE_INFO\n");
            ret = rmci_cmd_query(g_serial_fd, CMD_GET_APPNODE_INFO, rsp_payload,
                                 &out_len);
          }
          if (ret) {
            ret = 43;
            goto err;
          } else {
            memcpy(module.config.uid, rsp_payload, 8);
            memcpy(p_module_config->uid, rsp_payload, 8);
            module.config.channel = rsp_payload[8];
            module.config.power = rsp_payload[9];
            module.config.panid = (rsp_payload[10] << 8) | rsp_payload[11];
          }
          if (module.module_lora1_mesh0 == 1) {
            ret = rmci_cmd_query(g_serial_fd, CMD_QUERY_SYS_CTRL_INFO,
                                 rsp_payload, &out_len);
            if (ret) {
              ret = 44;
              goto err;
            } else {
              module.config.sys_ctrl_info[0].byte = rsp_payload[0];
              module.config.sys_ctrl_info[1].byte = rsp_payload[1];
              module.config.sys_ctrl_info[2].byte = rsp_payload[2];
              module.config.sys_ctrl_info[3].byte = rsp_payload[3];
            }
          }
          // ret = rmci_cmd_query(g_serial_fd, CMD_QUERY_REGION, rsp_payload,
          // &out_len); if (ret) {
          //   goto err;
          // } else {
          //   module.config.region = rsp_payload[0];
          // }
          debug_info_l5("get config channel 0x%02x,power 0x%02x, panid 0x%04x "
                        "sys_ctrl_info byte3(bit0-7) %02x\n",
                        module.config.channel, module.config.power,
                        module.config.panid,
                        module.config.sys_ctrl_info[3].byte);

          cmd_payload[0] = 1;
          debug_info_l5("CMD_SET_CMD_BATCH\n");
          ret = rmci_cmd_set(g_serial_fd, CMD_SET_CMD_BATCH, cmd_payload, 1,
                             rsp_payload, &out_len);
          cfg_chg = 0;
//coverity check
          if(ret!=0){
            debug_info_l5("set no response\n");
          }
          if ((p_module_config->channel != 0xff) &&
              (p_module_config->channel != module.config.channel)) {
            cmd_payload[0] = p_module_config->channel;
            debug_info_l5("chg channel %02x\n", p_module_config->channel);
            ret = rmci_cmd_set(g_serial_fd, CMD_SET_CHANNEL, cmd_payload, 1,
                               rsp_payload, &out_len);
            cfg_chg = CHG_SET_CFG_CHANNEL;
//coverity check
            if(ret!=0){
              debug_info_l5("set no response\n");
            }            
          }

          if ((p_module_config->panid != 0xffff) &&
              (p_module_config->panid != module.config.panid)) {
            // if (p_module_config->sys_ctrl_info.bits.roaming == 1) {
            //   // skip
            // } else {
            // big endian [0] most byte
            cmd_payload[0] = (p_module_config->panid & 0xff00) >> 8;
            cmd_payload[1] = (p_module_config->panid & 0x00ff); //[1] least byte

            debug_info_l5("chg panid %04x \n", p_module_config->panid);
            // reversebytes_uint16()
            ret = rmci_cmd_set(g_serial_fd, CMD_SET_PAN_ID, cmd_payload, 2,
                               rsp_payload, &out_len);
            cfg_chg = CHG_SET_CFG_PANID;
//coverity check
            if(ret!=0){
              debug_info_l5("set no response\n");
            } 
            // }
          }
          if ((cmd_function_flag != NULL) &&
              !strncmp(cmd_function_flag, CMD_FUNCTION_SET_CHANNEL, 1)) {
            // skip pow and chanmapping verfication
          } else if (!strncmp(p_module_config->region_name, "2400ISM",
                              7)) { // 20210927,special for 2400ISM
            // skip chanmapping verfication, but set power
            if ((p_module_config->power != 0xee) &&
                (p_module_config->power != module.config.power)) {
              cmd_payload[0] = p_module_config->power;
              debug_info_l5("chg power %02x\n", p_module_config->power);
              ret = rmci_cmd_set(g_serial_fd, CMD_SET_TX_POWER, cmd_payload, 1,
                                 rsp_payload, &out_len);
              cfg_chg = CHG_SET_CFG_TX_POWER;
//coverity check
              if(ret!=0){
                debug_info_l5("set no response\n");
              }               
            }
          } else {
            if ((p_module_config->power != 0xee) &&
                (p_module_config->power != module.config.power)) {
              cmd_payload[0] = p_module_config->power;
              debug_info_l5("chg power %02x\n", p_module_config->power);
              ret = rmci_cmd_set(g_serial_fd, CMD_SET_TX_POWER, cmd_payload, 1,
                                 rsp_payload, &out_len);
              cfg_chg = CHG_SET_CFG_TX_POWER;
//coverity check
              if(ret!=0){
                debug_info_l5("set no response\n");
              }               
            }
            // super usr set region ,then set channelmapping (power and channel
            // above set)
#if 1
            debug_info_l5("CMD_GET_CHNMAPPING\n"); 
            ret = rmci_cmd_query(g_serial_fd, CMD_GET_CHNMAPPING, rsp_payload,
                                 &out_len);
            if (ret) {
              ret = 45;
              goto err;
            } else {
              debug_info_l5(
                  "CMD_GET_CHNMAPPING %02x %02x %02x %02x %02x %02x\n",
                  rsp_payload[0], rsp_payload[1], rsp_payload[2],
                  rsp_payload[3], rsp_payload[4], rsp_payload[5]);
              debug_info_l5(
                  "CMD_GET_CHNMAPPING %02x %02x %02x %02x %02x %02x\n",
                  rsp_payload[6], rsp_payload[7], rsp_payload[8],
                  rsp_payload[9], rsp_payload[10], rsp_payload[11]);
              for (j = 0; j < 12; j++) {
                if (rsp_payload[j] != p_module_config->set_chnmapping_cmd[j]) {
                  break;
                }
              }
              // not same then set
#if 1
              if (j != 12) {
                memcpy(cmd_payload, p_module_config->set_chnmapping_cmd, 12);
                debug_info_l5("chg CMD_SET_CHNMAPPING \n");
                ret = rmci_cmd_set(g_serial_fd, CMD_SET_CHNMAPPING, cmd_payload,
                                   12, rsp_payload, &out_len);
                cfg_chg = CHG_SET_CFG_SET_CHNMAPPING;
              }
            }
#endif
          }
#endif
          //if (module.module_lora1_mesh0 == 1 &&
          //    module.config.sys_ctrl_info[3].bits.bit7 != 1) {
          if (module.module_lora1_mesh0 == 1 &&
              module.config.sys_ctrl_info[3].bits.bit7 != 0) {            
            module.config.sys_ctrl_info[3].bits.bit7 = 0;
            cmd_payload[0] = module.config.sys_ctrl_info[0].byte;
            cmd_payload[1] = module.config.sys_ctrl_info[1].byte;
            cmd_payload[2] = module.config.sys_ctrl_info[2].byte;
            cmd_payload[3] = module.config.sys_ctrl_info[3].byte;
            debug_info_l5("chg CHG_SET_CFG_SYS_CTRL_INFO \n");
            ret = rmci_cmd_set(g_serial_fd, CMD_SET_SYS_CTRL_INFO, cmd_payload,
                               4, rsp_payload, &out_len);
            cfg_chg = CHG_SET_CFG_SYS_CTRL_INFO;
          }
          usleep(500 * 1000);

          // if (p_module_config->region != module.config.region) {
          //   cmd_payload[0] = p_module_config->region;
          //   ret = rmci_cmd_set(g_serial_fd, CMD_SET_REGION, cmd_payload,
          //   1,rsp_payload, &out_len); cfg_chg++; debug_info_l5("chg region
          //   %02x\n", p_module_config->region);
          // }

          if (cfg_chg) {
            module.cfg_chg_cnt++;
            debug_info_l5("module.cfg_chg_cnt %d\n", module.cfg_chg_cnt);
            if (module.cfg_chg_cnt > 1) {
              if (cfg_chg == CHG_SET_CFG_CHANNEL) {
                ret = CHG_SET_CFG_CHANNEL_ERR;
              } else if (cfg_chg == CHG_SET_CFG_PANID) {
                ret = CHG_SET_CFG_PANID_ERR;
              } else if (cfg_chg == CHG_SET_CFG_TX_POWER) {
                ret = CHG_SET_CFG_TX_POWER_ERR;
              } else if (cfg_chg == CHG_SET_CFG_SET_CHNMAPPING) {
                ret = CHG_SET_CFG_SET_CHNMAPPING_ERR;
              } else if (cfg_chg == CHG_SET_CFG_SYS_CTRL_INFO) {
                ret = CHG_SET_CFG_SYS_CTRL_INFO_ERR;
              }
              debug_err_l5("cfg error code %d!!!.\n", ret);
              goto err;
            }
            debug_info_l5("step1 again\n");
            reset_modem();
            goto start;
          } else {
            g_baudrates[0] = module.baudrate;
            break; // then get datapack
          }
        }
      }
    } else {
      ret = MODULE_TYPE_GOT_ERR; // can't get module type
      // err
    }

  err:
    if (g_serial_fd > 0) {
      close(g_serial_fd);
      g_serial_fd = -1;
    }
    // zigbee_uart_status(ZIGBEE_UART_FAIL);
    if (module.cfg_chg_cnt > 1) {
      return ret;
    }
    break;
    // if (uart_err_count++ > 2) {

    //    uart_err_count = 0;
    //    reset_modem();
    // } else {
    //    sleep((2 * uart_err_count));
    // }
  }
  return ret;
}
