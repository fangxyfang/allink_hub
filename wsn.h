#ifndef __WSN_H__
#define __WSN_H__

#include <time.h>
#include "cJSON.h"
#include "dcprotocol.h"



#define SEND_CMD_NUM_ONEPERIOD	1 // send 1 cmd in one period
#define MAX_CLOUD_GROUPCMD_TRY	5 // max 5 times if failed

#define RELAY_INSTRUMENT_ID	0xC212
#define REMOTE_ALARM_PERIOD	600 // s

#define WSN_RECV_BUF_SIZE	256 //2048
#define WSN_SEND_BUF_SIZE	256

#define MAX_WSN_BUF_LEN		255
#define WSN_BUFPOOL_SIZE	5
#define DEV_SERIALNUM_LEN	12

#define GET_DEVICE_INFO_CMD_LEN			10
#define GET_NAME_SENSOR_FMT_CMD_LEN		10

#define ILLEGAL_SEN_INDEX				0xFF

#define WSN_CMD_HDR_LEN					1
#define WSN_CMD_END_LEN					1
#define WSN_CMD_VER_LEN					1
#define WSN_PKTLENGTH_LEN				1
#define WSN_SOURCE_ADDR_LEN				2
#define WSN_DEST_ADDR_LEN				2
#define WSN_CMD_ID_LEN					1
#define WSN_CHK_LEN						1

#define RMCI_SOP 0x5B
#define RMCI_EOP 0x5D
#define RMCI_CMD_BY_HUB 0x26
#define RMCI_CMD_BY_INST 0xC7

#define ECHO_VIEW_ID 0xC2010001
#define ECHOVIEW_NUM 6

#define GET_DEVINFO_V1_CMDID			0x20
#define GET_DEVINFO_V1_RESP				0x20

#define GET_DEVINFO_V2_CMDID			0x2E
#define GET_DEVINFO_V2_RESP				0x2E


#define GET_SEN_NAMEFORMAT_V1_CMDID		0x22
#define GET_SEN_NAMEFORMAT_V1_RESP		0x22


#define GET_SEN_NAMEFORMAT_V2_CMDID		0x2A
#define GET_SEN_NAMEFORMAT_V2_RESP		0x2A

#define GET_SEN_LIMIT_V1_CMDID			0x24
#define GET_SEN_LIMIT_V1_RESP			0x24

#define GET_SEN_LIMIT_V2_CMDID			0x2C
#define GET_SEN_LIMIT_V2_RESP			0x2C


#define WSN_DATA_PAK_V1_MSGID			0x40
#define WSN_DATA_PAK_V2_MSGID			0x4A

#define LOCATION_PAK_MSGID				0x45

//cmd from cloud,or other host
#define WSN_SNDMSG_CMDID				0x53
#define WSN_PUT_SLEEP_WAKEUP_CMDID		0x54

#define WSN_SECRET_COMM_REQ				0x55
#define WSN_ENCRYPT_DATA_PAK_MSGID		0x58
#define WSN_EXCHANGE_KEY_CMDID			0x59

#define GENERAL_REQUEST_V2_CMDID		0x5A

#define SET_REMOTE_DO_CMDID				0x3E
#define SET_REMOTE_DO_RESP				0x3E


#define WSN_PING_DEV_MSGID				0x34
#define WSN_DEV_EVENT_MSGID				0x44



#define SLEEP_ACTION_TYPE				0x01
#define WKUP_ACTION_TYPE				0x02

#define WSN_ASCIITXT_MSG_TYPE			0
#define WSN_UNICODETXT_MSG_TYPE			0x02


//hwinfo flag
#define GPS_DATA_FLAG					0x02 //bit1
#define DIO_DATA_FLAG					0x20 //bit5
#define ZIGBEE_DATA_FLAG				0x40 //bit6

//cmd-data len
#define GPS_DATA_LEN					8
#define ZIGBEE_DATA_LEN					3
#define DIO_DATA_LEN					4
#define ONE_SENSOR_DATA_LEN				(2+2)

#define WSN_RESP_TIMEOUT				90  // s
#define MAX_NO_RESP_NUM					3

#define MAX_DATA_TIMEOUT_NUM			6

#define MAX_NVRAM_VARNAME_LEN			30
#define MAX_DEV_SN_LEN					21//20 

#define MAX_NVRAM_CMD_LEN				128
#define MAX_NVRAM_CMD_RESULT_LEN		256

//#define MAX_SENSOR_NAME_LEN				30
//#define MAX_MEASURE_UNIT_LEN				20
#define MAX_GAS_NAME_LEN				64

#define MAX_SENSOR_NAME_LEN		20
#define MAX_SENSOR_TYPE_LEN		20
#define MAX_MEASURE_UNIT_STRLEN	20

#define CLOUD_SNDMSG_TYPE_STR		"SendMsg"
#define CLOUD_BRDCASTMSG_TYPE_STR	"BroadcastMsg"
#define CLOUD_SLEEP_TYPE_STR		"Sleep"
#define CLOUD_WKUP_TYPE_STR			"Wakeup"
#define CLOUD_CLRPMPALM_STR			"ClearPumpAlarm"

#define CLOUD_CMD_SUCCESS			0
#define CLOUD_CMD_FAILED			1
#define CLOUD_CMD_NOT_SUPPORT		2

#define DATA_TYPE_STR				1
#define DATA_TYPE_INT				2

