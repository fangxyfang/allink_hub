
#ifndef __DCPROTOCOL_H__
#define __DCPROTOCOL_H__
#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "cJSON.h"
#include "curlhttps.h"
//#define TEST_FIXEDDATA
#define SIGN "sign"
#define MSG_TYPE_UPLOAD "upload"

#define MSG_TYPE_COMMAND "command"
#define MSG_TYPE_COMMANDRESP "commandResponse"

#define MSG_ACTION_UP_GW_CFG "uploadGatewayConfig"
#define MSG_ACTION_DN_GW_CFG "uploadGatewayConfig"

#define MSG_ACTION_DN_UPDATE_GW_CFG "updateGatewayConfig"
#define MSG_ACTION_DN_UPGRADE_GW  "upgradeFirmwareForGateway"

#define MSG_ACTION_UP_REALDATA "uploadRecord"
#define MSG_ACTION_UP_DEV_CFG "deviceConfig"
#define MSG_ACTION_UP_OFFLINEDATA "uploadHistoryRecord"
#define MSG_ACTION_UP_CHKPERMISSONDATA	"checkDevicePermission"

#define MSG_ACTION_UP_DEV_DATALOG "uploadDeviceDataLog"
#define MSG_ACTION_UP_DEV_EVENT "uploadDeviceEvent"

#define MSG_ACTION_UP_TOPOLOGY "uploadTopology"

#define MSG_ACTION_DN_SENDCMD "SendCmd"
#define MSG_ACTION_UP_SENDCMD "SendCmd"
#define MSG_ACTION_GROUPALARM "groupAlarm"


#define ACTION_UP_NUM_REALDATA 1
#define ACTION_UP_NUM_DEV_CFG 2
#define ACTION_UP_NUM_OFFLINEDATA 3
#define ACTION_UP_NUM_SENDCMD 4
#define ACTION_UP_NUM_GATEWAY_CFG 5
#define ACTION_UP_NUM_CHKPERMISSONDATA 6

#define CHANNEL_CHG 1
#define PANID_CHG 2
#define ALARM_ENABLE_CHG 4
#define RELAY_ENABLE_CHG 8
#define LOCATION_CHG 16
#define PRESETSECRET_CHG 32
#define GATEWAYNAME_CHG 64


#define D2C_TOPIC "toCloud/"
#define C2D_TOPIC "toDevice/"

#define PROFILE_ID "centerGateway"
//0 最低等级 只保证发，不保证收到 1 中间等级 保证收到，可能会重复收到。 2 最高等级 保证收到，只收到一次
#define QOS0 0
#define QOS1 1
#define QOS2 2

#define DN_BODY_SIZE 8192

typedef struct
{
    unsigned char msgid[MESSAGE_ID_LEN];
    unsigned long long send_time_sec;
    unsigned char resp_flag;
} last_topology_t;

typedef struct
{
	unsigned char messageType[32];
	unsigned char flags[32];
	unsigned char action[32];
	unsigned char agentVerison[32];
	unsigned char errorEnum[4]; //only "flags": ["response"]
	unsigned char errorMsg[32]; //only "flags": ["response"]
	uint8 success;
	unsigned char messageId[MESSAGE_ID_LEN]; //not init for dn; 

	cJSON *data_json;			//json data payload
	unsigned char *binary_body; //binary data payload
	int binary_body_size;		//binary data payload size

	unsigned char dev_serinum[DEV_SERINUM_LEN];
	int dev_configv;
	
	uint8 retry; //send retry flag 0:first 1:retry


} dc_payload_info_t;
#define DN_PAYLOAD_NUM 20
#define DN_PAYLOAD_SERIALNO_NUM 64 
typedef struct
{
	unsigned char command_id[32];			
	unsigned char command_type[32];
	uint8 dev_num ;
	unsigned char serial_no[DN_PAYLOAD_SERIALNO_NUM][32];
	uint16 data_len ;
	unsigned char data[48];
} dn_payload_t;

typedef struct
{
	dn_payload_t dn_payload_array[DN_PAYLOAD_NUM];
	int recv_no;
	int read_no;
	volatile int left_size;

} dn_payload_cmd_buffer_t;
typedef struct
{
	cJSON* dn_payload_json;
	uint8 messageId[MESSAGE_ID_LEN];
} dn_galarm_t;
typedef struct
{
	dn_galarm_t dn_payload_array[DN_PAYLOAD_NUM];
	int recv_no;
	int read_no;
	volatile int left_size;
} dn_payload_galarm_buffer_t;

typedef struct
{
	uint8 serial_no[DN_PAYLOAD_SERIALNO_NUM][32];
	uint8 dev_num ;
} dn_permission_t;

typedef struct
{
	dn_permission_t buff_array[3];
	int recv_no;
	int read_no;
	volatile int left_size;
} dn_permission_buffer_t;

enum dn_protocl_err
{
	FIXED_HEAD_ERR = 30,
	PROTOCOL_VERSION_ERR,
	BYTE_ORDER_ERR,
	HEAD_SIZE_ERR,
	BODY_SIZE_ERR,
	BODY_ERR,
	RESERVER_HEAD_SIZE_ERR
};

typedef struct
{
	uint8 package_name[64+1];
	uint8 firmware_url[512]; 
	uint8 check_sum[64+1];	 
	uint16 major;
	uint16 minor;
	uint16 revision;
	uint16 build_number;
	uint8 revision_string[32];
	uint8 formatted_version[64];
	uint8 processing;
} dn_upgrade_firmware_t;

unsigned char *construct(proto_basicinfo_t *self, unsigned char *body_str, int body_size,
						 int *proto_packge_size);
unsigned char *pack_msg(dc_payload_info_t *p_dc_payload_info, proto_basicinfo_t *p_proto_basicinfo,
						unsigned char *body_str, int body_size, int *p_proto_packge_size);

unsigned char *pack_offline_data_from_database(unsigned long long sample_time,
											   unsigned char *message_id,
											   unsigned char *body_str,
											   int body_size,
											   int *p_proto_packge_size);
unsigned char *pack_msg_upload(dc_payload_info_t *p_dc_payload_info,
							   proto_basicinfo_t *p_proto_basicinfo,
							   int *p_proto_packge_size);

unsigned char *filldata_msg_sign(int *p_proto_packge_size);
int unpack_dn_package(dc_payload_info_t *p_dc_payload_info, proto_basicinfo_t *self, unsigned char *proto_packge,
					  int packlen);
void config_request_head(proto_basicinfo_t *p_proto_basicinfo);		
int check_circle_end(int index);
dn_payload_t *read_data_from_cloud_cmd(void);
void read_data_from_cloud_groupalram(void);
void *get_upgrade_file_thread(void * para);

#endif