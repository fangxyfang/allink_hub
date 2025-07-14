
/* <DESC>
 *
 * </DESC>
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <sqlite3.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>


//////////////

// #include <uuid/uuid.h>
////////////
#include "cjson/cJSON.h"
#include "debug.h"

#include "curlhttps.h"
#include "dcprotocol.h"
#include "mqtt_async_wrapper.h"
#include "rmci.h"
#include "wsn.h"
#include "sqlite_opertion.h"
#include "hashtable.h"
//#define MAX_TABLE_ROW_NUM 2700 // //fxy 0925 for test  //20

extern mqtt_state_t g_mqtt_state;
extern Mqttsetting g_mqttsetting;
extern gateway_profile_t g_profile;
extern module_config_t g_module_config;
extern hashTable_t g_perm_hashTable;

extern void nvram_get_trigerAlarmRelay(void);
////////////////////////////////////////////////
proto_basicinfo_t g_up_proto_basicinfo = {0};
last_topology_t g_last_topology ={0};
void init_last_topology(void){
    g_last_topology.resp_flag = 1;
    memset(g_last_topology.msgid,0,sizeof(g_last_topology.msgid));
    g_last_topology.send_time_sec = get_utcms()/1000;
}

typedef struct msgstru
{
    long msgtype;
    unsigned char msgtext[256];
}msgstru_s;

int g_ipc_msg_id = -1;
#define IPC_MSG_TYPE 0x44
void *history_delmsgid_thread(void *arg)
{
    msgstru_s msg_recv;
    key_t key;
    int recvlen = 0;
    int msglen = 0;
    int i = 0;

    key = ftok("/tmp",'a');
    if(-1 == key)
    {
		    debug_info_l5("history_delmsgid_thread ftok fail.\n");
        return NULL;
    }
    
    msglen = sizeof(msgstru_s) - sizeof(long);
    g_ipc_msg_id = msgget(key,IPC_CREAT);
    if(g_ipc_msg_id < 0)
    {
        debug_info_l5("history_delmsgid_thread msgget fail.\n");
        return NULL;        
    }
    
    while(1)
    {
        recvlen = msgrcv(g_ipc_msg_id,&msg_recv,msglen,0,0);
        if(-1 == recvlen)
        {
			      debug_info_l5("history_delmsgid_thread msgrcv fail.\n");
            return NULL;
        }
        debug_info_l5("history_delmsgid_thread msg rcv %s.\n",msg_recv.msgtext);
        deletedb_offline_data_by_msgid(msg_recv.msgtext,ACTION_UP_NUM_OFFLINEDATA);
        usleep(200*1000);//per 800ms send mcu message
        
    }
        
    return NULL;
}


int history_del_msgid_send(char *history_data_msgid,uint8 action_type){ 
	msgstru_s msgs;
	int msglen = 0;
  int ret = -1;

  msgs.msgtype = IPC_MSG_TYPE;
  memset(msgs.msgtext,0,sizeof(msgs.msgtext));
  memcpy(msgs.msgtext,history_data_msgid,strlen(history_data_msgid));
  msglen =sizeof(msgstru_s) - sizeof(long);

  ret = msgsnd(g_ipc_msg_id,&msgs,msglen,0);
  if(-1 == ret)
  {
    debug_info_l5("history_del_msgid_send msgsnd fail.\n");
    return -1;			  
  }
  return 0;
}
////////////////////////////////////////////////
unsigned char *construct(proto_basicinfo_t *self, unsigned char *body_str,
                         int body_size, int *p_proto_packge_size) {

  int i = 0, cur_size = 0;

  unsigned char message_id_size =
      (unsigned char)strlen(self->message_id); // fxy 0929
  // unsigned char msg_type_size =
  //     (unsigned char)strlen(self->msg_type); // max 255
  unsigned char client_id_size = (unsigned char)strlen(self->client_id);
  unsigned short int reserve_head_size =
      (unsigned short int)strlen(self->reserve_head);
  // unsigned char prifile_id[]= PROFILE_ID;
  unsigned char msg_profile_id_size = strlen(self->profile_id);
  unsigned char firmware_version_size =
      (unsigned char)strlen(self->firmware_version);
  int head_size = 2 + 2 + 1 + 4 + 1 + message_id_size + 1 + client_id_size + 8 +
                  1 + firmware_version_size + 2 + 2 + 4 + 1 + 2 + 1 +
                  msg_profile_id_size;
  // head_size = 2 + 2 + 1 + 4 + 1 + len(message_id) + 1 + len(msg_type) + 1 +
  //                  len(self.__client_id) + 8 + 1 +
  //                  len(self.__firmware_version) + 2 + 2 + 4

  // char *body_str = cJSON_PrintUnformatted(body);
  // body_bytes = bytearray(body_str.encode("utf-8"))  //tbd!!!!!
  // int body_size = strlen(body_str);
  *p_proto_packge_size = head_size + body_size; // may great than real size
  unsigned char *proto_packge = NULL;

  debug_info_l5("message_id:%s\n", self->message_id);
  debug_info_l5("client_id:%s\n", self->client_id);
  //debug_info_l5("firmware_version:%s\n", self->firmware_version);
  //debug_info_l5("reserve_head:%s\n", self->reserve_head);

  proto_packge = (unsigned char *)malloc(*p_proto_packge_size);
  if (proto_packge == NULL) {
    return NULL;
  }
  debug_info_l5("*p_proto_packge_size=%d\n", *p_proto_packge_size);
  // device->cloud
  *proto_packge = 'D';
  *(proto_packge + 1) = 'C';

  *(proto_packge + 2) = 0;
  *(proto_packge + 3) = 1; // protcol verstion 2Bytes
  if (self->byte_order == 0) {
    *(proto_packge + 4) = 0;
  } else {
    *(proto_packge + 4) = 1;
  }
  if (self->req0_or_resp1 == 0) {
    *(proto_packge + 4) &= 0x75;
  } else {
    *(proto_packge + 4) |= 0x02; // bit2: 0: Reqeust ，1: Response
  }
  cur_size = 5;
  memcpy(proto_packge + cur_size, &head_size, 4);
  cur_size += 4;

  memcpy(proto_packge + cur_size, &message_id_size, 1);
  cur_size += 1;
  memcpy(proto_packge + cur_size, self->message_id, message_id_size);
  cur_size += message_id_size;

  memcpy(proto_packge + cur_size, &msg_profile_id_size, 1);
  cur_size += 1;
  memcpy(proto_packge + cur_size, self->profile_id, msg_profile_id_size);
  cur_size += msg_profile_id_size;

  memcpy(proto_packge + cur_size, &client_id_size, 1);
  // *(proto_packge + cur_size) = client_id_size;
  cur_size += 1; // fxy

  memcpy(proto_packge + cur_size, self->client_id, client_id_size);
  cur_size += client_id_size;

  memcpy(proto_packge + cur_size, &self->send_time, 8);
  cur_size += 8;

  memcpy(proto_packge + cur_size, &firmware_version_size, 1);
  cur_size += 1;
  memcpy(proto_packge + cur_size, self->firmware_version,
         firmware_version_size);
  cur_size += firmware_version_size;

  memcpy(proto_packge + cur_size, &self->body_type,
         2); // 2B Body格式，0: JSON，  1:
  // Binary
  // *(proto_packge + cur_size) = 0;
  // *(proto_packge + cur_size + 1) = 0;
  cur_size += 2;

  if (self->req0_or_resp1 == 1) {

    memcpy(proto_packge + cur_size, &self->error_code, 2);
    cur_size += 2;
  }

  memcpy(proto_packge + cur_size, &reserve_head_size, 2);
  cur_size += 2;
  // debug_info_l5("reserve_head_size=%d\n", reserve_head_size);
  if (reserve_head_size != 0) {
    memcpy(proto_packge + cur_size, self->reserve_head, reserve_head_size);
    cur_size += reserve_head_size;
  }

  if (self->body_type == 1) { // binary body
    body_size += 2;           //
  }
  memcpy(proto_packge + cur_size, &body_size, 4);
  cur_size += 4;

  if (self->body_type == 1) { // binary body
    *(proto_packge + cur_size) = self->compress_flag;
    cur_size += 1;
    *(proto_packge + cur_size) = self->body_msg_type;
    cur_size += 1;
  }
  //!!!!!
  head_size = cur_size - 4;
  debug_info_l5("head_size=%d\n", head_size);

  memcpy(proto_packge + 5, &head_size, 4);
  //!!!!!
  if (self->body_type == 1) { // binary body
    body_size -= 2;           //
  }
  debug_info_l5("body_size=%d\n", body_size);
  memcpy(proto_packge + cur_size, body_str, body_size);
  cur_size += body_size;

  //debug_info_l5("package out body_str :%s\n", body_str);

  //debug_info_l5("total package size=%d\n", cur_size);
  // debug_info_l5("package out :\n");
  // alink_printf_string(proto_packge, cur_size);

  *p_proto_packge_size = cur_size;
  debug_info_l5("*p_proto_packge_size=%d\n", *p_proto_packge_size);
  return proto_packge;
}

void config_response_head(proto_basicinfo_t *p_proto_basicinfo,
                          int error_code) {
  // memset(p_proto_basicinfo,0,sizeof(proto_basicinfo_t));
  p_proto_basicinfo->byte_order = 0;
  p_proto_basicinfo->req0_or_resp1 = 1;
  p_proto_basicinfo->error_code = error_code;
  p_proto_basicinfo->body_type = 0;
  strlcpy(p_proto_basicinfo->profile_id, PROFILE_ID,
          sizeof(p_proto_basicinfo->profile_id));
  strlcpy(p_proto_basicinfo->client_id, g_mqttsetting.clientId,
          sizeof(p_proto_basicinfo->client_id));
  // memset(p_proto_basicinfo->reserve_head, 0,
  //        sizeof(p_proto_basicinfo->reserve_head));
}
// set gloab var
void config_request_head(proto_basicinfo_t *p_proto_basicinfo) { // fxy 0824
  // memset(p_proto_basicinfo,0,sizeof(proto_basicinfo_t));
  p_proto_basicinfo->byte_order = 0;
  p_proto_basicinfo->req0_or_resp1 = 0;
  p_proto_basicinfo->error_code = 0;
  p_proto_basicinfo->body_type = 0;
  strlcpy(p_proto_basicinfo->profile_id, PROFILE_ID,
          sizeof(p_proto_basicinfo->profile_id));
  strlcpy(p_proto_basicinfo->client_id, g_mqttsetting.clientId,
          sizeof(p_proto_basicinfo->client_id));
  // memset(p_proto_basicinfo->reserve_head, 0,
  //        sizeof(p_proto_basicinfo->reserve_head));
}
//pack and save db
unsigned char *pack_msg(dc_payload_info_t *p_dc_payload,
                        proto_basicinfo_t *p_proto_basicinfo,
                        unsigned char *body_str, int body_size,
                        int *p_proto_packge_size) {

  int dbfile_size = 0;
  int error_code = 0;
  int record_num =0;
  unsigned char *msg = NULL, *p_data_json_str = NULL;
  offline_buffer_query_t query = {0};
  if (body_str != NULL) {
    if (!strncmp(p_dc_payload->action, MSG_ACTION_UP_TOPOLOGY,
                 strlen(MSG_ACTION_UP_TOPOLOGY))) {
        strlcpy(g_last_topology.msgid,p_proto_basicinfo->message_id,sizeof(g_last_topology.msgid));
        g_last_topology.send_time_sec =p_proto_basicinfo->send_time/1000;
        msg =construct(p_proto_basicinfo, body_str, body_size, p_proto_packge_size);
        return msg;
    }
#if 1
    if (!strncmp(p_dc_payload->action, MSG_ACTION_UP_REALDATA,
                 strlen(MSG_ACTION_UP_REALDATA))) {
      query.action_type = ACTION_UP_NUM_REALDATA;
    } else if (!strncmp(p_dc_payload->action, MSG_ACTION_UP_DEV_CFG,
                        strlen(MSG_ACTION_UP_DEV_CFG))) {
      query.action_type = ACTION_UP_NUM_DEV_CFG;
      debug_notice_l5("MSG_ACTION_UP_DEV_CFG send msgid %s\n",p_proto_basicinfo->message_id);
    } else if (!strncmp(p_dc_payload->action, MSG_ACTION_UP_OFFLINEDATA,
                        strlen(MSG_ACTION_UP_OFFLINEDATA))) {
      query.action_type = ACTION_UP_NUM_OFFLINEDATA;
    } else if (!strncmp(p_dc_payload->action, MSG_ACTION_UP_SENDCMD,
                        strlen(MSG_ACTION_UP_SENDCMD))) {
      query.action_type = ACTION_UP_NUM_SENDCMD;
      debug_notice_l5("MSG_ACTION_UP_SENDCMD send msgid %s\n",p_proto_basicinfo->message_id);
    }else if (!strncmp(p_dc_payload->action, MSG_ACTION_UP_GW_CFG,
                        strlen(MSG_ACTION_UP_GW_CFG))) {
      query.action_type = ACTION_UP_NUM_GATEWAY_CFG;
      debug_notice_l5("MSG_ACTION_UP_GW_CFG send msgid %s\n",p_proto_basicinfo->message_id);      
    }else if (!strncmp(p_dc_payload->action, MSG_ACTION_UP_CHKPERMISSONDATA,
                        strlen(MSG_ACTION_UP_CHKPERMISSONDATA))) {
      query.action_type = ACTION_UP_NUM_CHKPERMISSONDATA;
      debug_notice_l5("MSG_ACTION_UP_CHKPERMISSONDATA send msgid %s\n",p_proto_basicinfo->message_id);      
    }
    //debug_info_l5("%s(%d) p_dc_payload->retry %d\n", __FUNCTION__, __LINE__,p_dc_payload->retry);
    //!!! request save to db ,response and retry no need
    if ((query.action_type)&&(p_dc_payload->retry==0)&&(!strncmp(p_dc_payload->flags,"request",strlen("request")))) {
      if (p_dc_payload->data_json != NULL) {
        p_data_json_str = cJSON_PrintUnformatted(p_dc_payload->data_json);
        // debug_info_l5("pack_msg body_json %s\n", body_str);

        // utc,msgid,value,cnt,sample_time,dev_serinum,dev_configv,action_type
        query.utc = p_proto_basicinfo->send_time;
        query.msgid = p_proto_basicinfo->message_id;
        query.buffer = p_data_json_str;
        query.cnt = 1; 
        query.sample_time = p_proto_basicinfo->send_time;
        query.dev_serinum = p_dc_payload->dev_serinum;
        query.dev_configv = p_dc_payload->dev_configv;
        query.length_buffer = strlen(p_data_json_str);
        query.id = 0;
        unsigned char db_file[DB_FILE_LEN]={0};
        int dbfile_size_max=0, dbfile_cnt_max=0;
        set_db_file_tmp_or_sd(query.action_type,db_file); //set db_file 20211118
        dbfile_size = get_file_size(db_file);
        set_db_file_size_max(query.action_type,&dbfile_size_max,&dbfile_cnt_max); //20211106
        if (dbfile_size < dbfile_size_max) {
          //these three type keep one for specific serinum or gateway
          if ((query.action_type == ACTION_UP_NUM_DEV_CFG)||             \
          (query.action_type == ACTION_UP_NUM_CHKPERMISSONDATA)||        \
          (query.action_type == ACTION_UP_NUM_GATEWAY_CFG) ) {

             if(query.action_type == ACTION_UP_NUM_GATEWAY_CFG){
                error_code =querydb_real_data_with_action_serinum(&query,0);
             }else{
                error_code =querydb_real_data_with_action_serinum(&query,1);
             }
            if (query.id == 0) { // no same serialnum devcfg exit
              error_code = insertdb_offline_data(&query);
              debug_info_l5("insertdb_offline_data1 error_code %d\n",
                       error_code);                
            }else{
              if(query.action_type == ACTION_UP_NUM_GATEWAY_CFG){
                 //remove all gateway_cfg
                  error_code =deletedb_offline_data_by_action_type(query.action_type);
              }else{
                error_code = deletedb_offline_data_by_id(query.id,query.action_type);
              }
              error_code = insertdb_offline_data(&query);
              debug_info_l5("insertdb_offline_data2 error_code %d\n",
                       error_code);                
            }
          } else {
            error_code = insertdb_offline_data(&query);
            debug_info_l5("insertdb_offline_data error_code %d\n",
                       error_code);              
          }
          //debug_info_l5("savedata_msg_to_database error_code %d\n", error_code);
        } else {
          
          record_num = get_table_record_num(db_file, "OFFLINE_DATA");
          debug_info_l5("dbfile_size >=DB_FILE_MAX_SIZE record_num %d dbfile_cnt_max %d\n",record_num,dbfile_cnt_max);
          query.id = 0;
          error_code = querydb_offline_data(&query, 0);
          if (query.id != 0 ) {
            //first remove one record;
            if(record_num>=dbfile_cnt_max){
              deletedb_offline_data_by_id(query.id,query.action_type);
            }else{
              //not remove
            }
          }else{
            
          }
          if ((query.action_type == ACTION_UP_NUM_DEV_CFG)||             \
          (query.action_type == ACTION_UP_NUM_CHKPERMISSONDATA)||        \
          (query.action_type == ACTION_UP_NUM_GATEWAY_CFG) ) {
            query.id = 0;
            if(query.action_type == ACTION_UP_NUM_GATEWAY_CFG){
                error_code =querydb_real_data_with_action_serinum(&query,0);
            }else{
                error_code =querydb_real_data_with_action_serinum(&query,1);
            }
            if ((error_code==0)&&(query.id == 0)) { // no same devcfg exit
              error_code = insertdb_offline_data(&query);
              debug_info_l5("insertdb_offline_data error_code %d\n",
                      error_code);                  
            }else {
              if (query.action_type == ACTION_UP_NUM_GATEWAY_CFG)
              {
                //remove all gateway_cfg
                  error_code =deletedb_offline_data_by_action_type(query.action_type);
              }
              else
              {
                error_code = deletedb_offline_data_by_id(query.id, query.action_type);
              }
              error_code = insertdb_offline_data(&query);
              debug_info_l5("insertdb_offline_data error_code %d\n",
                      error_code);                 
            }
          }else {
            error_code = insertdb_offline_data(&query);
            debug_info_l5("insertdb_offline_data error_code %d\n",
                      error_code);              
          }
          
        }
        // if(error_code!=0){
        //     debug_info_err_l5("insertdb_offline_data error_code %d\n",
        //                error_code);            
        // }  
        alink_free(p_data_json_str);
      }

    }
#endif
//!!!
#if 0
    if (strlen(p_proto_basicinfo->client_id) == 0 || !g_mqtt_state.mqttsigned) {
      debug_info_l5("p_proto_basicinfo->client_id %s\n",
                    p_proto_basicinfo->client_id);
      alink_free((void *)body_str); //!!!
      return NULL;
    }
#endif

    // unsigned long long send_time_in_head = get_utcms();
    msg =
        construct(p_proto_basicinfo, body_str, body_size, p_proto_packge_size);
  }
  return msg;
}
unsigned char *pack_offline_data_from_database(unsigned long long sample_time,
                                               unsigned char *message_id,
                                               unsigned char *body_str,
                                               int body_size,
                                               int *p_proto_packge_size) {

  proto_basicinfo_t updata_proto_basicinfo;
  unsigned char *msg = NULL;
  // char *msg_type = MSG_TYPE_UPLOAD;
  memcpy(&updata_proto_basicinfo, &g_up_proto_basicinfo,
         sizeof(updata_proto_basicinfo)); // get fix part
  updata_proto_basicinfo.send_time =
      sample_time; // get_utcms(); //fxy,2020-12-03
  // uuid_generate_string(updata_proto_basicinfo.message_id); //fxy,2020-12-03
  // fxy,2020-12-03
  strlcpy(updata_proto_basicinfo.message_id, message_id,
          sizeof(updata_proto_basicinfo.message_id));
  //
  // strlcpy(updata_proto_basicinfo.msg_type, msg_type,
  //         sizeof(updata_proto_basicinfo.msg_type));

  msg = construct(&updata_proto_basicinfo, body_str, body_size,
                  p_proto_packge_size);

  return msg;
}
// unsigned char *pack_response_msg(proto_basicinfo_t *p_proto_basicinfo,
//                                  cJSON *body, int *p_proto_packge_size) {

//   int proto_packge_size = 0, body_size = 0;
//   unsigned char *msg = NULL;
//   char *body_str = NULL;
//   if (body != NULL) {
//     body_str = cJSON_PrintUnformatted(body);
//     body_size = strlen(body_str);
//     cJSON_Delete(body);

//     msg =
//         construct(p_proto_basicinfo, body_str, body_size,
//         p_proto_packge_size);
//     alink_free(body_str);
//   }

//   return msg;
// }

// unsigned char *filldata_msg_sensorctlcmd_resp(char *msg_type, cJSON *body,
//                                               int error_code,
//                                               int *p_proto_packge_size) {
//   // !!! fxy 0918
//   unsigned char *msg_resp = NULL;
//   proto_basicinfo_t up_resp_proto_basicinfo = {0};

//   if (msg_type == NULL || body == NULL || p_proto_packge_size == NULL) {
//     debug_info_l5("msg_type == NULL || body == NULL\n");
//     return NULL;
//   }

//   config_response_head(&up_resp_proto_basicinfo, error_code);
//   // strlcpy(up_resp_proto_basicinfo.msg_type, msg_type,
//   //         sizeof(up_resp_proto_basicinfo.msg_type));
//   up_resp_proto_basicinfo.send_time = get_utcms();
//   uuid_generate_string(up_resp_proto_basicinfo.message_id);

//   msg_resp =
//       pack_response_msg(&up_resp_proto_basicinfo, body, p_proto_packge_size);

//   return (msg_resp);
// }
// test for long long number set to json
// void test_generate_req_or_rsp_json_msg()   {
//   cJSON *body = NULL,*root;
//   cJSON *sub = NULL;
//   cJSON *subsub = NULL;
//   cJSON *item = NULL;
//   cJSON *item_array = NULL;
//   char *body_str;
//   char *root_str;
//   proto_basicinfo_t proto_header_info = {0},*p_proto_basicinfo;
//   p_proto_basicinfo= &proto_header_info;
//   // memcpy(&proto_header_info, &g_up_proto_basicinfo,
//   // sizeof(proto_header_info));
//   p_proto_basicinfo->send_time = get_utcms();

//   body = cJSON_CreateObject();
//   item = cJSON_CreateString("aaa");
//   cJSON_AddItemToObject(body, "messageType", item);
//   item_array = cJSON_CreateArray();
//   cJSON_AddItemToArray(item_array, cJSON_CreateString("request"));
//   cJSON_AddItemToObject(body, "flags", item_array);
//   item = cJSON_CreateString("message_id");
//   cJSON_AddItemToObject(body, "messageId", item);

//   sub = cJSON_CreateObject();
//   item = cJSON_CreateString(PROFILE_ID);
//   cJSON_AddItemToObject(sub, "profileId", item);
//   // item = cJSON_CreateString(send_time);
//   item = cJSON_CreateNumber(922337203685477580);
//   cJSON_AddItemToObject(sub, "sndTime", item);
//   item = cJSON_CreateNumber(p_proto_basicinfo->send_time);
//   cJSON_AddItemToObject(sub, "sndTime1", item);
//   item = cJSON_CreateString("action");
//   cJSON_AddItemToObject(sub, "action", item);

//   cJSON_AddItemToObject(sub, "data", cJSON_CreateObject());

//   cJSON_AddItemToObject(body, "data", sub);
//   if (body != NULL) {
//       body_str = cJSON_PrintUnformatted(body);
//       debug_info_l5("pack_msg send_time %lld\n",
//       p_proto_basicinfo->send_time); debug_info_l5("pack_msg body_json %s\n",
//       body_str); root = cJSON_Parse(body_str); root_str =
//       cJSON_PrintUnformatted(root); debug_info_l5("pack_msg root_json %s\n",
//       root_str);
//   }
// }

///////////////////////sign//////////////////////////
cJSON *generate_req_or_rsp_json_msg(dc_payload_info_t *p_jsheader,
                                    proto_basicinfo_t *p_proto_basicinfo) {

  cJSON *body = NULL;
  cJSON *sub = NULL;
  cJSON *subsub = NULL;
  cJSON *item = NULL;
  cJSON *item_array = NULL;
  // proto_basicinfo_t proto_header_info = {0};
  char send_time[32] = {0};
  // memcpy(&proto_header_info, &g_up_proto_basicinfo,
  // sizeof(proto_header_info)); proto_header_info.send_time = get_utcms();
  snprintf(send_time, sizeof(send_time),"%lld", p_proto_basicinfo->send_time);

  body = cJSON_CreateObject();
  item = cJSON_CreateString(p_jsheader->messageType);
  cJSON_AddItemToObject(body, "messageType", item);
  item_array = cJSON_CreateArray();
  if(item_array==NULL){
    return NULL;
  }
  cJSON_AddItemToArray(item_array, cJSON_CreateString(p_jsheader->flags));
  cJSON_AddItemToObject(body, "flags", item_array);
  item = cJSON_CreateString(p_proto_basicinfo->message_id);
  cJSON_AddItemToObject(body, "messageId", item);
  if (!strncmp(p_jsheader->flags, "response", strlen("response"))) {
    if (!strncmp(p_jsheader->errorEnum, "0", 1)) {
      cJSON_AddItemToObject(body, "success", cJSON_CreateTrue());
    } else {
      cJSON_AddItemToObject(body, "success", cJSON_CreateFalse());
      item = cJSON_CreateString(p_jsheader->errorEnum);
      cJSON_AddItemToObject(body, "errorEnum", item);
      item = cJSON_CreateString(p_jsheader->errorMsg);
      cJSON_AddItemToObject(body, "errorMsg", item);
    }
  }
  sub = cJSON_CreateObject();
  item = cJSON_CreateString(PROFILE_ID);
  cJSON_AddItemToObject(sub, "profileId", item);
  // item = cJSON_CreateString(send_time);
  item = cJSON_CreateNumber(p_proto_basicinfo->send_time);
  cJSON_AddItemToObject(sub, "sndTime", item);
  item = cJSON_CreateString(p_jsheader->action);
  cJSON_AddItemToObject(sub, "action", item);
  item = cJSON_CreateString(p_jsheader->agentVerison);
  cJSON_AddItemToObject(sub, "agentVersion", item);

  if (p_jsheader->data_json == NULL) {
    cJSON_AddItemToObject(sub, "data", cJSON_CreateObject());
  } else {
    cJSON_AddItemToObject(sub, "data", p_jsheader->data_json);
  }

  cJSON_AddItemToObject(body, "data", sub);

  return (body);
}

///////////////////////sign//////////////////////////
unsigned char *filldata_msg_sign(int *p_proto_packge_size) {
  char sign_in_data[CLIENT_ID_LEN + 16] = {0}, sign_out[2048] = {0};
  unsigned int sign_out_len = 0;
  char *body_str = NULL;
  int body_size = 0;
  //////////////////////sign///////////////////////////////////
  // long long send_time= 1596525227491;
  cJSON *body = NULL;
  cJSON *sub = NULL;
  cJSON *subsub = NULL;
  cJSON *item = NULL;
  cJSON *item_array = NULL;
  proto_basicinfo_t proto_basicinfo = {0};
  char *msg_type = SIGN;
  unsigned char *sign_msg = NULL;
  memcpy(&proto_basicinfo, &g_up_proto_basicinfo, sizeof(proto_basicinfo));
  uuid_generate_string(proto_basicinfo.message_id);
  proto_basicinfo.send_time = get_utcms();
  char send_time[32] = {0};
  snprintf(send_time,sizeof(send_time), "%lld", proto_basicinfo.send_time);

  strlcpy(sign_in_data, g_mqttsetting.clientId, sizeof(sign_in_data));
  strlcat(sign_in_data, PROFILE_ID, sizeof(sign_in_data));
  strlcat(sign_in_data, send_time, sizeof(sign_in_data));
  debug_info_l5("sign before: %s\n", sign_in_data);
  // rsa_sha1_private_sign(sign_in_data, g_mqttsetting.encryptKey, sign_out,
  //                       &sign_out_len);
  rsa_sha384_private_sign(sign_in_data, g_mqttsetting.encryptKey, sign_out,
                        &sign_out_len);

  body = cJSON_CreateObject();
  if(body==NULL){
    return NULL;
  }
  item = cJSON_CreateString("upload");
  cJSON_AddItemToObject(body, "messageType", item);
  item_array = cJSON_CreateArray();
  if(item_array!=NULL){
    cJSON_AddItemToArray(item_array, cJSON_CreateString("request"));
    cJSON_AddItemToObject(body, "flags", item_array);
  }
  item = cJSON_CreateString(proto_basicinfo.message_id);
  cJSON_AddItemToObject(body, "messageId", item);

  sub = cJSON_CreateObject();
  item = cJSON_CreateString(PROFILE_ID);
  cJSON_AddItemToObject(sub, "profileId", item);
  item = cJSON_CreateString(send_time);
  cJSON_AddItemToObject(sub, "sndTime", item);
  item = cJSON_CreateString("signV2");  //sign->signV2 (sha384)
  cJSON_AddItemToObject(sub, "action", item);
  subsub = cJSON_CreateObject();
  item = cJSON_CreateString(sign_out);
  cJSON_AddItemToObject(subsub, "sign", item);
  cJSON_AddItemToObject(sub, "data", subsub);

  cJSON_AddItemToObject(body, "data", sub);

  if (body != NULL) {
    body_str = cJSON_PrintUnformatted(body);
    body_size = strlen(body_str);
    cJSON_Delete(body);
    body=NULL;
    sign_msg =
        construct(&proto_basicinfo, body_str, body_size, p_proto_packge_size);
    alink_free(body_str);
  }

  return (sign_msg);
}

/////////////////////////end sign mesg//////////////////////////

// int *p_proto_packge_size : out para
unsigned char *pack_msg_upload(dc_payload_info_t *p_dc_payload,
                               proto_basicinfo_t *p_proto_basicinfo,
                               int *p_proto_packge_size) {
  cJSON *body = NULL;
  unsigned char *snd_msg = NULL, *body_str = NULL;
  int body_size = 0;

  if (!strncmp(p_dc_payload->action, MSG_ACTION_UP_DEV_DATALOG,
               strlen(MSG_ACTION_UP_DEV_DATALOG)) &&
      !strncmp(p_dc_payload->flags, "response", strlen("response"))) {
    p_proto_basicinfo->body_type = 1;
    p_proto_basicinfo->compress_flag = 1;
    p_proto_basicinfo->body_msg_type = 1;

    snd_msg =
        pack_msg(p_dc_payload, p_proto_basicinfo, p_dc_payload->binary_body,
                 p_dc_payload->binary_body_size, p_proto_packge_size);

  } else if (!strncmp(p_dc_payload->action, MSG_ACTION_UP_DEV_EVENT,
                      strlen(MSG_ACTION_UP_DEV_EVENT)) &&
             !strncmp(p_dc_payload->flags, "response", strlen("response"))) {
    p_proto_basicinfo->body_type = 1;
    p_proto_basicinfo->compress_flag = 1;
    p_proto_basicinfo->body_msg_type = 2;
    snd_msg =
        pack_msg(p_dc_payload, p_proto_basicinfo, p_dc_payload->binary_body,
                 p_dc_payload->binary_body_size, p_proto_packge_size);
  } else {
    //debug_info_l5("%s(%d) p_dc_payload->retry %d\n", __FUNCTION__, __LINE__,p_dc_payload->retry);
    body = generate_req_or_rsp_json_msg(p_dc_payload, p_proto_basicinfo);

    if (body != NULL) {
      body_str = cJSON_PrintUnformatted(body);
      debug_info_l5("pack_msg body_json %s\n", body_str);
      body_size = strlen(body_str);
      snd_msg = pack_msg(p_dc_payload, p_proto_basicinfo, body_str, body_size,
                         p_proto_packge_size);
      alink_free((void *)body_str); //!!!
      body_str=NULL;
      cJSON_Delete(body); //imply delete data_json
      body=NULL;
    }
  }

  return (snd_msg);
}

unsigned short int reversebytes_uint16(unsigned short int value) {
  return ((value & 0x00FFU) << 8 | (value & 0xFF00U) >> 8);
}

unsigned int reversebytes_uint32(unsigned int value) {
  return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 |
         (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24;
}
// volatile
dn_payload_galarm_buffer_t g_dn_payload_galarm_buff = {0};
dn_payload_cmd_buffer_t g_dn_payload_cmd_buff = {0};
dn_permission_buffer_t g_dn_perm_buff ={0};
volatile dn_upgrade_firmware_t g_dn_upgrade={0};
static dn_upgrade_firmware_t s_dn_upgrade={0};
pthread_mutex_t upgrade_firmware_mutex;
void *get_upgrade_file_thread(void * para)
{
  int ret=0;
  uint8 need_to_done = 0, need_to_curl = 0,need_to_download=1;
  uint8 download_cnt =0, download_ok = 0;
  
  uint8 major_str[4] = {0};
  uint8 minor_str[4] = {0};
  uint8 revision_str[4] = {0};
  uint16 major = 0;
  uint16 minor = 0;
  uint16 revision = 0;
  uint8 ver_str[16];
  uint8 url[512]={0},dn_file_name[64]={0},upgrade_file_name[64]={0};
  uint8 rm_except_file_cmd[160]={0},rm_file_cmd[96]={0};
  uint8 cp_file_to_tmp_cmd[128]={0};
  uint8 sha256sum_cmd[64]={0},sha256sum_cmd_ret[64+1]={0},upgrade_cmd[96]={0};
  //test
  #if 0
  sleep(20);
  g_dn_upgrade.processing = 1;
  g_dn_upgrade.major=1;
  g_dn_upgrade.minor=1;
  g_dn_upgrade.revision=1;

  strlcpy(g_dn_upgrade.firmware_url,"https://ssdm-qa.honcloud.honeywell.com.cn/centergateway/Honey_FW_01_000.bin",sizeof(g_dn_upgrade.firmware_url));
  strlcpy(g_dn_upgrade.package_name ,"Honey_FW_01_000.bin",sizeof(g_dn_upgrade.package_name));
  strlcpy(g_dn_upgrade.check_sum,"8c810dd993f43aebedb0dab8c1d9b5f24dc998280f720a43b0c8c8484e25b476",sizeof(g_dn_upgrade.check_sum));
  //strlcpy(g_dn_upgrade.package_name ,"Honey_FW_01_000.bin",sizeof(g_dn_upgrade.package_name));
  //end test
  #endif
  while (1) {
    //debug_info_l5("get_upgrade_file_thread loop\n");
    pthread_mutex_lock(&upgrade_firmware_mutex);
    
    if (g_dn_upgrade.processing) {
      debug_info_l5("get_upgrade_file_thread process\n");
      g_dn_upgrade.processing = 0;
      need_to_done = 0;
      // if ((g_dn_upgrade.major != s_dn_upgrade.major) ||
      //     (g_dn_upgrade.minor != s_dn_upgrade.minor)) {
      memcpy(&s_dn_upgrade, &g_dn_upgrade, sizeof(s_dn_upgrade));
      need_to_done = 1;
      //} // else omit repeated same version cmd

      pthread_mutex_unlock(&upgrade_firmware_mutex);
      if (need_to_done) {
        // get external version
        memset(ver_str,0,sizeof(ver_str));
        memset(major_str,0,sizeof(major_str));
        memset(minor_str,0,sizeof(minor_str));
        memset(revision_str,0,sizeof(revision_str));
        char *get_hn_gw_ver_cmd =
            "cat /etc/fw_version"; //"/bin/nvram get HN_GW_FIRMWARE_VER";
        get_nvram_para(get_hn_gw_ver_cmd, ver_str, sizeof(ver_str));
        ret = sscanf(ver_str, "%[^.].%[^.].%s", major_str, minor_str,
                     revision_str);

        need_to_curl = 0;
        if (ret == 3) {
          major = atoi(major_str);
          minor = atoi(minor_str);
          revision = atoi(revision_str);
          debug_info_l5("firmware version format ok\n");
          if (s_dn_upgrade.major != major) {
            need_to_curl = 1;
          } else {
            if (s_dn_upgrade.minor != minor) {
              need_to_curl = 1;
            } else {
              if (s_dn_upgrade.revision != revision) {
                need_to_curl = 1;
              }
            }
          }
        } else {
          debug_err_l5("firmware version format err\n");
        }
        if (!need_to_curl){
          debug_notice_l5("firmware version is same no need curl\n");
        }

        if (need_to_curl) {
          //"/tmp/hch300_01_000.bin"
          memset(url,0,sizeof(url));
          memset(dn_file_name,0,sizeof(dn_file_name));
          memset(upgrade_file_name,0,sizeof(upgrade_file_name));
          memset(upgrade_cmd,0,sizeof(upgrade_cmd));
          memset(rm_except_file_cmd,0,sizeof(rm_except_file_cmd));
          memset(rm_file_cmd,0,sizeof(rm_file_cmd));
          memset(cp_file_to_tmp_cmd,0,sizeof(cp_file_to_tmp_cmd));

          strlcpy(url, s_dn_upgrade.firmware_url, sizeof(url));
          // strlcat(url,"/",sizeof(url));
          // strlcat(url,s_dn_upgrade.package_name,sizeof(url));
          strlcpy(dn_file_name, "/var/local/", sizeof(dn_file_name));
          strlcat(dn_file_name, s_dn_upgrade.package_name, sizeof(dn_file_name));
          debug_info_l5("dn_file_name %s\n", dn_file_name);

          strlcpy(upgrade_file_name, "/tmp/", sizeof(upgrade_file_name));
          strlcat(upgrade_file_name, s_dn_upgrade.package_name, sizeof(upgrade_file_name));

          snprintf(upgrade_cmd,sizeof(upgrade_cmd),"/bin/sysupgrade.sh -f %s  1>/dev/null 2>&1&",upgrade_file_name);
          //strlcat(upgrade_cmd, upgrade_file_name, sizeof(upgrade_cmd));
          debug_info_l5("upgrade_cmd %s\n", upgrade_cmd);
          //del all except filename in /var/local
          //find /home/root/ ! -name a.txt  -exec rm -f {} \;
          snprintf(rm_except_file_cmd,sizeof(rm_except_file_cmd),"find /var/local/ ! -name %s -exec rm -f {} \\;",dn_file_name);
          snprintf(rm_file_cmd,sizeof(rm_file_cmd),"rm -f %s;sync",dn_file_name); ////mod for coverity check
          exec_syscmd(rm_except_file_cmd,NULL,0);
          //end

          snprintf(cp_file_to_tmp_cmd,sizeof(cp_file_to_tmp_cmd),"sync; cp %s %s",dn_file_name,upgrade_file_name);
          
          need_to_download = 1;
          if (access(dn_file_name, R_OK) != -1) {  //has file
            memset(sha256sum_cmd,0,sizeof(sha256sum_cmd));
            memset(sha256sum_cmd_ret,0,sizeof(sha256sum_cmd_ret));
            snprintf(sha256sum_cmd, sizeof(sha256sum_cmd), "sha256sum %s",
                     dn_file_name);
            exec_syscmd(sha256sum_cmd, sha256sum_cmd_ret,
                        sizeof(sha256sum_cmd_ret));
            debug_info_l5("sha256sum_cmd_ret:     %s\n",sha256sum_cmd_ret);
            debug_info_l5("s_dn_upgrade.check_sum:%s\n",s_dn_upgrade.check_sum);                        
            if (!strncmp(sha256sum_cmd_ret, s_dn_upgrade.check_sum,
                         64)) {
              //check sum ok              
              need_to_download = 0;
            }else{
              //checksum not ok then rm filename
              debug_info_l5("start rm_file_cmd:%s\n",rm_file_cmd);
              exec_syscmd(rm_file_cmd,NULL,0);
            }
          }else{
            debug_info_l5("dn_file_name:%s not exist\n",dn_file_name);
          }
          // if access read this file ,hash
          if (need_to_download) {
            download_cnt = 0;
            download_ok = 0;
            do {
              download_cnt++;
              debug_notice_l5("http_get_file start url %s\n",url);
              exec_syscmd("echo 2 > /tmp/lora_cloud_conn_status",NULL,0);
              exec_syscmd("sync;echo 1 > /proc/sys/vm/drop_caches",NULL,0);
              http_get_file(url, dn_file_name);
              if(g_mqtt_state.mqttsigned){
                exec_syscmd("echo 1 > /tmp/lora_cloud_conn_status",NULL,0);
              }else{
                exec_syscmd("echo 0 > /tmp/lora_cloud_conn_status",NULL,0);
              }
              if (access(dn_file_name, R_OK) != -1) {
                memset(sha256sum_cmd,0,sizeof(sha256sum_cmd));
                memset(sha256sum_cmd_ret,0,sizeof(sha256sum_cmd_ret));                
                snprintf(sha256sum_cmd, sizeof(sha256sum_cmd), "sha256sum %s",
                         dn_file_name);
                exec_syscmd(sha256sum_cmd, sha256sum_cmd_ret,
                            sizeof(sha256sum_cmd_ret));
                debug_info_l5("sha256sum_cmd_ret:     %s\n",sha256sum_cmd_ret);
                debug_info_l5("s_dn_upgrade.check_sum:%s\n",s_dn_upgrade.check_sum);            
                if (!strncmp(sha256sum_cmd_ret, s_dn_upgrade.check_sum,
                             64)) {
                  download_ok = 1;
                  debug_info_l5("download_ok check_sum ok\n");
                  break;
                }else{
                  download_ok = 0;
                  exec_syscmd(rm_file_cmd,NULL,0);
                  debug_info_l5("download_ok check_sum ng\n");
                }
              }
            } while ((!download_ok) && (download_cnt < 3));
            if (download_ok) {
              // upgrade
 
              exec_syscmd("rm -rf /var/log/",NULL,0);
              exec_syscmd("rm -rf /tmp/datalog",NULL,0);
              exec_syscmd("rm -f /tmp/dtu_real.db db",NULL,0);
              exec_syscmd("rm -f /tmp/dtu_history.db",NULL,0);
              exec_syscmd("rm -f /tmp/dtu_tmp_serinum.db",NULL,0);
              exec_syscmd("sync",NULL,0);
              debug_info_l5("cp_file_to_tmp_cmd %s\n",cp_file_to_tmp_cmd);
              exec_syscmd(cp_file_to_tmp_cmd,NULL,0);
              debug_info_l5("download_ok then start %s\n",upgrade_cmd);
              exec_syscmd(upgrade_cmd,NULL,0);
              debug_notice_l5("end upgrade firmware\n");
            }
          } else { // no need_to_download
            // upgrade
            exec_syscmd("rm -rf /var/log/",NULL,0);
            exec_syscmd("rm -rf /tmp/datalog",NULL,0);
            exec_syscmd("rm -f /tmp/dtu_real.db db",NULL,0);
            exec_syscmd("rm -f /tmp/dtu_history.db",NULL,0);
            exec_syscmd("rm -f /tmp/dtu_tmp_serinum.db",NULL,0);
            exec_syscmd("sync",NULL,0);            
            debug_info_l5("cp_file_to_tmp_cmd %s\n",cp_file_to_tmp_cmd);
            exec_syscmd(cp_file_to_tmp_cmd,NULL,0);
            
            debug_info_l5("no need download then start %s\n",upgrade_cmd);
            exec_syscmd(upgrade_cmd,NULL,0);
            debug_notice_l5("end upgrade firmware\n");
          }
        }
      }
    }else{
      pthread_mutex_unlock(&upgrade_firmware_mutex);
    }
    sleep(20);
  }
  return NULL;
}
int process_dn_package(proto_basicinfo_t *self,
                       dc_payload_info_t *p_dc_payload_info, cJSON *root,
                       uint8 *p_need_restart_prog) {

  // debug_info_l5("dn body_str:%s\n",body_str);

  cJSON *item = NULL, *sub_item = NULL, *json_array_item = NULL,
        *payload_item = NULL, *str_json = NULL;//*rootdata_item = NULL;
  // volatile
  dn_payload_t *p_dn_payload = NULL;
  dn_permission_t *p_dn_perm =NULL;
  char *profileId = NULL, *sndTime = NULL,*p_payload_item_str = NULL;
  // use for unpack
  int errcode = 0, j = 0,dc_resp_errcode=0;
  int cjson_type = 0;
  proto_basicinfo_t proto_basicinfo = {0};
  dc_payload_info_t dc_payload_info = {0};

  item = cJSON_GetObjectItem(root, "messageType");
  
  if (item != NULL) {
    strlcpy(p_dc_payload_info->messageType, item->valuestring,
            sizeof(p_dc_payload_info->messageType));
  }
  //debug_info_l5("%s,%d.\n", __FILE__, __LINE__);
  item = cJSON_GetObjectItem(root, "messageId");
  if (item != NULL) {
    // messageId = item->valuestring;
    strlcpy(p_dc_payload_info->messageId, item->valuestring,
            sizeof(p_dc_payload_info->messageId));
  }
  debug_info_l5("%s,%d messageId %s.\n", __FILE__, __LINE__,p_dc_payload_info->messageId);
  item = cJSON_GetObjectItem(root, "flags");
  if (item != NULL) {
    // request = item->valueint;
    sub_item = cJSON_GetArrayItem(item, 0);
    if (sub_item != NULL) {
      strlcpy(p_dc_payload_info->flags, sub_item->valuestring,
              sizeof(p_dc_payload_info->flags));
    }
  }
  if (self->req0_or_resp1 == 1) {
    item = cJSON_GetObjectItem(root, "success");
    if (item != NULL) {
      cjson_type = item->type;
      //debug_info_l5("cjson_type %d\n", cjson_type);
      if (cjson_type == cJSON_False) {
        p_dc_payload_info->success = 0;
        //debug_info_l5("%s %s %d.\n", __FILE__, __FUNCTION__, __LINE__);
      } else if (cjson_type == cJSON_True) {
        p_dc_payload_info->success = 1;
        //debug_info_l5("%s %s %d.\n", __FILE__, __FUNCTION__, __LINE__);
      }
    }
    item = cJSON_GetObjectItem(root, "errorEnum");
    if (item != NULL) {
      errcode = atoi(item->valuestring);
      strlcpy(p_dc_payload_info->errorEnum, item->valuestring,
              sizeof(p_dc_payload_info->errorEnum));
    }
  }
  //debug_info_l5("%s,%d.\n", __FILE__, __LINE__);
  item = cJSON_GetObjectItem(root, "data");
  
  if (item != NULL) {
    sub_item = cJSON_GetObjectItem(item, "profileId");
    if (sub_item != NULL) {
      profileId = sub_item->valuestring;
    }
    sub_item = cJSON_GetObjectItem(item, "sndTime");
    if (sub_item != NULL) {
      sndTime = sub_item->valuestring;
    }
    sub_item = cJSON_GetObjectItem(item, "action");
    if (sub_item != NULL) {
      strlcpy(p_dc_payload_info->action, sub_item->valuestring,
              sizeof(p_dc_payload_info->action));
    }

    payload_item = cJSON_GetObjectItem(item, "data");
    //add for notice 20211127
    if(!strncmp(p_dc_payload_info->action, MSG_ACTION_UP_REALDATA,
                   strlen(MSG_ACTION_UP_REALDATA))){

    }else if(!strncmp(p_dc_payload_info->action, MSG_ACTION_UP_OFFLINEDATA,
                   strlen(MSG_ACTION_UP_OFFLINEDATA))){
                    
                     
    }else if(!strncmp(p_dc_payload_info->action, MSG_ACTION_UP_TOPOLOGY,
                   strlen(MSG_ACTION_UP_TOPOLOGY))){
         if(!strncmp(p_dc_payload_info->messageId, g_last_topology.msgid,
                   strlen(g_last_topology.msgid))){
             debug_info_l5("%s resp\n", p_dc_payload_info->messageId);       
             g_last_topology.resp_flag = 1; 
         }
    }
    else{
      if (payload_item != NULL)
      {
        p_payload_item_str = cJSON_PrintUnformatted(payload_item);
        if (p_payload_item_str != NULL)
        {
          debug_notice_l5("dn payload_item %s action %s\n", p_payload_item_str, p_dc_payload_info->action);
          alink_free(p_payload_item_str);
        }
        else
        {
          debug_notice_l5("dn payload_item cant't PrintUnformatted action %s\n", p_dc_payload_info->action);
        }
      }
      else
      {
        debug_notice_l5("dn payload_item empty action %s\n", p_dc_payload_info->action);
      }
    }
  }
  debug_info_l5("dn action:%s\n", p_dc_payload_info->action);
  debug_info_l5("dn success:%d\n", p_dc_payload_info->success);
  if (self->req0_or_resp1 == 0) { // cloud to gateway request
    debug_notice_l5("dn request action: %s\n",p_dc_payload_info->action);
    if (!strncmp(p_dc_payload_info->messageType, MSG_TYPE_COMMAND,
                 strlen(MSG_TYPE_COMMAND))) {
      if (!strncmp(p_dc_payload_info->action, MSG_ACTION_GROUPALARM,
                   strlen(MSG_ACTION_GROUPALARM))) {
        // 1 send resp
#if 0        
        memset(&dc_payload_info, 0, sizeof(dc_payload_info));
        memset(&proto_basicinfo, 0, sizeof(proto_basicinfo));
        //debug_info_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
        strlcpy(dc_payload_info.messageType, MSG_TYPE_COMMAND,
                sizeof(dc_payload_info.messageType));
        strlcpy(dc_payload_info.flags, "response",
                sizeof(dc_payload_info.flags));
        strlcpy(dc_payload_info.action, p_dc_payload_info->action,
                sizeof(dc_payload_info.action));
        strlcpy(dc_payload_info.agentVerison, "1.0",
                sizeof(dc_payload_info.agentVerison));
        strlcpy(dc_payload_info.messageId, p_dc_payload_info->messageId,
                sizeof(dc_payload_info.messageId));
        strlcpy(dc_payload_info.errorEnum, "0",
                sizeof(dc_payload_info.errorEnum));
        dc_payload_info.data_json = NULL;
        debug_info_l5("%s,%d.\n", __FUNCTION__, __LINE__);
        mqtt_wrapper_send_uploadData(&dc_payload_info, &proto_basicinfo,QOS0);
#endif        
        // 2 save data to buffer
        if(payload_item!=NULL){
          //debug_info_l5("%s,%d.\n", __FUNCTION__, __LINE__);
          if (g_dn_payload_galarm_buff.left_size < DN_PAYLOAD_NUM) {
            //debug_info_l5("%s,%d.\n", __FUNCTION__, __LINE__);
            g_dn_payload_galarm_buff.dn_payload_array[g_dn_payload_galarm_buff.recv_no].dn_payload_json =cJSON_Duplicate(payload_item,1);
            strlcpy(g_dn_payload_galarm_buff.dn_payload_array[g_dn_payload_galarm_buff.recv_no].messageId, p_dc_payload_info->messageId,sizeof(g_dn_payload_galarm_buff.dn_payload_array[g_dn_payload_galarm_buff.recv_no].messageId));
            g_dn_payload_galarm_buff.recv_no =
              check_circle_end(g_dn_payload_galarm_buff.recv_no);
            g_dn_payload_galarm_buff.left_size++;
            debug_info_l5("%s,%d g_dn_payload_galarm_buff.left_size %d\n", __FUNCTION__, __LINE__,g_dn_payload_galarm_buff.left_size);
          }else{
            debug_err_l5("group alarm command overflow\n");
          }
        }
        //////////////////////////////////
      }
      else if (!strncmp(p_dc_payload_info->action, MSG_ACTION_DN_SENDCMD,
                   strlen(MSG_ACTION_DN_SENDCMD))) {
        // 1 send resp
        memset(&dc_payload_info, 0, sizeof(dc_payload_info));
        memset(&proto_basicinfo, 0, sizeof(proto_basicinfo));
        //debug_info_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
        strlcpy(dc_payload_info.messageType, MSG_TYPE_COMMAND,
                sizeof(dc_payload_info.messageType));
        strlcpy(dc_payload_info.flags, "response",
                sizeof(dc_payload_info.flags));
        strlcpy(dc_payload_info.action, p_dc_payload_info->action,
                sizeof(dc_payload_info.action));
        strlcpy(dc_payload_info.agentVerison, "1.0",
                sizeof(dc_payload_info.agentVerison));
        strlcpy(dc_payload_info.messageId, p_dc_payload_info->messageId,
                sizeof(dc_payload_info.messageId));
        strlcpy(dc_payload_info.errorEnum, "0",
                sizeof(dc_payload_info.errorEnum));
        dc_payload_info.data_json = NULL;
        //debug_info_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
        mqtt_wrapper_send_uploadData(&dc_payload_info, &proto_basicinfo,QOS0);
        // 2 save data to buffer
        if (g_dn_payload_cmd_buff.left_size < DN_PAYLOAD_NUM) {
          p_dn_payload =
              &g_dn_payload_cmd_buff.dn_payload_array[g_dn_payload_cmd_buff.recv_no];

          sub_item = cJSON_GetObjectItem(payload_item, "commandId");
          if (sub_item != NULL) {
            strlcpy(p_dn_payload->command_id, sub_item->valuestring,
                    sizeof(p_dn_payload->command_id));
            debug_info_l5("p_dn_payload->command_id %s\n", p_dn_payload->command_id);
          }
          sub_item = cJSON_GetObjectItem(payload_item, "commandType");
          if (sub_item != NULL) {
            strlcpy(p_dn_payload->command_type, sub_item->valuestring,
                    sizeof(p_dn_payload->command_type));
            debug_info_l5("p_dn_payload->command_type %s\n",
                  p_dn_payload->command_type);
          }
          sub_item = cJSON_GetObjectItem(payload_item, "serialNo");
          if (sub_item != NULL) {

            do {
              json_array_item = cJSON_GetArrayItem(sub_item, j);
              if (json_array_item != NULL) {
                strlcpy(p_dn_payload->serial_no[j],
                        json_array_item->valuestring,
                        sizeof(p_dn_payload->serial_no[j]));
                debug_info_l5("p_dn_payload->serial_no[%d] %s\n", j,
                      p_dn_payload->serial_no[j]);
                j++;
                p_dn_payload->dev_num = j;
              }
            } while (json_array_item != NULL && j < DN_PAYLOAD_SERIALNO_NUM);
          }
          sub_item = cJSON_GetObjectItem(payload_item, "data");
          if (sub_item != NULL) {
            p_dn_payload->data_len = strlen(sub_item->valuestring);
            strlcpy(p_dn_payload->data, sub_item->valuestring,
                    sizeof(p_dn_payload->data));
            debug_info_l5("p_dn_payload->data %s\n", p_dn_payload->data);
          } else {
            p_dn_payload->data_len = 0;
          }
          g_dn_payload_cmd_buff.recv_no =
              check_circle_end(g_dn_payload_cmd_buff.recv_no);
          g_dn_payload_cmd_buff.left_size++;
        }else{
          debug_err_l5("command overflow\n");
        }
        //////////////////////////////////
      }else if (!strncmp(p_dc_payload_info->action, MSG_ACTION_DN_UPGRADE_GW,strlen(MSG_ACTION_DN_UPGRADE_GW))) {
        // 2 save data to buffer

        sub_item = cJSON_GetObjectItem(payload_item, "firmwareItem");
        if (sub_item != NULL) {
          cJSON *sub1_item[4] = {NULL};
          cJSON *sub2_item[6] = {NULL};
             
          sub1_item[0] = cJSON_GetObjectItem(sub_item, "packageName");
          sub1_item[1] = cJSON_GetObjectItem(sub_item, "firmwareUrl");
          sub1_item[2] = cJSON_GetObjectItem(sub_item, "checkSum");
          sub1_item[3] = cJSON_GetObjectItem(sub_item, "firmwareVersion");
          pthread_mutex_lock(&upgrade_firmware_mutex); 
          memset(&g_dn_upgrade,0,sizeof(g_dn_upgrade));  
          g_dn_upgrade.processing =1;       
          if(sub1_item[0]!=NULL){
            strlcpy(g_dn_upgrade.package_name, sub1_item[0]->valuestring,
                  sizeof(g_dn_upgrade.package_name));
            debug_info_l5("g_dn_upgrade.package_name %s\n", g_dn_upgrade.package_name);                  
          }else{
            dc_resp_errcode = 2;
            g_dn_upgrade.processing =0;
          }
          if(sub1_item[1]!=NULL){
            strlcpy(g_dn_upgrade.firmware_url, sub1_item[1]->valuestring,
                  sizeof(g_dn_upgrade.firmware_url));
            debug_info_l5("g_dn_upgrade.firmware_url %s\n", g_dn_upgrade.firmware_url);
          }else{
            dc_resp_errcode = 3;
            g_dn_upgrade.processing =0;
          }
          if(sub1_item[2]!=NULL){
            strlcpy(g_dn_upgrade.check_sum, sub1_item[2]->valuestring,
                  sizeof(g_dn_upgrade.check_sum));
            debug_info_l5("g_dn_upgrade.check_sum %s\n", g_dn_upgrade.check_sum);      
          }else{
            dc_resp_errcode = 4;
            g_dn_upgrade.processing =0;
          }   
          if(sub1_item[3]!=NULL){
            sub2_item[0] = cJSON_GetObjectItem(sub1_item[3], "major");
            sub2_item[1] = cJSON_GetObjectItem(sub1_item[3], "minor");
            sub2_item[2] = cJSON_GetObjectItem(sub1_item[3], "revision");
            sub2_item[3] = cJSON_GetObjectItem(sub1_item[3], "revisionString");           
            sub2_item[4] = cJSON_GetObjectItem(sub1_item[3], "buildNumber");            
            sub2_item[5] = cJSON_GetObjectItem(sub1_item[3], "formattedVersion");             
            if(sub2_item[0]!=NULL){
              g_dn_upgrade.major = sub2_item[0]->valueint;
            }else{
              dc_resp_errcode = 5;
              g_dn_upgrade.processing =0;
            }
            if(sub2_item[1]!=NULL){
              g_dn_upgrade.minor = sub2_item[1]->valueint;
            }else{
              dc_resp_errcode = 6;
              g_dn_upgrade.processing =0;
            }            
            if(sub2_item[2]!=NULL){
              g_dn_upgrade.revision = sub2_item[2]->valueint;
            }else{
              dc_resp_errcode = 7;
              g_dn_upgrade.processing =0;
            }    
            if(sub2_item[4]!=NULL){
              g_dn_upgrade.build_number = sub2_item[4]->valueint;
            }
            if (sub2_item[3] != NULL) {
              strlcpy(g_dn_upgrade.revision_string, sub2_item[3]->valuestring,
                      sizeof(g_dn_upgrade.revision_string));
            } 
            if (sub2_item[5] != NULL) {
              strlcpy(g_dn_upgrade.formatted_version, sub2_item[5]->valuestring,
                      sizeof(g_dn_upgrade.formatted_version));
            }             

          }else{
            dc_resp_errcode = 4;
            g_dn_upgrade.processing =0;
          } 
          pthread_mutex_unlock(&upgrade_firmware_mutex);                 
          debug_info_l5("g_dn_upgrade.package_name %s\n", g_dn_upgrade.package_name);
        } else {
          pthread_mutex_lock(&upgrade_firmware_mutex);//add for coverity check
          dc_resp_errcode = 1;
          g_dn_upgrade.processing =0;
          pthread_mutex_unlock(&upgrade_firmware_mutex); //add for coverity check
        }
                             
        // 1 send resp
        memset(&dc_payload_info, 0, sizeof(dc_payload_info));
        memset(&proto_basicinfo, 0, sizeof(proto_basicinfo));
        //debug_info_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
        strlcpy(dc_payload_info.messageType, MSG_TYPE_COMMAND,
                sizeof(dc_payload_info.messageType));
        strlcpy(dc_payload_info.flags, "response",
                sizeof(dc_payload_info.flags));
        strlcpy(dc_payload_info.action, p_dc_payload_info->action,
                sizeof(dc_payload_info.action));
        strlcpy(dc_payload_info.agentVerison, "1.0",
                sizeof(dc_payload_info.agentVerison));
        strlcpy(dc_payload_info.messageId, p_dc_payload_info->messageId,
                sizeof(dc_payload_info.messageId));
        sprintf(dc_payload_info.errorEnum,"%2d",dc_resp_errcode);

        dc_payload_info.data_json = NULL;
        //debug_info_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
        mqtt_wrapper_send_uploadData(&dc_payload_info, &proto_basicinfo,QOS0);


      }
 
      else if (!strncmp(
                     p_dc_payload_info->action, MSG_ACTION_DN_UPDATE_GW_CFG,
                     strlen(MSG_ACTION_DN_UPDATE_GW_CFG))) { // update gw cfg
        sub_item = cJSON_GetObjectItem(payload_item, "configJson");
        cJSON *str_json = NULL;
        module_config_t module_config = {0};
        //gateway_profile_t profile={0};
        char set_cmd[128]={0}, str_para_err[4]={0}, cmd_output[16]={0};
        char *get_channel_mask_cmd = "/bin/nvram get HN_CHANNEL_MASK";
        uint16 chg = 0, para_err = 0;
        uint32 channel_mask = 0, current_channel_mask = 0;
        int8 hstoi_ret=0,need_send_cfg =0;
        strlcpy(dc_payload_info.errorEnum, "1",
                sizeof(dc_payload_info.errorEnum));
        if (sub_item != NULL) {
          debug_notice_l5("recv updateGatewayConfig configJson %s\n", sub_item->valuestring);
          str_json = cJSON_Parse(sub_item->valuestring);
          if (str_json != NULL) {
            //debug_info_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
            //get_gateway_cfg_profile(&profile);
            get_gateway_profile(&g_profile);
            sub_item = cJSON_GetObjectItem(str_json, "channelCfg");
            if (sub_item != NULL) {
              module_config.channel = (uint8)(sub_item->valueint);
              exec_syscmd(get_channel_mask_cmd, cmd_output, sizeof(cmd_output));
              channel_mask = hstoi(cmd_output,&hstoi_ret);
              current_channel_mask = (1 << module_config.channel);
              debug_info_l5("%s,%s, channel %x current_channel_mask 0x%x.\n", __FILE__,
                    __FUNCTION__, module_config.channel, current_channel_mask);
              if ((channel_mask & current_channel_mask) ==
                  current_channel_mask) {
                // valid channel
                if (g_profile.modem_info.channel != module_config.channel)
                {
                  snprintf(set_cmd, sizeof(set_cmd), "/bin/nvram set HN_CHANNEL=%d",
                           module_config.channel);
                  debug_info_l5("%s,%s, set_cmd %s.\n", __FILE__, __FUNCTION__, set_cmd);
                  exec_syscmd(set_cmd, NULL, 0);
                  g_profile.modem_info.channel = module_config.channel;
                  chg |= CHANNEL_CHG;
                }
              } else {
                para_err |= CHANNEL_CHG;
                debug_err_l5("channel para_err\n");
              }
            }else{
              debug_err_l5("can't get channelCfg\n");
            }
            sub_item = cJSON_GetObjectItem(str_json, "panidCfg");
            if (sub_item != NULL) {
              module_config.panid = (uint16)sub_item->valueint;
              if ((module_config.panid <= 999) && (para_err == 0)) {
                if (g_profile.modem_info.panid != module_config.panid)
                {
                  snprintf(set_cmd, sizeof(set_cmd), "/bin/nvram set HN_PANID=%d",
                           module_config.panid);
                  debug_info_l5("%s,%s, set_cmd %s.\n", __FILE__, __FUNCTION__,
                                set_cmd);
                  exec_syscmd(set_cmd, NULL, 0);
                  g_profile.modem_info.panid = module_config.panid;
                  chg |= PANID_CHG;
                }
              } else {
                  para_err |= PANID_CHG;
                  debug_err_l5("panid  para_err\n");
              }
            }else{
              debug_err_l5("can't get panidCfg\n");
            }

              sub_item = cJSON_GetObjectItem(str_json, "trigAlarmEnableCfg");
              if (sub_item != NULL) {
                module_config.trigAlarmEnableCfg = (uint8)sub_item->valueint;
               //if (g_profile.modem_info.trig_alarm != module_config.trigAlarmEnableCfg)
                {
                  snprintf(set_cmd, sizeof(set_cmd), "/bin/nvram set HN_TRIG_ALARM=%d",
                           module_config.trigAlarmEnableCfg);
                  exec_syscmd(set_cmd, NULL, 0);
                  g_module_config.trigAlarmEnableCfg = module_config.trigAlarmEnableCfg;
                  g_profile.modem_info.trig_alarm = module_config.trigAlarmEnableCfg;
                  chg |= ALARM_ENABLE_CHG;
                }
              }
              sub_item = cJSON_GetObjectItem(str_json, "trigRelayEnableCfg");
              if (sub_item != NULL) {
                module_config.trigRelayEnableCfg = (uint8)sub_item->valueint;
                //if (g_profile.modem_info.trig_relay != module_config.trigRelayEnableCfg)
                {
                  snprintf(set_cmd, sizeof(set_cmd), "/bin/nvram set HN_TRIG_RELAY=%d",
                           module_config.trigRelayEnableCfg);
                  exec_syscmd(set_cmd, NULL, 0);
                  g_module_config.trigRelayEnableCfg = module_config.trigRelayEnableCfg;
                  g_profile.modem_info.trig_relay = module_config.trigRelayEnableCfg;
                  chg |= RELAY_ENABLE_CHG;
                }
              }
              sub_item = cJSON_GetObjectItem(str_json, "locationCfg");
              if (sub_item != NULL) {
                //if (strncmp(g_profile.gateway_info.location, sub_item->valuestring, sizeof(g_profile.gateway_info.location)))
                {
                  strlcpy(g_profile.gateway_info.location, sub_item->valuestring, sizeof(g_profile.gateway_info.location));
                  if(strlen(sub_item->valuestring)==0){
                    exec_syscmd("/bin/nvram delete HN_GW_LOCATION", NULL, 0);
                  }else{
                    memset(set_cmd,0,sizeof(set_cmd));
                    snprintf(set_cmd, sizeof(set_cmd), "/bin/nvram set HN_GW_LOCATION=\"%s\"",
                           g_profile.gateway_info.location);
                    exec_syscmd(set_cmd, NULL, 0);
                  }

                  chg |= LOCATION_CHG;
                }
              }              

              sub_item = cJSON_GetObjectItem(str_json, "gatewayName");
              if (sub_item != NULL) {
                //if (strncmp(g_profile.gateway_info.gateway_name, sub_item->valuestring, sizeof(g_profile.gateway_info.gateway_name)))
                {
                  strlcpy(g_profile.gateway_info.gateway_name, sub_item->valuestring, sizeof(g_profile.gateway_info.gateway_name));
                  if(strlen(sub_item->valuestring)==0){
                    exec_syscmd("/bin/nvram delete HN_GATEWAY_NAME", NULL, 0);
                    exec_syscmd("/bin/fact_nvram delete HN_GATEWAY_NAME", NULL, 0);
                  }else{
                    memset(set_cmd,0,sizeof(set_cmd));
                    snprintf(set_cmd, sizeof(set_cmd), "/bin/nvram set HN_GATEWAY_NAME=\"%s\"",
                           g_profile.gateway_info.gateway_name);
                    exec_syscmd(set_cmd, NULL, 0);
                    memset(set_cmd,0,sizeof(set_cmd));
                    snprintf(set_cmd, sizeof(set_cmd), "/bin/fact_nvram set HN_GATEWAY_NAME=\"%s\"",
                           g_profile.gateway_info.gateway_name);
                    exec_syscmd(set_cmd, NULL, 0);                    
                  }
                  exec_syscmd("/bin/fact_nvram commit; sync", NULL, 0);
                  chg |= GATEWAYNAME_CHG;
                }
              }              
              sub_item = cJSON_GetObjectItem(str_json, "preSetSecretCfg");
              if (sub_item != NULL) {
                debug_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
                if(sub_item->valuestring==NULL){
                  debug_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
                }
                if (strncmp(g_profile.modem_info.pre_shared_secret, sub_item->valuestring, sizeof(g_profile.modem_info.pre_shared_secret)))
                {
                  strlcpy(g_profile.modem_info.pre_shared_secret, sub_item->valuestring, sizeof(g_profile.modem_info.pre_shared_secret));
                  strlcpy(g_module_config.pre_shared_secret, sub_item->valuestring, sizeof(g_module_config.pre_shared_secret));
                  if(strlen(sub_item->valuestring)==0){
                    debug_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
                    exec_syscmd("/bin/nvram delete HN_PRE_SHARED_SECRET", NULL, 0);
                  }else{
                    memset(set_cmd,0,sizeof(set_cmd));
                    snprintf(set_cmd, sizeof(set_cmd), "/bin/nvram set HN_PRE_SHARED_SECRET=\"%s\"",
                           g_profile.modem_info.pre_shared_secret);
                    exec_syscmd(set_cmd, NULL, 0);
                  }

                  chg |= PRESETSECRET_CHG;
                }
              }     
          
            //}
            cJSON_Delete(str_json);
            str_json=NULL;
            snprintf(str_para_err,sizeof(str_para_err), "%d", para_err);
            strlcpy(dc_payload_info.errorEnum, str_para_err,
                    sizeof(dc_payload_info.errorEnum));
                    
            if(para_err&CHANNEL_CHG){
              strlcpy(dc_payload_info.errorMsg, "CHANNEL_CFG error",
                    sizeof(dc_payload_info.errorMsg));
            }else if(para_err&PANID_CHG){
              strlcpy(dc_payload_info.errorMsg, "PANID_CFG error",
                    sizeof(dc_payload_info.errorMsg));
            }else{  //coverity check
              strlcpy(dc_payload_info.errorMsg, "",
                    sizeof(dc_payload_info.errorMsg));
            }
          }
          
          if (chg)
          {
            exec_syscmd("/bin/nvram commit; sync", NULL, 0);
            if (((chg & PANID_CHG) == PANID_CHG) || ((chg & CHANNEL_CHG) == CHANNEL_CHG))
            {
              *p_need_restart_prog = 1;
            }else{
              need_send_cfg =1;
            }
          }
         
        }

        strlcpy(dc_payload_info.messageType, MSG_TYPE_COMMAND,
                sizeof(dc_payload_info.messageType));
        strlcpy(dc_payload_info.flags, "response",
                sizeof(dc_payload_info.flags));
        strlcpy(dc_payload_info.action, MSG_ACTION_UP_GW_CFG,
                sizeof(dc_payload_info.action));
        strlcpy(dc_payload_info.agentVerison, "1.0",
                sizeof(dc_payload_info.agentVerison));
        strlcpy(dc_payload_info.messageId, p_dc_payload_info->messageId,
                sizeof(dc_payload_info.messageId));
        dc_payload_info.data_json = NULL;
        mqtt_wrapper_send_uploadData(&dc_payload_info, &proto_basicinfo,QOS0);
        if(need_send_cfg) {
              //get_gateway_cfg_profile(&g_profile);
              send_cloud_gateway_cfg(1);
        }
        g_profile.chg =chg;
        debug_notice_l5("nvram_get_trigerAlarmRelay cloud cause\n");
        nvram_get_trigerAlarmRelay();
        // if((chg&ALARM_ENABLE_CHG)||(chg&RELAY_ENABLE_CHG)){
        //   g_module_config.chg=chg;
        //   nvram_get_trigerAlarmRelay();
        //   g_module_config.chg&=(~(ALARM_ENABLE_CHG|RELAY_ENABLE_CHG));
        // }

      } else if (!strncmp(p_dc_payload_info->action, MSG_ACTION_DN_GW_CFG,
                          strlen(MSG_ACTION_DN_GW_CFG))) { // requst upload cfg

        //debug_info_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
        strlcpy(dc_payload_info.messageType, MSG_TYPE_COMMAND,
                sizeof(dc_payload_info.messageType));
        strlcpy(dc_payload_info.flags, "response",
                sizeof(dc_payload_info.flags));
        strlcpy(dc_payload_info.action, MSG_ACTION_UP_GW_CFG,
                sizeof(dc_payload_info.action));
        strlcpy(dc_payload_info.agentVerison, "1.0",
                sizeof(dc_payload_info.agentVerison));
        strlcpy(dc_payload_info.messageId, p_dc_payload_info->messageId,
                sizeof(dc_payload_info.messageId));
        strlcpy(dc_payload_info.errorEnum, "0",
                sizeof(dc_payload_info.errorEnum));
        // get configJson string
        item = get_gateway_cfgjson();
        dc_payload_info.data_json = cJSON_CreateObject();

        cJSON_AddItemToObject(dc_payload_info.data_json, "configJson", item);

        //debug_info_l5("%s,%s,%d.\n", __FILE__, __FUNCTION__, __LINE__);
        mqtt_wrapper_send_uploadData(&dc_payload_info, &proto_basicinfo,QOS0);
      }
    }
    //

  } else { // response of gateway to cloud request
    if (0 == strncmp(p_dc_payload_info->action, MSG_ACTION_UP_GW_CFG,
                     strlen(MSG_ACTION_UP_GW_CFG))) {
      //if (success) {
      g_mqtt_state.gatewaycfg_forsign_sending=0;
      g_mqtt_state.mqttsigned = 1;
      debug_notice_l5("MSG_ACTION_UP_GW_CFG resp msgid %s\n",p_dc_payload_info->messageId);

      exec_syscmd("echo 1 > /tmp/lora_cloud_conn_status",NULL,0);
      deletedb_offline_data_by_msgid(p_dc_payload_info->messageId,ACTION_UP_NUM_GATEWAY_CFG);
      //}
    } else if (p_dc_payload_info->success &&
               (0 == strncmp(p_dc_payload_info->action, MSG_ACTION_UP_REALDATA,
                             strlen(MSG_ACTION_UP_REALDATA)))) {
      if (strlen(p_dc_payload_info->messageId) != 0) {
        deletedb_offline_data_by_msgid(p_dc_payload_info->messageId,ACTION_UP_NUM_REALDATA);
      }
    }else if (p_dc_payload_info->success &&
               (0 == strncmp(p_dc_payload_info->action, MSG_ACTION_UP_DEV_CFG,
                             strlen(MSG_ACTION_UP_DEV_CFG)))) {
      if (strlen(p_dc_payload_info->messageId) != 0) {
        //deletedb_offline_data_by_msgid(p_dc_payload_info->messageId);
        //update resp_flag =1
        debug_notice_l5("MSG_ACTION_UP_DEV_CFG resp msgid %s\n",p_dc_payload_info->messageId);
        updatedb_offline_data_respflag_by_msgid(p_dc_payload_info->messageId,1);
      }
    }else if (p_dc_payload_info->success &&
               (0 == strncmp(p_dc_payload_info->action,
                             MSG_ACTION_UP_OFFLINEDATA,
                             strlen(MSG_ACTION_UP_OFFLINEDATA)))) {
      if (strlen(p_dc_payload_info->messageId) != 0) {
        history_del_msgid_send(p_dc_payload_info->messageId,ACTION_UP_NUM_OFFLINEDATA); //20230114 
        //deletedb_offline_data_by_msgid(p_dc_payload_info->messageId,ACTION_UP_NUM_OFFLINEDATA);
      }
    } else if ((0 == strncmp(p_dc_payload_info->action,
                             MSG_ACTION_UP_CHKPERMISSONDATA,
                             strlen(MSG_ACTION_UP_CHKPERMISSONDATA)))) {

      // get permission instrument serialnum list  tbd
      int dev_num = 0;
      sub_item = cJSON_GetObjectItem(payload_item, "allow");
      if (sub_item != NULL) {
        dev_num = cJSON_GetArraySize(sub_item);
        for (j = 0; j < dev_num; j++) {
          json_array_item = cJSON_GetArrayItem(sub_item, j);
          if (json_array_item != NULL) {
            hash_table_insert(&g_perm_hashTable, json_array_item->valuestring,
                              1);
            debug_notice_l5("MSG_ACTION_UP_CHKPERMISSONDATA resp allow serial_no[%d] %s\n", j,
                          json_array_item->valuestring);
          }
        }
      } 
      sub_item = cJSON_GetObjectItem(payload_item, "notAllow");
      if (sub_item != NULL) {      
        dev_num = cJSON_GetArraySize(sub_item);
        for (j = 0; j < dev_num; j++) {
          json_array_item = cJSON_GetArrayItem(sub_item, j);
          if (json_array_item != NULL) {
            hash_table_insert(&g_perm_hashTable, json_array_item->valuestring,
                              0);
            debug_notice_l5("MSG_ACTION_UP_CHKPERMISSONDATA resp not allow serial_no[%d] %s\n", j,
                          json_array_item->valuestring);
          }
        }
      }      
      // else {
      //   // get serinum by messageId
      //   offline_buffer_query_t query = {0}, *p_query = NULL;
      //   int err_code = 0;
      //   p_query = &query;
      //   p_query->id = 0;
      //   p_query->action_type = ACTION_UP_NUM_CHKPERMISSONDATA; // 6;
      //   p_query->msgid = p_dc_payload_info->messageId;
      //   p_query->dev_serinum = NULL;
      //   err_code = querydb_get_serinum_by_msgid(p_query, 1);
      //   if ((err_code == SQLITE_OK) && (p_query->id != 0)) {
      //     if (p_query->dev_serinum != NULL) {
      //       hash_table_insert(&g_perm_hashTable, p_query->dev_serinum, 0);
      //       debug_notice_l5("MSG_ACTION_UP_CHKPERMISSONDATA resp not allow serial_no %s\n",
      //                     p_query->dev_serinum);
      //       alink_free(p_query->dev_serinum);
      //     }
      //   }
      // }
      if (strlen(p_dc_payload_info->messageId) != 0) {
        deletedb_offline_data_by_msgid(p_dc_payload_info->messageId,ACTION_UP_NUM_CHKPERMISSONDATA);
      }

    } else if (p_dc_payload_info->success &&
               (0 == strncmp(p_dc_payload_info->action, MSG_ACTION_UP_SENDCMD,
                             strlen(MSG_ACTION_UP_SENDCMD)))) {

      if (0 == strncmp(p_dc_payload_info->messageType, MSG_TYPE_COMMANDRESP,
                       strlen(MSG_TYPE_COMMANDRESP))) {
        // recv cmdresp then del command (the same msessageId)
        if (strlen(p_dc_payload_info->messageId) != 0) {
          deletedb_offline_data_by_msgid(p_dc_payload_info->messageId,ACTION_UP_NUM_SENDCMD);
        }
      }
    }
  }
  return errcode;
}
int check_circle_end(int index) {
  return (index + 1) == DN_PAYLOAD_NUM ? 0 : index + 1;
}

char s_dn_body_str[DN_BODY_SIZE]; //???
int unpack_dn_package(dc_payload_info_t *p_dc_payload_info,
                      proto_basicinfo_t *self, unsigned char *proto_packge,
                      int packlen) {
  int errcode = 0;
  int i = 0, cur_size = 0;
  int message_id_size = 0;
  unsigned char msg_type_size = 0; // max 255
  unsigned char client_id_size = 0;
  unsigned short int reserve_head_size = 0, body_type = 0;
  unsigned char firmware_version_size = 0;
  unsigned int head_size = 0, body_size = 0;
  unsigned long long send_time;
  cJSON *root = NULL;

  // cJSON *data_jason = NULL;

  int error_code = 0, proto_packge_size = 0;

  memset(s_dn_body_str, 0, DN_BODY_SIZE);

  if (0 != strncmp(proto_packge, "CD", 2)) {
    errcode = FIXED_HEAD_ERR;
    goto err;
  }

  if (!(*(proto_packge + 2) == 0 && *(proto_packge + 3) == 1)) {
    errcode = PROTOCOL_VERSION_ERR;
    goto err;
  }
  self->byte_order = *(proto_packge + 4) & 0x01;
  if ((*(proto_packge + 4) & 0x02) == 0) {
    self->req0_or_resp1 = 0;
  } else {
    self->req0_or_resp1 = 1;
  }

  debug_info_l5("dn self.byte_order:%d\n", self->byte_order);

  cur_size = 5;
  memcpy(&head_size, proto_packge + cur_size, 4);
  cur_size += 4;

  memcpy(&message_id_size, proto_packge + cur_size, 1);
  cur_size += 1;
  memcpy(self->message_id, proto_packge + cur_size, message_id_size);
  cur_size += message_id_size;
  //debug_info_l5("dn self->message_id:%s\n", self->message_id);

  memcpy(&client_id_size, proto_packge + cur_size, 1);
  cur_size += 1;
  memcpy(self->client_id, proto_packge + cur_size, client_id_size);
  cur_size += client_id_size;
  //debug_info_l5("dn self->client_id:%s\n", self->client_id);
  memcpy(&send_time, proto_packge + cur_size, 8);
  cur_size += 8;

  memcpy(&body_type, proto_packge + cur_size, 2); // 2B Body格式，0: JSON，  1:

  cur_size += 2;
  if (self->byte_order == 1) {
    body_type = reversebytes_uint16(body_type);
  }

  debug_info_l5("dn req0_or_resp1:%d\n", self->req0_or_resp1);
  if (self->req0_or_resp1 == 1) { // error code for response

    memcpy(&self->error_code, proto_packge + cur_size, 2);
    cur_size += 2;
  } else {
    cur_size += 2; // only skip for request
  }

  memcpy(&reserve_head_size, proto_packge + cur_size, 2);
  cur_size += 2;
  //debug_info_l5("dn reserve_head_size=%02x\n", reserve_head_size);
  if (self->byte_order == 1) {
    reserve_head_size = reversebytes_uint16(reserve_head_size);
  }
  if (reserve_head_size != 0) {
    errcode = RESERVER_HEAD_SIZE_ERR;
    goto err;
    // if (reserve_head_size < 128) { // tbd
    //   memcpy(self->reserve_head, proto_packge + cur_size, reserve_head_size);
    // }else{
    //       errcode = RESERVER_HEAD_SIZE_ERR;
    //       goto err;
    // }
    // cur_size += reserve_head_size;
  }

  memcpy(&body_size, proto_packge + cur_size, 4);
  cur_size += 4;
  if (body_type == 1) {
    memcpy(&self->compress_flag, proto_packge + cur_size, 1);
    cur_size += 1;
    memcpy(&self->body_msg_type, proto_packge + cur_size, 1);
    cur_size += 1;
  }

  if (self->byte_order == 1) {
    body_size = reversebytes_uint32(body_size);

    head_size = reversebytes_uint32(head_size);
  }
  // if(head_size != cur_size)
  // {
  //   errcode =  HEAD_SIZE_ERR;
  //   goto err;
  // }
  //debug_info_l5("dn body_size=%02x\n", body_size);
  //!!!!!
  //debug_info_l5("dn head_size:%02x\n", head_size);
  //debug_info_l5("dn cur_size:%d\n", cur_size);
  if (body_size >= DN_BODY_SIZE) {
    errcode = BODY_SIZE_ERR;
    goto err;
  }
  memcpy(s_dn_body_str, proto_packge + cur_size, body_size);
  cur_size += body_size - 2;

  debug_info_l5("dn body_str:%s\n", s_dn_body_str);
  root = cJSON_Parse(s_dn_body_str);

  if (root == NULL) {
    errcode = BODY_ERR;
    goto err;
  } else {
    uint8 need_restart_prog = 0;
    errcode =
        process_dn_package(self, p_dc_payload_info, root, &need_restart_prog);
    cJSON_Delete(root); //!!!
    root=NULL;
    if (need_restart_prog) {
      usleep(500*1000);
      alink_mqtt_async_disconnect();
      usleep(500*1000);
      restart_prog();
    }
  }
err:
  if(errcode){
    debug_err_l5("dn errcode:%d action %s\n", errcode,p_dc_payload_info->action);
  }
  return errcode;
}
// dn_permission_t *read_data_from_cloud_check_permision_resp(void) {
//   // volatile
//   dn_permission_t *p_dn_perm = NULL;

//   int index = 0;
//   if (g_dn_perm_buff.left_size > 0) {
//     p_dn_perm =
//         &g_dn_perm_buff.buff_array[g_dn_perm_buff.read_no];
//     index = g_dn_perm_buff.read_no;
//     g_dn_perm_buff.read_no = check_circle_end(index);
//     debug_info_l5("read_data_from_cloud_check_permision_resp read_no :%d\n", index);
//     debug_info_l5("read_data_from_cloud_check_permision_resp left_size :%d\n",
//                   g_dn_perm_buff.left_size);
//     g_dn_perm_buff.left_size--;
//   }
//   return p_dn_perm;
// }
dn_payload_t *read_data_from_cloud_cmd(void) {
  // volatile
  dn_payload_t *p_dn_payload = NULL;

  int index = 0;
  if (g_dn_payload_cmd_buff.left_size > 0) {
    p_dn_payload =
        &g_dn_payload_cmd_buff.dn_payload_array[g_dn_payload_cmd_buff.read_no];
    index = g_dn_payload_cmd_buff.read_no;
    g_dn_payload_cmd_buff.read_no = check_circle_end(index);
    debug_l5("read_data_from_cloud_cmd read_no:%d left_size:%d\n", index,g_dn_payload_cmd_buff.left_size);
    g_dn_payload_cmd_buff.left_size--;
  }
  //else //mod for  coverity check
  return p_dn_payload;
}
dn_galarm_t *get_json_from_cloud_groupalram(void) {
  // volatile
  dn_galarm_t *p_dn_payload = NULL;

  int index = 0;
  if (g_dn_payload_galarm_buff.left_size > 0) {
    p_dn_payload = &g_dn_payload_galarm_buff.dn_payload_array[g_dn_payload_galarm_buff.read_no];
    index = g_dn_payload_galarm_buff.read_no;
    g_dn_payload_galarm_buff.read_no = check_circle_end(index);
    debug_l5("get_json_from_cloud_groupalram read_no:%d left_size:%d\n", index,g_dn_payload_galarm_buff.left_size);
    g_dn_payload_galarm_buff.left_size--;
  }
  return p_dn_payload;
}

void read_data_from_cloud_groupalram(void) {
  dn_galarm_t *p_dn_payload = NULL;
  cJSON *p_json = NULL, *item = NULL, *sub2_item = NULL, *sub3_item = NULL;
  cJSON *sub_json_array_item = NULL, *json_array_item = NULL;
  cloud_groupAlarm_cmdNode_t *p_alarm_node = NULL;
  int8 source_dev_num = 0;
  int8 j = 0, k = 0, m = 0;
  cloud_alarmItem_t *p_cursor = NULL;

  debug_l5("%s(%d) left_size %d\n", __FUNCTION__, __LINE__,g_dn_payload_galarm_buff.left_size);
  while(g_dn_payload_galarm_buff.left_size > 0){  //read all buffer data

    p_dn_payload = get_json_from_cloud_groupalram();
    #if 0
    //test data
    uint8 *json_str="[{\"workerName\":\"w1\",\"targetSerianNo\":[\"target1\",\"target2\"],\"serialNo\":\"source1\",\"alarms\":[{\"alarm\":[3,4,5],\"name\":\"H2S\"}]}]";
    dn_galarm_t dn_payload={0};
    cJSON *root_json;
    p_dn_payload = &dn_payload;
    strlcpy(p_dn_payload->messageId,"12345678901234567890",20);
    root_json=cJSON_Parse(json_str);
    p_dn_payload->dn_payload_json=cJSON_Duplicate(root_json,1);
    
    #endif 
    //end test
    if (p_dn_payload == NULL) {
      debug_info_l5("%s,%d p_dn_payload == NULL\n", __FUNCTION__, __LINE__);
      return;
    }
    p_json = p_dn_payload->dn_payload_json;
    if (p_json == NULL) {
      debug_info_l5("%s,%d p_json == NULL\n", __FUNCTION__, __LINE__);
      return;
    }
    source_dev_num = cJSON_GetArraySize(p_json);
    debug_info_l5("source instrument num[%d] \n", source_dev_num);
    if (source_dev_num > DN_PAYLOAD_SERIALNO_NUM) {
      cJSON_Delete(p_json); //!!!
      p_json=NULL;
      return;
    }
    for (j = 0; j < source_dev_num; j++) {
      json_array_item = cJSON_GetArrayItem(p_json, j); // every source instrument
      if (json_array_item != NULL) {
        p_alarm_node = (cloud_groupAlarm_cmdNode_t *)malloc(
            sizeof(cloud_groupAlarm_cmdNode_t));
        
        if (p_alarm_node != NULL) {
          memset(p_alarm_node,0,sizeof(cloud_groupAlarm_cmdNode_t));
          strlcpy(p_alarm_node->msgID, p_dn_payload->messageId,
                  sizeof(p_alarm_node->msgID));
          item = cJSON_GetObjectItem(json_array_item, "serialNo");
          if(item!=NULL){
            strlcpy(p_alarm_node->alarm_dev_serial, item->valuestring,
                  20);
            debug_info_l5("p_alarm_node->alarm_dev_serial %s\n",
                                p_alarm_node->alarm_dev_serial);                
          }
          item = cJSON_GetObjectItem(json_array_item, "targetSerialNos"); // targetSerianNo,modified by yzz
          if(item!=NULL){
            p_alarm_node->target_dev_num = cJSON_GetArraySize(item);
          }
          if ((p_alarm_node->target_dev_num > 0) &&
              (p_alarm_node->target_dev_num < DN_PAYLOAD_SERIALNO_NUM)) {
            p_alarm_node->p_target_dev_array =
                malloc(p_alarm_node->target_dev_num * 20);
            if (p_alarm_node->p_target_dev_array != NULL) {
              for (k = 0; k < p_alarm_node->target_dev_num; k++) {
                sub_json_array_item = cJSON_GetArrayItem(item, k);
                if (sub_json_array_item != NULL) {
                  strlcpy(p_alarm_node->p_target_dev_array + k * 20,
                          sub_json_array_item->valuestring, 20);
                  debug_info_l5("p_target_dev_array[%d] %s\n",k,
                                (uint8*)(p_alarm_node->p_target_dev_array + k * 20));
                }
              }
            }
          }
          item = cJSON_GetObjectItem(json_array_item, "alarms");
          if(item!=NULL){
            p_alarm_node->alarm_item_num = cJSON_GetArraySize(item);
          }
          if ((p_alarm_node->alarm_item_num > 0) &&
              (p_alarm_node->alarm_item_num < 32)) {
            p_alarm_node->p_alarm_array = (cloud_alarmItem_t *)malloc(
                p_alarm_node->alarm_item_num * sizeof(cloud_alarmItem_t));
            if(p_alarm_node->p_alarm_array!=NULL){
              for (k = 0; k < p_alarm_node->alarm_item_num; k++) {
                debug_info_l5("alarm_item_num %d\n", k);
                sub_json_array_item = cJSON_GetArrayItem(item, k);
                if (sub_json_array_item != NULL) {
                  p_cursor = (cloud_alarmItem_t *)((uint8*)p_alarm_node->p_alarm_array +
                                                  k * sizeof(cloud_alarmItem_t));
                  sub2_item = cJSON_GetObjectItem(sub_json_array_item, "alarm");
                  if(sub2_item!=NULL){
                    p_cursor->alarm_Num = cJSON_GetArraySize(sub2_item);
                    p_cursor->p_alarm = (uint8 *)malloc(p_cursor->alarm_Num);
                    if(p_cursor->p_alarm!=NULL){
                      for (m = 0; m < p_cursor->alarm_Num; m++) {
                        sub3_item = cJSON_GetArrayItem(sub2_item, m);
                        if (sub3_item != NULL) {
                          *(p_cursor->p_alarm + m) = sub3_item->valueint;
                          debug_info_l5("alarm num(m %d) %d \n", m,*(p_cursor->p_alarm + m));
                        }
                      }
                    }
                  }
                  sub2_item = cJSON_GetObjectItem(sub_json_array_item, "name");
                  if(sub2_item!=NULL){
                    strlcpy(p_cursor->sensor_name, sub2_item->valuestring,
                            sizeof(p_cursor->sensor_name));
                    debug_info_l5("p_cursor->sensor_name %s\n", p_cursor->sensor_name);
                  }
                }
              }
            }
          }
          insert_groupAlarmCmd_intoList(p_alarm_node);
        }
      }
    }
    cJSON_Delete(p_json); //!!!
    p_json=NULL;
  }
}

// end of file