#define ENCRYPT_DATA2_LEN			8

#define WSN_CMD_VER1				1
#define WSN_CMD_VER2				2
//modbus cfg
#define MODBUS_CFG_MAXSIZE 64

//match the sensor special reading
enum _dection_specialReading_t
{
	TubeSampling = 1 ,
	WarmingUp = 2 ,
	DisabledTemporary = 3 ,
	AlarmOccurring = 255
	
} ;



typedef signed char sint8_t ;
typedef unsigned char uint8_t ;
typedef unsigned short uint16_t ;
typedef unsigned int uint32_t ;



//modbus cfg
typedef struct{ 
	   char dev_sn[20];
       uint16_t modbus_addr;
}modbus_sn_addr_t;
typedef struct{ 
	   int itemNum;
	   modbus_sn_addr_t modbus_sn_addr[MODBUS_CFG_MAXSIZE];
}modbus_cfg_t;

typedef struct _wsn_pkt_t
{
	uint16_t wsn_data_count ;
	uint8_t wsn_data[MAX_WSN_BUF_LEN] ;
} wsn_pkt_t ;

typedef struct _sensor_data_format_t
{
	uint8_t sensor_index ;
	uint8_t sensor_id ;
	uint8_t sensor_sub_id ;
	float res ; //efective when sen_fmt_data_len=10,sen_fmt_data[8-9]
	uint8_t res_dp ;
	uint8_t unit_id ; //measurement unit string
	uint8_t detection_mode ;
	uint8_t current_senLimit_recvFlag ;
	uint8_t sensor_dp ;
	uint8_t sensor_df ;
	uint8_t sensor_threshold_mask_v1 ; //only for cmd_v1
	uint8_t sensor_threshold_mask_high ; //cmd_v2
	uint8_t sensor_threshold_mask_low ; //cmd_v2
	uint16_t gas_id ;
	uint8_t sensor_data_len ;
	uint8_t format_data[10] ;
	uint8_t limit_data[11] ;

	void * p_next_sensor_format ;
	uint8_t pad_info[3] ;
} sensor_data_format_t ;


typedef struct _alarm_node_t
{
	uint16_t alarm_count ;
	uint8_t alarm_dev_sn[20] ;
	void *p_next_alarm_node ;
} alarm_node_t ;

#if 0
typedef struct _alarm_list_t
{
	uint8_t groupAlarmRelay_flag ; // ==1 if alarm==GroupAlarm
	alarm_node_t *p_high_alarm_listHead ;
	alarm_node_t *p_low_alarm_listHead ;
	alarm_node_t *p_fault_alarm_listHead ;

	alarm_node_t *p_relay1_listHead ;
	alarm_node_t *p_relay2_listHead ;
	alarm_node_t *p_relay3_listHead ;
	alarm_node_t *p_relay4_listHead ;
	alarm_node_t *p_relay5_listHead ;
	
} alarm_list_t ;
#endif


typedef struct _device_info_t 
{
	uint8_t cmd_ver ; //cmd_v1 and cmd_v2
	uint8_t pakSend_cnt ; //we can send only one paket to the dev in one data-period
	uint8_t powerCfg ; //cmd_v1
	uint16_t skt_mask ; // cmd_v1,not used,we used sen_mask only
	uint8_t sen_fmt_recvNO ; // handle sen_fmt_data sub pak
	uint8_t sen_data_recvNO ; // handle data subpackage
	uint8_t sub_data_pak ; // handle data subpackage,sub-sen-fmt---sub-data-pak
	uint16_t msg_id ; //used when send txt msg to dev
	uint16_t device_addr ; // corresponding to the source in the wsn paket
	uint8_t new_dev_flag ; // used when add new dev into list
	uint8_t dev_offline_flag ; //set when dev-offline,initial-val==0
	uint8_t dev_online_flag ; //set when dev-online
	uint8_t dev_info_recv_flag ;
	uint8_t sen_fmt_recv_flag ;
	uint8_t data_pak_recv_flag ;
	uint8_t sen_limit_recv_flag ; 
	char dev_rssi_val ;
	uint16_t parent_device_addr ; 
	uint8_t parent_dev_serialNum[MAX_DEV_SN_LEN] ;

	time_t dev_offline_timer ; //start to count when insert new dev,renew when recv data-pak
	time_t dev_info_recv_time ; // add for v1_dev  bug
	uint8_t active_sensor_num ;
	uint8_t sen_limit_recvNum ;
	uint8_t active_sen_index[16] ;
	//uint8_t cmd_ver ;
	uint8_t instr_family_id ;
	uint8_t instr_model_id ;
	uint8_t instr_hw_id ;
	uint8_t instr_fw_id ;
	uint8_t echoview_flag ;  //fxy 240226
	uint8_t dev_serialNum[MAX_DEV_SN_LEN] ;
	uint8_t dev_fw_ver[10] ; //6 chars
	uint8_t cfgSet_cnt ;
	uint8_t alarmSet_cnt ; //all sensors share ?
	uint8_t hw_info ;
	uint8_t modem_type ;
	uint16_t data_interval ;
	uint16_t sensor_mask ;
	uint8_t dioBank0_map ;
	uint8_t dioBank1_map ;
	uint8_t function_info ;
	uint8_t userID[10] ; //8 chars
	uint8_t exchangeKey_success ;
	uint8_t dev_support_encrypt ; // set when recv dev-comm-req (0x55)
	uint8_t dev_dpsk[32] ; // for secret-comm
	uint8_t exchangeKey_counter[16] ;
	uint32_t gen_sk_time ; 
	uint8_t session_key[32] ; // encrypt data from secret-dev
	uint8_t dataPak_sk[32] ; //not used
	uint8_t key_rand[16] ;

	uint8_t dev_local_alarm_info ; // only include dev-local alarm
	uint8_t dev_alarm_info ; // not used now!!!,used for local-remote-alarm,include dev-local-alarm and remote-alarm, cmd_v2,remote_alarm,bit7-support remote_alarm or not,bit0-high_alarm;bit1-low_alarm;bit2-fault_alarm
	uint8_t dev_remoteAlarm_info ; //used for groupAlarm cmd from cloud and local-remote-alarm,read only and never change it
	uint8_t dio_bank0_status ;
	uint8_t dio_bank1_status ;

	uint8_t recved_groupAlarm ; //recved-groupAlarm cmd flag,set when recved groupAlarm cmd,and never clear the dev-remote-alarm 
	uint8_t total_groupAlarm ; //bit0-bit2,include all-cmdNode-alarmInfo
	uint8_t total_groupRelay ; //bit0-bit4--relay1-relay5,include all cmdNode

	//cloud groupAlarmRelay alarm-dev list,only used for cloud groupAlarm cmd
	alarm_node_t groupRelay1_list_head ;
	alarm_node_t groupRelay2_list_head ;
	alarm_node_t groupRelay3_list_head ;
	alarm_node_t groupRelay4_list_head ;
	alarm_node_t groupRelay5_list_head ;

	alarm_node_t groupHigh_list_head ;
	alarm_node_t groupLow_list_head ;
	alarm_node_t groupFault_list_head ;
	
	//for check groupAlarm-relay
	uint8_t relay_bank0 ; // match dio_bank0_status,bit0-bit4--relay1-relay5
	uint8_t relay_bank1 ; // match dio_bank1_status
	
	uint8_t pading_info[3] ;
	
} device_info_t ;



typedef struct _waiting_cmd_list_t
{
	uint8_t total_cmd ;
	uint8_t *p_cmd_data ;
	uint8_t cmd_data_len ;
	uint8_t priority ;
	uint8_t cmd_id ;
	uint16_t dest_addr ;
	uint8_t target_dev_serial[20] ;
	void *p_next_cmd ;
} waiting_cmd_list_t ;

typedef struct _device_node_info_t
{
	device_info_t device_info ;
	uint8_t data_pak_count ;
	time_t sendCmd_timer ; //record the cmd-send-time
	uint8_t remain_send_cnt ; //only send one cmd in one data-period
	waiting_cmd_list_t *p_devInfo_cmd ; //only one cmd at the same time
	waiting_cmd_list_t *p_senFmt_cmd ; //only one cmd at the same time
	waiting_cmd_list_t *p_senLimit_cmd ; //only one cmd at the same time
	waiting_cmd_list_t waiting_cmd_listHead ; //common cmd list,maybe more cmd in this list;handle them one by one
	uint8_t relay_info ; //record all-dev alarm_info, bit0-bit4---relay1-relay5,generated when dev alarm

	uint8_t devinfo_buf[256-0x20-6];
	uint8_t devinfo_buf_len;
	uint8_t sensor_format_buf[2][256-0x20-6];
	uint8_t sensor_format_buf_len[2];
	uint8_t sensor_format_recv_num;
	uint8_t sensor_format_recv_finished;
	uint8_t sensor_limit_buf[256-0x20-6];
	uint8_t sensor_limit_buf_len;
	
	#if 0
	alarm_node_t relay1_list_head ;
	alarm_node_t relay2_list_head ;
	alarm_node_t relay3_list_head ;
	alarm_node_t relay4_list_head ;
	alarm_node_t relay5_list_head ;
	#endif
	uint8_t current_all_alarm ;//for remote_alarm,generated when dev alarm
	#if 0
	alarm_node_t high_list_head ;
	alarm_node_t low_list_head ;
	alarm_node_t fault_list_head ;
	#endif
	time_t cur_alarm_timer ; //start when dev_remote_alarm begin,clear after 10mins
	uint16_t total_dev_count ; //in the head,save total dev num,not used:
	uint16_t offline_dev_count ; //in head
	uint16_t online_dev_count ; // in list head
	sensor_data_format_t *p_sendor_format_data_head ;
	void *p_next_device ;

	uint8_t pading_info[3] ;
} device_node_info_t ;

typedef struct _expected_resp_cmd_list_t
{
	time_t resp_timer ;
	uint8_t no_resp_count ;
	uint16_t dest_addr ;
	uint8_t dest_serial[MAX_DEV_SN_LEN];
	uint8_t cmd_id ;
	uint8_t cmd_len ;
	uint8_t cmd[MAX_WSN_BUF_LEN] ;
	void *p_next ;
} expected_resp_cmd_list_t ;

typedef struct _dev_diag_info_t
{
	uint16_t dev_addr ;
	uint8_t dev_sn[MAX_DEV_SN_LEN] ;
	uint8_t data_pak_flag ;
	uint8_t dev_info_flag ;
	uint8_t sen_format_flag ;
	uint8_t sen_limit_flag ;
	signed char rssi_val ; //-127-127
	uint8_t dev_online_flag ;
	
} dev_diag_info_t ;

typedef struct _nvram_dev_varname_sn_pair_t
{
	uint8_t total_dev_num ;
	uint8_t nvram_var_name[MAX_NVRAM_VARNAME_LEN] ;
	uint8_t nvram_dev_sn[MAX_DEV_SN_LEN] ;
	//uint16_t dev_addr ;

	void *p_next ;
} nvram_dev_varname_sn_pair_t ;

typedef struct _wsn_net_conf_para_t
{
	uint8_t dev_offline_threshold ;
	sint8_t trig_alarm ;
	sint8_t trig_relay ;
	uint8_t clear_remoteAlarmRelay ;
} wsn_net_conf_para_t ;

typedef struct _gps_info_t
{
	uint8_t new_data_flag ;
	float x_val ;
	float y_val ;
} gps_info_t ;


typedef struct _comm_dev_data_t
{
	uint8_t hw_info ;
	uint8_t app_alarm ;
	uint8_t rm_debug_field ;
	uint8_t cfg_set_cnt ;
	uint8_t alarm_set_cnt ;
	uint8_t unit_err ;
	uint8_t unit_info ;
	uint8_t power_percent ;
	uint8_t power_cfg ; //for cmd_v1
} comm_dev_data_t ;

typedef struct _zigbee_data_t 
{
	uint8_t new_data_flag ;
	uint8_t rcm_signal ;
	uint16_t rcm_addr ;
} zigbee_data_t ;

typedef struct _sensor_data_t
{
	uint8_t total_sensor ;
	uint8_t specialReading ;//match dectionMode in the cloud-json-real-data
	uint8_t sensor_index ;
	uint8_t sensor_err ;
	uint8_t sensor_dp ;
	uint8_t sensor_df ;
	uint8_t sensor_data[4] ; //max 4 bytes,MSB,bigEndian
	float sensor_real_data ;
	void *p_next_sensor_data ;
} sensor_data_t ;

typedef struct _dev_subDataPak_list_t
{
	uint8_t dev_sn[MAX_DEV_SN_LEN] ;
	sensor_data_t *p_sen_data_list ;
	void *p_next ;
} dev_subDataPak_list_t ;

typedef struct _dio_data_t
{
	uint8_t new_data_flag ;
	uint8_t dio_bank1_status ;
	uint8_t dio_bank0_status ;
	uint8_t dio_bank1_setting ;
	uint8_t dio_bank0_setting ;
} dio_data_t ;


typedef struct _sensor_id_name_tab_t
{
	uint32_t id;
	uint8_t sensor_name[MAX_SENSOR_NAME_LEN];
	uint32_t sensor_id ;
	uint32_t sensor_subID ;
	uint8_t sensor_type[MAX_SENSOR_TYPE_LEN] ; //maybe null
} sensor_id_name_tab_t ;

typedef struct _modbusSensor_id_name_tab_t
{
	uint8_t modbus_sensorID ;
	uint8_t modbus_sensorName[MAX_SENSOR_NAME_LEN];
} modbusSensor_id_name_tab_t ;

typedef struct _measure_unit_tab_t
{
	uint32_t unit_id ;
	uint8_t unit_str[MAX_MEASURE_UNIT_STRLEN];
} measure_unit_tab_t ;

typedef struct _instrument_id_name_tab_t
{
	uint32_t id;
	uint8_t name[32];
	uint8_t model[24];  
	uint8_t brand[24]; 
	uint8_t category[16];
	uint8_t subcategory[16];
	uint32_t oldid; //not use 
} instrument_id_name_tab_t ;

typedef struct _gas_id_name_tab_t
{
	uint16_t gas_id ;
	uint8_t gas_name[MAX_GAS_NAME_LEN] ;
} gas_id_name_tab_t ;

typedef struct _unit_err_info_t
{
	uint8_t unit_err_id ; //bit0-bit5
	uint8_t unit_err_subID ; //not used
	uint8_t err_info[32] ;
} unit_err_info_t ;

typedef struct _sensor_err_info_t
{
	uint8_t sensor_err_id ; //bit0-bit7
	uint8_t sensor_err_subID ; //not used
	uint8_t err_info[64] ;
} sensor_err_info_t ;


//only one wsn_cmdID and serial pair in the cmd-list at the same time
typedef struct _cloud_cmd_list_t
{
	uint8_t result_resp ; //success--0,failed--1,not-support---2
	uint8_t action_type ; // 1 sleep,2 wake up
	uint16_t msg_type ; // 0 text ms or ascii msg,unicode msg
	uint8_t wsn_cmdID ;
	uint8_t serial_num[MAX_DEV_SN_LEN] ;
	dn_payload_t cmd_data ;
	void *p_next_cmd ;
} cloud_cmd_list_t ;

enum _GroupAlarm_Result
{
	SET_SUCCESS 	= 1 ,
	NOT_SUPPORT = 2 ,
	OFFLINE 	= 3 ,
	SET_FAILURE	= 4,
	MAX_FAILURE
};


typedef struct _handling_dev_node_t
{
	uint8_t dev_sn[20] ; //max dev sn 12
	uint8_t send_count ; //max 5 times if failed
	uint8_t groupAlarm_result ; //0-success,else err
	void *p_next_node ;
} handling_dev_node_t ;

typedef struct _cloud_alarmItem_t
{
	//alarms in one sensor
	uint8_t alarm_Num ;
	uint8_t *p_alarm ; //size=alarm_Num
	uint8_t sensor_name[20] ; 
} cloud_alarmItem_t ;

typedef struct _cloud_groupAlarm_cmdNode_t
{
	//all sensor-alarms in one alarm-dev
	uint8_t msgID[64] ; //max 64?
	uint8_t alarm_dev_serial[20] ; //maybe NULL
	uint8_t target_dev_num ;
	uint8_t *p_target_dev_array ; // target_dev_num*20
	uint8_t alarm_item_num ;
	cloud_alarmItem_t *p_alarm_array ; // alarm_item_num*sizeof(cloud_alarmItem_t)

	uint8_t cloud_alarm ; // total alarm in one cmdNode
	uint8_t cloud_relay ; //total relay in one cmdNode
	uint8_t fin_dev_num ; //include success and failed
	uint8_t success_cnt ;
	uint8_t fail_cnt ;
	handling_dev_node_t *p_handled_dev_list ;

	void *p_next_cmd ;
} cloud_groupAlarm_cmdNode_t ;

//echo view state
typedef struct _echo_view_state_t
{
	uint32_t id;
	uint8_t online ;
	uint8_t addr_msb;
	uint8_t addr_lsb;
	uint16_t fix_wsn_addr;
	uint16_t rmci_addr;
	uint8_t req_transfer_flg;
	device_info_t device_info;
} echo_view_state_t ;

//unit_err bit flag
#define UNIT_PWR_BIT_FLAG			0x01 //bit 0
#define UNIT_BATTERY_BIT_FLAG		0x02 //bit 1
#define UNIT_PUMP_BIT_FLAG			0x04 //bit 2
#define UNIT_MEM_BIT_FLAG			0x08 //bit 3
#define UNIT_SENSORMASK_BIT_FLAG	0x10 //bit 4
#define UNIT_FAILURE_BIT_FLAG		0x20 //bit 5


//unit_err info id
#define UNIT_PWR_INFO_ID			0
#define UNIT_BATTERY_INFO_ID		1
#define UNIT_PUMP_INFO_ID			2
#define UNIT_MEM_INFO_ID			3
#define UNIT_SENSOR_MASK_INFO_ID	4
#define UNIT_FAILURE_INFO_ID		5


//sensor err bit flag
#define SENSOR_OVERRANGE_BIT_FLAG	0x01 //bit 0
#define SENSOR_MAX_BIT_FLAG		0x02 //bit 2
#define SENSOR_FAILURE_BIT_FLAG		0x04
#define SENSOR_HIGH_LIMIT_BIT_FLAG	0x08
#define SENSOR_LOW_LIMIT_BIT_FLAG	0x10 //bit 5
#define SENSOR_STEL_LIMIT_BIT_FLAG	0x20
#define SENSOR_TWA_LIMIT_BIT_FLAG	0x40
#define SENSOR_DRIFT_BIT_FLAG		0x80


//sensor err info id
#define SENSOR_OVER_RANGE_ID		0
#define SENSOR_MAX_ID			1
#define SENSOR_FAILURE_ID			2
#define SENSOR_HIGH_LIMIT_ID		3
#define SENSOR_LOW_LIMIT_ID			4
#define SENSOR_STEL_LIMIT_ID		5
#define SENSOR_TWA_LIMIT_ID			6
#define SENSOR_DRIFT_ID				7



int get_serial_fd(void) ;
uint8_t get_uart_config_lock(void) ;
void set_uart_config_lock(uint8_t lock) ;


int wsn_recv_main_task(void) ;
int parse_wsn_packet(uint8_t *p_data,int data_len) ;
int parse_wsn_data(uint8_t *p_data,uint8_t data_len) ;
uint8_t * delimit_data_packet(uint8_t *p_data,uint16_t data_len,uint8_t *pkt_num) ;

uint8_t get_powerPercent_fromDataPkt(uint8_t *p_data,uint8_t data_len) ;
uint8_t get_unitInfo_fromDataPkt(uint8_t *p_data,uint8_t data_len) ;
uint8_t get_unitErr_fromDataPkt(uint8_t *p_data,uint8_t data_len) ;
uint8_t get_AlarmSetCnt_fromDataPkt(uint8_t *p_data,uint8_t data_len) ;
uint8_t get_cfgsetCnt_fromDataPkt(uint8_t *p_data,uint8_t data_len) ;
uint8_t get_rmDebugField_fromDataPkt(uint8_t *p_data,uint8_t data_len) ;
uint8_t get_appAlarm_fromDataPkt(uint8_t *p_data,uint8_t data_len) ;
uint8_t get_hwInfo_fromDataPkt(uint8_t *p_data,uint8_t data_len) ;
uint8_t get_senNum_fromDataPkt(uint8_t *p_data,uint8_t data_len) ;
uint8_t get_app_alarm(void) ;
uint8_t get_sateliteNum(void) ;


int get_app_alarmStr(uint8_t *p_str_buf,uint8_t buf_len) ;



int get_wsn_cmdID_fromPkt(uint8_t *p_data,uint8_t data_len) ;
uint16_t get_destAddr_fromPkt(uint8_t *p_data,uint8_t data_len) ;
uint16_t get_sourceAddr_fromPkt(uint8_t *p_data,uint8_t data_len) ;

uint8_t * find_pkt_hdr(uint8_t *p_data,uint16_t data_len,uint16_t *p_skip_bytes) ;
int build_get_nameSensor_format_cmdV2(uint8_t *buf,uint16_t buf_len,uint16_t source_addr,uint16_t dest_addr) ;
int build_get_device_info_cmdV2(uint8_t *buf,uint16_t buf_len,uint8_t cmd_id,uint16_t cmd_data_len,uint16_t source_addr,uint16_t dest_addr) ;
int build_wsn_cmdV2(uint8_t *buf,uint16_t buf_len,uint8_t cmd_id,uint8_t *cmd_data,uint16_t cmd_data_len,uint16_t source_addr,uint16_t dest_addr) ;
int fill_cmd_data(uint8_t *buf,uint16_t buf_len,uint8_t *cmd_data,uint16_t cmd_data_len) ;
int calc_mesh_cmd_chksum(uint8_t *data,uint16_t data_len,uint16_t cmd_data_len) ;
int parse_sensor_format_dataV2(uint8_t *p_data,uint8_t data_len) ;
sensor_data_format_t *query_sensor_format_data(uint16_t device_addr,uint8_t sen_index) ;
int insert_sensor_format_data(sensor_data_format_t *p_sensor_format_data,uint16_t device_addr) ;
void free_sensor_format_data_mem(sensor_data_format_t *p_sensor_format_data_mem) ;
int copy_device_info_toBuf(device_node_info_t *p_buf,uint8_t *p_wsn_data) ;
int insert_device_info_to_list(device_node_info_t *p_device_info) ;
device_node_info_t *query_device_info_by_deviceAddr(uint16_t dev_addr) ;
device_node_info_t *query_device_info_by_serialNum(uint8_t *p_device_serialNum) ;
void init_device_info_list(void) ;
int send_cmd_toRemote(uint8_t *p_data,uint8_t data_len) ;
int send_expectedResp_cmd_toRemote(uint8_t *p_data,uint8_t data_len) ;
waiting_cmd_list_t *query_cmd_inDevWaitingList(uint8_t cmdID,device_node_info_t *p_dev_node) ;
int insert_cmd_toDevWaitingList(waiting_cmd_list_t *p_waiting_cmd,device_node_info_t *p_dev_node) ;
int free_cmdNode_inDevWaitingList(waiting_cmd_list_t *p_cmd_node,device_node_info_t *p_dev_node) ;
int free_specifiedCmd_inDevCmdList(waiting_cmd_list_t *p_cmd_node) ;
int free_specifiedCmd_inDevCmdList(waiting_cmd_list_t *p_cmd_node) ;
int send_cmd_inDevCmdList(device_node_info_t *p_dev_node) ;
int send_cmd_toDevWaitingList(uint8_t *p_data,uint8_t data_len,uint8_t cmdID,device_node_info_t *p_dev_node) ; 







int set_senIndex_inDevInfo(device_node_info_t *p_dev_node) ;
uint8_t calc_sensorNum_accordingToMask(uint16_t sen_mask) ;
void set_local_addr(uint16_t addr) ;
int insert_expected_cmd_list(expected_resp_cmd_list_t *p_expected_cmd) ;
expected_resp_cmd_list_t *query_expected_cmd_list(uint16_t pkt_source_addr,uint8_t cmd_id) ;
void check_and_reSend_timeout_cmd(void) ;
int del_expected_resp_cmd(uint16_t pkt_source_addr,uint8_t cmd_id) ;
void check_dev_offline_status(void) ;
void set_dev_online_status(device_node_info_t *p_node) ;
void set_dev_offline_status(device_node_info_t *p_node) ;

nvram_dev_varname_sn_pair_t *query_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len) ;
void free_varName_sn_pair_list(void) ;
nvram_dev_varname_sn_pair_t *query_varName_sn_pairList_end(void);
int add_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len,uint8_t *p_var_name,uint8_t var_name_len) ;
int del_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len) ;
int del_varName_sn_pair_list(void) ;

int nvram_update_devNode_num(int dev_node_num) ;
void nvram_delete_dev_diaginfo_list(void) ;
int nvram_del_dev_diaginfo(uint8_t *p_sn,uint8_t sn_len) ;
int nvram_update_online_devNum(void) ;
int nvram_update_dev_diaginfo(dev_diag_info_t *p_diag_info) ;
int nvram_add_dev_diaginfo(dev_diag_info_t *p_diag_info) ;
int nvram_query_dev_diaginfo(uint8_t *p_sn,uint8_t sn_len) ;

void nvram_del_varName_sn_pair_list(void) ;
int nvram_del_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len) ;
int nvram_add_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len,uint8_t *p_var_name,uint8_t var_name_len) ;
int nvram_query_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len,uint8_t *p_varName_buf,uint8_t varName_buf_len) ;
int nvram_get_varName_sn_pair_list(void) ;

int nvram_get_gw_gpsData(void) ;
sint8_t nvram_get_trigRelay_conf(sint8_t *p_trig_relay) ;
sint8_t nvram_get_trigAlarm_conf(sint8_t *p_trig_alarm) ;
uint8_t nvram_get_devOffline_threshold(uint8_t *p_threshold) ;
void reset_clearRemoteAlarm_flag(void) ;
int nvram_get_clearRemoteAlarm_conf(uint8_t *p_clearRemoteAlarm) ;





//json funcs
int build_json_config_data(device_node_info_t *p_node) ;
int send_json_confData_toCloud(device_node_info_t *p_node) ;
cJSON *build_devInfo_jsonData(device_node_info_t *p_node) ;
cJSON *build_sensorfmt_jsonData(device_node_info_t *p_node) ;
cJSON * build_dev_cmdFunc_item(device_node_info_t *p_node) ;
int insert_sensor_jsonFmtDataObj(cJSON *p_sen_fmtObj,device_node_info_t *p_node,uint8_t current_sen_index) ;
int insert_sensor_jsonLimitInfo(cJSON *p_sen_fmtObj,device_node_info_t *p_node,uint8_t current_sen_index) ;
int get_sensorName_bysenIDandSubID(uint8_t sensorID,uint8_t subID,uint8_t *p_sen_name_buf,uint8_t buf_len) ;
int get_sensor_measurement_unit(uint8_t unit_id,uint8_t *p_sen_unit_buf,uint8_t buf_len) ;
int get_sensor_gasName(uint8_t sen_index,uint8_t *p_gas_name_buf,uint8_t buf_len) ;

int build_json_real_data(device_node_info_t *p_node) ;
cJSON * bulid_dev_realJsonData(device_node_info_t *p_node) ;
cJSON *build_sensor_realJsonData(device_node_info_t *p_node) ;
int save_realDataRecord(device_node_info_t *p_node) ;
int get_dev_total_sensorErr(device_node_info_t *p_node) ;

int build_offLine_jsonData(device_node_info_t *p_node) ;
cJSON * build_dev_offlineJsonData(device_node_info_t *p_node) ;





//cloud cmd funcs
cloud_cmd_list_t *query_cloudCmd_node(uint8_t *p_sn,uint8_t sn_len,uint8_t wsn_cmdID) ;
void set_cloudCmd_result(uint8_t result) ;
uint8_t get_cloudCmd_result(void) ;
int build_and_send_cloudCmdResp(cloud_cmd_list_t *p_cmd_node,device_node_info_t *p_dev_node) ;
uint8_t get_cloud_wsnCmdID(dn_payload_t *p_cloud_cmd) ;
uint8_t get_cloud_actionType(dn_payload_t *p_cloud_cmd) ;
int build_dev_cmdData(uint8_t *p_buf,uint8_t buf_len,cloud_cmd_list_t *p_cmd_node) ;
int handle_cloud_cmd(dn_payload_t *p_cloud_cmd) ;
int handle_cloud_cmdList(void) ;
int free_cloudCmdNode_byDevSn(uint8_t *p_sn,uint8_t sn_len) ;
int handle_cloud_cmdNode_bySnAndCmdID(uint8_t *p_sn,uint8_t sn_len,uint8_t wsn_cmdID) ;
int insert_cloudCmd_node(cloud_cmd_list_t *p_node) ;

//cmd_v1 funcs
int get_comm_devData_v1(uint8_t *p_data,uint8_t data_len) ;
int parse_wsn_dataPakV1(uint8_t *p_data,uint8_t data_len) ;
int copy_deviceInfo_v1_toBuf(device_node_info_t *p_buf,uint8_t *p_wsn_data) ;
int parse_wsn_devInfo_v1(uint8_t *p_data,uint8_t data_len) ;
int parse_wsn_senNameFmt_v1(uint8_t *p_data,uint8_t data_len) ;
int parse_wsn_senLimit_v1(uint8_t * p_data,uint8_t data_len) ;
uint8_t sensorErr_transV1toV2(uint8_t sensor_errV1,uint8_t *sensor_name) ;
uint8_t sensorErr_tranV2toV1(uint8_t sensor_errV2 ) ; //modbus



//sub-data-pak
sensor_data_t *query_sensorData_bySenIndex(sensor_data_t *p_list,uint8_t sen_index) ;
dev_subDataPak_list_t *query_subSenData_list(device_node_info_t *p_node) ;
int insert_subSenData_list(sensor_data_t *p_sen_data,uint8_t sen_number,device_node_info_t *p_node) ;
int free_subSenData_node( uint8_t *p_dev_sn) ;
int free_subSenData_list(void) ;
void free_specified_sensorData_memList(sensor_data_t *p_list);


//dev alarm and relay
alarm_node_t *query_alarmNode_inList(alarm_node_t *p_list_head,uint8_t *p_dev_sn) ;
int append_alarmNode_intoList(alarm_node_t *p_list_head,uint8_t *p_dev_sn) ;
int del_alarmNode_fromList(alarm_node_t *p_list_head,uint8_t *p_dev_sn) ;
int check_and_renew_alarmList(uint8_t cur_dev_alarmInfo,device_node_info_t *p_cur_dev_node) ;
int check_and_renew_relayList(uint8_t cur_dev_relay,device_node_info_t *p_cur_dev_node) ;
int triger_specifiedDev_alarm(device_node_info_t *p_cur_node) ;
int triger_relay_action(device_node_info_t *p_relay_node) ;
int check_and_set_remoteAlarmInfo(device_node_info_t *p_cur_node,sensor_data_t *p_cur_sensor_data) ;
int check_and_set_relayInfo(device_node_info_t *p_alarm_node,sensor_data_t *p_alarm_sensor_data) ;

int check_and_renew_groupAlarmList(device_node_info_t *p_cur_dev_node,uint8_t groupAlarm,uint8_t *p_alarmDev_sn) ;
int check_and_renew_groupRelayList(device_node_info_t *p_cur_dev_node,uint8_t groupRelay,uint8_t *p_alarmDev_sn) ;



int build_and_send_remoteRelayCmd(device_node_info_t *p_dev_node,uint8_t relay_cmd) ;
int build_and_send_remoteAlarmCmd(device_node_info_t *p_dev_node,uint8_t alarm_cmd) ;
int check_and_clear_RemoteRelay(device_node_info_t *p_dev_node) ;
int check_and_clear_RemoteAlarm(device_node_info_t *p_dev_node) ;





//groupAlarm
int triger_cloudRelay_action(device_node_info_t *p_relay_node) ;
int triger_specifiedDev_cloudAlarm(device_node_info_t *p_cur_node) ;
//void set_closeGroupAlarm_flag(void) ;
//void reset_closeGroupAlarm_flag(void) ;
//uint8_t query_closeGroupAlarm_flag(void) ;
cJSON * build_json_groupAlarmRespData(cloud_groupAlarm_cmdNode_t *p_cmd_node) ;
int build_and_send_groupAlarmCmd_resp(cloud_groupAlarm_cmdNode_t *p_cmd_node) ;
int generate_cloud_alarmRelay(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode) ;
int insert_groupAlarmHandling_devNode(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode,handling_dev_node_t *p_handling_node) ;
handling_dev_node_t *query_groupAlarmHandling_devNode(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode,uint8_t *p_dev_sn) ;
int free_groupAlarmHandling_devList(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode) ;
int insert_groupAlarmCmd_intoList(cloud_groupAlarm_cmdNode_t *p_groupAlarmCmd) ;
int free_alarmItemArray(cloud_alarmItem_t *p_array,uint8_t array_item_num) ;
int free_groupAlarmNode_byMsgID(cloud_groupAlarm_cmdNode_t *p_groupAlarm_node) ;
int free_groupAlarmNode_byMsgIDandAlarmSn(cloud_groupAlarm_cmdNode_t *p_groupAlarm_node) ;
int free_groupAlarm_list(void) ;
int query_dev_inGroupAlarmTarget(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode,uint8_t *p_dev_sn) ;
cloud_groupAlarm_cmdNode_t * query_groupAlarmCmdNode_bydevSn(cloud_groupAlarm_cmdNode_t *p_cmd_list,uint8_t *p_dev_sn) ;
int check_and_handle_offlineDev_inGroupCmd(cloud_groupAlarm_cmdNode_t *p_cmd_node) ;
int handle_failedDev_inOneCmdNode(cloud_groupAlarm_cmdNode_t *p_cmd_node) ;
int handle_allOpenGroupAlarmCmd_failedResp(void) ;
int handle_relatedOpenGroupAlarmCmd_failedResp(uint8_t *p_dev_sn) ;
int handle_closeGroupAlarmCmd_failedResp(void) ;
int handle_relatedCloseGroupAlarmCmd_failedResp(uint8_t *p_dev_sn) ;
int handle_groupAlarmCmd_node(cloud_groupAlarm_cmdNode_t *p_cmd_node,device_node_info_t *p_dev_node) ;
int query_and_handle_groupAlarmCmdList(device_node_info_t *p_dev_node) ;
int nvram_set_trigRelayAlarm_conf(void) ;

int handle_relatedDev_OpenGroupAlarmCmd_failed(uint8_t *p_targetDev_sn,uint8_t *p_alarmDev_sn) ;
int handle_relatedDev_CloseGroupAlarmCmd_failed(uint8_t *p_targetDev_sn,uint8_t *p_alarmDev_sn) ;



//called in the main
void clear_digaInfo_inNvram(void) ;




//idtable.c
int query_sensor_name_type(uint32_t sensor_id,uint32_t sensor_sub_id,uint8_t *p_sen_name_buf,uint8_t senName_bufLen,uint8_t *p_sen_type_buf,uint8_t senType_bufLen) ;
int query_unit_err_tab(uint8_t unit_err_id,uint8_t sub_id,uint8_t *p_info_buf,uint8_t buf_len) ;
int query_sensor_err_tab(uint8_t sen_err_id,uint8_t sub_id,uint8_t *p_info_buf,uint8_t buf_len) ;
int query_sensor_err_tab_v2(uint8_t sen_err_id,uint8_t sub_id,uint8_t *p_info_buf,uint8_t buf_len) ;
int query_unitID_TAB(uint8_t unitID,uint8_t *p_unit_buf,uint8_t buf_len) ;
int query_instrument_tab(uint32_t product_id,uint8_t *p_instrument_name_buf,uint8_t buf_len) ;
int query_gas_name_bySensorFmtGasID(uint16_t gas_id,uint8_t *p_gas_name_buf,uint8_t buf_len) ;




int transfer_dirChar_inSn(uint8_t *p_sn,uint8_t *buf,uint8_t buf_len) ;



uint16_t query_modbus_addr_bySerial(uint8_t *p_serial_num) ;
uint8_t query_instruID_byInstruName(uint8_t *p_instruName) ;
int build_and_send_modbusData(device_node_info_t *p_dev_node,uint8_t *p_modbus_data_buf,uint16_t buf_len) ;





















































































#endif
