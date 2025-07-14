

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include<sys/time.h>


#include "wsn.h"
#include "rmci.h"
#include "debug.h"
#include "dcprotocol.h"
#include "cJSON.h"
#include "uart_api.h"
#include "mqtt_async_wrapper.h"
#include "modbustcp_slave/modbustcp_slave.h"
extern last_topology_t g_last_topology;

extern module_config_t g_module_config;
echo_view_state_t g_echoview={0};
echo_view_state_t g_echoview_list[ECHOVIEW_NUM]={0};

device_node_info_t device_info_list_head ;
device_node_info_t offline_dev_info_list_head ;

uint16_t local_addr = 0x0001 ;
uint8_t rmci_recv_buf[WSN_RECV_BUF_SIZE*8+6*5*8] ;
uint8_t wsn_recv_buf[WSN_RECV_BUF_SIZE*8] ;
uint8_t wsn_decrypt_buf[255] ;
uint8_t wsn_decrypt_data_buf[512] ;
uint8_t wsn_send_buf[WSN_SEND_BUF_SIZE] ;
uint16_t bit_mask[16] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000};
expected_resp_cmd_list_t *p_expected_resp_cmd_head = NULL ;
nvram_dev_varname_sn_pair_t varname_sn_pair_head ;
wsn_net_conf_para_t wsn_net_conf_data ;

cJSON *p_config_jsonData = NULL ;
cJSON *p_real_jsonData = NULL ;

comm_dev_data_t comm_dev_data ;
dio_data_t dev_dio_data ;
zigbee_data_t dev_zigbee_data ;
gps_info_t dev_gps_data ;
gps_info_t gw_gps_data ;

sensor_data_t *p_dev_sensorData_head = NULL ;
dev_subDataPak_list_t *p_dev_subDataPak_list = NULL ;
cloud_cmd_list_t *p_cloud_cmd_list_head = NULL ;
uint8_t cloud_cmd_result = 0 ;
uint16_t encrypt_panID = 0x574e ;


uint8_t record_fmt[1024];
uint8_t record_data[2048] ;
uint8_t temp_fmt[1024] ;
uint8_t temp_cmd_data[255] ;

//uint8_t test_buf[255] = {0x7b,0x38,0x14,0x00,0x10,0x00,0x01,0x58,0x61,0x39,0xcf,0xb9,0xd1,0x48,0xc6,0x45,0xc0,0x29,0x9b,0x11,0x1e,0xd0,0x7d} ;
//uint8_t test_buf[255] = {0x7b,0x38,0x14,0x00,0x10,0x00,0x01,0x58,0x61,0x3a,0xbb,0x6b,0xa7,0xdf,0x66,0xcd,0x6c,0x33,0x1d,0xa4,0x08,0xee,0x7d} ;
uint8_t test_buf[255] = {0x7b,0x38,0x14,0x00,0x10,0x00,0x01,0x58,0x61,0x3a,0xc9,0x46,0x08,0x20,0x8e,0x04,0x3d,0x56,0xf0,0xa2,0x55,0xf2,0x7d} ;
uint8_t test_data_len = 23 ;

//for alarm dev list
//uint8_t relay_info ; //record all-dev alarm_info, bit0-bit4---relay1-relay5,generated when dev alarm
alarm_node_t relay1_list_head ;
alarm_node_t relay2_list_head ;
alarm_node_t relay3_list_head ;
alarm_node_t relay4_list_head ;
alarm_node_t relay5_list_head ;
//uint8_t current_all_alarm ;//for remote_alarm,generated when dev alarm
alarm_node_t high_list_head ;
alarm_node_t low_list_head ;
alarm_node_t fault_list_head ;

//alarm_list_t all_alarmRelay_list ;


cloud_groupAlarm_cmdNode_t *p_cloud_groupAlarm_list = NULL ; //groupAlarm cmd list
//uint8_t close_GroupAlarm = 0 ; //set when handle close groupAlarm cmd,reset when finish and send close-resp or recved new open cmd
uint8_t gw_serial[32] ;
uint8_t recv_data_pak_flag = 0 ;
uint8_t modbus_data_buf[256] ;
uint8_t modbus_offlineData_buf[256] ;
modbus_data_buf_t *p_wsn_modbus_dataList = NULL ;
uint8_t is_datapack_from_echoview(uint16_t pkt_source_addr){
    uint8_t i=0,shot=0;
	for(i=0;i<ECHOVIEW_NUM;i++){
		//if(strlen(g_echoview_list[i].device_info.dev_serialNum)==0){
		if(g_echoview_list[i].fix_wsn_addr == pkt_source_addr &&g_echoview_list[i].online == 1){	
			shot=1;
			debug("is_datapack_from_echoview\n");	
            break;
		}
	}
	return shot;
}
void add_dev_to_echoview_array(device_node_info_t *p_node_info){
    uint8_t i=0,shot=0;
	for(i=0;i<ECHOVIEW_NUM;i++){
			if ((g_echoview_list[i].online == 1) && ((g_echoview_list[i].fix_wsn_addr==p_node_info->device_info.device_addr)  \ 
			    || (!strncmp(g_echoview_list[i].device_info.dev_serialNum,p_node_info->device_info.dev_serialNum,strlen(p_node_info->device_info.dev_serialNum)))))
			{
			    debug("add_dev_to_echoview_array reduant echoview i %d serialNum %s fix_wsn_addr 0x%x \n",i,g_echoview_list[i].device_info.dev_serialNum,g_echoview_list[i].fix_wsn_addr);	
			    debug_notice_l5("add_dev_to_echoview_array reduant echoview i %d serialNum %s fix_wsn_addr 0x%x \n",i,g_echoview_list[i].device_info.dev_serialNum,g_echoview_list[i].fix_wsn_addr);	
				memset(g_echoview_list[i].device_info.dev_serialNum,0,sizeof(g_echoview_list[i].device_info.dev_serialNum));
				g_echoview_list[i].addr_lsb = 0;
				g_echoview_list[i].addr_msb = 0;    //fxy echoview
				g_echoview_list[i].fix_wsn_addr = 0;
				g_echoview_list[i].online = 0;				
			}		
	}	
	for(i=0;i<ECHOVIEW_NUM;i++){
		//if(strlen(g_echoview_list[i].device_info.dev_serialNum)==0){
		if(g_echoview_list[i].online == 0){	
			strlcpy(g_echoview_list[i].device_info.dev_serialNum,p_node_info->device_info.dev_serialNum,sizeof(g_echoview.device_info.dev_serialNum));
			g_echoview_list[i].addr_lsb = p_node_info->device_info.device_addr &0x00FF;
			g_echoview_list[i].addr_msb = (p_node_info->device_info.device_addr &0xFF00)>8;    //fxy echoview
			g_echoview_list[i].fix_wsn_addr = p_node_info->device_info.device_addr;
			g_echoview_list[i].online = 1;
			shot=1;
			debug("add_dev_to_echoview_array echoview i %d serialNum %s fix_wsn_addr 0x%x \n",i,g_echoview_list[i].device_info.dev_serialNum,g_echoview_list[i].fix_wsn_addr);	
			debug_notice_l5("add_dev_to_echoview_array echoview i %d serialNum %s fix_wsn_addr 0x%x \n",i,g_echoview_list[i].device_info.dev_serialNum,g_echoview_list[i].fix_wsn_addr);	
            break;
		}
	}
	if(shot==0){
		debug("too many echoview \n");
	}
}
void rm_dev_to_echoview_array(device_node_info_t *p_node_info){
    uint8_t i=0;
	for(i=0;i<ECHOVIEW_NUM;i++){
		if (!strncmp(g_echoview_list[i].device_info.dev_serialNum,p_node_info->device_info.dev_serialNum,strlen(p_node_info->device_info.dev_serialNum))){
			debug("rm_dev_to_echoview_array echoview i %d serialNum %s fix_wsn_addr 0x%x \n",i,g_echoview_list[i].device_info.dev_serialNum,g_echoview_list[i].fix_wsn_addr);
			debug_notice_l5("rm_dev_to_echoview_array echoview i %d serialNum %s fix_wsn_addr 0x%x \n",i,g_echoview_list[i].device_info.dev_serialNum,g_echoview_list[i].fix_wsn_addr);
			memset(g_echoview_list[i].device_info.dev_serialNum,0,sizeof(g_echoview_list[i].device_info.dev_serialNum));
			g_echoview_list[i].addr_lsb = 0;
			g_echoview_list[i].addr_msb = 0;    //fxy echoview
			g_echoview_list[i].fix_wsn_addr = 0;
			g_echoview_list[i].online = 0;
		}
	}
}


int nvram_get_gwSn(void)
{
	uint8_t cmd_buf[128] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int ret_val = 0 ;

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	snprintf(cmd_buf,sizeof(cmd_buf),"nvram get DeviceSN");
	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0 || strlen(cmd_result) == 0)
	{
		debug("nvram_get_gwSn failed  \n");
		strlcpy(gw_serial,"0123456789",sizeof(gw_serial)) ;
	}
	else
	{
		strlcpy(gw_serial,cmd_result,sizeof(gw_serial)) ;
		debug("gw serial=%s \n",gw_serial);
	}

	return 0;
}



cJSON * build_json_groupAlarmRespData(cloud_groupAlarm_cmdNode_t *p_cmd_node)
{
	cJSON *p_respData_root = NULL ;
	cJSON *p_temp = NULL ;
	cJSON *p_dataObj = NULL ;
	cJSON *p_fail_array = NULL ;
	handling_dev_node_t *p_handling = NULL ;
	int count = 0 ;
	
	long long int time_val = 0 ; 
	
	if(p_cmd_node == NULL)
	{
		debug("err para when build_groupAlarmResp_jsonData \n");
		return NULL ;
	}

	//create p_respData_root
	p_respData_root = cJSON_CreateObject() ;
	if(p_respData_root == NULL)
	{
		debug("err crate respData_root when build_groupAlarmResp_jsonData \n");
		return NULL ;
	}

	//add sndTime
	time_val = get_utcms();
	p_temp = cJSON_CreateNumber(time_val) ;
	if(p_temp == NULL)
	{	
		debug("err create sndTime when build_json_groupAlarmRespData \n");
		cJSON_Delete(p_respData_root) ;
		return NULL ;
	}
	//add into p_respData_root
	cJSON_AddItemToObject(p_respData_root,"sndTime",p_temp) ;

	//add profileId
	p_temp = cJSON_CreateString("centerGateway") ;
	if(p_temp == NULL)
	{	
		debug("err create profileId when build_json_groupAlarmRespData \n");
		cJSON_Delete(p_respData_root) ;
		return NULL ;
	}
	//add into p_respData_root
	cJSON_AddItemToObject(p_respData_root,"profileId",p_temp) ;

	//add action
	p_temp = cJSON_CreateString("groupAlarm") ;
	if(p_temp == NULL)
	{	
		debug("err create action when build_json_groupAlarmRespData \n");
		cJSON_Delete(p_respData_root) ;
		return NULL ;
	}
	//add into p_respData_root
	cJSON_AddItemToObject(p_respData_root,"action",p_temp) ;

	//add data
	p_dataObj = cJSON_CreateObject();
	if(p_dataObj == NULL)
	{
		debug("err create p_dataObj when build_json_groupAlarmRespData \n") ;
		cJSON_Delete(p_respData_root) ;
		return NULL ;
	}
	//add into p_respData_root 
	cJSON_AddItemToObject(p_respData_root,"data",p_dataObj) ;

	//add fail array
	p_fail_array = cJSON_CreateArray() ;
	if(p_fail_array == NULL)
	{
		debug("err create p_fail_array when build_json_groupAlarmRespData \n");
		cJSON_Delete(p_respData_root) ;
		return NULL ;
	}
	//add into p_dataObj
	cJSON_AddItemToObject(p_dataObj,"fail",p_fail_array) ;

	//add fail dev serial
	p_handling = p_cmd_node->p_handled_dev_list ;
	while(p_handling != NULL)
	{
		if(p_handling->groupAlarm_result != SET_SUCCESS)
		{
			//create dev string and add into p_fail_array
			p_temp = cJSON_CreateString(p_handling->dev_sn) ;
			if(p_temp == NULL)
			{
				debug("err create fail dev string when build_json_groupAlarmRespData \n");
				cJSON_Delete(p_respData_root) ;
				return NULL ;
			}
			//add into p_fail_array
			cJSON_AddItemToArray(p_fail_array,p_temp) ;
		}
		
		p_handling = p_handling->p_next_node ;
	}

	return p_respData_root ;
}

int build_and_send_groupAlarmCmd_resp(cloud_groupAlarm_cmdNode_t *p_cmd_node)
{
	proto_basicinfo_t out_data ;
	dc_payload_info_t dc_payload_info ;
	cJSON *p_data_root = NULL ;
	
	if(p_cmd_node == NULL)
	{
		return 0 ;
	}

	debug("finish handle this cmdNode,targetNum=%d,begin to send groupAlarm resp now \n",p_cmd_node->target_dev_num);

	p_data_root = build_json_groupAlarmRespData(p_cmd_node);
	if(p_data_root == NULL)
	{
		debug("err build p_data_root when build_and_send_groupAlarmCmd_resp \n");
		return -1 ;
	}

	strlcpy(dc_payload_info.messageType,"command",sizeof(dc_payload_info.messageType));
	strlcpy(dc_payload_info.flags,"response",sizeof(dc_payload_info.flags));
	strlcpy(dc_payload_info.agentVerison,"1.0",sizeof(dc_payload_info.agentVerison)) ;
	strlcpy(dc_payload_info.action,MSG_ACTION_GROUPALARM,sizeof(dc_payload_info.action)) ;
	strlcpy(dc_payload_info.messageId,p_cmd_node->msgID,sizeof(dc_payload_info.messageId)) ;

	if(p_cmd_node->fail_cnt > 0)
	{
		//maybe more than 1 dev failed
		//dc_payload_info.success = 0 ;
		strlcpy(dc_payload_info.errorEnum,"1",sizeof(dc_payload_info.errorEnum)) ;
		strlcpy(dc_payload_info.errorMsg,"error",sizeof(dc_payload_info.errorMsg));
		
	}
	else
	{
		//dc_payload_info.success = 1 ;
		strlcpy(dc_payload_info.errorEnum,"0",sizeof(dc_payload_info.errorEnum)) ;
	}

	//strlcpy(dc_payload_info.dev_serinum,p_node->device_info.dev_serialNum,sizeof(dc_payload_info.dev_serinum)) ;
	//dc_payload_info.dev_configv = p_node->device_info.cfgSet_cnt ;
	dc_payload_info.retry =0;

	dc_payload_info.data_json = p_data_root ;
		
	mqtt_wrapper_send_uploadData(&dc_payload_info,&out_data,QOS0);
		
	//free mem
	//need not to free,it has been freeed when mqtt_wrapper_send_uploadData 
	//cJSON_Delete(p_real_jsonData) ;
	p_data_root = NULL ;
	

	return 0 ;
}

int generate_cloud_alarmRelay(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode)
{
	uint8_t count = 0 ;
	uint8_t sub_cnt = 0 ;
	uint8_t alarmID = 0 ;
	uint8_t relay4_sen_name[10] = "LEL" ;
	uint8_t relay4_sen_name_1[10] = "CH4" ;
	uint8_t relay5_sen_name[10] = "H2S" ;
	
	if(p_groupAlarmNode == NULL)
	{
		return -1 ;
	}

	//init value before generate
	p_groupAlarmNode->cloud_alarm = 0 ;
	p_groupAlarmNode->cloud_relay = 0 ;

	for(count=0;count<p_groupAlarmNode->alarm_item_num;count++) //more sensor-alarms
	{
		//debug("alarm_item_num=%d,hand cnt=%d,subAlarm_cnt=%d \n",p_groupAlarmNode->alarm_item_num,count,p_groupAlarmNode->p_alarm_array[count].alarm_Num) ;
		for(sub_cnt=0;sub_cnt<p_groupAlarmNode->p_alarm_array[count].alarm_Num;sub_cnt++) //more alarm in one sensor
		{
			alarmID = (p_groupAlarmNode->p_alarm_array[count].p_alarm)[sub_cnt] ;
			if(alarmID == 0)
			{
				debug("alarmItemID=%d,subID=%d,alarmID=0 when generate_cloud_alarmRelay \n",count,sub_cnt);
				//p_groupAlarmNode->cloud_relay = 0 ;
				//p_groupAlarmNode->cloud_alarm = 0 ;
				continue ;
			}

			debug("sub_item_num=%d,alarmID=%d,sensorName=%s \n",p_groupAlarmNode->p_alarm_array[count].alarm_Num,alarmID,p_groupAlarmNode->p_alarm_array[count].sensor_name);
			//set relay1
			p_groupAlarmNode->cloud_relay |= 0x01 ; //relay1,bit0

			//max,over,high,highHigh,0x40-43
			if(alarmID >= 64 && alarmID <= 67)
			{
				//generate high alarm
				p_groupAlarmNode->cloud_alarm |= 0x01 ; // bit0,high alarm
				if(strcmp(p_groupAlarmNode->p_alarm_array[count].sensor_name,"O2") != 0) // not O2,generate low alarm
				{
					p_groupAlarmNode->cloud_alarm |= 0x02 ; //bit1,low alarm
				}

				//generate relay
				//set relay3
				p_groupAlarmNode->cloud_relay |= 0x04 ; //relay3,bit2
				//it is lel,h2s or not?
				if(strcmp(p_groupAlarmNode->p_alarm_array[count].sensor_name,relay4_sen_name) == 0 ||
										strcmp(p_groupAlarmNode->p_alarm_array[count].sensor_name,relay4_sen_name_1) == 0 )
				{
					//match and set relay4,bit3
					p_groupAlarmNode->cloud_relay |= 0x08 ; //relay4,bit3
				}
				else if(strcmp(p_groupAlarmNode->p_alarm_array[count].sensor_name,relay5_sen_name) == 0)
				{
					//match and set relay5,bit4
					p_groupAlarmNode->cloud_relay |= 0x10 ; //relay5,bit4
				}
				
			}
			else if(alarmID >= 68 && alarmID <= 69)
			{
				//0x44-0x45,low,lowLow
				//generate low alarm
				p_groupAlarmNode->cloud_alarm |= 0x02 ; //bit1,low alarm

				//generate relay2
				p_groupAlarmNode->cloud_relay |= 0x02 ; //relay2,bit1
			}
			else
			{
				// all other fault
				//set fault alarm
				p_groupAlarmNode->cloud_alarm |= 0x04 ; //bit2,fault alarm
		
			}
			
		}

		//debug("handle next alarmItem \n");
	}

	debug("generated cmdnode msgID=%s,targetNum=%d,Grelay=0x%x,Galarm=0x%x \n",p_groupAlarmNode->msgID,p_groupAlarmNode->target_dev_num,
																				p_groupAlarmNode->cloud_relay,p_groupAlarmNode->cloud_alarm);

	return 0 ;
}

int insert_groupAlarmHandling_devNode(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode,handling_dev_node_t *p_handling_node)
{
	handling_dev_node_t *p_tmp_handling = NULL ;
	
	if(p_groupAlarmNode == NULL || p_handling_node == NULL)
	{
		return 0 ;
	}

	if(p_groupAlarmNode->p_handled_dev_list == NULL)
	{
		p_groupAlarmNode->p_handled_dev_list = p_handling_node ;
		return 0 ;
	}

	p_tmp_handling = p_groupAlarmNode->p_handled_dev_list ;
	while(p_tmp_handling != NULL)
	{
		if(p_tmp_handling->p_next_node == NULL)
		{
			p_tmp_handling->p_next_node = p_handling_node ;
			break ;
		}

		p_tmp_handling = p_tmp_handling->p_next_node ;
	}

	return 0 ;
}


handling_dev_node_t *query_groupAlarmHandling_devNode(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode,uint8_t *p_dev_sn)
{
	handling_dev_node_t *p_tmp_node = NULL ;
	
	if(p_groupAlarmNode == NULL || p_dev_sn == NULL || p_dev_sn[0] == 0) //NULL pointer or null string
	{
		return NULL ;
	}

	p_tmp_node = p_groupAlarmNode->p_handled_dev_list ;
	while(p_tmp_node != NULL)
	{
		if(strcmp(p_tmp_node->dev_sn,p_dev_sn) == 0) //match
		{
			return p_tmp_node ;
		}

		p_tmp_node = p_tmp_node->p_next_node ;
	}

	return NULL ;
}


int free_groupAlarmHandling_devList(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode)
{
	handling_dev_node_t *p_next = NULL ;
	handling_dev_node_t *p_tmp = NULL ;
	
	if(p_groupAlarmNode == NULL)
	{
		return 0 ;
	}

	p_tmp = p_groupAlarmNode->p_handled_dev_list ;
	while(p_tmp != NULL)
	{
		p_next = p_tmp->p_next_node ;
		free(p_tmp) ;
		p_tmp = p_next ;
	}
	p_groupAlarmNode->p_handled_dev_list = NULL ;
	
	return 0 ;
}



int insert_groupAlarmCmd_intoList(cloud_groupAlarm_cmdNode_t *p_groupAlarmCmd)
{
	cloud_groupAlarm_cmdNode_t *p_end = NULL ;
	int count = 0 ;
	
	if(p_groupAlarmCmd == NULL)
	{
		return -1 ;
	}

	//only insert single cmd 
	p_groupAlarmCmd->p_next_cmd = NULL ;

	debug("begin to insert groupAlarm cmd into list,generate groupAlarm and relay now \n");

	if(p_groupAlarmCmd->alarm_dev_serial[0] == 0)
	{
		debug("err NULL alarm_dev_serial when insert_groupAlarmCmd_intoList,discard it now \n");
		//free_groupAlarmNode_byMsgIDandAlarmSn(p_groupAlarmCmd);
		//free mem now
		if(p_groupAlarmCmd->p_target_dev_array != NULL)
		{
			free(p_groupAlarmCmd->p_target_dev_array) ;
			p_groupAlarmCmd->p_target_dev_array = NULL ;
		}
			
		free_alarmItemArray(p_groupAlarmCmd->p_alarm_array,p_groupAlarmCmd->alarm_item_num) ;
		p_groupAlarmCmd->p_alarm_array = NULL ;

		free_groupAlarmHandling_devList(p_groupAlarmCmd) ;
		p_groupAlarmCmd->p_handled_dev_list = NULL ;
			
		free(p_groupAlarmCmd) ;
		
		return -1 ;
	}

	generate_cloud_alarmRelay(p_groupAlarmCmd);

	#if 1
	//for specified dev,only one kind of cmd (close or normal-alarmCMD ) in the list
	if(p_groupAlarmCmd->cloud_alarm == 0)
	{
		//this is close-groupAlarm cmd,handle all normal-groupAlarm cmd failed about the specified dev now
		for(count=0;count<p_groupAlarmCmd->target_dev_num;count++)
		{
			//maybe handle all dev failed,resp to cloud at once
			//handle_relatedOpenGroupAlarmCmd_failedResp( &(p_groupAlarmCmd->p_target_dev_array[count*20]) ) ;

			//hand the specified dev in the cmd node
			handle_relatedDev_OpenGroupAlarmCmd_failed( &(p_groupAlarmCmd->p_target_dev_array[count*20]),p_groupAlarmCmd->alarm_dev_serial ) ;
		}
	}
	else
	{
		//this is normal groupAlarm cmd,hand all close-groupAlarm cmd failed about the specified dev now
		for(count=0;count<p_groupAlarmCmd->target_dev_num;count++)
		{
			//maybe handle all dev failed,resp to cloud at once
			//handle_relatedCloseGroupAlarmCmd_failedResp( &(p_groupAlarmCmd->p_target_dev_array[count*20]) ) ;

			//hand the specifed dev in the cmd node
			handle_relatedDev_CloseGroupAlarmCmd_failed( &(p_groupAlarmCmd->p_target_dev_array[count*20]),p_groupAlarmCmd->alarm_dev_serial) ;
		}
	}
	#endif
	

	//maybe some dev offline,handle it now
	check_and_handle_offlineDev_inGroupCmd(p_groupAlarmCmd) ;
	if(p_groupAlarmCmd->fin_dev_num >= p_groupAlarmCmd->target_dev_num)
	{
		debug("maybe all target offline,groupAlarm cmd failed and return now \n");
		//send resp to cloud now
		build_and_send_groupAlarmCmd_resp(p_groupAlarmCmd) ;

		//free cmd_node
		//free_groupAlarmNode_byMsgIDandAlarmSn(p_groupAlarmCmd) ;
		//free mem now
		if(p_groupAlarmCmd->p_target_dev_array != NULL)
		{
			free(p_groupAlarmCmd->p_target_dev_array) ;
			p_groupAlarmCmd->p_target_dev_array = NULL ;
		}
			
		free_alarmItemArray(p_groupAlarmCmd->p_alarm_array,p_groupAlarmCmd->alarm_item_num) ;
		p_groupAlarmCmd->p_alarm_array = NULL ;

		free_groupAlarmHandling_devList(p_groupAlarmCmd) ;
		p_groupAlarmCmd->p_handled_dev_list = NULL ;
			
		free(p_groupAlarmCmd) ;
		
		return 0 ;
	}

	//new groupAlarm comming,cloud take over remoteAlarm management now,close local-clear-remoteAlarm activity
	//reset_clearRemoteAlarm_flag(); //not reset,clear all-dev's remoteAlarm not included in the group-cmd list

	debug("begin to insert groupAlarm cmd into list \n");
	if(p_cloud_groupAlarm_list == NULL)
	{
		p_cloud_groupAlarm_list = p_groupAlarmCmd ;
		return 0 ;
	}

	p_end = p_cloud_groupAlarm_list ;
	while(p_end != NULL)
	{
		if(p_end->p_next_cmd == NULL)
		{
			p_end->p_next_cmd = p_groupAlarmCmd ;
			break ;
		}

		p_end = p_end->p_next_cmd ;
	}

	return 0 ;
}



int free_alarmItemArray(cloud_alarmItem_t *p_array,uint8_t array_item_num)
{
	uint8_t count = 0 ;
	if(p_array == NULL || array_item_num <= 0)
	{
		return 0 ;
	}

	for(count=0;count<array_item_num;count++)
	{
		if(p_array[count].p_alarm != NULL)
		{
			free(p_array[count].p_alarm) ;
			p_array[count].p_alarm = NULL ;
		}
	}

	free(p_array) ;
	
	return 0 ;
}

//only match msgID
int free_groupAlarmNode_byMsgID(cloud_groupAlarm_cmdNode_t *p_groupAlarm_node)
{
	cloud_groupAlarm_cmdNode_t *p_cur = NULL ;
	cloud_groupAlarm_cmdNode_t *p_prev = NULL ;

	if(p_groupAlarm_node == NULL )
	{
		return 0 ;
	}

	p_cur = p_cloud_groupAlarm_list ;
	while(p_cur != NULL)
	{
		if(strcmp(p_cur->msgID,p_groupAlarm_node->msgID) == 0)
		{
			if(p_prev == NULL)
			{
				//list head
				p_cloud_groupAlarm_list = p_cur->p_next_cmd ;
			}
			else
			{
				p_prev->p_next_cmd = p_cur->p_next_cmd ;
			}

			//free mem now
			if(p_cur->p_target_dev_array != NULL)
			{
				free(p_cur->p_target_dev_array) ;
				p_cur->p_target_dev_array = NULL ;
			}
			
			free_alarmItemArray(p_cur->p_alarm_array,p_cur->alarm_item_num) ;
			p_cur->p_alarm_array = NULL ;

			free_groupAlarmHandling_devList(p_cur) ;
			p_cur->p_handled_dev_list = NULL ;
			
			free(p_cur) ;
			
			break ;
		}

		p_prev = p_cur ;
		p_cur = p_cur->p_next_cmd ;
	}

	return 0 ;
}


int free_groupAlarmNode_byMsgIDandAlarmSn(cloud_groupAlarm_cmdNode_t *p_groupAlarm_node)
{
	cloud_groupAlarm_cmdNode_t *p_cur = NULL ;
	cloud_groupAlarm_cmdNode_t *p_prev = NULL ;

	if(p_groupAlarm_node == NULL )
	{
		return 0 ;
	}

	if(p_groupAlarm_node->alarm_dev_serial[0] == 0) //serial NULL
	{
		free_groupAlarmNode_byMsgID(p_groupAlarm_node);
		return 0 ;
	}

	p_cur = p_cloud_groupAlarm_list ;
	while(p_cur != NULL)
	{
		if(strcmp(p_cur->msgID,p_groupAlarm_node->msgID) == 0 && strcmp(p_cur->alarm_dev_serial,p_groupAlarm_node->alarm_dev_serial) == 0)
		{
			//msgID and alarm_dev match
			if(p_prev == NULL)
			{
				//list head
				p_cloud_groupAlarm_list = p_cur->p_next_cmd ;
			}
			else
			{
				p_prev->p_next_cmd = p_cur->p_next_cmd ;
			}

			//free mem now
			if(p_cur->p_target_dev_array != NULL)
			{
				free(p_cur->p_target_dev_array) ;
				p_cur->p_target_dev_array = NULL ;
			}
			
			free_alarmItemArray(p_cur->p_alarm_array,p_cur->alarm_item_num) ;
			p_cur->p_alarm_array = NULL ;

			free_groupAlarmHandling_devList(p_groupAlarm_node) ;
			
			free(p_cur) ;
			
			break ;
		}

		p_prev = p_cur ;
		p_cur = p_cur->p_next_cmd ;
	}

	return 0 ;
}

int free_groupAlarm_list(void)
{
	cloud_groupAlarm_cmdNode_t *p_cur = NULL ;
	cloud_groupAlarm_cmdNode_t *p_next = NULL ;

	p_cur = p_cloud_groupAlarm_list ;
	while(p_cur != NULL)
	{
		p_next = p_cur->p_next_cmd ;

		//free mem now
		if(p_cur->p_target_dev_array != NULL)
		{
			free(p_cur->p_target_dev_array) ;
			p_cur->p_target_dev_array = NULL ;
		}
			
		free_alarmItemArray(p_cur->p_alarm_array,p_cur->alarm_item_num) ;
		p_cur->p_alarm_array = NULL ;

		free_groupAlarmHandling_devList(p_cur) ;
		
		free(p_cur) ;
		
		p_cur = p_next ;
	}

	p_cloud_groupAlarm_list = NULL ;

	return 0 ;
}

// return -1 ---not found,return 0- ----found
int query_dev_inGroupAlarmTarget(cloud_groupAlarm_cmdNode_t *p_groupAlarmNode,uint8_t *p_dev_sn)
{
	uint8_t count = 0 ;
	if(p_groupAlarmNode == NULL || p_dev_sn == NULL)
	{
		return -1 ; //not exist
	}

	for(count=0;count<p_groupAlarmNode->target_dev_num;count++)
	{
		// 1 dev-serial ==20 bytes
		if(strcmp(&(p_groupAlarmNode->p_target_dev_array[count*20]),p_dev_sn) == 0)
		{
			return 0 ;
		}
	}

	return -1 ;
}

//return -1 ---not found,return 0 ---found
int query_dev_in_allGroupCmdList(device_node_info_t *p_dev_node)
{
	cloud_groupAlarm_cmdNode_t *p_list = NULL ;
	int ret_val = 0 ;
	//found return 0,not found return -1 
	if(p_dev_node == NULL)
	{
		return -1 ;
	}

	p_list = p_cloud_groupAlarm_list ;
	while(p_list != NULL)
	{
		ret_val = query_dev_inGroupAlarmTarget(p_list,p_dev_node->device_info.dev_serialNum);
		if(ret_val == 0)
		{
			//found the dev in this cmd_node,return 0
			return 0 ;
		}

		p_list = p_list->p_next_cmd ;
	}

	return -1 ;
}

cloud_groupAlarm_cmdNode_t * query_groupAlarmCmdNode_bydevSn(cloud_groupAlarm_cmdNode_t *p_cmd_list,uint8_t *p_dev_sn)
{
	cloud_groupAlarm_cmdNode_t *p_tmp = NULL ;
	uint8_t count = 0 ;

	if(p_cmd_list == NULL || p_dev_sn == NULL || p_dev_sn[0] == 0)
	{
		return NULL ;
	}

	p_tmp = p_cmd_list ;
	while(p_tmp != NULL)
	{
		if(query_dev_inGroupAlarmTarget(p_tmp,p_dev_sn) != -1)
		{
			return p_tmp ;
		}

		p_tmp = p_tmp->p_next_cmd ;
	}
	

	return NULL ;
}


int check_and_handle_offlineDev_inGroupCmd(cloud_groupAlarm_cmdNode_t *p_cmd_node)
{
	int count = 0 ;
	device_node_info_t *p_dev_node = NULL ;
	handling_dev_node_t *p_handling = NULL ;

	if(p_cmd_node == NULL || p_cmd_node->target_dev_num == 0)
	{
		debug("err para when check_and_handle_offlineDev_inGroupCmd \n");
		return 0 ;
	}

	debug("begin to check_and_handle_offlineDev_inGroupCmd \n");
	for(count=0;count<p_cmd_node->target_dev_num;count++)
	{
		p_dev_node = query_device_info_by_serialNum(&(p_cmd_node->p_target_dev_array[count*20])) ;
		if(p_dev_node == NULL || p_cmd_node->p_target_dev_array[count*20] == 0 || p_dev_node->device_info.cmd_ver == WSN_CMD_VER1)
		{
			debug("dev=%s offline when check_and_handle_offlineDev_inGroupCmd \n",&(p_cmd_node->p_target_dev_array[count*20]) );
			//dev not online,handle it now
			p_handling = malloc(sizeof(handling_dev_node_t)) ;
			if(p_handling == NULL)
			{
				debug("err malloc when check_and_handle_offlineDev_inGroupCmd \n");
				return -1 ;
			}
			memset(p_handling,0,sizeof(handling_dev_node_t));
			if(p_cmd_node->p_target_dev_array[count*20] == 0)
			{
				//NULL targetSn
				p_handling->groupAlarm_result = SET_FAILURE ;
			}
			else if((p_dev_node != NULL) && (p_dev_node->device_info.cmd_ver == WSN_CMD_VER1))
			{
				//v1 not support
				p_handling->groupAlarm_result = NOT_SUPPORT ;
			}
			else
			{
				p_handling->groupAlarm_result = OFFLINE ;
			}
			strlcpy(p_handling->dev_sn,&(p_cmd_node->p_target_dev_array[count*20]),sizeof(p_handling->dev_sn)) ;

			p_cmd_node->fail_cnt++ ;
			p_cmd_node->fin_dev_num++ ;
			
			//insert into handling list
			insert_groupAlarmHandling_devNode(p_cmd_node,p_handling) ;
		}
		else
		{
			//dev online,new groupCmd come,reset old groupAlarm value
			debug("recved new groupAlarm cmd, the target dev=%s online,set recved-groupAlarm now \n",p_dev_node->device_info.dev_serialNum);

			p_dev_node->device_info.recved_groupAlarm = 1 ;
			//reset the groupAlarm and groupRelay untill recved cloud cmd
			//p_dev_node->device_info.total_groupAlarm = 0 ;
			//p_dev_node->device_info.total_groupRelay = 0 ;
			
		}
	}

	
	return 0 ;
}

//called when set_dev_offline
int handle_offlineDev_groupAlarmCmdList(device_node_info_t *p_offline_node)
{
	cloud_groupAlarm_cmdNode_t *p_list = NULL ;
	cloud_groupAlarm_cmdNode_t *p_temp = NULL ;
	handling_dev_node_t *p_handling = NULL ;
	
	if(p_offline_node == NULL)
	{
		return 0 ;
	}

	p_offline_node->device_info.recved_groupAlarm = 0 ; //reset the recv-groupAlarm flag
	p_list = p_cloud_groupAlarm_list;
	while(1)
	{
		p_temp = query_groupAlarmCmdNode_bydevSn(p_list,p_offline_node->device_info.dev_serialNum) ;
		if(p_temp != NULL)
		{
			//handle offline dev-groupCmd now
			p_handling = query_groupAlarmHandling_devNode(p_temp,p_offline_node->device_info.dev_serialNum) ;
			#if 0
			if(p_handling != NULL && p_handling->groupAlarm_result != 0) // initial value == 0
			{
				//this dev has triger success,or offline,or not_support,or set_failure
				p_list = p_temp->p_next_cmd ; // continue to handle next cmdNode about this dev
				continue ;
			}
			#endif
			
			if(p_handling == NULL)
			{
				//maybe not handled this dev 
				p_handling = malloc(sizeof(handling_dev_node_t)) ;
				if(p_handling != NULL)
				{
					memset(p_handling,0,sizeof(handling_dev_node_t));
					strlcpy(p_handling->dev_sn,p_offline_node->device_info.dev_serialNum,sizeof(p_handling->dev_sn)) ;
					//insert into handling list
					insert_groupAlarmHandling_devNode(p_temp,p_handling) ;
				}
				else
				{
					debug("err malloc handling when handle_offlineDev_groupAlarmCmdList \n");
					//need to hande other cmdNode 
					p_temp->fail_cnt++ ;
					p_temp->fin_dev_num++ ;
				}
			}
			
			if(p_handling != NULL && p_handling->groupAlarm_result == 0)
			{
				//handle offline now,set fail_cnt and fin_dev_num 
				p_handling->groupAlarm_result = OFFLINE ;
				p_temp->fail_cnt++ ;
				p_temp->fin_dev_num++ ;
			}

			p_list = p_temp->p_next_cmd ; // continue to handle next cmdNode about this dev
			if(p_temp->fin_dev_num >= p_temp->target_dev_num)
			{	
				//send resp to cloud now
				build_and_send_groupAlarmCmd_resp(p_temp) ;
				
				//free cmd_node
				free_groupAlarmNode_byMsgIDandAlarmSn(p_temp) ;
			}
			
			continue ;
		}
		else
		{
			debug("no more cmdNode about this dev=%s when  handle_offlineDev_groupAlarmCmdList \n",p_offline_node->device_info.dev_serialNum);
			break ;
		}
	}
	
	return 0 ;
}


//handle the specified dev failed if dev not handle or offline in one cmd-node
int handle_failedSpecifiedDev_inOneCmdNode(cloud_groupAlarm_cmdNode_t *p_cmd_node,uint8_t *p_dev_sn)
{
	handling_dev_node_t *p_handling = NULL ;
	int count = 0 ;
	int ret_val = 0 ;
	
	if(p_cmd_node == NULL || p_dev_sn == NULL)
	{
		return 0 ;
	}

	if(query_dev_inGroupAlarmTarget(p_cmd_node,p_dev_sn) != 0)
	{
		debug("dev=%s not in the groupAlarm cmd-node when handle_failedSpecifiedDev_inOneCmdNode \n",p_dev_sn);
		return 0 ;
	}

	//for(count=0;count<p_cmd_node->target_dev_num;count++)
	{
		p_handling = query_groupAlarmHandling_devNode(p_cmd_node,p_dev_sn ) ;
		if(p_handling == NULL)
		{
			//dev not in handling list,create and insert now
			p_handling = malloc(sizeof(handling_dev_node_t)) ;
			if(p_handling == NULL)
			{
				debug("err malloc when handle_failedSpecifiedDev_inOneCmdNode \n");
				return -1 ;
			}
			memset(p_handling,0,sizeof(handling_dev_node_t));
			strlcpy(p_handling->dev_sn,&(p_cmd_node->p_target_dev_array[count*20]),sizeof(p_handling->dev_sn));
			p_handling->groupAlarm_result = SET_FAILURE ;
			insert_groupAlarmHandling_devNode(p_cmd_node,p_handling) ;

			//set failNum and finNum now
			p_cmd_node->fail_cnt++;
			p_cmd_node->fin_dev_num++ ;
		}
		else
		{
			if(p_handling->groupAlarm_result == 0)
			{
				//not handle,set failNum and finNum now
				p_cmd_node->fail_cnt++;
				p_cmd_node->fin_dev_num++ ;
			}
			
			if(p_handling->groupAlarm_result == 0 || p_handling->groupAlarm_result == OFFLINE)
			{
				//not handle or dev-offline,set failure now
				p_handling->groupAlarm_result = SET_FAILURE ;
			}
		}
		
	}

	return 0 ;
}



//handle all dev in one cmd-node,and resp to cloud to cloud at once
int handle_failedDev_inOneCmdNode(cloud_groupAlarm_cmdNode_t *p_cmd_node)
{
	handling_dev_node_t *p_handling = NULL ;
	int count = 0 ;
	
	if(p_cmd_node == NULL)
	{
		return 0 ;
	}

	for(count=0;count<p_cmd_node->target_dev_num;count++)
	{
		p_handling = query_groupAlarmHandling_devNode(p_cmd_node,&(p_cmd_node->p_target_dev_array[count*20]) ) ;
		if(p_handling == NULL)
		{
			//dev not in handling list,create and insert now
			p_handling = malloc(sizeof(handling_dev_node_t)) ;
			if(p_handling == NULL)
			{
				debug("err malloc when handle_failedDev_inOneCmdNode \n");
				return -1 ;
			}
			memset(p_handling,0,sizeof(handling_dev_node_t));
			strlcpy(p_handling->dev_sn,&(p_cmd_node->p_target_dev_array[count*20]),sizeof(p_handling->dev_sn));
			p_handling->groupAlarm_result = SET_FAILURE ;
			insert_groupAlarmHandling_devNode(p_cmd_node,p_handling) ;

			continue ;
		}

		if(p_handling->groupAlarm_result == 0)
		{
			p_handling->groupAlarm_result = SET_FAILURE ;
		}
		
	}

	return 0 ;
}

#if 0
//called when inset new close_cmd,handle all normal-alarm cmd-node
int handle_allOpenGroupAlarmCmd_failedResp(void)
{
	cloud_groupAlarm_cmdNode_t *p_list = NULL ;
	cloud_groupAlarm_cmdNode_t *p_next = NULL ;
	uint16_t productID = 0 ;
	int count = 0 ;
	handling_dev_node_t * p_handling = NULL ;
	
	p_list = p_cloud_groupAlarm_list;
	while(p_list != NULL)
	{
		p_next = p_list->p_next_cmd ;

		//alarm and relay generated at the same time when generate_cloud_alarmRelay
		if(p_list->cloud_alarm != 0 || p_list->cloud_relay != 0)
		{
			//finish and send resp now
			handle_failedDev_inOneCmdNode(p_list) ;
			p_list->fin_dev_num = p_list->target_dev_num ;
			p_list->fail_cnt = p_list->target_dev_num - p_list->success_cnt ;
			build_and_send_groupAlarmCmd_resp(p_list) ;
			free_groupAlarmNode_byMsgIDandAlarmSn(p_list) ;
		}

		//handle next
		p_list = p_next ;
	}


	return 0 ;
}


//called when inset new close_cmd,handle dev-related cmd-node,handle all dev and resp to cloud at once
int handle_relatedOpenGroupAlarmCmd_failedResp(uint8_t *p_dev_sn)
{
	cloud_groupAlarm_cmdNode_t *p_list = NULL ;
	cloud_groupAlarm_cmdNode_t *p_temp = NULL ;
	uint16_t productID = 0 ;
	int count = 0 ;
	handling_dev_node_t * p_handling = NULL ;

	if(p_dev_sn == NULL)
	{
		return 0 ;
	}
	
	p_list = p_cloud_groupAlarm_list;
	while(1)
	{
		
		p_temp = query_groupAlarmCmdNode_bydevSn(p_list,p_dev_sn) ;
		if(p_temp != NULL)
		{
			p_list = p_temp->p_next_cmd ; //handle next cmd-node
			if(p_temp->cloud_alarm == 0)
			{
				//this is close cmd,ignore it
				continue ;
			}
			
			//normal-alarm cmd,finish and send resp now
			handle_failedDev_inOneCmdNode(p_temp) ;
			p_temp->fin_dev_num = p_temp->target_dev_num ;
			p_temp->fail_cnt = p_temp->target_dev_num - p_temp->success_cnt ;
			build_and_send_groupAlarmCmd_resp(p_temp) ;
			free_groupAlarmNode_byMsgIDandAlarmSn(p_temp) ;

			continue ;
		}
		else
		{
			//no more cmd-node in the list
			break ;
		}
		
	}


	return 0 ;
}

#endif


//called when inset new close_cmd,handle dev-related cmd-node,only handle the specified-dev in the cmd-node
int handle_relatedDev_OpenGroupAlarmCmd_failed(uint8_t *p_targetDev_sn,uint8_t *p_alarmDev_sn)
{
	cloud_groupAlarm_cmdNode_t *p_list = NULL ;
	cloud_groupAlarm_cmdNode_t *p_temp = NULL ;
	uint16_t productID = 0 ;
	int count = 0 ;
	handling_dev_node_t * p_handling = NULL ;

	if(p_targetDev_sn == NULL || p_alarmDev_sn == NULL)
	{
		return 0 ;
	}
	
	p_list = p_cloud_groupAlarm_list;
	while(1)
	{
		
		p_temp = query_groupAlarmCmdNode_bydevSn(p_list,p_targetDev_sn) ;
		if(p_temp != NULL )
		{
			p_list = p_temp->p_next_cmd ; //handle next cmd-node
			if(p_temp->cloud_alarm == 0)
			{
				//this is close cmd,ignore it
				continue ;
			}

			if(strcmp(p_temp->alarm_dev_serial,p_alarmDev_sn) == 0)
			{
				//only handle when targetDev and alarmDev match
				//normal-alarm cmd,handle and set the specified dev groupAlarm result
				handle_failedSpecifiedDev_inOneCmdNode(p_temp,p_targetDev_sn) ;
				if(p_temp->fin_dev_num >= p_temp->target_dev_num)
				{
					build_and_send_groupAlarmCmd_resp(p_temp) ;
					free_groupAlarmNode_byMsgIDandAlarmSn(p_temp) ;
				}
			}

			continue ;
		}
		else
		{
			//no more cmd-node in the list
			break ;
		}
		
	}


	return 0 ;
}


#if 0
//called when recved new normal-alarm cmd,handle all close-cmd
int handle_closeGroupAlarmCmd_failedResp(void)
{
	cloud_groupAlarm_cmdNode_t *p_list = NULL ;
	cloud_groupAlarm_cmdNode_t *p_next = NULL ;

	//normally,only one close-cmd in the list
	p_list = p_cloud_groupAlarm_list ;
	while(p_list != NULL)
	{
		p_next = p_list->p_next_cmd ;
		//alarm and relay generated at the same time when generate_cloud_alarmRelay
		if(p_list->cloud_alarm == 0 || p_list->cloud_relay == 0)
		{
			//finish and send resp now
			handle_failedDev_inOneCmdNode(p_list) ;
			p_list->fin_dev_num = p_list->target_dev_num ;
			p_list->fail_cnt = p_list->target_dev_num - p_list->success_cnt ;
			build_and_send_groupAlarmCmd_resp(p_list) ;
			free_groupAlarmNode_byMsgIDandAlarmSn(p_list) ;
		}

		p_list = p_next ;
	}

	return 0 ;
}


//called when recved new normal-alarm cmd,handle dev-related cmd-node,handle all dev in the cmd-node ande resp to cloud at once
int handle_relatedCloseGroupAlarmCmd_failedResp(uint8_t *p_dev_sn)
{
	cloud_groupAlarm_cmdNode_t *p_list = NULL ;
	cloud_groupAlarm_cmdNode_t *p_temp = NULL ;
	uint16_t productID = 0 ;
	int count = 0 ;
	handling_dev_node_t * p_handling = NULL ;

	if(p_dev_sn == NULL)
	{
		return 0 ;
	}
	
	p_list = p_cloud_groupAlarm_list;
	while(1)
	{
		
		p_temp = query_groupAlarmCmdNode_bydevSn(p_list,p_dev_sn) ;
		if(p_temp != NULL)
		{
			p_list = p_temp->p_next_cmd ; //handle next cmd-node
			if(p_temp->cloud_alarm != 0)
			{
				//this is normal-alarm cmd,ignore it
				continue ;
			}
			
			//close cmd,finish and send resp now
			handle_failedDev_inOneCmdNode(p_temp) ;
			p_temp->fin_dev_num = p_temp->target_dev_num ;
			p_temp->fail_cnt = p_temp->target_dev_num - p_temp->success_cnt ;
			build_and_send_groupAlarmCmd_resp(p_temp) ;
			free_groupAlarmNode_byMsgIDandAlarmSn(p_temp) ;

			continue ;
		}
		else
		{
			//no more cmd-node in the list
			break ;
		}
		
	}


	return 0 ;
}

#endif


//called when recved new normal-alarm cmd,handle dev-related cmd-node, only handle the specified-dev in the cmd-node
int handle_relatedDev_CloseGroupAlarmCmd_failed(uint8_t *p_targetDev_sn,uint8_t *p_alarmDev_sn)
{
	cloud_groupAlarm_cmdNode_t *p_list = NULL ;
	cloud_groupAlarm_cmdNode_t *p_temp = NULL ;
	uint16_t productID = 0 ;
	int count = 0 ;
	handling_dev_node_t * p_handling = NULL ;

	if(p_targetDev_sn == NULL || p_alarmDev_sn == NULL)
	{
		return 0 ;
	}
	
	p_list = p_cloud_groupAlarm_list;
	while(1)
	{
		
		p_temp = query_groupAlarmCmdNode_bydevSn(p_list,p_targetDev_sn) ;
		if(p_temp != NULL)
		{
			p_list = p_temp->p_next_cmd ; //handle next cmd-node
			if(p_temp->cloud_alarm != 0)
			{
				//this is normal-alarm cmd,ignore it
				continue ;
			}

			if( strcmp(p_temp->alarm_dev_serial,p_alarmDev_sn) == 0)
			{
				//only handle when targetDev and alarmDev match
				//close cmd,handle and set the specified dev groupAlarm result
				handle_failedSpecifiedDev_inOneCmdNode(p_temp,p_targetDev_sn) ;
				if(p_temp->fin_dev_num >= p_temp->target_dev_num)
				{
					build_and_send_groupAlarmCmd_resp(p_temp) ;
					free_groupAlarmNode_byMsgIDandAlarmSn(p_temp) ;
				}
			}
			
			continue ;
		}
		else
		{
			//no more cmd-node in the list
			break ;
		}
		
	}


	return 0 ;
}




int handle_groupAlarmCmd_node(cloud_groupAlarm_cmdNode_t *p_cmd_node,device_node_info_t *p_dev_node)
{
	handling_dev_node_t *p_handling = NULL ;
	uint16_t productID = 0 ;
	uint8_t bank0_support = 0 ;
	uint8_t bank1_support = 0 ;

	if(p_cmd_node == NULL || p_dev_node == NULL)
	{
		debug("err para when handle_groupAlarmCmd_node \n");
		return -1 ;
	}

	//handle close groupAlarm cmd,so keep it
	#if 0
	if(p_cmd_node->cloud_alarm == 0 && p_cmd_node->cloud_relay == 0)
	{
		debug("alarm and relay = 0 in this cmd_node when handle_groupAlarmCmd_node \n");
		//add handle fin here
		p_cmd_node->fin_dev_num = p_cmd_node->target_dev_num ;//invalid alarm value,free this node
		return 0 ;
	}
	#endif

	bank0_support = p_dev_node->device_info.hw_info & 0x10 ; //bank0 is DO or not
	bank1_support = p_dev_node->device_info.hw_info & 0x08 ; //bank1 is DO or not

	debug("begin to handle dev=%s groupAlarm or relay \n",p_dev_node->device_info.dev_serialNum);

	productID = (p_dev_node->device_info.instr_family_id << 8) | p_dev_node->device_info.instr_model_id ;
	
	p_handling = query_groupAlarmHandling_devNode(p_cmd_node,p_dev_node->device_info.dev_serialNum) ;
	if(p_handling != NULL)
	{
		debug("found dev=%s in handling list when handle_groupAlarmCmd_node,groupResult=%d \n",p_dev_node->device_info.dev_serialNum,p_handling->groupAlarm_result);

		// check dev offline or not-support status
		if(p_handling->groupAlarm_result == NOT_SUPPORT || p_handling->groupAlarm_result == SET_SUCCESS || p_handling->groupAlarm_result == SET_FAILURE)
		{
			debug("this dev in this groupAlarm-cmd has been handled, not support remote alarm or relay,or set_success,or set_failure \n");
			return 0 ;
		}

		//check offline status and handle it
		if(p_handling->groupAlarm_result == OFFLINE)
		{
			
			//dev online,handle it now
			p_dev_node->device_info.recved_groupAlarm = 1 ; //set the recv-groupalarm flag,and never clear the dev-remoteAlarm
			p_handling->groupAlarm_result = 0 ; //clear offline status
			if(p_cmd_node->fail_cnt > 0)
			{
				p_cmd_node->fail_cnt-- ;
			}
			if(p_cmd_node->fin_dev_num > 0)
			{
				p_cmd_node->fin_dev_num-- ;
			}
			
			//append groupAlarm,groupRelay into dev total_groupAlarm,total_groupRelay
			p_dev_node->device_info.total_groupRelay |= p_cmd_node->cloud_relay ;
			p_dev_node->device_info.total_groupAlarm |= p_cmd_node->cloud_alarm ;

			//maybe clear alarm_dev_serial's groupAlarm and groupRelay here
			check_and_renew_groupAlarmList(p_dev_node,p_cmd_node->cloud_alarm,p_cmd_node->alarm_dev_serial) ;
			check_and_renew_groupRelayList(p_dev_node,p_cmd_node->cloud_relay,p_cmd_node->alarm_dev_serial) ;
			
			p_handling->send_count++ ;

			return 0 ;
			
		}
		
		if(productID != RELAY_INSTRUMENT_ID)
		{
			// handle alarm
			//renew handling dev info
			if( ((p_dev_node->device_info.dev_remoteAlarm_info & 0x07) & p_cmd_node->cloud_alarm) ==  p_cmd_node->cloud_alarm  )
			{
				//alarm success
				debug("dev=%s groupAlarm success \n",p_dev_node->device_info.dev_serialNum);
				p_cmd_node->success_cnt++ ;
				p_cmd_node->fin_dev_num++ ;
				p_handling->groupAlarm_result = SET_SUCCESS ;
			}	
			else
			{
				
				if(p_handling->send_count > MAX_CLOUD_GROUPCMD_TRY)
				{
					debug("dev=%s groupAlarm failed,sendcnt=%d \n",p_dev_node->device_info.dev_serialNum,p_handling->send_count);
					p_cmd_node->fail_cnt++ ;
					p_cmd_node->fin_dev_num++ ;
					p_handling->groupAlarm_result = SET_FAILURE ;
					
				}
				else
				{
					p_dev_node->device_info.total_groupAlarm |=  p_cmd_node->cloud_alarm ;
					p_handling->send_count++ ;
				}
			}
		}
		else
		{
			
			//handle relay,relay1-relay5---bit0-bit4,success when bank0 or bank1 ok
			if( (bank0_support && ((p_dev_node->device_info.relay_bank0 & 0x1F) &  p_cmd_node->cloud_relay) ==  p_cmd_node->cloud_relay) || 
				 (bank1_support && ((p_dev_node->device_info.relay_bank1 & 0x1F) &  p_cmd_node->cloud_relay) ==  p_cmd_node->cloud_relay) )
			{
				//relay success
				debug("this relay_dev bank status:bank0=0x%x,bank1=0x%x \n",p_dev_node->device_info.dio_bank0_status,p_dev_node->device_info.dio_bank1_status);
				debug("dev=%s groupRelay success,bank0_relay=0x%x,bank1_relay=0x%x \n",p_dev_node->device_info.dev_serialNum,p_dev_node->device_info.relay_bank0,
																						p_dev_node->device_info.relay_bank1);
				p_cmd_node->success_cnt++ ;
				p_cmd_node->fin_dev_num++ ;
				p_handling->groupAlarm_result = SET_SUCCESS ;
			}
			else
			{
				if(p_handling->send_count > MAX_CLOUD_GROUPCMD_TRY)
				{
					debug("dev=%s groupRelay failed,sendcnt=%d \n",p_dev_node->device_info.dev_serialNum,p_handling->send_count);
					p_cmd_node->fail_cnt++ ;
					p_cmd_node->fin_dev_num++ ;
					p_handling->groupAlarm_result = SET_FAILURE ;
					
				}
				else
				{
					p_dev_node->device_info.total_groupRelay |=  p_cmd_node->cloud_relay ;
					p_handling->send_count++ ;
				}
			}
		}

		return 0 ;
		
	}


	//malloc handling node and insert into handling-list
	p_handling = malloc(sizeof(handling_dev_node_t)) ;
	if(p_handling == NULL)
	{
		debug("err malloc handling failed when handle_groupAlarmCmd_node \n");
		return -1 ;
	}
	memset(p_handling,0,sizeof(handling_dev_node_t));
	strlcpy(p_handling->dev_sn,p_dev_node->device_info.dev_serialNum,sizeof(p_handling->dev_sn));

	//insert into list
	insert_groupAlarmHandling_devNode(p_cmd_node,p_handling) ;

	// check dev support remote alarm-relay or not
	if(productID != RELAY_INSTRUMENT_ID)
	{
		// support remote or not
		if( (p_dev_node->device_info.dev_remoteAlarm_info & 0x80) == 0 )
		{
			p_cmd_node->fail_cnt++ ;
			p_cmd_node->fin_dev_num++ ;
			p_handling->groupAlarm_result = NOT_SUPPORT ;

			debug("dev=%s not support remote_alarm when handle_groupAlarmCmd_node \n",p_dev_node->device_info.dev_serialNum);
			return 0 ;
		}
	}
	else
	{
		//support relay or not
		if( (p_dev_node->device_info.hw_info & 0x20) == 0 ) // bit5, DIO not enable  
		{	
			//not support relay
			p_cmd_node->fail_cnt++ ;
			p_cmd_node->fin_dev_num++ ;
			p_handling->groupAlarm_result = NOT_SUPPORT ; 

			debug("dev=%s not support remote_relay when handle_groupAlarmCmd_node \n",p_dev_node->device_info.dev_serialNum);
			return 0 ;
		}

		if( (p_dev_node->device_info.hw_info & 0x10) == 0 && (p_dev_node->device_info.hw_info & 0x08) == 0) // bit4 and bit3 not DO
		{
			//not support relay
			p_cmd_node->fail_cnt++ ;
			p_cmd_node->fin_dev_num++ ;
			p_handling->groupAlarm_result = NOT_SUPPORT ; 

			debug("dev=%s not support remote_relay when handle_groupAlarmCmd_node \n",p_dev_node->device_info.dev_serialNum);
			return 0 ;
		}
			
	}

	//append groupAlarm,groupRelay into dev total_groupAlarm,total_groupRelay
	p_dev_node->device_info.total_groupRelay |= p_cmd_node->cloud_relay ;
	p_dev_node->device_info.total_groupAlarm |= p_cmd_node->cloud_alarm ;

	//maybe clear alarm_dev_serial's groupAlarm and groupRelay here
	check_and_renew_groupAlarmList(p_dev_node,p_cmd_node->cloud_alarm,p_cmd_node->alarm_dev_serial) ;
	check_and_renew_groupRelayList(p_dev_node,p_cmd_node->cloud_relay,p_cmd_node->alarm_dev_serial) ;
	
	p_handling->send_count++ ;

	return 0 ;
}

int query_and_handle_groupAlarmCmdList(device_node_info_t *p_dev_node)
{
	cloud_groupAlarm_cmdNode_t *p_list = NULL ;
	cloud_groupAlarm_cmdNode_t *p_temp = NULL ;
	uint16_t productID = 0 ;
	int count = 0 ;
	handling_dev_node_t * p_handling = NULL ;
	

	//get cloud-groupAlarmCmd
	read_data_from_cloud_groupalram() ;
	
	if(p_dev_node == NULL || p_dev_node->device_info.dev_serialNum[0] == 0) //null pointer or null serial
	{
		debug("err para or null-serial when query_and_handle_groupAlarmCmdList \n");
		return -1 ;
	}

	

	if(p_cloud_groupAlarm_list == NULL)
	{
		//not exist cloud groupAlarm
		debug("not exist cloud Group-alarm \n");
		return -2 ;
	}
	else
	{
		//exist groupAlarm cmd,close local remoteAlarm triger if necessary,and send conf data to cloud now
		if(wsn_net_conf_data.trig_alarm != 0 || wsn_net_conf_data.trig_relay != 0)
		{
			//nvram set now
			wsn_net_conf_data.trig_alarm = 0;
			wsn_net_conf_data.trig_relay = 0 ;
			nvram_set_trigRelayAlarm_conf();
			usleep(300*1000) ; //300 ms
			//send conf data to cloud now
			send_cloud_gateway_cfg_chg_trig_alarm();
		}
		
	}

	p_list = p_cloud_groupAlarm_list;
	
	while(1)
	{
		p_temp = query_groupAlarmCmdNode_bydevSn(p_list,p_dev_node->device_info.dev_serialNum) ;
		if(p_temp != NULL)
		{
			handle_groupAlarmCmd_node(p_temp,p_dev_node);

			debug("handle groupAlarm msgID=%.32s,alarm_val=0x%x,relay_val=0x%x,success=%d,fail=%d,fin=%d,targetNum=%d \n",p_temp->msgID,
																							p_temp->cloud_alarm,p_temp->cloud_relay,
																							p_temp->success_cnt,p_temp->fail_cnt,
																							p_temp->fin_dev_num,p_temp->target_dev_num);
			p_list = p_temp->p_next_cmd ; //handle next cmdNode
			if(p_temp->fin_dev_num >= p_temp->target_dev_num || (p_temp->fail_cnt+p_temp->success_cnt) >= p_temp->target_dev_num )
			{
				#if 0
				//for debug
				debug("this cmdNode targetNum=%d,success=%d,fail=%d \n",p_temp->target_dev_num,p_temp->success_cnt,p_temp->fail_cnt) ;
				p_handling = p_temp->p_handled_dev_list ;
				while(p_handling != NULL)
				{
					debug("handling dev=%s,result=%d \n",p_handling->dev_sn,p_handling->groupAlarm_result) ;
					p_handling = p_handling->p_next_node ;
				}
				////end
				#endif
				
				//send resp to cloud now
				build_and_send_groupAlarmCmd_resp(p_temp) ;

				//free cmd_node
				free_groupAlarmNode_byMsgIDandAlarmSn(p_temp) ;
			}
			
			continue ;
		}
		else
		{
			break ;
		}
	}

	// triger when cloud_alarm(or cloud_relay) != dev->groupAlarm
	debug("before triger groupAlarm,dev_remoteAlarm=0x%x,groupAlarm=0x%x \n",p_dev_node->device_info.dev_remoteAlarm_info,p_dev_node->device_info.total_groupAlarm);
	triger_specifiedDev_cloudAlarm(p_dev_node) ;

	return 0 ;
}



void clear_digaInfo_inNvram(void)
{
	init_device_info_list(); //init onlineNum==0
	nvram_update_online_devNum(); // 0
	nvram_delete_dev_diaginfo_list();
	nvram_update_devNode_num(0);

	return ;
}

int insert_into_subSenData_listEnd(sensor_data_t *p_list,sensor_data_t*p_sub_list)
{
	sensor_data_t *p_temp = NULL ;
	if(p_list == NULL || p_sub_list == NULL)
	{
		return -1 ;
	}

	p_temp = p_list ;
	while(p_temp != NULL)
	{
		if(p_temp->p_next_sensor_data == NULL)
		{
			p_temp->p_next_sensor_data = p_sub_list ;
			break ;
		}
		
		p_temp = p_temp->p_next_sensor_data ;
	}

	return 0 ;
}


dev_subDataPak_list_t *query_subSenData_list(device_node_info_t *p_node)
{
	dev_subDataPak_list_t *p_temp = NULL ;
	
	if(p_node == NULL)
	{
		debug("err para when query_subSenData_list \n");
		return NULL ;
	}

	p_temp = p_dev_subDataPak_list ;

	while(p_temp != NULL)
	{
		if(strcmp(p_temp->dev_sn,p_node->device_info.dev_serialNum) == 0)
		{
			//found match node
			return p_temp ;
		}

		p_temp = p_temp->p_next ;
	}

	return NULL ;
}


int insert_subSenData_list(sensor_data_t *p_sen_data,uint8_t sen_number,device_node_info_t *p_node)
{
	dev_subDataPak_list_t *p_temp = NULL ;
	dev_subDataPak_list_t *p_current = NULL ;
	sensor_data_t *p_sen_data_end = NULL ;
	sensor_data_t *p_senData_tmp = NULL ;
	
	if(p_sen_data == NULL || p_node == NULL)
	{	
		debug("err para when insert_subSenData_list \n");
		return -1 ;
	}

	if(p_node->device_info.dev_serialNum[0] == 0)
	{
		debug("err dev serial when insert_subSenData_list \n");
		return -1 ;
	}

	p_temp = query_subSenData_list(p_node) ;
	if(p_temp != NULL)
	{
		p_sen_data_end = p_temp->p_sen_data_list ;
		if(p_sen_data_end == NULL)
		{
			p_temp->p_sen_data_list = p_sen_data ;
			//set senData recvNO
			p_node->device_info.sen_data_recvNO = sen_number ;
		}
		else
		{
			p_senData_tmp = query_sensorData_bySenIndex(p_sen_data_end,p_sen_data->sensor_index) ;
			if(p_senData_tmp != NULL)
			{
				//sen_data han been in the list,renew it and free old senData list
				debug("senData has been in the list,renew it now \n") ;
				free_specified_sensorData_memList(p_sen_data_end) ;
				//renew the dev senData list
				p_temp->p_sen_data_list = p_sen_data ;
				p_node->device_info.sen_data_recvNO = sen_number ;
			}
			else
			{
				//compare first node's sensor_index
				if(p_sen_data->sensor_index < p_sen_data_end->sensor_index )
				{
					//insert into list head
					p_temp->p_sen_data_list = p_sen_data ;
					//append old sen-data-list into the list end
					insert_into_subSenData_listEnd(p_sen_data,p_sen_data_end);
				}
				else
				{
					//append into the list end
					insert_into_subSenData_listEnd(p_sen_data_end,p_sen_data) ;
				}
				
				p_node->device_info.sen_data_recvNO += sen_number ; 
				debug("success to append sub sen_data_list \n");
				
			}
		}

		return 0 ;
		
	}

	// not found match dev subSenData node,create now
	debug("not found match dev sub-data-pak node,create now \n");
	p_current = malloc(sizeof(dev_subDataPak_list_t)) ;
	if(p_current == NULL)
	{
		debug("malloc failed when insert_subSenData_list \n");
		return -1 ;
	}
	memset(p_current,0,sizeof(dev_subDataPak_list_t));
	strncpy(p_current->dev_sn,p_node->device_info.dev_serialNum,sizeof(p_current->dev_sn)-1) ;
	p_current->p_sen_data_list = p_sen_data ;
	p_current->p_next = NULL ;

	//insert into the subData_pak list end 
	if(p_dev_subDataPak_list == NULL)
	{
		p_dev_subDataPak_list = p_current ;
	}
	else
	{
		p_temp = p_dev_subDataPak_list ;
		while(p_temp != NULL)
		{
			if(p_temp->p_next == NULL)
			{
				p_temp->p_next = p_current ;
				break ;
			}

			p_temp = p_temp->p_next ;
		}
	}

	//set the senData_recvNO
	p_node->device_info.sen_data_recvNO = sen_number ;
	

	return 0 ;
}


int free_subSenData_node( uint8_t *p_dev_sn)
{
	dev_subDataPak_list_t *p_current = NULL ;
	dev_subDataPak_list_t *p_prev = NULL ;
	
	if(p_dev_sn == NULL )
	{
		debug("err para when free_subSenData_node \n");
		return -1 ;
	}


	p_current = p_dev_subDataPak_list ;
	while(p_current != NULL)
	{
		if(strcmp(p_current->dev_sn,p_dev_sn) == 0)
		{
			//found match node,detach the node from the list
			if(p_prev == NULL)
			{
				//first node
				p_dev_subDataPak_list = p_current->p_next ;
			}
			else
			{
				p_prev->p_next = p_current->p_next ;
			}

			//free sen-data list in this node
			if(p_current->p_sen_data_list != NULL)
			{
				free_specified_sensorData_memList(p_current->p_sen_data_list) ;
			}

			//free senSubData node
			free(p_current) ;
			
			
			break ;
		}

		p_prev = p_current ;
		p_current = p_prev->p_next ;
	}

	return 0 ;
	
}

int free_subSenData_list(void)
{
	dev_subDataPak_list_t *p_current = NULL ;
	dev_subDataPak_list_t *p_next = NULL ;

	p_current = p_dev_subDataPak_list ;

	while(p_current != NULL)
	{
		p_next = p_current->p_next ;
		
		//free sen-data list in this node
		if(p_current->p_sen_data_list != NULL)
		{
			free_specified_sensorData_memList(p_current->p_sen_data_list) ;
		}

		//free senSubData node
		free(p_current) ;

		p_current = p_next ;
	}

	return 0 ;
}


char* wsn_strdup(const char* str)
{
    size_t len;
    char* copy;

    len = strlen(str) + 1;
    if (!(copy = (char*)malloc(len)))
    {
        return 0;
    }
    memcpy(copy, str, len);

    return copy;
}

int save_in_bigEndian(uint8_t *p_dest,uint8_t *p_src,uint8_t data_len,uint8_t data_type)
{
	int count = 0 ;
	
	if(p_dest == NULL || p_src == NULL || data_len <= 0)
	{
		debug("err para when save_in_bigEndian \n");
		return -1 ;
	}

	if(data_type == DATA_TYPE_STR)
	{
		memcpy(p_dest,p_src,data_len);
	}
	else
	{
		for(count=0;count<data_len;count++)
		{
			p_dest[count] = p_src[data_len-1-count] ;
		}
	}

	return 0 ;
}

int send_json_permissionData_toCloud(device_node_info_t *p_node)
{
	proto_basicinfo_t out_data ;
	dc_payload_info_t dc_payload_info ;
	cJSON *p_temp = NULL ;
	cJSON *p_dataObj = NULL ;
	cJSON *p_dev_array = NULL ;
	cJSON *p_dev_objMember = NULL ;
	uint8_t product_id[20] ;
	uint32_t productID = 0 ;
	uint8_t product_name[32] ;
	long long int time_val = 0 ;

	if(p_node == NULL)
	{
		debug("err para when send_json_permissionData_toCloud \n");
		return -1 ;
	}

	debug("begin to send chkPermission to cloud \n");
	time_val = get_utcms();

	strcpy(dc_payload_info.messageType,"upload");
	strcpy(dc_payload_info.flags,"request");
	strcpy(dc_payload_info.agentVerison,"1.0") ;
	strcpy(dc_payload_info.action,MSG_ACTION_UP_CHKPERMISSONDATA) ;

	strlcpy(dc_payload_info.dev_serinum,p_node->device_info.dev_serialNum,sizeof(dc_payload_info.dev_serinum)) ;
	dc_payload_info.dev_configv = p_node->device_info.cfgSet_cnt ;
	//dc_payload_info.dev_configv = time_val ;
	dc_payload_info.retry =0 ;

	//root node
	p_dataObj = cJSON_CreateObject() ;
	if(p_dataObj == NULL)
	{
		debug("err create dataObj when send_json_confData_toCloud \n");
		return -1 ;
	}

	dc_payload_info.data_json = p_dataObj ;

	//add device array
	p_dev_array = cJSON_CreateArray() ;
	if(p_dev_array == NULL)
	{
		debug("err create device_array when  send_json_permissionData_toCloud \n");
		if(p_dataObj != NULL)
		{
			cJSON_Delete(p_dataObj) ;
		}
		return -1 ;
	}
	cJSON_AddItemToObject(p_dataObj,"device",p_dev_array) ;

	//add device object into device_array
	p_dev_objMember = cJSON_CreateObject() ;
	if(p_dev_objMember == NULL)
	{
		debug("err create device obj_item when  send_json_permissionData_toCloud \n");
		if(p_dataObj != NULL)
		{
			cJSON_Delete(p_dataObj) ;
		}
		return -1 ;
	}
	cJSON_AddItemToArray(p_dev_array,p_dev_objMember) ;

	//add serialNo into p_dev_objMember
	p_temp = cJSON_CreateString(p_node->device_info.dev_serialNum) ;
	if(p_temp == NULL)
	{
		debug("err create serialNo when  send_json_permissionData_toCloud \n");
		if(p_dataObj != NULL)
		{
			cJSON_Delete(p_dataObj) ;
		}
		return -1 ;
	}
	cJSON_AddItemToObject(p_dev_objMember,"serialNo",p_temp) ;

	//add prdId into p_dev_objMember
	productID = (p_node->device_info.instr_family_id << 24) | (p_node->device_info.instr_model_id << 16 )
					| (p_node->device_info.instr_hw_id << 8) | p_node->device_info.instr_fw_id ;
	productID = productID & 0xFFFF0000 ;
	#if 0
	memset(product_id,0,sizeof(product_id));
	sprintf(product_id,"%d%d%d%d",p_node->device_info.instr_family_id,p_node->device_info.instr_model_id,
										p_node->device_info.instr_hw_id,p_node->device_info.instr_fw_id);
	p_temp = cJSON_CreateString(product_id) ;
	#endif	
	p_temp = cJSON_CreateNumber(productID) ;
	if(p_temp == NULL)
	{
		debug("err create prdId when  send_json_permissionData_toCloud \n");
		if(p_dataObj != NULL)
		{
			cJSON_Delete(p_dataObj) ;
		}
		return -1 ;
	}
	cJSON_AddItemToObject(p_dev_objMember,"prdId",p_temp) ;

	//add prdName into p_dev_objMember
	memset(product_name,0,sizeof(product_name));
	debug("query productName,productid= 0x%02x %d\n",productID,productID) ;
	query_instrument_tab(productID,product_name,sizeof(product_name)-1 ) ;
	if(product_name[0] != 0)
	{
		p_temp = cJSON_CreateString(product_name) ;
	}
	else
	{
		p_temp = cJSON_CreateString("NULL") ;
	}
	if(p_temp == NULL)
	{
		debug("err create prdName when  send_json_permissionData_toCloud \n");
		if(p_dataObj != NULL)
		{
			cJSON_Delete(p_dataObj) ;
		}
		return -1 ;
	}
	cJSON_AddItemToObject(p_dev_objMember,"prdName",p_temp) ;


	//send to cloud
	mqtt_wrapper_send_uploadData(&dc_payload_info,&out_data,QOS1);
	//mem be freeed when mqtt_wrapper_send_uploadData

	return 0 ;
}


int send_json_confData_toCloud(device_node_info_t *p_node)
{
	proto_basicinfo_t out_data ;
	dc_payload_info_t dc_payload_info ;
	cJSON *p_dataObj = NULL ;
	long long int time_val = 0 ;

	if(p_node == NULL)
	{
		debug("err para when send_json_confData_toCloud \n") ;
		return -1 ;
	}

	send_json_permissionData_toCloud(p_node) ;
	usleep(200*1000) ; //200 ms
	
	debug("begin to send dev_config data to cloud now \n");
	time_val = get_utcms();
	
	if(p_config_jsonData != NULL)
	{
		//send to cloud
		strcpy(dc_payload_info.messageType,"upload");
		strcpy(dc_payload_info.flags,"request");
		strcpy(dc_payload_info.agentVerison,"1.0") ;
		strcpy(dc_payload_info.action,MSG_ACTION_UP_DEV_CFG) ;

		strlcpy(dc_payload_info.dev_serinum,p_node->device_info.dev_serialNum,sizeof(dc_payload_info.dev_serinum)) ;
		dc_payload_info.dev_configv = p_node->device_info.cfgSet_cnt ;
		//dc_payload_info.dev_configv = time_val ;
		dc_payload_info.retry =0 ;
		
		//build root data-obj
		p_dataObj = cJSON_CreateObject() ;
		if(p_dataObj == NULL)
		{
			debug("err create dataObj when send_json_confData_toCloud \n");
			if(p_config_jsonData != NULL)
			{
				cJSON_Delete(p_config_jsonData) ;
				p_config_jsonData = NULL ;
			}
		}
		cJSON_AddItemToObject(p_dataObj,"config",p_config_jsonData) ;

		dc_payload_info.data_json = p_dataObj ;
		
		mqtt_wrapper_send_uploadData(&dc_payload_info,&out_data,QOS1);
		
		//free mem
		//need not to free,it has been freeed when mqtt_wrapper_send_uploadData 
		//cJSON_Delete(p_config_jsonData) ;
		p_config_jsonData = NULL ;
	}

	return 0 ;
}

int build_json_config_data(device_node_info_t *p_node)
{
	cJSON *p_temp = NULL ;
	cJSON *p_dev_info = NULL ;
	
	if(p_node == NULL)
	{
		debug("err input_para when build_json_config_data \n");
		return -1 ;
	}

	//create config array(object array)
	p_config_jsonData = cJSON_CreateArray() ;
	if(p_config_jsonData == NULL)
	{
		debug("err cJSON_CreateArray when build_json_config_data \n");
		return -1 ;
	}

	//set config array_name
	p_config_jsonData->string = wsn_strdup("config") ;
	
	//build dev_info object
	p_dev_info = build_devInfo_jsonData(p_node);
	if(p_dev_info == NULL )
	{
		debug("err build_devInfo_jsonData when build_json_config_data \n");
		cJSON_Delete(p_config_jsonData) ;
		return -1 ;
	}

	//add dev_info object into p_config array
	cJSON_AddItemToArray(p_config_jsonData,p_dev_info) ;

	
	//maybe we can add more devinfo ?
	//dev_info[2]?

	return 0 ;
	
}


cJSON *build_devInfo_jsonData(device_node_info_t *p_node)
{
	cJSON *p_dev_info = NULL ;
	cJSON *p_temp = NULL ;
	uint8_t product_id[20] ;
	uint32_t productID = 0 ;
	uint8_t product_name[32] ;
	long long int time_val = 0 ;
	static uint8_t cfg_cnt = 23 ;


	if(p_node == NULL)
	{
		debug("err para when build_devInfo_jsonData \n");
		return NULL ;
	}

	//create dev_info object
	p_dev_info = cJSON_CreateObject();
	if(p_dev_info == NULL )
	{
		debug("err create dev-info when build_devInfo_jsonData \n");
		//cJSON_Delete(p_config_jsonData) ;
		return NULL ;
	}
	p_dev_info->string = wsn_strdup("DevInfo") ;

	//new,add sentime
	time_val = get_utcms();
	p_temp = cJSON_CreateNumber(time_val) ;
	if(p_temp == NULL)
	{
		debug("err create sendtime when  build_devInfo_jsonData \n");
		cJSON_Delete(p_dev_info) ;
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_info,"time",p_temp) ;
	

	//begin to add items into dev_info object now

	//add configV item
	time_val = get_utcms();
	//p_temp = cJSON_CreateNumber(p_node->device_info.cfgSet_cnt) ;  
	p_temp = cJSON_CreateNumber(time_val) ;
	if(p_temp == NULL)
	{
		debug("err create configV when  build_devInfo_jsonData \n");
		cJSON_Delete(p_dev_info) ;
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_info,"configV",p_temp) ;

	//add serialNo
	p_temp = cJSON_CreateString(p_node->device_info.dev_serialNum) ;
	if(p_temp == NULL)
	{
		debug("err create serialNo when  build_devInfo_jsonData \n");
		cJSON_Delete(p_dev_info) ;
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_info,"serialNo",p_temp) ;

	//add prdId
	productID = (p_node->device_info.instr_family_id << 24) | (p_node->device_info.instr_model_id << 16 )
					| (p_node->device_info.instr_hw_id << 8) | p_node->device_info.instr_fw_id ;
	productID = productID & 0xFFFF0000 ;
	#if 0
	memset(product_id,0,sizeof(product_id)) ;
	sprintf(product_id,"%d%d%d%d",p_node->device_info.instr_family_id,p_node->device_info.instr_model_id,
										p_node->device_info.instr_hw_id,p_node->device_info.instr_fw_id);
	p_temp = cJSON_CreateString(product_id) ;
	#endif
	p_temp = cJSON_CreateNumber(productID);
	if(p_temp == NULL)
	{
		debug("err create prdId when  build_devInfo_jsonData \n");
		cJSON_Delete(p_dev_info) ;
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_info,"prdId",p_temp) ;

	//add prdName
	memset(product_name,0,sizeof(product_name));
	debug("query productName,productid= 0x%02x %d\n",productID,productID) ;
	query_instrument_tab(productID,product_name,sizeof(product_name)-1 ) ;
	if(product_name[0] != 0)
	{
		p_temp = cJSON_CreateString(product_name) ;
	}
	else
	{
		p_temp = cJSON_CreateString("NULL") ;
	}
	if(p_temp == NULL)
	{
		debug("err create prdName when  build_devInfo_jsonData \n");
		cJSON_Delete(p_dev_info) ;
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_info,"prdName",p_temp) ;

	//add fwV
	p_temp = cJSON_CreateString(p_node->device_info.dev_fw_ver) ;
	if(p_temp == NULL)
	{
		debug("err create fw_ver when  build_devInfo_jsonData \n");
		cJSON_Delete(p_dev_info) ;
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_info,"fwV",p_temp) ;

	//add powerType,not found powerType-info in dev-info,cann't use power-status in data-pak
	p_temp = cJSON_CreateNumber(0) ;
	if(p_temp == NULL)
	{
		debug("err create powerType when  build_devInfo_jsonData \n");
		cJSON_Delete(p_dev_info) ;
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_info,"powerType",p_temp) ;

	//add commandFunction
	p_temp = build_dev_cmdFunc_item(p_node) ;
	if(p_temp == NULL)
	{
		p_temp = cJSON_CreateNull() ;
	}
	if(p_temp == NULL)
	{
		debug("err create commandFunction when  build_devInfo_jsonData \n");
		cJSON_Delete(p_dev_info) ;
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_info,"commandFunction",p_temp) ;

	//add unitId,no unitID in dev-info
	//p_temp = cJSON_CreateNull() ;
	p_temp = cJSON_CreateNumber(p_node->device_info.device_addr) ;
	if(p_temp == NULL)
	{
		debug("err create unitId when  build_devInfo_jsonData \n");
		cJSON_Delete(p_dev_info) ;
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_info,"unitId",p_temp) ;

	//add sensorInfo
	if(p_node->device_info.sensor_mask != 0)
	{
		p_temp = build_sensorfmt_jsonData(p_node) ;
		if(p_temp == NULL)
		{
			debug("err build_sensorfmt_jsonData when build_devInfo_jsonData \n");
		}
		else
		{
			cJSON_AddItemToObject(p_dev_info,"sensorInfo",p_temp) ;
		}
	}
	

	return p_dev_info ;
}


cJSON *build_sensorfmt_jsonData(device_node_info_t *p_node)
{
	cJSON *p_sen_fmt = NULL ;
	cJSON *p_temp = NULL ;
	int  count = 0 ;
	uint8_t sensor_str[20] ;

	if(p_node == NULL)
	{
		debug("err para when build_sensorfmt_jsonData \n");
		return NULL ;
	}

	if(p_node->device_info.active_sensor_num)
	{
		debug("dev:%s active sensor num=%d \n",p_node->device_info.dev_serialNum,p_node->device_info.active_sensor_num) ;
		p_sen_fmt = cJSON_CreateArray() ;
		if(p_sen_fmt == NULL)
		{
			debug("err create sensor fmt array when build_sensorfmt_jsonData \n") ;
			return NULL ;
		}
		
		for(count=1;count<=p_node->device_info.active_sensor_num;count++)
		{
			p_temp = cJSON_CreateObject();
			if(p_temp == NULL)
			{
				debug("err create sensor fmt obj when build_sensorfmt_jsonData \n");
				cJSON_Delete(p_sen_fmt) ;
				return NULL ;
			}
			insert_sensor_jsonFmtDataObj(p_temp,p_node,p_node->device_info.active_sen_index[count-1]);

			memset(sensor_str,0,sizeof(sensor_str)) ;
			sprintf(sensor_str,"sensorInfo_%d",count);
			//cJSON_AddItemToArray(p_sen_fmt,sensor_str,p_temp) ;
			cJSON_AddItemToArray(p_sen_fmt,p_temp) ;
		}
	}
	
	
	return p_sen_fmt ;
}


// 1--surport ascII-msg ; 2--surport unicode-msg ; 3---surport wake-sleep
cJSON * build_dev_cmdFunc_item(device_node_info_t *p_node)
{
	cJSON *p_cmdFunc_item = NULL ;
	cJSON *p_temp = NULL ;
	
	if(p_node == NULL)
	{
		debug("err para when build_dev_cmdFunc_item \n");
		return NULL ;
	}

	//for debug
	//p_node->device_info.function_info = 0x07;
	
	if(p_node->device_info.function_info & 0x07 ) //bit0-bit2
	{
		p_cmdFunc_item = cJSON_CreateArray() ;
		if(p_cmdFunc_item == NULL )
		{
			debug("err create p_cmdFunc_item when build_dev_cmdFunc_item \n") ;
			return NULL ;
		}

		//bit0 --surport ascii-msg or not ?
		if(p_node->device_info.function_info & 0x01) 
		{
			p_temp = cJSON_CreateNumber(1) ;
			if(p_temp != NULL)
			{
				cJSON_AddItemToArray(p_cmdFunc_item,p_temp) ;
			}
			else
			{
				debug("err create assII-msg member when build_dev_cmdFunc_item \n");
				cJSON_Delete(p_cmdFunc_item) ;
				return NULL ;
			}
		}

		//bit1 --surport wakeup-sleep or not
		if(p_node->device_info.function_info & 0x02) 
		{
			p_temp = cJSON_CreateNumber(3) ;
			if(p_temp != NULL)
			{
				cJSON_AddItemToArray(p_cmdFunc_item,p_temp) ;
			}
			else
			{
				debug("err create wake-sleep member when build_dev_cmdFunc_item \n");
				cJSON_Delete(p_cmdFunc_item) ;
				return NULL ;
			}
		}

		//bit2 --surport wakeup-sleep or not
		if(p_node->device_info.function_info & 0x04) 
		{
			p_temp = cJSON_CreateNumber(2) ;
			if(p_temp != NULL)
			{
				cJSON_AddItemToArray(p_cmdFunc_item,p_temp) ;
			}
			else
			{
				debug("err create surport-unicode member when build_dev_cmdFunc_item \n");
				cJSON_Delete(p_cmdFunc_item) ;
				return NULL ;
			}
		}


		return p_cmdFunc_item ;
		
	}
	else
	{
		debug("dev:%s func_val=0x%x not support cmdFunc \n",p_node->device_info.dev_serialNum,p_node->device_info.function_info);
	}

	return NULL ;
}


int insert_sensor_jsonFmtDataObj(cJSON *p_sen_fmtObj,device_node_info_t *p_node,uint8_t current_sen_index)
{
	cJSON *p_temp = NULL ;
	sensor_data_format_t *p_sen_fmt_data = NULL ;
	uint8_t sen_name[MAX_SENSOR_NAME_LEN] ;
	uint8_t measure_unitStr[MAX_MEASURE_UNIT_STRLEN] ;
	uint8_t gasName_str[MAX_GAS_NAME_LEN] ;
	int ret_val = 0 ;

	if(p_sen_fmtObj == NULL || p_node == NULL || current_sen_index >= 16)
	{
		debug("err para when insert_sensor_jsonFmtDataObj \n");
		return -1 ;
	}

	//add idx
	p_temp = cJSON_CreateNumber(p_node->device_info.active_sen_index[current_sen_index]) ;
	if(p_temp == NULL)
	{
		debug("err create idx when insert_sensor_jsonFmtDataObj \n");
		return -1 ;
	}
	cJSON_AddItemToObject(p_sen_fmtObj,"idx",p_temp);

	//add name (sensorID?)
	p_sen_fmt_data = query_sensor_format_data(p_node->device_info.device_addr,current_sen_index) ;
	if(p_sen_fmt_data == NULL)
	{
		debug("err not found index=%d senfmt data \n",current_sen_index);
		return -1 ;
	}
	memset(sen_name,0,sizeof(sen_name)) ;
	ret_val = get_sensorName_bysenIDandSubID(p_sen_fmt_data->sensor_id,p_sen_fmt_data->sensor_sub_id,sen_name,(sizeof(sen_name)-1) );
	if(ret_val != 0 || sen_name[0] == 0)
	{
		debug("not found sen name idx=%d when insert_sensor_jsonFmtDataObj\n",current_sen_index) ;
		return -1 ;
	}
	p_temp = cJSON_CreateString(sen_name) ;
	if(p_temp == NULL)
	{
		debug("err create sen name when insert_sensor_jsonFmtDataObj \n");
		return -1 ;
	}
	cJSON_AddItemToObject(p_sen_fmtObj,"name",p_temp);

	//add unit
	memset(measure_unitStr,0,sizeof(measure_unitStr)) ;
	ret_val = get_sensor_measurement_unit(p_sen_fmt_data->unit_id,measure_unitStr,sizeof(measure_unitStr)-1 );
	if(ret_val != 0 || measure_unitStr[0] == 0)
	{
		debug("not found sen measurement unit idx=%d when insert_sensor_jsonFmtDataObj\n",current_sen_index) ;
		return -1 ;
	}
	p_temp = cJSON_CreateString(measure_unitStr) ;
	if(p_temp == NULL)
	{
		debug("err create sen name when insert_sensor_jsonFmtDataObj \n");
		return -1 ;
	}
	cJSON_AddItemToObject(p_sen_fmtObj,"unit",p_temp);

	//add vocGasName
	memset(gasName_str,0,sizeof(gasName_str)) ;
	if(strcmp(sen_name,"VOC") == 0)
	{
		//match,query gas name from g0_gas tab,return gasName or VOC
		query_gas_name_bySensorFmtGasID(p_sen_fmt_data->gas_id,gasName_str,sizeof(gasName_str));
	}
	else
	{
		//not VOC sensor,gasName==sensorName
		strlcpy(gasName_str,sen_name,sizeof(gasName_str));
	}
	////////////////////////////////////////////
	p_temp = cJSON_CreateString(gasName_str) ;
	if(p_temp == NULL)
	{
		debug("err create sen name when insert_sensor_jsonFmtDataObj \n");
		return -1 ;
	}
	cJSON_AddItemToObject(p_sen_fmtObj,"vocGasName",p_temp);


	//add limitInfo
	insert_sensor_jsonLimitInfo(p_sen_fmtObj,p_node,current_sen_index);
	

	return 0 ;
}

int insert_sensor_jsonLimitInfo(cJSON *p_sen_fmtObj,device_node_info_t *p_node,uint8_t current_sen_index)
{
	cJSON *p_temp = NULL ;
	sensor_data_format_t *p_sen_fmt_data = NULL ;
	
	if(p_sen_fmtObj == NULL || p_node == NULL )
	{
		debug("error para when insert_sensor_jsonLimitInfo \n");
		return -1 ;
	}

	p_sen_fmt_data = query_sensor_format_data(p_node->device_info.device_addr,current_sen_index) ;
	if(p_sen_fmt_data == NULL)
	{
		debug("err not found index=%d senfmt data \n",current_sen_index);
		return -1 ;
	}

	

	return 0 ;
}



int get_sensorName_bysenIDandSubID(uint8_t sensorID,uint8_t subID,uint8_t *p_sen_name_buf,uint8_t buf_len)
{
	if(p_sen_name_buf == NULL || buf_len <= 0)
	{
		debug("err para when get_sensorName_bysenIDandSubID \n");
		return -1 ;
	}

	query_sensor_name_type(sensorID,subID,p_sen_name_buf,buf_len,NULL,0) ;
	//debug
	//strcpy(p_sen_name_buf,"sensorName") ;

	return 0 ;
}

int get_sensor_measurement_unit(uint8_t unit_id,uint8_t *p_sen_unit_buf,uint8_t buf_len)
{
	
	if(p_sen_unit_buf == NULL || buf_len <= 0)
	{
		debug("err para when get_sensor_measurement_uint \n");
		return -1 ;
	}

	query_unitID_TAB(unit_id,p_sen_unit_buf,buf_len) ;
	//debug
	//strcpy(p_sen_unit_buf,"mg");
	
	return 0 ;
}

int get_sensor_gasName(uint8_t sen_index,uint8_t *p_gas_name_buf,uint8_t buf_len)
{
	if(p_gas_name_buf == NULL || buf_len <= 0)
	{
		debug("err para when get_sensor_gasName \n");
		return -1 ;
	}

	//debug
	strcpy(p_gas_name_buf,"no_gasName");
	
	return 0 ;
}
cJSON * bulid_topology_data(void)
{
	cJSON *p_dev_topologyObj = NULL ;
	cJSON *p_dev_alltopoModuleObj = NULL ;
	cJSON *p_dev_topoModuleArray = NULL ;
	cJSON *p_dev_topoModuleObj = NULL ;
	cJSON *p_dev_nodeArray = NULL ;
	cJSON *p_dev_nodeObj = NULL ;

	cJSON *p_dev_location = NULL ;
	cJSON *p_dev_instrumentAlarm = NULL ;
	cJSON *p_temp = NULL ;
	char product_name[32] ={0};
//topology
	p_dev_topologyObj = cJSON_CreateObject() ;
	if(p_dev_topologyObj == NULL)
	{
		debug("create p_dev_topologyObj err when bulid_topology_data\n");
		return NULL ;
	}
	p_dev_alltopoModuleObj = cJSON_CreateObject() ;
	if(p_dev_alltopoModuleObj == NULL)
	{
		debug("create p_dev_alltopoModuleObj err when bulid_topology_data \n");
		cJSON_Delete(p_dev_topologyObj) ;
		return NULL ;
	}
    cJSON_AddItemToObject(p_dev_topologyObj,"topology",p_dev_alltopoModuleObj) ;
	
	p_dev_topoModuleArray = cJSON_CreateArray();
	if(p_dev_topoModuleArray == NULL)
	{
		debug("create p_dev_topoModuleArray err when bulid_topology_data \n");
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_alltopoModuleObj,"topoModule",p_dev_topoModuleArray) ;

	p_dev_topoModuleObj = cJSON_CreateObject() ;
	if(p_dev_topoModuleObj == NULL)
	{
		debug("create p_dev_topoModuleObj err when bulid_topology_data \n");
		cJSON_Delete(p_dev_topologyObj) ;
		return NULL ;
	}
	//add into dev_realJsonData_array
	cJSON_AddItemToArray(p_dev_topoModuleArray,p_dev_topoModuleObj) ;

	//create nodelist array
	p_dev_nodeArray = cJSON_CreateArray();
	if(p_dev_nodeArray == NULL)
	{
		debug("create p_dev_nodelistArray array err when bulid_topology_data \n");
		return NULL ;
	}
	cJSON_AddItemToObject(p_dev_topoModuleObj,"nodeList",p_dev_nodeArray) ;
/////
	device_node_info_t *p_temp_device_info = NULL ;

	p_temp_device_info = device_info_list_head.p_next_device ;
	while(p_temp_device_info != NULL)
	{
		if((p_temp_device_info->device_info.dev_online_flag == 1) &&(p_temp_device_info->device_info.echoview_flag!=1)) //match
		{
			p_dev_nodeObj = cJSON_CreateObject() ;
			if(p_dev_nodeObj == NULL)
			{
				debug("create p_dev_nodeObj obj err when bulid_topology_data \n");
				cJSON_Delete(p_dev_topologyObj) ;
				return NULL ;
			}
			//add into dev_realJsonData_array
			cJSON_AddItemToArray(p_dev_nodeArray,p_dev_nodeObj) ;	
			//add serialNo
			p_temp = cJSON_CreateString(p_temp_device_info->device_info.dev_serialNum) ;
			if(p_temp == NULL)
			{
				debug("create serialNo json item err when bulid_topology_data \n");
				cJSON_Delete(p_dev_topologyObj) ;
				return NULL ;
			}
			//add into p_dev_nodeObj
			cJSON_AddItemToObject(p_dev_nodeObj,"sn",p_temp) ;
			//add dev addr
			char dev_addr_buffer[8]={0};
			sprintf(dev_addr_buffer,"%4X",p_temp_device_info->device_info.device_addr);

			p_temp = cJSON_CreateString(dev_addr_buffer) ;
			if(p_temp == NULL)
			{
				debug("create devaddr json item err when bulid_topology_data \n");
				cJSON_Delete(p_dev_topologyObj) ;
				return NULL ;
			}
			//add into p_dev_nodeObj
			cJSON_AddItemToObject(p_dev_nodeObj,"uniqueId",p_temp) ;

			uint32_t productID = (p_temp_device_info->device_info.instr_family_id << 24) | (p_temp_device_info->device_info.instr_model_id << 16 )
							| (p_temp_device_info->device_info.instr_hw_id << 8) | p_temp_device_info->device_info.instr_fw_id ;
			productID = productID & 0xFFFF0000 ;

			//add prdName into p_dev_objMember
			memset(product_name,0,sizeof(product_name));
			query_instrument_tab(productID,product_name,sizeof(product_name)-1 ) ;
			debug("query product_name %s,productid= 0x%x\n",product_name,productID) ;
			if(product_name[0] != 0)
			{
				p_temp = cJSON_CreateString(product_name) ;
			}
			else
			{
				p_temp = cJSON_CreateString("NULL") ;
			}
			if(p_temp == NULL)
			{
				debug("err create prdName when bulid_topology_data \n");
				cJSON_Delete(p_dev_topologyObj) ;
				return NULL;
			}
			cJSON_AddItemToObject(p_dev_nodeObj,"modelName",p_temp) ;
		// add parentSN
			p_temp = cJSON_CreateString(p_temp_device_info->device_info.parent_dev_serialNum) ;
			if(p_temp == NULL)
			{
				debug("create parent_dev_serialNum json item err when bulid_topology_data \n");
				cJSON_Delete(p_dev_topologyObj) ;
				return NULL ;
			}
			cJSON_AddItemToObject(p_dev_nodeObj,"parentSN",p_temp) ;

			char dev_rssi_val_buffer[8]={0};
			sprintf(dev_rssi_val_buffer,"%d",p_temp_device_info->device_info.dev_rssi_val);
			p_temp = cJSON_CreateString(dev_rssi_val_buffer) ;
			if(p_temp == NULL)
			{
				debug("create signal json item err when bulid_topology_data \n");
				cJSON_Delete(p_dev_topologyObj) ;
				return NULL ;
			}
			//add into p_dev_nodeObj
			cJSON_AddItemToObject(p_dev_nodeObj,"signal",p_temp) ;
		// end add
		}

		p_temp_device_info = p_temp_device_info->p_next_device ;
	}

////
	return p_dev_topologyObj;

}
//////////send_json_topology_toCloud//////////////
int send_json_topology_toCloud(void)
{
	proto_basicinfo_t out_data ;
	dc_payload_info_t dc_payload_info ;
	//long long int time_val = 0 ;
	cJSON *p_dev_topologyObj = NULL;
	//cJSON *p_dataObj = NULL ;
	debug("begin to send_json_topology_toCloud\n");
	//time_val = get_utcms();
	p_dev_topologyObj = bulid_topology_data();
	if(p_dev_topologyObj != NULL)
	{
		//send to cloud
		strcpy(dc_payload_info.messageType,"upload");
		strcpy(dc_payload_info.flags,"request");
		strcpy(dc_payload_info.agentVerison,"1.0") ;
		strcpy(dc_payload_info.action,MSG_ACTION_UP_TOPOLOGY) ;

		dc_payload_info.retry =0;
		dc_payload_info.data_json = p_dev_topologyObj ;

		g_last_topology.resp_flag = 0;
		mqtt_wrapper_send_uploadData(&dc_payload_info,&out_data,QOS0);
		
		//free mem
		//need not to free,it has been freeed when mqtt_wrapper_send_uploadData 
		#if 0
		cJSON_Delete(p_real_jsonData) ;
		#endif
		p_real_jsonData = NULL ;
	}

	return 0 ;
}


//////////real_json_data//////////////
int send_json_realData_toCloud(device_node_info_t *p_node)
{
	proto_basicinfo_t out_data ;
	dc_payload_info_t dc_payload_info ;
	long long int time_val = 0 ;
	//cJSON *p_dataObj = NULL ;

	if(p_node == NULL)
	{
		debug("err para when send_json_realData_toCloud \n") ;
		return -1 ;
	}
	
	debug("begin to send dev_real data to cloud now \n");
	time_val = get_utcms();
	
	if(p_real_jsonData != NULL)
	{
		//send to cloud
		strcpy(dc_payload_info.messageType,"upload");
		strcpy(dc_payload_info.flags,"request");
		strcpy(dc_payload_info.agentVerison,"1.0") ;
		strcpy(dc_payload_info.action,MSG_ACTION_UP_REALDATA) ;

		strlcpy(dc_payload_info.dev_serinum,p_node->device_info.dev_serialNum,sizeof(dc_payload_info.dev_serinum)) ;
		dc_payload_info.dev_configv = p_node->device_info.cfgSet_cnt ;
		dc_payload_info.retry =0;

		dc_payload_info.data_json = p_real_jsonData ;
		
		mqtt_wrapper_send_uploadData(&dc_payload_info,&out_data,QOS0);
		
		//free mem
		//need not to free,it has been freeed when mqtt_wrapper_send_uploadData 
		#if 0
		cJSON_Delete(p_real_jsonData) ;
		#endif
		p_real_jsonData = NULL ;
	}

	return 0 ;
}


void delete_real_JsonData(void)
{
	if(p_real_jsonData != NULL)
	{
		cJSON_Delete(p_real_jsonData) ;
		p_real_jsonData = NULL ;
	}

	return ;
}


int build_json_real_data(device_node_info_t *p_node)
{
	cJSON *p_temp = NULL ;
	cJSON *p_position = NULL ;
	cJSON *p_gps = NULL ;
	long long int time_val = 0 ;
	int ret_val = 0 ;
	
	if(p_node == NULL)
	{
		debug("err para when build_json_real_data \n");
		return -1 ;
	}

	if(p_real_jsonData != NULL)
	{	
		debug("p_real_jsonData!= NULL when build_json_real_data,release it now! warning mem leak !!!!!!!!\n") ;
		cJSON_Delete(p_real_jsonData) ;
		p_real_jsonData = NULL ;
	}

	//build root data-obj 
	p_real_jsonData = cJSON_CreateObject() ;
	if(p_real_jsonData == NULL)
	{
		debug("err create root data-obj when build_json_real_data \n");
		return -1 ;
	}

	//add position,gateway gps info 
	if(gw_gps_data.x_val == 0 && gw_gps_data.y_val == 0)
	{
		//renew gps info
		ret_val = nvram_get_gw_gpsData() ;
	}
	if(ret_val == -1 || (gw_gps_data.x_val == 0 && gw_gps_data.y_val == 0) )
	{
		//cann't get the gps info or x_val & y_val == 0
		p_position = cJSON_CreateNull() ;
		if(p_position == NULL)
		{
			debug("err create gw null-positon when build_json_real_data \n");
			delete_real_JsonData();
			return -1 ;
		}
		//add into real_data_jsonObj
		cJSON_AddItemToObject(p_real_jsonData,"position",p_position) ;
	}
	else
	{
		//success to get gw gps data,add to json 
		//add position
		p_position = cJSON_CreateObject() ;
		if(p_position == NULL)
		{
			debug("err create gw positon when build_json_real_data \n");
			delete_real_JsonData();
			return -1 ;
		}
		//add into real_data_jsonObj
		cJSON_AddItemToObject(p_real_jsonData,"position",p_position) ;

		//add gps
		p_gps = cJSON_CreateObject();
		if(p_gps == NULL)
		{
			debug("err create gw gps when build_json_real_data \n");
			delete_real_JsonData();
			return -1 ;
		}
		//add into p_position
		cJSON_AddItemToObject(p_position,"gps",p_gps) ;

		//add lat,x-val
		p_temp = cJSON_CreateNumber(gw_gps_data.x_val);
		if(p_temp == NULL)
		{
			debug("err create gw lat when build_json_real_data \n");
			delete_real_JsonData();
			return -1 ;
		}
		//add into p_gps
		cJSON_AddItemToObject(p_gps,"lat",p_temp) ;

		//add lng
		p_temp = cJSON_CreateNumber(gw_gps_data.y_val);
		if(p_temp == NULL)
		{
			debug("err create gw lng when build_json_real_data \n");
			delete_real_JsonData();
			return -1 ;
		}
		//add into p_gps
		cJSON_AddItemToObject(p_gps,"lng",p_temp) ;
		
		
	}

	///////////////////////////

	#if 0
	//add userAlarm,no userAlarm in data-pak
	p_temp = cJSON_CreateNumber(0) ;
	if(p_temp == NULL)
	{
		debug("err create userAlarm when build_json_real_data \n");
		delete_real_JsonData();
		return -1 ;
	}
	//add to real_data_jsonObj
	cJSON_AddItemToObject(p_real_jsonData,"userAlarm",p_temp) ;
	#endif


	//new,add sentime
	time_val = get_utcms();
	p_temp = cJSON_CreateNumber(time_val) ;
	if(p_temp == NULL)
	{
		debug("err create sendtime when build_json_real_data \n");
		delete_real_JsonData();
		return -1 ;
	}
	//add to real_data_jsonObj
	cJSON_AddItemToObject(p_real_jsonData,"time",p_temp) ;


	//add device array,device real-data
	p_temp = bulid_dev_realJsonData(p_node) ;
	if(p_temp == NULL)
	{
		debug("create divice data array err when build_json_real_data \n");
		delete_real_JsonData();
		return -1 ;
	}
	//add to real_data_jsonObj
	cJSON_AddItemToObject(p_real_jsonData,"device",p_temp) ;

		

	return 0 ;	
}


cJSON * bulid_dev_realJsonData(device_node_info_t *p_node)
{
	cJSON *p_dev_realDataArray = NULL ;
	cJSON *p_dev_realDataObj = NULL ;
	cJSON *p_dev_location = NULL ;
	cJSON *p_dev_instrumentAlarm = NULL ;
	cJSON *p_dev_gps = NULL ;
	cJSON *p_dev_faultStatus = NULL ;
	cJSON *p_temp = NULL ;
	struct timeval tv;
	long long int time_ms = 0 ;
	uint16_t total_senErr = 0 ;
	
	if(p_node == NULL )
	{
		debug("err para when bulid_dev_realJsonData \n");
		return NULL ;
	}

	//create dev_realJsonData array,dev data root-node
	p_dev_realDataArray = cJSON_CreateArray();
	if(p_dev_realDataArray == NULL)
	{
		debug("create dev_realJsonData array err when bulid_dev_realJsonData \n");
		return NULL ;
	}

	//create dev_realData obj
	p_dev_realDataObj = cJSON_CreateObject() ;
	if(p_dev_realDataObj == NULL)
	{
		debug("create dev_real_data obj err when bulid_dev_realJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into dev_realJsonData_array
	cJSON_AddItemToArray(p_dev_realDataArray,p_dev_realDataObj) ;
// add parentSN
	p_temp = cJSON_CreateString(p_node->device_info.parent_dev_serialNum) ;
	if(p_temp == NULL)
	{
		debug("create parent_dev_serialNum json item err when bulid_dev_realJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"parentSN",p_temp) ;

    char dev_rssi_val_buffer[8]={0};
    sprintf(dev_rssi_val_buffer,"%d",p_node->device_info.dev_rssi_val);
	p_temp = cJSON_CreateString(dev_rssi_val_buffer) ;
	if(p_temp == NULL)
	{
		debug("create signal json item err when bulid_dev_realJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"signal",p_temp) ;
// end add
	//add useConfigV 
	time_ms = get_utcms() ;
	//p_temp = cJSON_CreateNumber(p_node->device_info.cfgSet_cnt);
	p_temp = cJSON_CreateNumber(time_ms);
	if(p_temp == NULL)
	{
		debug("create useConfigV json item err when bulid_dev_realJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"useConfigV",p_temp) ;

	//add serialNo
	p_temp = cJSON_CreateString(p_node->device_info.dev_serialNum) ;
	if(p_temp == NULL)
	{
		debug("create serialNo json item err when bulid_dev_realJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"serialNo",p_temp) ;

	//add time
	time_ms = get_utcms() ;
	debug("sizeof(long)=%d,data recv_ms = %lld \n",sizeof(long int),time_ms);
	p_temp = cJSON_CreateNumber(time_ms) ;
	if(p_temp == NULL)
	{
		debug("create time json item err when bulid_dev_realJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"time",p_temp) ;

	//add appAlarm
	//p_temp = cJSON_CreateNumber(comm_dev_data.app_alarm) ;
	p_temp = cJSON_CreateNumber(get_app_alarm()) ;
	if(p_temp == NULL)
	{
		debug("create appAlarm json item err when bulid_dev_realJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"appAlarm",p_temp) ;


	//add instrumentAlarm,include sensor-alarm,unit-fault-alert,,,,,
	p_dev_instrumentAlarm = cJSON_CreateArray();
	if(p_dev_instrumentAlarm == NULL)
	{
		debug("create instrumentAlarm json item err when bulid_dev_realJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add to p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"instrumentAlarm",p_dev_instrumentAlarm) ;

	total_senErr = get_dev_total_sensorErr(p_node) ;
	debug("total senERR=%d when bulid_dev_realJsonData \n",total_senErr) ;
	if(total_senErr == 0 && (comm_dev_data.unit_err&0x2E) == 0 && get_app_alarm() == 0 )
	{
		//no alarm , no alert
		p_temp = cJSON_CreateNumber(0) ;
		if(p_temp == NULL)
		{
			debug("create instrumentAlarm json item err when bulid_dev_realJsonData \n");
			cJSON_Delete(p_dev_realDataArray) ;
			return NULL ;
		}
		cJSON_AddItemToArray(p_dev_instrumentAlarm,p_temp) ;
	}
	else
	{
		if( (comm_dev_data.unit_err & 0x2E) != 0 || total_senErr == 3 || total_senErr == 7) //bit 1,2,3,5
		{
			//add alert info
			p_temp = cJSON_CreateNumber(3) ; // 3==alert
			if(p_temp == NULL)
			{
				debug("create instrumentAlarm json item err when bulid_dev_realJsonData \n");
				cJSON_Delete(p_dev_realDataArray) ;
				return NULL ;
			}
			cJSON_AddItemToArray(p_dev_instrumentAlarm,p_temp) ;
		}

		if(total_senErr == 4 || total_senErr == 7 || get_app_alarm() != 0 )
		{
			// add alarm info
			p_temp = cJSON_CreateNumber(4) ; // 4 ==alarm
			if(p_temp == NULL)
			{
				debug("create instrumentAlarm json item err when bulid_dev_realJsonData \n");
				cJSON_Delete(p_dev_realDataArray) ;
				return NULL ;
			}
			cJSON_AddItemToArray(p_dev_instrumentAlarm,p_temp) ;
		}
	}
	//end instrumentAlarm//////////////////////////////

	//add location
	if( (dev_gps_data.x_val == 0 && dev_gps_data.y_val == 0) ||
			(p_node->device_info.cmd_ver == WSN_CMD_VER2 && get_sateliteNum() == 0) )
	{
		//replace with gw_gps if dev_gps is invalid
		dev_gps_data.x_val = gw_gps_data.x_val ;
		dev_gps_data.y_val = gw_gps_data.y_val ;
	}

#if 0
	if( (dev_gps_data.x_val == 0 && dev_gps_data.y_val == 0) ||
			(p_node->device_info.cmd_ver == WSN_CMD_VER2 && get_sateliteNum() == 0)  )
#endif
	if( dev_gps_data.x_val == 0 && dev_gps_data.y_val == 0 )
	{
		
		p_dev_location = cJSON_CreateNull() ;
		if(p_dev_location == NULL)
		{
			debug("create null-location err when bulid_dev_realJsonData \n");
			cJSON_Delete(p_dev_realDataArray) ;
			return NULL ;
		}
		//add into p_dev_realDataObj
		cJSON_AddItemToObject(p_dev_realDataObj,"location",p_dev_location) ;
	}
	else
	{
		p_dev_location = cJSON_CreateObject() ;
		if(p_dev_location == NULL)
		{
			debug("create location err when bulid_dev_realJsonData \n");
			cJSON_Delete(p_dev_realDataArray) ;
			return NULL ;
		}
		//add into p_dev_realDataObj
		cJSON_AddItemToObject(p_dev_realDataObj,"location",p_dev_location) ;


		//create gps and add into location
		p_dev_gps = cJSON_CreateObject();
		if(p_dev_gps == NULL)
		{
			debug("create location gps err when bulid_dev_realJsonData \n");
			cJSON_Delete(p_dev_realDataArray) ;
			return NULL ;
		}
		cJSON_AddItemToObject(p_dev_location,"gps",p_dev_gps);

		//create lat and add into p_dev_gps
		p_temp = cJSON_CreateNumber(dev_gps_data.x_val) ;
		if(p_temp == NULL)
		{
			debug("create lat err when bulid_dev_realJsonData \n") ;
			cJSON_Delete(p_dev_realDataArray) ;
			return NULL ;
		}
		//add into p_dev_gps
		cJSON_AddItemToObject(p_dev_gps,"lat",p_temp) ;

		//create lng and add into dev_gps
		p_temp = cJSON_CreateNumber(dev_gps_data.y_val) ;
		if(p_temp == NULL)
		{
			debug("create lng err when bulid_dev_realJsonData \n") ;
			cJSON_Delete(p_dev_realDataArray) ;
			return NULL ;
		}
		//add into p_dev_gps
		cJSON_AddItemToObject(p_dev_gps,"lng",p_temp) ;
	}
	//end location////////////////////////////////////

	//batteryPer
	p_temp = cJSON_CreateNumber(comm_dev_data.power_percent) ;
	if(p_temp == NULL)
	{
		debug("create batteryPer err when bulid_dev_realJsonData \n") ;
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"batteryPer",p_temp) ;

	//add faultStatus,now use uniterr as faultStatus
	p_dev_faultStatus = cJSON_CreateArray() ;
	if(p_dev_faultStatus == NULL)
	{
		debug("create faultStatus err when bulid_dev_realJsonData \n") ;
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add faultStatus into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"faultStatus",p_dev_faultStatus) ;

	//create uniterr and add into faultStatus-array
	if((comm_dev_data.unit_err & 0x2E) == 0) // bit 1,2,3,5 == 0
	{
		//no unit fault info
		p_temp = cJSON_CreateNumber(0) ;
		if(p_temp == NULL)
		{
			debug("create uniterr err when bulid_dev_realJsonData \n") ;
			cJSON_Delete(p_dev_realDataArray) ;
			return NULL ;
		}
		//add into  p_dev_faultStatus
		cJSON_AddItemToArray(p_dev_faultStatus,p_temp) ;
	}
	else
	{
		if(comm_dev_data.unit_err & 0x02) // bit 1,battery low flag
		{
			p_temp = cJSON_CreateNumber(2) ; // 2 == battery low
			if(p_temp == NULL)
			{
				debug("create uniterr err when bulid_dev_realJsonData \n") ;
				cJSON_Delete(p_dev_realDataArray) ;
				return NULL ;
			}
			//add into  p_dev_faultStatus
			cJSON_AddItemToArray(p_dev_faultStatus,p_temp) ;
		}

		if(comm_dev_data.unit_err & 0x04) // bit 2,pump err flag
		{
			p_temp = cJSON_CreateNumber(4) ; // 4==pump err
			if(p_temp == NULL)
			{
				debug("create uniterr err when bulid_dev_realJsonData \n") ;
				cJSON_Delete(p_dev_realDataArray) ;
				return NULL ;
			}
			//add into  p_dev_faultStatus
			cJSON_AddItemToArray(p_dev_faultStatus,p_temp) ;
		}

		if(comm_dev_data.unit_err & 0x08) //bit 3,mem full err flag
		{
			p_temp = cJSON_CreateNumber(8) ; // 8== mem full err
			if(p_temp == NULL)
			{
				debug("create uniterr err when bulid_dev_realJsonData \n") ;
				cJSON_Delete(p_dev_realDataArray) ;
				return NULL ;
			}
			//add into  p_dev_faultStatus
			cJSON_AddItemToArray(p_dev_faultStatus,p_temp) ;
		}

		if(comm_dev_data.unit_err & 0x20) //bit 5,unit fail flag
		{
			p_temp = cJSON_CreateNumber(32) ; // 32==unit fail
			if(p_temp == NULL)
			{
				debug("create uniterr err when bulid_dev_realJsonData \n") ;
				cJSON_Delete(p_dev_realDataArray) ;
				return NULL ;
			}
			//add into  p_dev_faultStatus
			cJSON_AddItemToArray(p_dev_faultStatus,p_temp) ;
		}
	}
	//end faultStatus

	//add sensor,add sendor data
	p_temp = build_sensor_realJsonData(p_node) ;
	if(p_temp == NULL)
	{
		debug("no sensor data when bulid_dev_realJsonData \n");
	}
	else
	{
		//add into p_dev_realDataObj
		cJSON_AddItemToObject(p_dev_realDataObj,"sensor",p_temp) ;
	}
	

	return p_dev_realDataArray ;
}


cJSON *build_sensor_realJsonData(device_node_info_t *p_node)
{
	cJSON *p_sensor_realDataArray = NULL ; //sensor data root node
	cJSON *p_sensor_realDataObj = NULL ;
	cJSON *p_temp = NULL ;
	cJSON *p_alarm_array = NULL ;
	int count = 0 ;
	sensor_data_t *p_sensor_data = NULL ;
	sensor_data_format_t *p_sen_fmt = NULL ;
	uint8_t temp_sen_name[32] ;
	uint8_t unit_str[32] ;
	dev_subDataPak_list_t *p_subData_node = NULL ;
	uint8_t tmp_sensorErr = 0 ;
	uint8_t gas_name[MAX_GAS_NAME_LEN] ;
	
	if(p_node == NULL || p_node->device_info.active_sensor_num <= 0)
	{
		debug("err para or active_sen_num==0 when build_sensor_realJsonData \n");
		return NULL ;
	}

	//check node has sensor data or not?
	if(p_node->device_info.sub_data_pak)
	{
		p_subData_node = query_subSenData_list(p_node) ;
		if(p_subData_node != NULL)
		{
			p_sensor_data = p_subData_node->p_sen_data_list ;
		}
	}
	else
	{
		p_sensor_data = p_dev_sensorData_head ;
	}
	if(p_sensor_data == NULL || p_sensor_data->total_sensor <= 0)
	{
		//no sensor data
		debug("debug no sensor data when build_sensor_realJsonData \n");
		return NULL ;
	}

	//create sensor data root node
	p_sensor_realDataArray = cJSON_CreateArray() ;
	if(p_sensor_realDataArray == NULL)
	{
		debug("err create p_sensor_realDataArray when build_sensor_realJsonData \n") ;
		return NULL ;
	}

	
	//add sensor data
	if(p_node->device_info.sub_data_pak)
	{
		p_subData_node = query_subSenData_list(p_node) ;
		if(p_subData_node != NULL)
		{
			p_sensor_data = p_subData_node->p_sen_data_list ;
		}
	}
	else
	{
		p_sensor_data = p_dev_sensorData_head ;
	}

	//p_sensor_data = p_dev_sensorData_head ;
	while(p_sensor_data != NULL)
	{
		p_sen_fmt = query_sensor_format_data(p_node->device_info.device_addr,p_sensor_data->sensor_index) ;
		if(p_sen_fmt == NULL)
		{
			debug("get p_sen_fmt err when build_sensor_realJsonData \n");
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		
		p_sensor_realDataObj = cJSON_CreateObject() ;
		if(p_sensor_realDataObj == NULL)
		{
			debug("err create p_sensor_realDataObj when build_sensor_realJsonData \n") ;
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		//add into p_sensor_realDataArray
		cJSON_AddItemToArray(p_sensor_realDataArray,p_sensor_realDataObj) ;

		//add idx,sensors index
		p_temp = cJSON_CreateNumber(p_sensor_data->sensor_index) ;
		if(p_temp == NULL)
		{
			debug("err create sensor index=%d when build_sensor_realJsonData \n",p_sensor_data->sensor_index) ;
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		//add into p_sensor_realDataObj
		cJSON_AddItemToObject(p_sensor_realDataObj,"idx",p_temp) ;

		// add alarm,
		memset(temp_sen_name,0,sizeof(temp_sen_name)) ;
		get_sensorName_bysenIDandSubID(p_sen_fmt->sensor_id,p_sen_fmt->sensor_sub_id,temp_sen_name,(sizeof(temp_sen_name)-1) ) ;
		
		p_alarm_array = cJSON_CreateArray();
		if(p_alarm_array == NULL)
		{
			debug("err create alarm array when build_sensor_realJsonData \n") ;
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		//add into p_sensor_realDataObj
		cJSON_AddItemToObject(p_sensor_realDataObj,"alarm",p_alarm_array) ;
		//add alarm data
		if(p_node->device_info.cmd_ver == WSN_CMD_VER1)
		{
			tmp_sensorErr = sensorErr_transV1toV2(p_sensor_data->sensor_err,temp_sen_name) ;
		}
		else
		{
			tmp_sensorErr = p_sensor_data->sensor_err ;
		}
		
		p_temp = cJSON_CreateNumber(tmp_sensorErr) ;
		if(p_temp == NULL)
		{
			debug("err create alarm data when build_sensor_realJsonData \n") ;
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		//add alarm data into p_alarm_array
		cJSON_AddItemToArray(p_alarm_array,p_temp) ;

		// add detectionMode
		//p_temp = cJSON_CreateNumber(p_sen_fmt->detection_mode) ;
		p_temp = cJSON_CreateNumber(p_sensor_data->specialReading) ;
		if(p_temp == NULL)
		{
			debug("err create detectionMode  when build_sensor_realJsonData \n") ;
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		//add into p_sensor_realDataObj
		cJSON_AddItemToObject(p_sensor_realDataObj,"detectionMode",p_temp) ;

		//add val,sensor value
		p_temp = cJSON_CreateNumber(p_sensor_data->sensor_real_data) ;
		if(p_temp == NULL)
		{
			debug("err create val  when build_sensor_realJsonData \n") ;
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		//addd into p_sensor_realDataObj
		cJSON_AddItemToObject(p_sensor_realDataObj,"val",p_temp) ;

		//add decimalPoint
		if(p_sen_fmt->res != 0)
		{
			p_temp = cJSON_CreateNumber(p_sen_fmt->res_dp) ;
		}
		else
		{
			//p_temp = cJSON_CreateNumber(p_sensor_data->sensor_dp) ;
			p_temp = cJSON_CreateNumber(p_sen_fmt->sensor_dp) ;
		}
		if(p_temp == NULL)
		{
			debug("err create decimalPoint  when build_sensor_realJsonData \n") ;
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		//addd into p_sensor_realDataObj
		cJSON_AddItemToObject(p_sensor_realDataObj,"decimalPoint",p_temp) ;

		//add name,sensor name or voc gas_name
		memset(temp_sen_name,0,sizeof(temp_sen_name)) ;
		get_sensorName_bysenIDandSubID(p_sen_fmt->sensor_id,p_sen_fmt->sensor_sub_id,temp_sen_name,(sizeof(temp_sen_name)-1) ) ;
		if(strcmp(temp_sen_name,"VOC") == 0)
		{
			//sensorName == VOC
			memset(gas_name,0,sizeof(gas_name)) ;
			query_gas_name_bySensorFmtGasID(p_sen_fmt->gas_id,gas_name,sizeof(gas_name));
			//use voc gasName replace sensorName
			p_temp = cJSON_CreateString(gas_name) ;
		}
		else
		{
			//not voc sensor,use sensorName
			p_temp = cJSON_CreateString(temp_sen_name) ;
		}
		if(p_temp == NULL)
		{
			debug("err create name  when build_sensor_realJsonData \n") ;
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		//addd into p_sensor_realDataObj
		cJSON_AddItemToObject(p_sensor_realDataObj,"name",p_temp) ;

		//add unit,measurement unit 
		memset(unit_str,0,sizeof(unit_str));
		query_unitID_TAB(p_sen_fmt->unit_id,unit_str,sizeof(unit_str)-1 ) ;
		p_temp = cJSON_CreateString(unit_str) ;
		if(p_temp == NULL)
		{
			debug("err create unit str  when build_sensor_realJsonData \n") ;
			if(p_sensor_realDataArray != NULL)
			{
				cJSON_Delete(p_sensor_realDataArray) ;
			}
			return NULL ;
		}
		//add into p_sensor_realDataObj
		cJSON_AddItemToObject(p_sensor_realDataObj,"unit",p_temp) ;
		
		//handle next sensor data
		p_sensor_data = p_sensor_data->p_next_sensor_data ;
	}
		
	

	return p_sensor_realDataArray ;
}


cJSON * build_dev_offlineJsonData(device_node_info_t *p_node)
{
	cJSON *p_dev_realDataArray = NULL ;
	cJSON *p_dev_realDataObj = NULL ;
	cJSON *p_dev_location = NULL ;
	cJSON *p_dev_instrumentAlarm = NULL ;
	cJSON *p_dev_gps = NULL ;
	cJSON *p_dev_faultStatus = NULL ;
	cJSON *p_temp = NULL ;
	struct timeval tv;
	long long int time_ms = 0 ;
	uint16_t total_senErr = 0 ;
	
	if(p_node == NULL )
	{
		debug("err para when build_dev_offlineJsonData \n");
		return NULL ;
	}

	//create dev_realJsonData array,dev data root-node
	p_dev_realDataArray = cJSON_CreateArray();
	if(p_dev_realDataArray == NULL)
	{
		debug("create dev_realJsonData array err when build_dev_offlineJsonData \n");
		return NULL ;
	}

	//create dev_realData obj
	p_dev_realDataObj = cJSON_CreateObject() ;
	if(p_dev_realDataObj == NULL)
	{
		debug("create dev_real_data obj err when build_dev_offlineJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into dev_realJsonData_array
	cJSON_AddItemToArray(p_dev_realDataArray,p_dev_realDataObj) ;

	//add useConfigV 
	time_ms = get_utcms() ;
	//p_temp = cJSON_CreateNumber(p_node->device_info.cfgSet_cnt);
	p_temp = cJSON_CreateNumber(time_ms);
	if(p_temp == NULL)
	{
		debug("create useConfigV json item err when build_dev_offlineJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"useConfigV",p_temp) ;

	//add serialNo
	p_temp = cJSON_CreateString(p_node->device_info.dev_serialNum) ;
	if(p_temp == NULL)
	{
		debug("create serialNo json item err when build_dev_offlineJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"serialNo",p_temp) ;

	//add time
	time_ms = get_utcms() ;
	debug("sizeof(long)=%d,data recv_ms = %lld \n",sizeof(long int),time_ms);
	p_temp = cJSON_CreateNumber(time_ms) ;
	if(p_temp == NULL)
	{
		debug("create time json item err when build_dev_offlineJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"time",p_temp) ;

	//add appAlarm
	p_temp = cJSON_CreateNumber(comm_dev_data.app_alarm) ;
	if(p_temp == NULL)
	{
		debug("create appAlarm json item err when build_dev_offlineJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add into p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"appAlarm",p_temp) ;


	//add instrumentAlarm,include sensor-alarm,unit-fault-alert,offline-alarm...
	p_dev_instrumentAlarm = cJSON_CreateArray();
	if(p_dev_instrumentAlarm == NULL)
	{
		debug("create instrumentAlarm json item err when build_dev_offlineJsonData \n");
		cJSON_Delete(p_dev_realDataArray) ;
		return NULL ;
	}
	//add to p_dev_realDataObj
	cJSON_AddItemToObject(p_dev_realDataObj,"instrumentAlarm",p_dev_instrumentAlarm) ;

	total_senErr = get_dev_total_sensorErr(p_node) ;
	//if(total_senErr == 0 && (comm_dev_data.unit_err&0x2E) == 0)
	{
		//add offline alarm
		p_temp = cJSON_CreateNumber(1) ; //1--offline
		if(p_temp == NULL)
		{
			debug("create instrumentAlarm json item err when build_dev_offlineJsonData \n");
			cJSON_Delete(p_dev_realDataArray) ;
			return NULL ;
		}
		cJSON_AddItemToArray(p_dev_instrumentAlarm,p_temp) ;
	}
	
	//end instrumentAlarm//////////////////////////////

	return p_dev_realDataArray ;
}


int build_offLine_jsonData(device_node_info_t *p_node)
{
	cJSON *p_temp = NULL ;
	cJSON *p_position = NULL ;
	cJSON *p_gps = NULL ;
	long long int time_val = 0 ;
	int ret_val = 0 ;
	
	if(p_node == NULL)
	{
		debug("err para when build_offLine_jsonData \n");
		return -1 ;
	}

	if(p_real_jsonData != NULL)
	{	
		debug("p_real_jsonData!= NULL when build_json_real_data,release it now! warning mem leak !!!!!!!!\n") ;
		cJSON_Delete(p_real_jsonData) ;
		p_real_jsonData = NULL ;
	}

	//build root data-obj 
	p_real_jsonData = cJSON_CreateObject() ;
	if(p_real_jsonData == NULL)
	{
		debug("err create root data-obj when build_offLine_jsonData \n");
		return -1 ;
	}


	//add userAlarm,no userAlarm in data-pak
	p_temp = cJSON_CreateNumber(0) ;
	if(p_temp == NULL)
	{
		debug("err create userAlarm when build_offLine_jsonData \n");
		delete_real_JsonData();
		return -1 ;
	}
	//add to real_data_jsonObj
	cJSON_AddItemToObject(p_real_jsonData,"userAlarm",p_temp) ;


	//new,add sentime
	time_val = get_utcms();
	p_temp = cJSON_CreateNumber(time_val) ;
	if(p_temp == NULL)
	{
		debug("err create sendtime when build_offLine_jsonData \n");
		delete_real_JsonData();
		return -1 ;
	}
	//add to real_data_jsonObj
	cJSON_AddItemToObject(p_real_jsonData,"time",p_temp) ;


	//add device array,device real-data
	p_temp = build_dev_offlineJsonData(p_node) ;
	if(p_temp == NULL)
	{
		debug("create divice data array err when build_offLine_jsonData \n");
		delete_real_JsonData();
		return -1 ;
	}
	//add to real_data_jsonObj
	cJSON_AddItemToObject(p_real_jsonData,"device",p_temp) ;

		

	return 0 ;	
}

int send_devOfflineData_toCloud(device_node_info_t *p_node)
{
	proto_basicinfo_t out_data ;
	dc_payload_info_t dc_payload_info ;
	long long int time_val = 0 ;
	//cJSON *p_dataObj = NULL ;

	if(p_node == NULL)
	{
		debug("err para when send_devOfflineData_toCloud \n") ;
		return -1 ;
	}

	build_offLine_jsonData(p_node);
	
	debug("begin to send dev_offline data to cloud now \n");
	time_val = get_utcms();
	
	if(p_real_jsonData != NULL)
	{
		//send to cloud
		strcpy(dc_payload_info.messageType,"upload");
		strcpy(dc_payload_info.flags,"request");
		strcpy(dc_payload_info.agentVerison,"1.0") ;
		strcpy(dc_payload_info.action,MSG_ACTION_UP_REALDATA) ;

		strlcpy(dc_payload_info.dev_serinum,p_node->device_info.dev_serialNum,sizeof(dc_payload_info.dev_serinum)) ;
		dc_payload_info.dev_configv = p_node->device_info.cfgSet_cnt ;
		dc_payload_info.retry =0 ;
		

		dc_payload_info.data_json = p_real_jsonData ;
		
		mqtt_wrapper_send_uploadData(&dc_payload_info,&out_data,QOS1);//offline--qos1
		
		//free mem
		//need not to free,it has been freeed when mqtt_wrapper_send_uploadData 
		//cJSON_Delete(p_real_jsonData) ;
		p_real_jsonData = NULL ;
	}

	return 0 ;
}

extern int g_sd_mounted ;
int bak_realDataRecord(device_node_info_t *p_node)
{
	//uint8_t err_info[64] ;
	//uint8_t temp_sen_name[MAX_SENSOR_NAME_LEN];
	
	uint8_t record_fileName[96] ;
	uint8_t record_fileNameBak[96] ;
	uint8_t record_file_path[64] ;
	uint8_t cmd_buf[200] ;
	uint8_t sen_count = 0 ;
	sensor_data_t *p_sensor_data = NULL ;
	sensor_data_format_t *p_sen_fmt = NULL ;
	dev_subDataPak_list_t *p_subData_node = NULL ;
	//uint8_t time_str[32] ;
	//time_t time_val = 0 ;
	uint8_t count = 0 ;
	uint8_t err_id = 0 ;
	uint8_t sub_id = 0 ;
	uint32_t productID = 0 ;
	//uint8_t instrument_name[32] ;
	//uint8_t *p_record_fileName = NULL ;
	//uint8_t *p_record_fileName_bak = NULL ;
	//uint16_t file_name_len = 0 ;
	uint8_t dev_sn[41] ;
	

	if(p_node == NULL)
	{
		debug("err para when bak_realDataRecord \n");
		return -1 ;
	}

	if(!g_sd_mounted)  
	{
	    debug("err sd not mounted\n");
	    return -1;
	} 

    //fxy
	uint8_t timenowstr[16] ;
    uint8_t a_buffer_filename[32]={0} ; //max 20+4+1
	uint8_t a_buffer_filename_escape[64]={0} ; //max 40+4+1
	
	memset(timenowstr,0,sizeof(timenowstr));
	get_yearmonthday(timenowstr,sizeof(timenowstr));	
	memset(dev_sn,0,sizeof(dev_sn));
	transfer_dirChar_inSn(p_node->device_info.dev_serialNum,dev_sn,sizeof(dev_sn)) ;

    snprintf(a_buffer_filename_escape,sizeof(a_buffer_filename_escape),"%s.csv",dev_sn);
    snprintf(a_buffer_filename,sizeof(a_buffer_filename),"%s.csv",p_node->device_info.dev_serialNum);

    cp_csv_remainData_to_sd_and_del(a_buffer_filename,a_buffer_filename_escape,timenowstr);
    //end fxy
	memset(record_fileName,0,sizeof(record_fileName)) ;
	memset(record_fileNameBak,0,sizeof(record_fileNameBak));

	strlcpy(record_file_path,SD_RECORD_FILE_PATH,sizeof(record_file_path)) ;
	strlcat(record_file_path,timenowstr,sizeof(record_file_path));
	snprintf(record_fileName,sizeof(record_fileName)-1,"%s/%s.csv",record_file_path,dev_sn);
	snprintf(record_fileNameBak,sizeof(record_fileNameBak)-1,"%s/%s_bak.csv",record_file_path,dev_sn);
	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	snprintf(cmd_buf,sizeof(cmd_buf)-1,"mv -f %s %s",record_fileName,record_fileNameBak);
	debug("bak file cmd= %s \n",cmd_buf);
	system(cmd_buf);

	return 0 ;
}

int save_realDataRecord(device_node_info_t *p_node)
{
	
	
	uint8_t err_info[64] ;
	uint8_t temp_sen_name[MAX_SENSOR_NAME_LEN];
	
	uint8_t record_fileName_escape[MAX_SENSOR_NAME_LEN*2+12] ;
	uint8_t record_fileName[MAX_SENSOR_NAME_LEN+12] ;
	uint8_t sen_count = 0 ;
	sensor_data_t *p_sensor_data = NULL ;
	sensor_data_format_t *p_sen_fmt = NULL ;
	dev_subDataPak_list_t *p_subData_node = NULL ;
	uint8_t time_str[32] ;
	time_t time_val = 0 ;
	struct tm *timenow;  //fxy
	uint8_t count = 0 ;
	uint8_t err_id = 0 ;
	uint8_t sub_id = 0 ;
	uint32_t productID = 0 ;
	uint8_t instrument_name[32] ;
	uint8_t dev_sn[41] ;
	uint8_t app_alarm_str[20];

	if(p_node == NULL)
	{
		debug("err para when save_realDataRecord \n");
		return -1 ;
	}




	memset(record_fmt,0,sizeof(record_fmt)) ;
	memset(record_data,0,sizeof(record_data)) ;
	memset(record_fileName,0,sizeof(record_fileName)) ;
    
	//save_realDataRecord record file name
	memset(dev_sn,0,sizeof(dev_sn));
	transfer_dirChar_inSn(p_node->device_info.dev_serialNum,dev_sn,sizeof(dev_sn)) ;

	snprintf(record_fileName_escape,sizeof(record_fileName_escape),"%s.csv",dev_sn);
	debug("record_fileName_escape name: %s \n",record_fileName_escape);
	snprintf(record_fileName,sizeof(record_fileName),"%s.csv",p_node->device_info.dev_serialNum);
	debug("record_fileName name: %s \n",record_fileName);
	
	//format
	//instrument name
	memset(instrument_name,0,sizeof(instrument_name));
	productID = (p_node->device_info.instr_family_id << 24) | (p_node->device_info.instr_model_id << 16 )
					| (p_node->device_info.instr_hw_id << 8) | p_node->device_info.instr_fw_id ;
	productID = productID & 0xFFFF0000 ;
	query_instrument_tab(productID,instrument_name,sizeof(instrument_name)-1 ) ;
	sprintf(record_fmt,"          instrument_name:     %s \n",instrument_name);

	//serial num
	memset(temp_fmt,0,sizeof(temp_fmt)) ;
	  sprintf(temp_fmt,"          serialNO :           %s \n",p_node->device_info.dev_serialNum);
	strcat(record_fmt,temp_fmt) ;

	//firmware VER:
	memset(temp_fmt,0,sizeof(temp_fmt)) ;
	  sprintf(temp_fmt,"          firmware VER :       %s \n",p_node->device_info.dev_fw_ver);
	strcat(record_fmt,temp_fmt) ;
	
	
	//date info
	memset(temp_fmt,0,sizeof(temp_fmt)) ;
	sprintf(temp_fmt,"date info,");
	strcat(record_fmt,temp_fmt) ;

	//sensor name
	if(p_node->device_info.sub_data_pak)
	{
		p_subData_node = query_subSenData_list(p_node) ;
		if(p_subData_node != NULL)
		{
			p_sensor_data = p_subData_node->p_sen_data_list ;
		}
	}
	else
	{
		p_sensor_data = p_dev_sensorData_head ;
	}
	if(p_node->device_info.active_sensor_num > 0 && p_sensor_data && p_sensor_data->total_sensor > 0)
	{
		//p_sensor_data = p_dev_sensorData_head ;
		while(p_sensor_data != NULL)
		{
			memset(temp_fmt,0,sizeof(temp_fmt)) ;
			memset(temp_sen_name,0,sizeof(temp_sen_name)) ;

			//get sen fmt
			p_sen_fmt = query_sensor_format_data(p_node->device_info.device_addr,p_sensor_data->sensor_index) ;
			if(p_sen_fmt == NULL)
			{
				debug("get sen_fmt err,serial=%s,sen_index=%d when save_realDataRecord\n",p_node->device_info.dev_serialNum,p_sensor_data->sensor_index);
				return -1 ;
			}

			//get sen_name
			get_sensorName_bysenIDandSubID(p_sen_fmt->sensor_id,p_sen_fmt->sensor_sub_id,temp_sen_name,(sizeof(temp_sen_name)-1) ) ;
			if(temp_sen_name[0] == 0)
			{
				debug("get sensorName err when save_realDataRecord \n") ;
				return -1 ;
			}
			sprintf(temp_fmt,"%s,",temp_sen_name);

			//connect to record_fmt
			strcat(record_fmt,temp_fmt);
			p_sensor_data = p_sensor_data->p_next_sensor_data ;
		}
	}

	//  intrument status
	memset(temp_fmt,0,sizeof(temp_fmt));
	sprintf(temp_fmt,"instrument status,");
	//connect to record_fmt
	strcat(record_fmt,temp_fmt) ;

	//gps info
	memset(temp_fmt,0,sizeof(temp_fmt));
	sprintf(temp_fmt,"gps info,");
	//connect to record_fmt
	strcat(record_fmt,temp_fmt) ;

	//signal,last para
	memset(temp_fmt,0,sizeof(temp_fmt));
	sprintf(temp_fmt,"signal \n");
	//connect to record_fmt
	strcat(record_fmt,temp_fmt) ;

	debug("save_realDataRecord: record_fmt: \n%s \n",record_fmt);


	//real data//////////////////////////////////
	//date info
	memset(time_str,0,sizeof(time_str));
	time(&time_val) ;
    //fxy
	timenow = localtime(&time_val);  
    strftime(time_str, sizeof(time_str), "%Y%m%d %H%M%S", timenow); 
	//strncpy(time_str,ctime(&time_val),(strlen(ctime(&time_val))-1) ); //-1 to skip \n
	//end fxy
    sprintf(record_data,"%s,",time_str);
	//debug("record time: %s \n",record_data);

	//sensor data
	p_sensor_data = NULL ; 
	if(p_node->device_info.sub_data_pak)
	{
		p_subData_node = query_subSenData_list(p_node) ;
		if(p_subData_node != NULL)
		{
			p_sensor_data = p_subData_node->p_sen_data_list ;
		}
	}
	else
	{
		p_sensor_data = p_dev_sensorData_head ;
	}
	if(p_node->device_info.active_sensor_num > 0 && p_sensor_data && p_sensor_data->total_sensor > 0)
	{
		//p_sensor_data = p_dev_sensorData_head ;
		while(p_sensor_data != NULL)
		{
			memset(temp_fmt,0,sizeof(temp_fmt));
			//debug("add sensordata: %f \n",p_sensor_data->sensor_real_data);
			sprintf(temp_fmt,"%0.2f,",p_sensor_data->sensor_real_data);
			//connect to record_data
			strcat(record_data,temp_fmt);

			p_sensor_data = p_sensor_data->p_next_sensor_data ;
		}
	}
	else
	{
		for(sen_count=0;sen_count<p_node->device_info.active_sensor_num;sen_count++)
		{
			memset(temp_fmt,0,sizeof(temp_fmt));
			sprintf(temp_fmt,"NULL,") ;
			//connect to record_data
			strcat(record_data,temp_fmt) ;
		}
	}

	//debug("after add sensordata: %s \n",record_data);

	//instrument status
	memset(temp_fmt,0,sizeof(temp_fmt)) ;
	//sprintf(temp_fmt,"unit_err info: ");
	
	//get unit err info
	if(comm_dev_data.unit_err == 0)
	{
		//no unit err
		memset(err_info,0,sizeof(err_info));
		//strcpy(err_info,"OK");
		strcpy(err_info," "); // show nothing
		strcat(temp_fmt,err_info) ;
	}
	else
	{
		for(count=1;count<=5;count++)
		{
			if(count == 4)
			{
				//not care bit0-power and bit4-sensor-alarm
				continue ;
			}
			memset(err_info,0,sizeof(err_info));
			//bit0-bit5,errID=1-2-4-8-16-32
			err_id = comm_dev_data.unit_err & (0x01 << count) ;
			if(err_id)
			{
				sub_id = 1 ;
				query_unit_err_tab(err_id,sub_id,err_info,(sizeof(err_info)-1) ) ;
				strcat(temp_fmt,err_info) ;
			}
			
		}
	}

	//get sensor err info
	//strcat(temp_fmt,"sensor_err info: ") ;
	p_sensor_data = NULL ; 
	if(p_node->device_info.sub_data_pak)
	{
		p_subData_node = query_subSenData_list(p_node) ;
		if(p_subData_node != NULL)
		{
			p_sensor_data = p_subData_node->p_sen_data_list ;
		}
	}
	else
	{
		p_sensor_data = p_dev_sensorData_head ;
	}
	while(p_sensor_data != NULL)
	{
		if(p_sensor_data->sensor_err != 0)
		{
			if(p_node->device_info.cmd_ver == 2) //cmd-v2
			{
				sub_id = 1 ;
				err_id = p_sensor_data->sensor_err ;
				query_sensor_err_tab_v2(err_id,sub_id,err_info,(sizeof(err_info)-1) ) ;
				if(err_info[0] == 0)
				{
					strcpy(err_info,"other err ");
				}
				strcat(temp_fmt,err_info) ;
			}
			else
			{
				//cmd-v1
				for(count=0;count<=7;count++)
				{
					memset(err_info,0,sizeof(err_info));
					err_id = p_sensor_data->sensor_err & (0x01 << count) ;
					if(err_id)
					{
						sub_id = 1 ;
						query_sensor_err_tab(err_id,sub_id,err_info,(sizeof(err_info)-1) ) ;
						if(strlen(temp_fmt) >= (sizeof(temp_fmt)-64) )
						{
							debug("sensor_err info overflow!!! \n");
							break ;
						}
						if(err_info[0] == 0)
						{
							strcpy(err_info,"other err ");
						}
						strcat(temp_fmt,err_info) ;
					}
										
				}
			}
		}

		p_sensor_data = p_sensor_data->p_next_sensor_data ;
	}

	//appAlarm
	if(get_app_alarm() != 0)
	{
		memset(app_alarm_str,0,sizeof(app_alarm_str)) ;
		get_app_alarmStr(app_alarm_str,sizeof(app_alarm_str));
		strcat(temp_fmt,app_alarm_str) ;
	}
	
	strcat(temp_fmt,",") ;
	
	
	//connect to record_data
	strcat(record_data,temp_fmt);
	//debug("temp_fmt len=%d,info:%s \n",strlen(temp_fmt),temp_fmt) ;

	//debug("after add instrument: %s \n",record_data);

	//gps data
	memset(temp_fmt,0,sizeof(temp_fmt)) ;
	if(dev_gps_data.new_data_flag)
	{
		sprintf(temp_fmt,"Lat=%f;long=%f,",dev_gps_data.x_val,dev_gps_data.y_val) ;
	}
	else
	{
		sprintf(temp_fmt,"NULL,");
	}
	//connect to record_data
	strcat(record_data,temp_fmt);
	//debug("after add gps: %s \n",record_data);

	//signal data,last para
	memset(temp_fmt,0,sizeof(temp_fmt)) ;
	sprintf(temp_fmt,"%d \n",dev_zigbee_data.rcm_signal);
	//connect to record_data
	strcat(record_data,temp_fmt);

	debug("save_realDataRecord: record_data: %s \n",record_data) ;
    //debug("save_realDataRecord: p_record_fileName: %s \n",p_record_fileName) ;


	//debug("before write to file: fmt=%s \n",record_fmt);
	//debug("before write to file: data=%s \n",record_data);
	//save record into dev-file

	write_csv_fmtData_file(record_data,record_fmt,record_fileName,record_fileName_escape,time_str);


	return 0 ;
	
}


void set_cloudCmd_result(uint8_t result)
{
	cloud_cmd_result = result ;

	return ;
}

uint8_t get_cloudCmd_result(void)
{
	return cloud_cmd_result ;
}


int build_and_send_cloudCmdResp(cloud_cmd_list_t *p_cmd_node,device_node_info_t *p_dev_node)
{
	proto_basicinfo_t out_data ;
	dc_payload_info_t dc_payload_info ;
	cJSON *p_jsonData_root = NULL ;
	cJSON *p_temp = NULL ;
	cJSON *p_result_jsonArray = NULL ;
	cJSON *p_result_objMember = NULL ;
	
	
	if(p_cmd_node == NULL )
	{
		debug("err para when build_and_send_cloudCmdResp \n");
		return -1 ;
	}

	strcpy(dc_payload_info.messageType,"commandResponse");
	strcpy(dc_payload_info.flags,"request");
	strcpy(dc_payload_info.agentVerison,"1.0") ;
	strcpy(dc_payload_info.action,"SendCmd") ;

	strlcpy(dc_payload_info.dev_serinum,p_cmd_node->serial_num,sizeof(dc_payload_info.dev_serinum)) ;
	if(p_dev_node != NULL)
	{
		dc_payload_info.dev_configv = p_dev_node->device_info.cfgSet_cnt ;
	}
	else
	{
		dc_payload_info.dev_configv = 0 ; //no dev-info in the buf
	}
	dc_payload_info.retry =0 ;

	//build json data
	p_jsonData_root = cJSON_CreateObject() ;
	if(p_jsonData_root == NULL)
	{
		debug("err create jsonData root when build_and_send_cloudCmdResp \n");
		return -1 ;
	}

	//add commandId
	p_temp = cJSON_CreateString(p_cmd_node->cmd_data.command_id) ;
	if(p_temp == NULL)
	{
		debug("err create commandId when build_and_send_cloudCmdResp \n");
		cJSON_Delete(p_jsonData_root) ;
		return -1 ;
	}
	//add into p_jsonData_root obj
	cJSON_AddItemToObject(p_jsonData_root,"commandId",p_temp) ;

	//add commandType
	p_temp = cJSON_CreateString(p_cmd_node->cmd_data.command_type) ;
	if(p_temp == NULL)
	{
		debug("err create commandType when build_and_send_cloudCmdResp \n");
		cJSON_Delete(p_jsonData_root) ;
		return -1 ;
	}
	//add into p_jsonData_root obj
	cJSON_AddItemToObject(p_jsonData_root,"commandType",p_temp) ;

	//add result
	p_result_jsonArray = cJSON_CreateArray() ;
	if(p_result_jsonArray == NULL)
	{
		debug("err create p_result_jsonArray when build_and_send_cloudCmdResp \n");
		cJSON_Delete(p_jsonData_root) ;
		return -1 ;
	}
	//add into p_jsonData_root
	cJSON_AddItemToObject(p_jsonData_root,"result",p_result_jsonArray) ;

	//add member to p_result_jsonArray
	p_result_objMember = cJSON_CreateObject() ;
	if(p_result_objMember == NULL)
	{
		debug("err create p_result_objMember when build_and_send_cloudCmdResp \n") ;
		cJSON_Delete(p_jsonData_root) ;
		return -1 ;
	}
	//add into p_result_jsonArray
	cJSON_AddItemToArray(p_result_jsonArray,p_result_objMember) ;

	//add serialNo
	p_temp = cJSON_CreateString(p_cmd_node->serial_num) ;
	if(p_temp == NULL)
	{
		debug("err create serialNo when build_and_send_cloudCmdResp \n");
		cJSON_Delete(p_jsonData_root) ;
		return -1 ;
	}
	//add into p_result_objMember
	cJSON_AddItemToObject(p_result_objMember,"serialNo",p_temp) ;

	//add errorCode
	p_temp = cJSON_CreateNumber(p_cmd_node->result_resp) ;
	if(p_temp == NULL)
	{
		debug("err create errorCode when build_and_send_cloudCmdResp \n");
		cJSON_Delete(p_jsonData_root) ;
		return -1 ;
	}
	//add into p_result_objMember
	cJSON_AddItemToObject(p_result_objMember,"errorCode",p_temp) ;

	//send to cloud now
	dc_payload_info.data_json = p_jsonData_root ;
	mqtt_wrapper_send_uploadData(&dc_payload_info,&out_data,QOS0);
	//json mem freeed when mqtt_wrapper_send_uploadData

	return 0 ;
}


uint8_t get_cloud_wsnCmdID(dn_payload_t *p_cloud_cmd)
{
	uint8_t wsn_cmdID = 0 ;
	
	if(p_cloud_cmd == NULL)
	{
		debug("err para when get_cloud_wsnCmdID \n");
		return 0 ;
	}

	if(strncmp(p_cloud_cmd->command_type,CLOUD_SNDMSG_TYPE_STR,strlen(CLOUD_SNDMSG_TYPE_STR)) == 0 ||
		  strncmp(p_cloud_cmd->command_type,CLOUD_BRDCASTMSG_TYPE_STR,strlen(CLOUD_BRDCASTMSG_TYPE_STR)) == 0)
	{
		wsn_cmdID = WSN_SNDMSG_CMDID ;
	}
	else if( strncmp(p_cloud_cmd->command_type,CLOUD_SLEEP_TYPE_STR,strlen(CLOUD_SLEEP_TYPE_STR))==0 || 
			strncmp(p_cloud_cmd->command_type,CLOUD_WKUP_TYPE_STR,strlen(CLOUD_WKUP_TYPE_STR))==0 )
	{
		wsn_cmdID = WSN_PUT_SLEEP_WAKEUP_CMDID ;
	}
	
	return wsn_cmdID ;
}


uint8_t get_cloud_actionType(dn_payload_t *p_cloud_cmd)
{
	uint8_t action_type = 0 ;
	
	if(p_cloud_cmd == NULL)
	{
		debug("err para when get_cloud_wsnCmdID \n");
		return 0 ;
	}

	if( strncmp(p_cloud_cmd->command_type,CLOUD_SLEEP_TYPE_STR,strlen(CLOUD_SLEEP_TYPE_STR))==0 )
	{
		action_type = SLEEP_ACTION_TYPE ;
	}
	else if( strncmp(p_cloud_cmd->command_type,CLOUD_WKUP_TYPE_STR,strlen(CLOUD_WKUP_TYPE_STR))==0 )
	{
		action_type = WKUP_ACTION_TYPE ;
	}
	
	return action_type ;
}



int build_dev_cmdData(uint8_t *p_buf,uint8_t buf_len,cloud_cmd_list_t *p_cmd_node)
{	
	int count = 0 ;
	//uint16_t modem_mac = 0x0001 ; //default value
	//uint8_t gw_modem_mac_high = 0x01 ;
	//uint8_t gw_modem_mac_low = 0x02 ;
	//uint8_t gw_sn[20] = {0} ;
	device_node_info_t *p_dev_node = NULL ;
	uint16_t msg_id = 0 ;
	
	if(p_buf == NULL || buf_len <= 0 || p_cmd_node == NULL)
	{
		debug("err para when build_dev_cmdData \n");
		return -1 ;
	}

	

	switch( p_cmd_node->wsn_cmdID )
	{
		case WSN_SNDMSG_CMDID :
			p_dev_node = query_device_info_by_serialNum(p_cmd_node->serial_num) ;
			if(p_dev_node == NULL )
			{
				debug("not found dev-info in buf dev=%s when build_dev_cmdData \n",p_cmd_node->serial_num);
				return -1 ;
			}

			//gw modem mac ,last 2 bytes
			p_buf[count++] = (local_addr>>8) & 0xFF ; //high byte
			p_buf[count++] = local_addr & 0xFF ; //low byte

			//gw sn
			memcpy(&p_buf[count],gw_serial,10) ;
			count += 10 ;

			//msg id
			msg_id = p_dev_node->device_info.msg_id++ ;
			p_buf[count++] = (msg_id >> 8)& 0xFF ; //high byte
			p_buf[count++] = msg_id & 0xFF ; //low byte

			//msg type
			p_buf[count++] = (p_cmd_node->msg_type >> 8) & 0xFF ; //high byte
			p_buf[count++] = p_cmd_node->msg_type & 0xFF ; //low byte

			//padding 1 byte
			p_buf[count++] = 0x00 ;

			//msg priority
			p_buf[count++] = 0x02 ; //default value

			//if(p_cmd_node->cmd_data.data != NULL )  //fxy 
			{
				if(p_cmd_node->cmd_data.data_len > 0 && (p_cmd_node->cmd_data.data_len+count) <= buf_len )
				{
					memcpy(&p_buf[count],p_cmd_node->cmd_data.data,p_cmd_node->cmd_data.data_len) ;
					count += p_cmd_node->cmd_data.data_len ;
				}
				else
				{
					debug("cmd data=%d too big when build_dev_cmdData \n",p_cmd_node->cmd_data.data_len) ;
				}
			}
			
			break ;
			
		case WSN_PUT_SLEEP_WAKEUP_CMDID :
			p_buf[count++] = p_cmd_node->action_type ;
			p_buf[count++] = 0 ; //duration high,only surport default value=0 now
			p_buf[count++] = 0 ; //duration low,only surport default value=0 now
			p_buf[count++] = 0 ; // reserved byte,default value==0
			break ;
		default :
			debug("maybe cmdID err,cmdID=0x%x \n",p_cmd_node->wsn_cmdID);
			break ;
	}

	return count ;
}





int handle_cloud_cmd(dn_payload_t *p_cloud_cmd)
{
	cloud_cmd_list_t *p_cmd_node = NULL ;
	//cloud_cmd_list_t *p_temp = NULL ;
	uint8_t wsn_cmdID = 0 ;
	uint8_t action_type = 0 ;
	int dev_num = 0 ;
	int count = 0 ;
	int data_count = 0 ;
	uint16_t source_addr = 0 ;
	uint16_t dest_addr = 0 ;
	device_node_info_t *p_dev_node = NULL ;
	int cmd_data_len = 0 ;
	cloud_cmd_list_t temp_cmd_node ;
	int wr_cmdData_count = 0 ;
	
	if(p_cloud_cmd == NULL)
	{
		debug("err para when handle_cloud_cmd \n");
		return -1 ;
	}
    debug("handle_cloud_cmd enter\n");   //fxy test
	wsn_cmdID = get_cloud_wsnCmdID(p_cloud_cmd) ;
	action_type = get_cloud_actionType(p_cloud_cmd) ;
	dev_num = p_cloud_cmd->dev_num ;
	debug("begin to handle cloud_cmd: wsn_cmdID=0x%x,dev_num=%d \n",wsn_cmdID,dev_num);
	if(strncmp(p_cloud_cmd->command_type,CLOUD_BRDCASTMSG_TYPE_STR,strlen(CLOUD_BRDCASTMSG_TYPE_STR)) == 0) 
	{
		//handle broadcast msg
		//save into cmd_list
		for(count=0;count<dev_num;count++)
		{
			//p_cmd_node = query_cloudCmd_node(&p_cloud_cmd->serial_no[count],strlen(&p_cloud_cmd->serial_no[count]),wsn_cmdID) ;
			p_cmd_node = query_cloudCmd_node(&(p_cloud_cmd->serial_no[count][0]),31,wsn_cmdID) ;
			if(p_cmd_node != NULL)
			{
				//only one sn-wsn_cmdID pair in cloud_cmd list at the same time
				debug("found sn-wsn_cmdID pair in cloud_cmd list,discard this cmd now \n");
				continue ;
			}
				
			p_cmd_node = malloc(sizeof(cloud_cmd_list_t)) ;
			if(p_cmd_node == NULL)
			{
				debug("malloc cloud_cmd node failed when handle_cloud_cmd \n");
				return -1 ;
			}
			memset(p_cmd_node,0,sizeof(cloud_cmd_list_t));
			p_cmd_node->wsn_cmdID = wsn_cmdID ;
			p_cmd_node->action_type = action_type ;
			p_cmd_node->msg_type = WSN_ASCIITXT_MSG_TYPE ; // default msg type
			//strncpy(p_cmd_node->serial_num,&(p_cloud_cmd->serial_no[count][0]),sizeof(p_cmd_node->serial_num)) ;
			strlcpy(p_cmd_node->serial_num,&(p_cloud_cmd->serial_no[count][0]),sizeof(p_cmd_node->serial_num)) ;
			memcpy(&p_cmd_node->cmd_data,p_cloud_cmd,sizeof(dn_payload_t));
			p_cmd_node->p_next_cmd = NULL ;

			//build dev_cmd_data
			if(wr_cmdData_count == 0)
			{
				//only one time
				wr_cmdData_count += 1 ; 
				memset(temp_cmd_data,0,sizeof(temp_cmd_data));
				cmd_data_len = build_dev_cmdData(temp_cmd_data,sizeof(temp_cmd_data),p_cmd_node) ;
			}

			//insert into cloud-cmd list
			//p_cmd_node->cmd_data.data = NULL ; //this pointer be freeed when handle_cloud_cmdList
			p_cmd_node->cmd_data.data_len = 0 ;
			
			insert_cloudCmd_node(p_cmd_node) ;
		}

		//send broadcast msg now
		dest_addr = 0xFFFF ; //broadcast
		if(cmd_data_len > 0)
		{
			data_count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),wsn_cmdID,temp_cmd_data,cmd_data_len,local_addr,dest_addr);
		}
		else
		{
			data_count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),wsn_cmdID,NULL,0,local_addr,dest_addr);
		}
		if(data_count > 0)
		{
			send_cmd_toRemote(wsn_send_buf,count); //broadcast msg ,not append into dev waiting list
			//send_expectedResp_cmd_toRemote(wsn_send_buf,data_count) ;
		}

		
	}
	else
	{
		//save into cmd-list
		debug("handle not broadcast cloud cmd \n");
		for(count=0;count<dev_num;count++)
		{
			//p_cmd_node = query_cloudCmd_node(&p_cloud_cmd->serial_no[count],strlen(&p_cloud_cmd->serial_no[count]),wsn_cmdID) ;
			p_cmd_node = query_cloudCmd_node(&(p_cloud_cmd->serial_no[count][0]),31,wsn_cmdID) ;
			if(p_cmd_node != NULL)
			{
				//only one sn-wsn_cmdID pair in cloud_cmd list at the same time
				debug("found sn-wsn_cmdID pair in cloud_cmd list,discard this cmd now \n");
				continue ;
			}
				
			p_cmd_node = malloc(sizeof(cloud_cmd_list_t)) ;
			if(p_cmd_node == NULL)
			{
				debug("malloc cloud_cmd node failed when handle_cloud_cmd \n");
				return -1 ;
			}
			memset(p_cmd_node,0,sizeof(cloud_cmd_list_t));
			p_cmd_node->wsn_cmdID = wsn_cmdID ;
			p_cmd_node->action_type = action_type ;
			p_cmd_node->msg_type = WSN_ASCIITXT_MSG_TYPE ; // default msg type
			//strncpy(p_cmd_node->serial_num,&(p_cloud_cmd->serial_no[count][0]),sizeof(p_cmd_node->serial_num)) ;
			strlcpy(p_cmd_node->serial_num,&(p_cloud_cmd->serial_no[count][0]),sizeof(p_cmd_node->serial_num)) ;
			memcpy(&p_cmd_node->cmd_data,p_cloud_cmd,sizeof(dn_payload_t));
			p_cmd_node->p_next_cmd = NULL ;


			//send cmd to dev now
			p_dev_node = query_device_info_by_serialNum(p_cmd_node->serial_num) ;
			if(p_dev_node == NULL)
			{
				// not found dev-info in buf,send fail jsonMsg to cloud,and free this node now
				debug("not found dev-info in dev_list,sn: %s when handle_cloud_cmd \n",p_cmd_node->serial_num);
				
				//send fail jsonMsg to cloud

				//free this node 
				free(p_cmd_node) ;
				continue ;
			}
			
			memset(temp_cmd_data,0,sizeof(temp_cmd_data));
			cmd_data_len = build_dev_cmdData(temp_cmd_data,sizeof(temp_cmd_data),p_cmd_node) ;
			dest_addr = p_dev_node->device_info.device_addr ;
			if(cmd_data_len > 0)
			{
				data_count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),wsn_cmdID,temp_cmd_data,cmd_data_len,local_addr,dest_addr);
			}
			else
			{
				data_count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),wsn_cmdID,NULL,0,local_addr,dest_addr);
			}
			if(data_count > 0)
			{
				//send_cmd_toRemote(wsn_send_buf,count);
				//send_expectedResp_cmd_toRemote(wsn_send_buf,data_count) ;
				send_cmd_toDevWaitingList(wsn_send_buf,count,wsn_cmdID,p_dev_node) ; //send it when recv dev data-pak
			}

			//insert into cloud-cmd list
			//p_cmd_node->cmd_data.data = NULL ; //this pointer be freeed when handle_cloud_cmdList
			p_cmd_node->cmd_data.data_len = 0 ;
			
			insert_cloudCmd_node(p_cmd_node) ;
		}
	}
	

	return 0 ;
}
int handle_cloud_cmdList(void)
{

  dn_payload_t *p_temp = NULL;
  // handle cloud cmd list now
  p_temp = read_data_from_cloud_cmd();
  while (p_temp != NULL) {
    handle_cloud_cmd(p_temp);
    p_temp = read_data_from_cloud_cmd();
  }
  return 0;
}
#if 0
int handle_cloud_cmdList(void)
{
	int recv_cmd_num = 0 ;
	void *p_cloud_cmd_array = NULL ;
	dn_payload_t *p_temp = NULL ;
	int count = 0 ;

	p_cloud_cmd_array = query_cloud_cmd(&recv_cmd_num) ;
	if(p_cloud_cmd_array != NULL )
	{
		if(recv_cmd_num <= 0)
		{
			debug("maybe cloud-cmd para err,recv_cmd_num=%d \n",recv_cmd_num);
			free(p_cloud_cmd_array) ;
			return -1 ;
		}

		debug("recved %d cloud cmd,handle it now \n",recv_cmd_num);
		
		//handle cloud cmd list now
		p_temp = (dn_payload_t *)p_cloud_cmd_array ;
		for(count=0;count<recv_cmd_num;count++)
		{
			handle_cloud_cmd(&p_temp[count]) ;
			if(p_temp[count].data != NULL)
			{
				free(p_temp[count].data);
				p_temp[count].data = NULL ;
			}
		}

		//finish,free p_cloud_cmd_array now
		free(p_cloud_cmd_array);
		
	}
	else
	{
		//debug("no cloud-cmd \n");
	}


	return 0 ;
}
#endif
//for cloud-cmd-list

//called when dev_off_line,to free cloud-cmd nodes about this dev
int free_cloudCmdNode_byDevSn(uint8_t *p_sn,uint8_t sn_len)
{
	cloud_cmd_list_t *p_cur_node = NULL ;
	cloud_cmd_list_t *p_prev_node = NULL ;
	device_node_info_t *p_dev_node = NULL ;
	
	if(p_sn == NULL || sn_len <= 0)
	{
		debug("err para when free_cloudCmdNode_byDevSn \n");
		return 0 ;
	}

	p_cur_node = p_cloud_cmd_list_head ;

	while(p_cur_node != NULL)
	{
		if(strcmp(p_cur_node->serial_num,p_sn) == 0) //serial match
		{
			//send fail json-msg to cloud
			p_cur_node->result_resp = get_cloudCmd_result();
			p_dev_node = query_device_info_by_serialNum(p_sn) ;
			build_and_send_cloudCmdResp(p_cur_node,p_dev_node) ;

			//free this node now
			if(p_prev_node == NULL) //first node
			{
				p_cloud_cmd_list_head = p_cur_node->p_next_cmd ; //detacth p_cur_node
				//free mem
				free(p_cur_node) ;
				p_cur_node = p_cloud_cmd_list_head ;
			}
			else
			{
				p_prev_node->p_next_cmd = p_cur_node->p_next_cmd ; //detach p_cur_node
				//free mem
				free(p_cur_node) ;
				p_cur_node = p_prev_node->p_next_cmd ;
			}
			
		}
		else
		{
			p_prev_node = p_cur_node ;
			p_cur_node = p_cur_node->p_next_cmd ;
		}
	}

	return 0 ;
}

//called when recv dev resp,send success resp to cloud and free this node,usually only one match-node in the list
int handle_cloud_cmdNode_bySnAndCmdID(uint8_t *p_sn,uint8_t sn_len,uint8_t wsn_cmdID)
{
	cloud_cmd_list_t *p_cur_node = NULL ;
	cloud_cmd_list_t *p_prev_node = NULL ;
	device_node_info_t *p_dev_node = NULL ;

	if(p_sn == NULL || sn_len <= 0)
	{
		debug("err para when handle_cloud_cmdNode_bySnAndCmdID \n");
		return 0 ;
	}

	p_cur_node = p_cloud_cmd_list_head ;
	while(p_cur_node != NULL)
	{
		if( strcmp(p_cur_node->serial_num,p_sn) == 0 && wsn_cmdID == p_cur_node->wsn_cmdID) //serial match
		{
			//send success json-msg to cloud
			p_cur_node->result_resp = get_cloudCmd_result() ;
			p_dev_node = query_device_info_by_serialNum(p_sn) ;
			build_and_send_cloudCmdResp(p_cur_node,p_dev_node) ;

			//free this node now
			if(p_prev_node == NULL) //first node
			{
				p_cloud_cmd_list_head = p_cur_node->p_next_cmd ; //detacth p_cur_node
				//free mem
				free(p_cur_node) ;
				p_cur_node = p_cloud_cmd_list_head ;
			}
			else
			{
				p_prev_node->p_next_cmd = p_cur_node->p_next_cmd ; //detach p_cur_node
				//free mem
				free(p_cur_node) ;
				p_cur_node = p_prev_node->p_next_cmd ;
			}
			
		}
		else
		{
			p_prev_node = p_cur_node ;
			p_cur_node = p_cur_node->p_next_cmd ;
		}
	}

	return 0 ;
}


cloud_cmd_list_t *query_cloudCmd_node(uint8_t *p_sn,uint8_t sn_len,uint8_t wsn_cmdID)
{
	cloud_cmd_list_t *p_cur_node = NULL ;

	if(p_sn == NULL || sn_len <= 0)
	{
		debug("err para when query_cloudCmd_node \n");
		return 0 ;
	}

	p_cur_node = p_cloud_cmd_list_head ;
	while(p_cur_node != NULL)
	{
		if(strcmp(p_sn,p_cur_node->serial_num) == 0 && wsn_cmdID == p_cur_node->wsn_cmdID) //serial match
		{
			return p_cur_node ;
		}

		p_cur_node = p_cur_node->p_next_cmd ;
	}

	return NULL ;
}


int insert_cloudCmd_node(cloud_cmd_list_t *p_node)
{
	cloud_cmd_list_t *p_cur_node = NULL ;
	
	if(p_node == NULL )
	{
		debug("err para when insert_cloudCmd_node \n");
		return -1 ;
	}

	p_cur_node = p_cloud_cmd_list_head ;
	if(p_cloud_cmd_list_head == NULL)
	{
		p_cloud_cmd_list_head = p_node ;
		return 0 ;
	}

	while(p_cur_node != NULL)
	{
		if(p_cur_node->p_next_cmd == NULL)
		{
			p_cur_node->p_next_cmd = p_node ;
			break ;
		}

		p_cur_node = p_cur_node->p_next_cmd ;
	}

	return 0 ;
}


//end cloud-cmd-list


int nvram_get_gw_gpsData(void)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int ret_val = 0 ;

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram_status get gps_info");
	ret_val = execute_cmd(cmd_buf,cmd_result) ;

	if(ret_val < 0)
	{
		debug("err when nvram_get_gw_gpsData \n");
		return -1 ;
	}

	if(strlen(cmd_result) == 0)
	{
		debug("not found gps info in nvram \n");
		return -1 ;
	}

	// gps_info        113.595616,34.797783  //x,y
	sscanf(cmd_result,"%f,%f",&gw_gps_data.x_val,&gw_gps_data.y_val) ;
	debug("gw_gps_info: x=%f,y=%f \n",gw_gps_data.x_val,gw_gps_data.y_val);

	return 0 ;
}



int nvram_set_trigRelayAlarm_conf(void)
{
	uint8_t cmd_buf[128] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int ret_val = 0 ;

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	snprintf(cmd_buf,sizeof(cmd_buf)-1,"nvram set HN_TRIG_RELAY=%d",wsn_net_conf_data.trig_relay);
	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("nvram set HN_TRIG_RELAY failed when nvram_set_trigRelayAlarm_conf \n");
	}

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	snprintf(cmd_buf,sizeof(cmd_buf)-1,"nvram set HN_TRIG_ALARM=%d",wsn_net_conf_data.trig_alarm);
	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("nvram set HN_TRIG_ALARM failed when nvram_set_trigRelayAlarm_conf \n");
	}

	return 0 ;
}

sint8_t nvram_get_trigRelay_conf(sint8_t *p_trig_relay)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int ret_val = 0 ;
	sint8_t temp_trig_relay = 0 ;

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram get HN_TRIG_RELAY");
	ret_val = execute_cmd(cmd_buf,cmd_result) ;

	if(ret_val < 0 )
	{
		debug("err when nvram_get_trigRelay_conf,maybe no trig_relay conf in nvram now \n");
		if(p_trig_relay != NULL)
		{
			*p_trig_relay = 0 ;
		}
		return -1 ;
	}

	temp_trig_relay = atoi(cmd_result) ;
	//debug("nvram_get_trigRelay_conf trig_relay=%d \n",temp_trig_relay) ;

	if(p_trig_relay != NULL)
	{
		*p_trig_relay = temp_trig_relay ;
	}

	return temp_trig_relay ;
}


sint8_t nvram_get_trigAlarm_conf(sint8_t *p_trig_alarm)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int ret_val = 0 ;
	sint8_t temp_trig_alarm = 0 ;

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram get HN_TRIG_ALARM");
	ret_val = execute_cmd(cmd_buf,cmd_result) ;

	if(ret_val < 0 )
	{
		debug("err when nvram_get_trigAlarm_conf,maybe no trig_alarm conf in nvram now \n");
		if(p_trig_alarm != NULL)
		{
			*p_trig_alarm = 0 ;
		}
		return -1 ;
	}

	temp_trig_alarm = atoi(cmd_result) ;
	//debug("nvram_get_trigAlarm_conf trig_alarm=%d \n",temp_trig_alarm) ;

	if(p_trig_alarm != NULL)
	{
		*p_trig_alarm = temp_trig_alarm ;
	}

	return temp_trig_alarm ;
}

void nvram_get_trigerAlarmRelay(void)
{
	nvram_get_trigAlarm_conf(&wsn_net_conf_data.trig_alarm) ;
	nvram_get_trigRelay_conf(&wsn_net_conf_data.trig_relay) ;

	return ;
}

int nvram_get_clearRemoteAlarm_conf(uint8_t *p_clearRemoteAlarm)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int ret_val = 0 ;
	uint8_t clear_remoteAlarm = 0 ;

	#if 0
	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram get HN_CLEAR_REMOTE_ALARM");
	ret_val = execute_cmd(cmd_buf,cmd_result) ;

	if(ret_val < 0 || strlen(cmd_result) == 0)
	{
		debug("err when nvram_get_trigAlarm_conf,maybe no trig_alarm conf in nvram now \n");
		if(p_clearRemoteAlarm != NULL)
		{
			*p_clearRemoteAlarm = 0 ;
		}
		return -1 ;
	}

	clear_remoteAlarm = atoi(cmd_result) ;
	//debug("nvram_get_trigAlarm_conf trig_alarm=%d \n",temp_trig_alarm) ;
	#endif

	if(p_clearRemoteAlarm != NULL)
	{
		//*p_clearRemoteAlarm = clear_remoteAlarm ;
		*p_clearRemoteAlarm = 1 ; //initial-value==1
	}

	return 0 ;
}

//called when recved cloud_groupAlarm cmd,cloud take over the alarm-manage now
void reset_clearRemoteAlarm_flag(void)
{
	//only reset the var in the buf,not change the var in the nvram,(clearRemoteAlarm be efective even if gw-hub centrlhub program restart or gw restart)
	// the clearRemoteAlarm var in the nvram changed only when triger-set changed
	wsn_net_conf_data.clear_remoteAlarmRelay = 0 ;
	
	return ;
}


uint8_t nvram_get_devOffline_threshold(uint8_t *p_threshold)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int ret_val = 0 ;
	uint8_t temp_threshold = 0 ;

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram get HN_OFFLINE_NUM");
	ret_val = execute_cmd(cmd_buf,cmd_result) ;

	if(ret_val < 0 )
	{
		debug("err when nvram_get_devOffline_threshold,maybe no threshold conf in nvram now \n");
		if(p_threshold != NULL)
		{
			*p_threshold = 0 ;
		}
		return 0 ;
	}

	temp_threshold = atoi(cmd_result) ;
	debug("nvram_get_devOffline_threshold threshold=%d \n",temp_threshold) ;

	if(p_threshold != NULL)
	{
		*p_threshold = temp_threshold ;
	}

	return temp_threshold ;
}

int nvram_query_devNode_num(void)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int dev_node_num = 0 ;
	int ret_val = 0 ;

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram_status get HN_DIAG_DEV_TOTAL_NUM");
	ret_val = execute_cmd(cmd_buf,cmd_result) ;

	if(ret_val < 0 )
	{
		debug("err when nvram_query_dev_node_num,maybe no dev in nvram now \n");
		return 0 ;
	}

	dev_node_num = atoi(cmd_result) ;
	debug("dev_node_num=%d \n",dev_node_num) ;

	return dev_node_num ;
}

int nvram_update_devNode_num(int dev_node_num)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int ret_val = 0 ;
	//int dev_node_num = 1 ;

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram_status set HN_DIAG_DEV_TOTAL_NUM=%d",dev_node_num);
	ret_val = execute_cmd(cmd_buf,cmd_result) ;

	if(ret_val <0)
	{
		debug("nvram_status set err when nvram_insert_dev_node_num \n") ;
		return -1 ;
	}

	#if 0
	ret_val = execute_cmd("nvram commit",cmd_result);
	if(ret_val < 0 )
	{
		debug("nvram_cmmit err when nvram_insert_dev_node_num \n");
		return -1 ;
	}
	#endif

	return 0 ;
	
}

int nvram_updateAdd_devNode_num(void)
{	
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int dev_node_num = 0 ;
	int ret_val = 0 ;

	dev_node_num = nvram_query_devNode_num() ;
	if(dev_node_num >= 0)
	{
		dev_node_num++ ;
		memset(cmd_buf,0,sizeof(cmd_buf)) ;
		memset(cmd_result,0,sizeof(cmd_result)) ;
		sprintf(cmd_buf,"nvram_status set HN_DIAG_DEV_TOTAL_NUM=%d",dev_node_num);

		ret_val = execute_cmd(cmd_buf,cmd_result) ;
		if(ret_val < 0)
		{
			debug("execute nvram_status_set failed when nvram_updateAdd_devNode_num \n"); 
			return -1 ;
		}

		#if 0
		ret_val = execute_cmd("nvram commit",cmd_result);
		if(ret_val < 0 )
		{
			debug("nvram_cmmit err when nvram_updateAdd_devNode_num \n");
			return -1 ;
		}
		#endif

		
	}
	else
	{
		debug("execute query dev_node_num failed when nvram_updateAdd_devNode_num \n");
	}


	return dev_node_num ;
	
}

int nvram_updateSub_devNode_num(void)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int dev_node_num = 0 ;
	int ret_val = 0 ;

	dev_node_num = nvram_query_devNode_num();
	if(dev_node_num > 0)
	{
		dev_node_num-- ;
		memset(cmd_buf,0,sizeof(cmd_buf)) ;
		memset(cmd_result,0,sizeof(cmd_result)) ;
		sprintf(cmd_buf,"nvram_status set HN_DIAG_DEV_TOTAL_NUM=%d",dev_node_num);

		ret_val = execute_cmd(cmd_buf,cmd_result) ;
		if(ret_val < 0)
		{
			debug("execute nvram_status_set failed when nvram_updateSub_devNode_num \n");
			return -1 ;
		}

		#if 0
		ret_val = execute_cmd("nvram commit",cmd_result);
		if(ret_val < 0 )
		{
			debug("nvram_cmmit err when nvram_updateSub_devNode_num \n");
			return -1 ;
		}
		#endif
		
	}
	else
	{
		debug("err when nvram_updateSub_devNode_num,maybe no dev_node in nvram \n") ;
		return -1 ;
	}

	return dev_node_num ;
}

int nvram_sync_devNode_num(void)
{	
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int dev_node_num = 0 ;
	int ret_val = 0 ;


	dev_node_num = varname_sn_pair_head.total_dev_num ;
	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram_status set HN_DIAG_DEV_TOTAL_NUM=%d",dev_node_num);

	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("execute nvram_status_set failed when nvram_sync_devNode_num \n"); 
		return -1 ;
	}

	#if 0
	ret_val = execute_cmd("nvram commit",cmd_result);
	if(ret_val < 0 )
	{
		debug("nvram_cmmit err when nvram_sync_devNode_num \n");
		return -1 ;
	}
	#endif

	return dev_node_num ;
	
}

static modbus_cfg_t s_modbus_cfg;
int nvram_get_all_devModbusCfg(void)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int mod_bus_itemNum = 0 ;
	int ret_val = 0 ;
	uint16_t modbus_addr = 0xFFFF ;
	int count = 0 ;
	//uint8_t dev_sn[20] ;
	uint8_t modbus_addr_str[10] ;
	//init
	for(count=0;count<MODBUS_CFG_MAXSIZE;count++)
	{
	  memset(s_modbus_cfg.modbus_sn_addr[count].dev_sn,0,sizeof(s_modbus_cfg.modbus_sn_addr[count].dev_sn));
      s_modbus_cfg.modbus_sn_addr[count].modbus_addr=0xFFFF;
	}

	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram get MODBUS_INST_SIZE");
	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("execute nvram_get MODBUS_INST_SIZE failed when nvram_get_devModbusAddr \n"); 
		return -1;
	}
	mod_bus_itemNum = atoi(cmd_result) ;
	if(mod_bus_itemNum <= 0 || mod_bus_itemNum > MODBUS_CFG_MAXSIZE)
	{
		debug("mod_bus_itemNum = %d err when nvram_get_devModbusAddr \n",mod_bus_itemNum);
		return -1 ;
	}
	s_modbus_cfg.itemNum = mod_bus_itemNum ;

	//get modbus item 
	for(count=1;count<=mod_bus_itemNum;count++)
	{
		memset(cmd_buf,0,sizeof(cmd_buf)) ;
		memset(cmd_result,0,sizeof(cmd_result)) ;
		//memset(dev_sn,0,sizeof(dev_sn)) ;
		memset(modbus_addr_str,0,sizeof(modbus_addr_str));
		sprintf(cmd_buf,"nvram get MODBUS_INST_ADDR%d",count);
		ret_val = execute_cmd(cmd_buf,cmd_result) ;
		if(ret_val < 0)
		{
			debug("nvram get MODBUS_INST_ADDR%d failed \n",count);
			//maybe addr be not continus,get next one
			continue ;  
		}

		//sscanf(cmd_result,"%12s,%d",dev_sn,&modbus_addr) ;
		sscanf(cmd_result,"%[^,],%[^,]",s_modbus_cfg.modbus_sn_addr[count-1].dev_sn,modbus_addr_str) ;
		debug("count=%d cmd_result=%s,dev_sn=%s,modbus_addr_str=%s \n",count,cmd_result,s_modbus_cfg.modbus_sn_addr[count-1].dev_sn,modbus_addr_str);
		s_modbus_cfg.modbus_sn_addr[count-1].modbus_addr = atoi(modbus_addr_str) ;
		
	}
	return 0;
}


int nvram_get_devModbusAddr(uint8_t *p_dev_sn)
{
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int mod_bus_itemNum = s_modbus_cfg.itemNum ;
	int ret_val = 0 ;
	uint16_t modbus_addr = 0xFFFF ;
	int count = 0 ;
	uint8_t dev_sn[20] ;
	uint8_t modbus_addr_str[10] ;
	
	
	if(p_dev_sn == NULL || p_dev_sn[0] == 0)
	{
		return -1 ;
	}

	//get modbus item 
	for(count=0;count<mod_bus_itemNum;count++)
	{
        //debug("strcmp %s %s\n",s_modbus_cfg.modbus_sn_addr[count].dev_sn,p_dev_sn);
		if(strncmp(s_modbus_cfg.modbus_sn_addr[count].dev_sn,p_dev_sn,strlen(p_dev_sn)) == 0)
		{
			//match
			modbus_addr = s_modbus_cfg.modbus_sn_addr[count].modbus_addr;
			debug("found dev sn=%s,modbus_addr=%d \n",p_dev_sn,modbus_addr);
			break ;
		}
		else
		{
			//not match,reset modbus_addr
			modbus_addr = 0xFFFF ;
		}
		
	}

	return modbus_addr ;
}



void free_varName_sn_pair_list(void)
{
	nvram_dev_varname_sn_pair_t *p_temp = NULL ;
	nvram_dev_varname_sn_pair_t *p_next = NULL ;

	p_temp = varname_sn_pair_head.p_next ;
	while(p_temp != NULL)
	{
		p_next = p_temp->p_next ;
		free(p_temp) ;
		p_temp = p_next ;
	}

	return ;
}


nvram_dev_varname_sn_pair_t *query_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len)
{
	nvram_dev_varname_sn_pair_t *p_pair = NULL ;

	if(p_sn == NULL || sn_len <= 0)
	{
		debug("err para when query_varName_sn_pair \n") ;
		return NULL ;
	}

	debug("query_varName_sn_pair: sn=%s,sn_len=%d \n",p_sn,sn_len);

	if(varname_sn_pair_head.total_dev_num <= 0)
	{
		debug("total dev num == 0 when query_varName_sn_pair \n");
		return NULL ;
	}
	
	p_pair = varname_sn_pair_head.p_next ;
	//debug("varname_sn pair head.p_next=0x%x \n",varname_sn_pair_head.p_next);

	while(p_pair != NULL)
	{
		//debug("varname_sn_list:dev sn:%s \n",p_pair->nvram_dev_sn);
		//if(strncmp(p_pair->nvram_dev_sn,p_sn,sn_len) == 0)
		if(strcmp(p_pair->nvram_dev_sn,p_sn) == 0)
		{
			return p_pair ;
		}
		p_pair = p_pair->p_next ;
	}

	return NULL ;
}


nvram_dev_varname_sn_pair_t *query_varName_sn_pairList_end(void)
{
	nvram_dev_varname_sn_pair_t *p_list_end = NULL ;

	#if 0
	if(varname_sn_pair_head.total_dev_num <= 0 || varname_sn_pair_head.p_next == NULL)
	{
		p_list_end = &varname_sn_pair_head ;
		return p_list_end ;
	}

	p_list_end = varname_sn_pair_head.p_next ;
	while(p_list_end != NULL)
	{
		if(p_list_end->p_next == NULL)
		{
			break ;
		}
		
		p_list_end = p_list_end->p_next ;
	}
	#endif

	p_list_end = &varname_sn_pair_head ;
	while(p_list_end != NULL)
	{
		if(p_list_end->p_next == NULL)
		{
			return p_list_end ;
		}
		
		p_list_end = p_list_end->p_next ;
	}

	return NULL ;
}


int add_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len,uint8_t *p_var_name,uint8_t var_name_len)
{
	nvram_dev_varname_sn_pair_t *p_pair = NULL ;
	nvram_dev_varname_sn_pair_t *p_list_end = NULL ;

	if(p_sn == NULL || sn_len<= 0 || p_var_name == NULL || var_name_len <= 0)
	{
		debug("err para when add_varName_sn_pair \n");
		return -1 ;
	}

	debug("add varName_sn pair,sn=%s,varName=%s \n",p_sn,p_var_name);
	
	p_pair = query_varName_sn_pair(p_sn,sn_len);
	if(p_pair != NULL)
	{
		debug("sn:%s already in buf list \n",p_sn);
		return 0 ;
	}

	p_list_end = query_varName_sn_pairList_end();
	if(p_list_end == NULL)
	{
		debug("err not found list_end in buf when add_varName_sn_pair \n") ;
		return -1 ;
	}

	if(sn_len >= MAX_DEV_SN_LEN || var_name_len >= MAX_NVRAM_VARNAME_LEN)
	{
		debug("err sn_len or var_name_len when add_varName_sn_pair \n");
		return -1 ;
	}

	p_pair = malloc(sizeof(nvram_dev_varname_sn_pair_t)) ;
	if(p_pair == NULL)
	{
		debug("malloc err when add_varName_sn_pair \n");
		return -1 ;
	}
	memset(p_pair,0,sizeof(nvram_dev_varname_sn_pair_t)) ;
	strlcpy(p_pair->nvram_dev_sn,p_sn,sizeof(p_pair->nvram_dev_sn)) ;
	strlcpy(p_pair->nvram_var_name,p_var_name,sizeof(p_pair->nvram_var_name));

	//attach the pair into varname_sn_pair_head list
	p_list_end->p_next = p_pair ;
	
	varname_sn_pair_head.total_dev_num++ ;

	return 0 ;
	
}

int del_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len)
{
	nvram_dev_varname_sn_pair_t *p_list ;
	nvram_dev_varname_sn_pair_t *p_prev ;
	
	if(p_sn == NULL || sn_len <= 0)
	{
		debug("err para when del_varName_sn_pair \n");
		return -1 ;
	}

	p_list = varname_sn_pair_head.p_next ;
	p_prev = &varname_sn_pair_head ;
	while(p_list != NULL)
	{
		//if(strncmp(p_list->nvram_dev_sn,p_sn,sn_len) == 0)
		if(strcmp(p_list->nvram_dev_sn,p_sn) == 0)
		{
			p_prev->p_next = p_list->p_next ;
			free(p_list) ;
			if(varname_sn_pair_head.total_dev_num > 0)
			{
				varname_sn_pair_head.total_dev_num-- ;
			}
			
			break ;
		}

		p_prev = p_list ;
		p_list = p_list->p_next ;
	}

	return 0 ;
}

int del_varName_sn_pair_list(void)
{
	nvram_dev_varname_sn_pair_t *p_list ;
	nvram_dev_varname_sn_pair_t *p_next ;

	p_list = varname_sn_pair_head.p_next ;
	while(p_list != NULL)
	{
		p_next = p_list->p_next ;
		free(p_list) ;
		p_list = p_next ;
	}

	return 0 ;
}


//called when app-restart
int nvram_get_varName_sn_pair_list(void)
{
	int dev_node_num = 0 ;
	int count = 0 ;
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	uint8_t var_name[MAX_NVRAM_VARNAME_LEN] ;
	uint8_t dev_sn[MAX_DEV_SN_LEN];

	nvram_dev_varname_sn_pair_t *p_temp_pair = NULL ;
	nvram_dev_varname_sn_pair_t *p_list = NULL ;

	dev_node_num = nvram_query_devNode_num();

	if(dev_node_num <= 0 || dev_node_num>255)
	{
		debug("not found dev node in nvram \n");
		nvram_delete_dev_diaginfo_list() ;
		nvram_del_varName_sn_pair_list() ;
		return 0 ;
	}

	varname_sn_pair_head.total_dev_num = dev_node_num ;
	p_list = &varname_sn_pair_head ; 
	
	//dev_node_num start from 1 to dev_node_num
	for(count=1;count<=dev_node_num;count++)
	{
		memset(cmd_buf,0,sizeof(cmd_buf)) ;
		memset(cmd_result,0,sizeof(cmd_result)) ;
		memset(var_name,0,sizeof(var_name));
		memset(dev_sn,0,sizeof(dev_sn));
		sprintf(cmd_buf,"nvram get HN_DIAG_DEV_INFO_SN_%d",count);
		ret_val = execute_cmd(cmd_buf,cmd_result) ;
		if(ret_val < 0 )
		{
			debug("nvram_get HN_DIAG_DEV_INFO_SN_ %d err when nvram_get_varName_sn_pair_list \n",count);
			continue ;
		}

		sscanf(cmd_result,"%[^,],%[^,]",var_name,dev_sn) ;
		if(strlen(var_name) == 0 || strlen(dev_sn) == 0)
		{
			debug("nvram get HN_DIAG_DEV_INFO_SN_ %d err,var_name or dev_sn NULL \n",count);
			continue ;
		}

		if(strlen(var_name) >= sizeof(p_temp_pair->nvram_var_name) || strlen(dev_sn) >= sizeof(p_temp_pair->nvram_dev_sn))
		{
			debug("HN_DIAG_DEV_INFO_SN_ %d var_name_len or dev_sn len err when nvram_get_varName_sn_pair_list \n",count);
			//free_varName_sn_pair_list();
			//return -1 ;
			p_temp_pair = NULL ;
			continue ;
		}

		p_temp_pair = malloc(sizeof(nvram_dev_varname_sn_pair_t)) ;
		if(p_temp_pair == NULL)
		{
			debug("malloc err when nvram_get_varName_sn_pair_list \n");
			return -1 ;
		}
		memset(p_temp_pair,0,sizeof(nvram_dev_varname_sn_pair_t)) ;
		
		strcpy(p_temp_pair->nvram_var_name,var_name);
		strcpy(p_temp_pair->nvram_dev_sn,dev_sn) ;

		//atach into the list
		p_list->p_next = p_temp_pair ;
		p_list = p_temp_pair ;
		
	}


	return 0 ;
	
}




int nvram_query_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len,uint8_t *p_varName_buf,uint8_t varName_buf_len)
{
	int dev_node_num = 0 ;
	int count = 0 ;
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	uint8_t var_name[MAX_NVRAM_VARNAME_LEN] ;
	uint8_t dev_sn[MAX_DEV_SN_LEN];

	if(p_sn == NULL || sn_len <= 0)
	{
		return 0 ;
	}

	dev_node_num = nvram_query_devNode_num();
	if(dev_node_num <= 0 || dev_node_num>255)
	{
		debug("no dev_varName_sn pair in nvram when nvram_query_varName_sn_pair\n");
		return 0 ;
	}

	//dev_node_num start from 1 to dev_node_num
	for(count=1;count<=dev_node_num;count++)
	{
		memset(cmd_buf,0,sizeof(cmd_buf)) ;
		memset(cmd_result,0,sizeof(cmd_result)) ;
		memset(var_name,0,sizeof(var_name));
		memset(dev_sn,0,sizeof(dev_sn));
		sprintf(cmd_buf,"nvram get HN_DIAG_DEV_INFO_SN_%d",count);
		ret_val = execute_cmd(cmd_buf,cmd_result) ;
		if(ret_val < 0 )
		{
			debug("nvram_get HN_DIAG_DEV_INFO_SN_ %d err when nvram_query_varName_sn_pair \n",count);
			continue ;
		}

		sscanf(cmd_result,"%[^,],%[^,]",var_name,dev_sn) ;
		if(strlen(var_name) == 0 || strlen(dev_sn) == 0)
		{
			debug("not found HN_DIAG_DEV_INFO_SN_ %d pair when  nvram_query_varName_sn_pair \n",count) ;
			continue ;
		}

		//if(strncmp(dev_sn,p_sn,sn_len) == 0) //match
		if(strcmp(dev_sn,p_sn) == 0) //match
		{
			if(p_varName_buf != NULL && strlen(var_name) < varName_buf_len)
			{
				strcpy(p_varName_buf,var_name);
			}
			
			return 1 ;
		}
		
	}

	return 0 ;
}

int nvram_add_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len,uint8_t *p_var_name,uint8_t var_name_len)
{
	int dev_node_num = 0 ;
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	
	
	if(p_sn == NULL || sn_len <= 0 || p_var_name == NULL || var_name_len <= 0)
	{
		debug("err para when nvram_add_varName_sn_pair \n");
		return -1 ;
	}

	if(nvram_query_varName_sn_pair(p_sn,sn_len,NULL,0) > 0) //found pair in nvram
	{	
		debug("sn: %s already in nvram \n",p_sn) ;
		return 0 ;
	}

	dev_node_num = nvram_query_devNode_num();
	//dev_node_num++ ; //start from 1,before add varName_sn pair,we have added dev_diaginfo and dev_node_num already
	debug("before add varName_sn pair,dev_node_num=%d in nvram \n",dev_node_num);
	memset(cmd_buf,0,sizeof(cmd_buf)) ;
	memset(cmd_result,0,sizeof(cmd_result)) ;
	
	sprintf(cmd_buf,"nvram set HN_DIAG_DEV_INFO_SN_%d=%s,%s",dev_node_num,p_var_name,p_sn);
	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("execute nvram_set failed when nvram_add_varName_sn_pair \n");
		return -1 ;
	}

	#if 0
	ret_val = execute_cmd("nvram commit",cmd_result);
	if(ret_val < 0 )
	{
		debug("nvram_cmmit err when nvram_add_varName_sn_pair \n");
		return -1 ;
	}
	#endif
	
	return 0 ;
	
}


int nvram_del_varName_sn_pair(uint8_t *p_sn,uint8_t sn_len)
{
	int dev_node_num = 0 ;
	int count = 0 ;
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	uint8_t var_name[MAX_NVRAM_VARNAME_LEN] ;
	uint8_t dev_sn[MAX_DEV_SN_LEN];

	if(p_sn == NULL || sn_len <= 0)
	{
		debug("err para when nvram_del_varName_sn_pair");
		return 0 ;
	}
	
	dev_node_num = nvram_query_devNode_num();
	if(dev_node_num <= 0 || dev_node_num>255)
	{
		debug("no varName_sn pair in nvram when nvram_del_varName_sn_pair \n");
		return 0 ;
	}

	//dev_node_num start from 1 to dev_node_num
	for(count=1;count<=dev_node_num;count++)
	{
		memset(cmd_buf,0,sizeof(cmd_buf)) ;
		memset(cmd_result,0,sizeof(cmd_result)) ;
		memset(var_name,0,sizeof(var_name));
		memset(dev_sn,0,sizeof(dev_sn));
		sprintf(cmd_buf,"nvram get HN_DIAG_DEV_INFO_SN_%d",count);
		ret_val = execute_cmd(cmd_buf,cmd_result) ;
		if(ret_val < 0 )
		{
			debug("nvram_get HN_DIAG_DEV_INFO_SN_ %d err when nvram_del_varName_sn_pair \n",count);
			continue ;
		}

		sscanf(cmd_result,"%[^,],%[^,]",var_name,dev_sn) ;
		if(strlen(var_name) == 0 || strlen(dev_sn) == 0)
		{
			debug("not found HN_DIAG_DEV_INFO_SN_ %d pair when  nvram_del_varName_sn_pair \n",count) ;
			continue ;
		}

		//if(strncmp(dev_sn,p_sn,sn_len) == 0) //match
		if(strcmp(dev_sn,p_sn) == 0) //match
		{
			sprintf(cmd_buf,"nvram delete HN_DIAG_DEV_INFO_SN_%d",count);
			ret_val = execute_cmd(cmd_buf,cmd_result) ;
			if(ret_val < 0)
			{	
				debug("nvram unset err when nvram_del_varName_sn_pair \n");
				return -1 ;
			}

			#if 0
			ret_val = execute_cmd("nvram commit",cmd_result);
			if(ret_val < 0 )
			{
				debug("nvram commit err when nvram_del_varName_sn_pair \n");
				return -1 ;
			}
			#endif
		}
		
	}


	return 0 ;
	
}


void nvram_del_varName_sn_pair_list(void)
{
	int count = 0 ; //default max 20
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int dev_node_num = 0 ;

	dev_node_num = nvram_query_devNode_num() ;
	if(dev_node_num <=0 || dev_node_num>255)
	{
		dev_node_num = 5 ;
	}

	for(count=1;count<=dev_node_num;count++)
	{
		memset(cmd_buf,0,sizeof(cmd_buf));
		memset(cmd_result,0,sizeof(cmd_result)) ;
		snprintf(cmd_buf,sizeof(cmd_buf)-1,"nvram delete HN_DIAG_DEV_INFO_SN_%d",count);

		ret_val = execute_cmd(cmd_buf,cmd_result) ;
		if(ret_val == -1)
		{

			break ;
		}
	}

	//execute_cmd("nvram commit",cmd_result);

	return ;
}


int transfer_sn(uint8_t *p_sn,uint8_t *buf,uint8_t buf_len)
{
	uint8_t buf_count = 0 ;
	uint8_t src_count = 0 ;

	if(p_sn == NULL || buf == NULL || buf_len<=0)
	{
		debug("err para when transfer_sn \n");
		return -1 ;
	}

	buf_count = 0 ;
	src_count = 0 ;
	while( p_sn[src_count] != 0)
	{
		if(p_sn[src_count] != ' ') //space
		{
			buf[buf_count++] = p_sn[src_count++] ;
		}
		else
		{
			strncpy(&buf[buf_count],"\ ",2) ;
			buf_count += 2 ;
			src_count++ ;
		}

		if(buf_count >= buf_len)
		{
			break ;
		}
	}


	return 0;
}

int transfer_dirChar_inSn(uint8_t *p_sn,uint8_t *buf,uint8_t buf_len)
{
	uint8_t buf_count = 0 ;
	uint8_t src_count = 0 ;

	if(p_sn == NULL || buf == NULL || buf_len<=0)
	{
		debug("err para when transfer_sn \n");
		return -1 ;
	}

	buf_count = 0 ;
	src_count = 0 ;
	while( p_sn[src_count] != 0)
	{
      if(((p_sn[src_count]>='A') &&(p_sn[src_count]<='Z'))    \
      ||((p_sn[src_count]>='a') &&(p_sn[src_count]<='z'))    \
      ||((p_sn[src_count]>='0') &&(p_sn[src_count]<='9'))){
        buf[buf_count++] = p_sn[src_count++] ;
      }else if(p_sn[src_count] == '/') {  // '/' -> "\\"
        buf[buf_count++]='\\';
        buf[buf_count++]='\\';
		src_count++ ;
  	  }else{
        buf[buf_count++]='\\';
        buf[buf_count++]=p_sn[src_count++];
	  }
  	  if(buf_count >= buf_len)
	  {
	    break ;
	  }
	}



	return 0;
}



int nvram_query_dev_diaginfo(uint8_t *p_sn,uint8_t sn_len)
{
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	//uint8_t temp_var_name[MAX_NVRAM_VARNAME_LEN] ;
	nvram_dev_varname_sn_pair_t *p_pair = NULL ;

	if(p_sn == NULL  || sn_len <= 0)
	{
		debug("err para when nvram_query_dev_diaginfo \n");
		return -1 ;
	}

	memset(cmd_buf,0,sizeof(cmd_buf));
	memset(cmd_result,0,sizeof(cmd_result)) ;
	//memset(temp_var_name,0,sizeof(temp_var_name));

	p_pair = query_varName_sn_pair(p_sn,sn_len) ;
	if(p_pair == NULL)
	{
		debug("not found varName_sn pair in the buf when nvram_query_dev_diaginfo\n");
		return -1 ;
	}

	sprintf(cmd_buf,"nvram_status get %s",p_pair->nvram_var_name);
	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("err execute nvram_status get %s when nvram_query_dev_diaginfo \n",p_pair->nvram_var_name);
		return -1 ;
	}
	else
	{
		debug("found dev diaginfo in nvram:%s \n",cmd_result);
	}
	
	return 1 ;
	
}


int nvram_addOrUpdate_dev_diaginfo(dev_diag_info_t *p_diag_info)
{
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	uint8_t temp_var_name[MAX_NVRAM_VARNAME_LEN] ;
	nvram_dev_varname_sn_pair_t *p_pair = NULL ;
	uint8_t dev_nod_num = 0 ;
	uint8_t nvram_sn[20] ;

	if(p_diag_info == NULL)
	{
		debug("err para when nvram_add_dev_diaginfo \n");
		return -1 ;
	}

	ret_val = nvram_query_dev_diaginfo(p_diag_info->dev_sn,strlen(p_diag_info->dev_sn)) ;
	if(ret_val > 0)
	{
		debug("dev_diaginfo already in nvram when nvram_addAndUpdate_dev_diaginfo ,update now\n");
		nvram_update_dev_diaginfo(p_diag_info);
		return 0 ;
	}

	
	//insert varName_sn pair into buf
	dev_nod_num = varname_sn_pair_head.total_dev_num + 1 ;
	memset(temp_var_name,0,sizeof(temp_var_name));
	sprintf(temp_var_name,"HN_DIAG_DEV_INFO%d",dev_nod_num);
	
	add_varName_sn_pair(p_diag_info->dev_sn,strlen(p_diag_info->dev_sn),temp_var_name,strlen(temp_var_name)) ;
	dev_nod_num = varname_sn_pair_head.total_dev_num ;

	memset(nvram_sn,0,sizeof(nvram_sn));
	transfer_sn(p_diag_info->dev_sn,nvram_sn,sizeof(nvram_sn)-1) ;
	debug("after transfer_sn,sn=%s \n",nvram_sn) ;

	//insert diaginfo into nvram
	memset(cmd_buf,0,sizeof(cmd_buf));
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram_status set HN_DIAG_DEV_INFO%d=%d,%s,%d,%d,%d,%d,%d,%d",dev_nod_num,
																			p_diag_info->dev_addr,
																			nvram_sn,
																			p_diag_info->data_pak_flag,
																			p_diag_info->dev_info_flag,
																			p_diag_info->sen_format_flag,
																			p_diag_info->sen_limit_flag,
																			p_diag_info->rssi_val,
																			p_diag_info->dev_online_flag);

	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("err when nvram_status set when nvram_addAndUpdate_dev_diaginfo \n") ;
		return -1 ;
	}

	#if 0
	ret_val = execute_cmd("nvram commit",cmd_result);
	if(ret_val < 0)
	{
		debug("err nvram commit when nvram_addAndUpdate_dev_diaginfo \n") ;
		return -1 ;
	}
	#endif

	//update dev_nod_num
	nvram_sync_devNode_num();

	#if 0
	//insert varName_sn pair into nvram
	ret_val = nvram_add_varName_sn_pair(p_diag_info->dev_sn,strlen(p_diag_info->dev_sn),temp_var_name,strlen(temp_var_name)) ;
	if(ret_val < 0 )
	{
		debug("err nvram_add_varName_sn_pair when nvram_add_dev_diaginfo \n");
		return -1 ;
	}
	#endif

	return 0 ;
	
}


int nvram_add_dev_diaginfo(dev_diag_info_t *p_diag_info)
{
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	uint8_t temp_var_name[MAX_NVRAM_VARNAME_LEN] ;
	nvram_dev_varname_sn_pair_t *p_pair = NULL ;
	uint8_t dev_nod_num = 0 ;
	uint8_t nvram_sn[20] ;

	if(p_diag_info == NULL)
	{
		debug("err para when nvram_add_dev_diaginfo \n");
		return -1 ;
	}

	ret_val = nvram_query_dev_diaginfo(p_diag_info->dev_sn,strlen(p_diag_info->dev_sn)) ;
	if(ret_val > 0)
	{
		debug("dev_diaginfo already in nvram when nvram_add_dev_diaginfo \n");
		return 0 ;
	}

	
	//insert varName_sn pair into buf
	dev_nod_num = varname_sn_pair_head.total_dev_num + 1 ;
	memset(temp_var_name,0,sizeof(temp_var_name));
	sprintf(temp_var_name,"HN_DIAG_DEV_INFO%d",dev_nod_num);
	
	add_varName_sn_pair(p_diag_info->dev_sn,strlen(p_diag_info->dev_sn),temp_var_name,strlen(temp_var_name)) ;
	dev_nod_num = varname_sn_pair_head.total_dev_num ;

	memset(nvram_sn,0,sizeof(nvram_sn));
	transfer_sn(p_diag_info->dev_sn,nvram_sn,sizeof(nvram_sn)-1) ;
	debug("after transfer_sn,sn=%s \n",nvram_sn) ;

	//insert diaginfo into nvram
	memset(cmd_buf,0,sizeof(cmd_buf));
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram_status set HN_DIAG_DEV_INFO%d=%d,%s,%d,%d,%d,%d,%d,%d",dev_nod_num,
																			p_diag_info->dev_addr,
																			nvram_sn,
																			p_diag_info->data_pak_flag,
																			p_diag_info->dev_info_flag,
																			p_diag_info->sen_format_flag,
																			p_diag_info->sen_limit_flag,
																			p_diag_info->rssi_val,
																			p_diag_info->dev_online_flag);

	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("err when nvram_status set when nvram_add_dev_diaginfo \n") ;
		return -1 ;
	}

	#if 0
	ret_val = execute_cmd("nvram commit",cmd_result);
	if(ret_val < 0)
	{
		debug("err nvram commit when nvram_add_dev_diaginfo \n") ;
		return -1 ;
	}
	#endif

	//update dev_nod_num
	nvram_sync_devNode_num();

	#if 0
	//insert varName_sn pair into nvram
	ret_val = nvram_add_varName_sn_pair(p_diag_info->dev_sn,strlen(p_diag_info->dev_sn),temp_var_name,strlen(temp_var_name)) ;
	if(ret_val < 0 )
	{
		debug("err nvram_add_varName_sn_pair when nvram_add_dev_diaginfo \n");
		return -1 ;
	}
	#endif

	return 0 ;
	
}

int nvram_update_dev_diaginfo(dev_diag_info_t *p_diag_info)
{
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	//uint8_t temp_var_name[MAX_NVRAM_VARNAME_LEN] ;
	nvram_dev_varname_sn_pair_t *p_pair = NULL ;
	uint8_t dev_nod_num = 0 ;
	uint8_t nvram_sn[20] ;
	

	if(p_diag_info == NULL)
	{
		debug("err para when nvram_update_dev_diaginfo \n");
		return -1 ;
	}

	ret_val = nvram_query_dev_diaginfo(p_diag_info->dev_sn,strlen(p_diag_info->dev_sn)) ;
	if(ret_val <= 0)
	{
		debug("err not found diaginfo in nvram when nvram_update_dev_diaginfo \n");
		return -1 ;
	}

	p_pair = query_varName_sn_pair(p_diag_info->dev_sn,strlen(p_diag_info->dev_sn)) ;
	if(p_pair == NULL)
	{
		debug("err not found varName_sn pair in buf when nvram_update_dev_diaginfo\n") ;
		return -1 ;
	}

	memset(nvram_sn,0,sizeof(nvram_sn));
	transfer_sn(p_diag_info->dev_sn,nvram_sn,sizeof(nvram_sn)-1) ;
	debug("after transfer_sn,sn=%s \n",nvram_sn) ;

	memset(cmd_buf,0,sizeof(cmd_buf));
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram_status set %s=%d,%s,%d,%d,%d,%d,%d,%d",p_pair->nvram_var_name,
															p_diag_info->dev_addr,
															nvram_sn,
															p_diag_info->data_pak_flag,
															p_diag_info->dev_info_flag,
															p_diag_info->sen_format_flag,
															p_diag_info->sen_limit_flag,
															p_diag_info->rssi_val,
															p_diag_info->dev_online_flag);

	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("err when nvram_status set when nvram_update_dev_diaginfo \n") ;
		return -1 ;
	}

	#if 0
	ret_val = execute_cmd("nvram commit",cmd_result);
	if(ret_val < 0)
	{
		debug("err nvram commit when nvram_update_dev_diaginfo \n") ;
		return -1 ;
	}
	#endif

	return 0 ;
}

int nvram_update_online_devNum(void)
{
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;

	memset(cmd_buf,0,sizeof(cmd_buf));
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram_status set HN_ONLINE_NUM=%d",device_info_list_head.online_dev_count);
	//fxy add for led
	if(device_info_list_head.online_dev_count>0){  
		exec_syscmd("echo 2 > /tmp/lora_status",NULL,0);
	}else{
		exec_syscmd("echo 1 > /tmp/lora_status",NULL,0);
	}
    //end
	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("err when nvram_status set when nvram_update_online_devNum \n") ;
		return -1 ;
	}

	#if 0
	ret_val = execute_cmd("nvram commit",cmd_result);
	if(ret_val < 0)
	{
		debug("err nvram commit when nvram_update_online_devNum \n") ;
		return -1 ;
	}
	#endif

	return 0;
}


int nvram_del_dev_diaginfo(uint8_t *p_sn,uint8_t sn_len)
{
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	//uint8_t temp_var_name[MAX_NVRAM_VARNAME_LEN] ;
	nvram_dev_varname_sn_pair_t *p_pair = NULL ;
	uint8_t dev_nod_num = 0 ;
	
	
	if(p_sn == NULL || sn_len <= 0)
	{
		debug("err para when nvram_del_dev_diaginfo \n");
		return -1 ;
	}

	ret_val = nvram_query_dev_diaginfo(p_sn,sn_len) ;
	if(ret_val <= 0)
	{
		debug("err not found diaginfo in nvram when nvram_del_dev_diaginfo \n");
		return -1 ;
	}

	p_pair = query_varName_sn_pair(p_sn,sn_len) ;
	if(p_pair == NULL)
	{
		debug("err not found varName_sn pair in buf when nvram_del_dev_diaginfo\n") ;
		return -1 ;
	}

	memset(cmd_buf,0,sizeof(cmd_buf));
	memset(cmd_result,0,sizeof(cmd_result)) ;
	sprintf(cmd_buf,"nvram_status delete %s",p_pair->nvram_var_name);
	ret_val = execute_cmd(cmd_buf,cmd_result) ;
	if(ret_val < 0)
	{
		debug("err when nvram_status del when nvram_del_dev_diaginfo \n") ;
		return -1 ;
	}

	#if 0
	ret_val = execute_cmd("nvram commit",cmd_result);
	if(ret_val < 0)
	{
		debug("err nvram commit when nvram_del_dev_diaginfo \n") ;
		return -1 ;
	}
	#endif

	//del varName_sn_pair in the buf
	del_varName_sn_pair(p_sn,sn_len) ;
	
	//update dev_nod_num
	nvram_sync_devNode_num();

	return 0 ;
}

void nvram_delete_dev_diaginfo_list(void)
{
	int count = 0 ; //default max 20
	int ret_val = 0 ;
	uint8_t cmd_buf[MAX_NVRAM_CMD_LEN] ;
	uint8_t cmd_result[MAX_NVRAM_CMD_RESULT_LEN] ;
	int dev_node_num = 0 ;

	dev_node_num = nvram_query_devNode_num() ;
	if(dev_node_num <=0 || dev_node_num>255)
	{
		dev_node_num = 5 ;
	}

	for(count=1;count<=dev_node_num;count++)
	{
		memset(cmd_buf,0,sizeof(cmd_buf));
		memset(cmd_result,0,sizeof(cmd_result)) ;
		snprintf(cmd_buf,sizeof(cmd_buf)-1,"nvram_status delete HN_DIAG_DEV_INFO%d",count);

		ret_val = execute_cmd(cmd_buf,cmd_result) ;
		if(ret_val == -1)
		{
			debug("err when nvram_delete_dev_diaginfo_list \n");
			break ;
		}
	}

	ret_val = execute_cmd("nvram_status commit",cmd_result);
	if(ret_val == -1)
	{
		debug("err when nvram_delete_dev_diaginfo_list \n");
	}

	return ;
}


void set_local_addr(uint16_t addr)
{
	if(addr == 0xFFFF)
	{
		debug("err addr when set_local_addr \n");
		return ;
	}

	local_addr = addr ;
}

uint8_t calc_sensorNum_accordingToMask(uint16_t sen_mask)
{
	uint8_t sen_num = 0 ;
	uint8_t count = 0 ;
	//uint8_t bit_mask[16] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000};
	
	if(sen_mask == 0 )
	{
		sen_num = 0 ;
		return sen_num ;
	}

	for(count=0;count<16;count++)
	{
		if((sen_mask & bit_mask[count]) != 0)
		{
			sen_num++ ;
		}
	}

	return sen_num ;
}

int set_senIndex_inDevInfo(device_node_info_t *p_dev_node)
{
	uint8_t index_count = 0 ;
	uint8_t bit_mask_count = 0 ;
	
	if(p_dev_node == NULL)
	{
		return -1 ;
	}

	if(p_dev_node->device_info.sensor_mask == 0)
	{
		return -1 ;
	}

	index_count = 0 ;
	debug("sen_mask=0x%x \n",p_dev_node->device_info.sensor_mask);
	for(bit_mask_count = 0 ; bit_mask_count < 16 ; bit_mask_count++)
	{
		//sen_index from 0 to 15,match the bit-order of the sen-mask
		if(((p_dev_node->device_info.sensor_mask) & (bit_mask[bit_mask_count]) ) != 0)
		{
			p_dev_node->device_info.active_sen_index[index_count] = bit_mask_count ;
			debug("sensor[%d] index=%d \n",index_count,bit_mask_count);
			index_count++ ;
		}
	}

	return 0 ;
}


uint8_t get_next_sen_index(uint16_t source_addr,uint8_t cur_sen_index)
{
	device_node_info_t *p_node_info = NULL ;
	uint8_t sen_count = 0 ;

	p_node_info = query_device_info_by_deviceAddr(source_addr) ;
	if(p_node_info == NULL)
	{
		debug("cann't find node src_addr=%d when get_next_sen_index \n",source_addr);
		return ILLEGAL_SEN_INDEX ;
	}

	for(sen_count=0;sen_count<p_node_info->device_info.active_sensor_num;sen_count++)
	{
		if(p_node_info->device_info.active_sen_index[sen_count] == cur_sen_index)
		{
			break ;
		}
	}

	sen_count++ ;
	if(sen_count < p_node_info->device_info.active_sensor_num)
	{
		return p_node_info->device_info.active_sen_index[sen_count] ;
	}

	return ILLEGAL_SEN_INDEX ;
	
}


uint8_t query_next_senLmt_index(device_node_info_t *p_dev_node)
{
	uint8_t count = 0 ;
	sensor_data_format_t *p_sen_fmt = NULL ;
	//uint8_t sen_index = 0 ;

	if(p_dev_node == NULL )
	{
		return ILLEGAL_SEN_INDEX ;
	}

	for(count=0;count<p_dev_node->device_info.active_sensor_num;count++)
	{
		p_sen_fmt = query_sensor_format_data(p_dev_node->device_info.device_addr,p_dev_node->device_info.active_sen_index[count]) ;
		if(p_sen_fmt == NULL)
		{
			return ILLEGAL_SEN_INDEX ;
		}

		if(p_sen_fmt->current_senLimit_recvFlag == 0)
		{
			//this sensor not recved limit Data
			return p_dev_node->device_info.active_sen_index[count] ;
		}
	}

	return ILLEGAL_SEN_INDEX ;
}


int reset_dev_limitData(device_node_info_t *p_dev_node)
{
	sensor_data_format_t *p_sen_fmt = NULL ;

	if(p_dev_node == NULL || p_dev_node->p_sendor_format_data_head == NULL)
	{
		return 0 ;
	}

	p_sen_fmt = p_dev_node->p_sendor_format_data_head ;
	while(p_sen_fmt != NULL)
	{
		p_sen_fmt->current_senLimit_recvFlag = 0 ;

		p_sen_fmt = p_sen_fmt->p_next_sensor_format ;
	}

	p_dev_node->device_info.sen_limit_recv_flag = 0 ;
	p_dev_node->device_info.sen_limit_recvNum = 0 ;

	return 0 ;
}


uint8_t get_current_senArrayID(uint16_t source_addr,uint8_t cur_sen_index)
{
	device_node_info_t *p_node_info = NULL ;
	uint8_t sen_count = 0 ;

	p_node_info = query_device_info_by_deviceAddr(source_addr) ;
	if(p_node_info == NULL)
	{
		debug("cann't find node src_addr=%d when get_current_senArrayID \n",source_addr);
		return ILLEGAL_SEN_INDEX ;
	}

	for(sen_count=0;sen_count<p_node_info->device_info.active_sensor_num;sen_count++)
	{
		if(p_node_info->device_info.active_sen_index[sen_count] == cur_sen_index)
		{
			return sen_count ; //sensorIndex_arrayID
		}
	}


	return ILLEGAL_SEN_INDEX ;
	
}


int send_cmd_toRemote(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_id =0;
	uint16_t dst_addr = 0;
	int serial_fd = -1 ;
	
	if(p_data == NULL || data_len <= 0)
	{
		debug("err input para when send_cmd_toRemote \n");
		return -1 ;
	}
	serial_fd = get_serial_fd() ;
	if(serial_fd < 0)
	{
		debug("get err fd when send_cmd_toRemote \n");
		return -1 ;
	}
    if(g_module_config.module_lora1_mesh0==0){
	    send_uint8_buf(serial_fd,p_data,data_len);
	}else if(g_module_config.module_lora1_mesh0==1){
		if(data_len > 256-0x20-6){
			debug("err data_len when send_cmd_toRemote \n");
		}
		uint8_t rmci_pack[256];   //fxy 1
		rmci_pack[0]=RMCI_SOP;
		rmci_pack[1]=0x20+data_len+6;
		rmci_pack[2]=RMCI_CMD_BY_HUB;
		rmci_pack[3]=*(p_data+3);   //get dst addr
		rmci_pack[4]=*(p_data+4); 
		memcpy(&rmci_pack[5],p_data,data_len);
		rmci_pack[5+data_len]=RMCI_EOP;
		dst_addr= *(p_data+3)<<8|*(p_data+4);
		cmd_id = *(p_data+7);
		switch(cmd_id)
		{
			case GET_DEVINFO_V1_CMDID :
				debug("cmd_id=0x%x dst_addr 0x%x GET_DEVINFO_V1_CMDID\n",cmd_id,dst_addr);
				break;
			case GET_DEVINFO_V2_CMDID :
				debug("cmd_id=0x%x dst_addr 0x%x GET_DEVINFO_V2_CMDID\n",cmd_id,dst_addr);
				break;
			case GET_SEN_NAMEFORMAT_V1_CMDID :
				debug("cmd_id=0x%x dst_addr 0x%x GET_SEN_FORMAT_V1_CMDID\n",cmd_id,dst_addr);
				break;
			case GET_SEN_NAMEFORMAT_V2_CMDID :
				debug("cmd_id=0x%x dst_addr 0x%x GET_SEN_FORMAT_V2_CMDID\n",cmd_id,dst_addr);
				break;
			case GET_SEN_LIMIT_V1_CMDID :
				debug("cmd_id=0x%x dst_addr 0x%x GET_SEN_LIMIT_V1_CMDID\n",cmd_id,dst_addr);
				break;
			case GET_SEN_LIMIT_V2_CMDID :
				debug("cmd_id=0x%x dst_addr 0x%x GET_SEN_LIMIT_V2_CMDID\n",cmd_id,dst_addr);
				break;						
			default:
				debug("cmd_id=0x%x dst_addr 0x%x\n",cmd_id,dst_addr);
				break;
		}
	    send_uint8_buf(serial_fd,rmci_pack,6+data_len);
	}
	return 0 ;
}


waiting_cmd_list_t *query_cmd_inDevWaitingList(uint8_t cmdID,device_node_info_t *p_dev_node)
{
	waiting_cmd_list_t *p_list = NULL ;
	waiting_cmd_list_t *p_next = NULL  ;

	p_list = p_dev_node->waiting_cmd_listHead.p_next_cmd ;
	while(p_list != NULL)
	{
		if(p_list->cmd_id == cmdID)
		{
			return p_list ;
		}

		p_list = p_list->p_next_cmd ;
	}
	

	return NULL ;
}


int insert_cmd_toDevWaitingList(waiting_cmd_list_t *p_waiting_cmd,device_node_info_t *p_dev_node)
{
	waiting_cmd_list_t *p_list = NULL ;
	waiting_cmd_list_t *p_next = NULL  ;
	
	if(p_waiting_cmd == NULL || p_dev_node == NULL)
	{
		return -1 ;
	}

	p_list = p_dev_node->waiting_cmd_listHead.p_next_cmd ;
	if(p_list == NULL)
	{
		//insert into head
		p_dev_node->waiting_cmd_listHead.p_next_cmd = p_waiting_cmd ;
		p_dev_node->waiting_cmd_listHead.total_cmd++ ;
		return 0 ;
	}

	while(p_list != NULL)
	{
		if(p_list->p_next_cmd == NULL)
		{
			p_list->p_next_cmd = p_waiting_cmd ;
			p_dev_node->waiting_cmd_listHead.total_cmd++ ;
			break ;
		}

		p_list = p_list->p_next_cmd ;
	}

	return 0 ;
}

int free_cmdNode_inDevWaitingList(waiting_cmd_list_t *p_cmd_node,device_node_info_t *p_dev_node)
{
	waiting_cmd_list_t *p_list = NULL ;
	waiting_cmd_list_t *p_prev = NULL ;

	if(p_cmd_node == NULL || p_dev_node == NULL)
	{
		return -1 ;
	}

	p_list = p_dev_node->waiting_cmd_listHead.p_next_cmd ;
	while(p_list != NULL)
	{
		if(p_list->cmd_id == p_cmd_node->cmd_id)
		{
			//match,free it
			if(p_prev == NULL)
			{
				//head node,detach this node 
				p_dev_node->waiting_cmd_listHead.p_next_cmd = p_list->p_next_cmd ;
			}
			else
			{
				p_prev->p_next_cmd = p_list->p_next_cmd ;
			}

			//free mem now
			if(p_list->p_cmd_data != NULL)
			{
				free(p_list->p_cmd_data) ;
				p_list->p_cmd_data = NULL ;
			}

			free(p_list) ;

			if(p_dev_node->waiting_cmd_listHead.total_cmd > 0)
			{
				p_dev_node->waiting_cmd_listHead.total_cmd-- ;
			}
			
			break ;
			
		}

		p_prev = p_list ;
		p_list = p_list->p_next_cmd ;
	}
	
	return 0 ;
}


int free_specifiedCmd_inDevCmdList(waiting_cmd_list_t *p_cmd_node)
{
	if(p_cmd_node == NULL)
	{
		return 0 ;
	}
    debug("free_specifiedCmd_inDevCmdList\n");
	if(p_cmd_node->p_cmd_data != NULL)
	{
		free(p_cmd_node->p_cmd_data) ;
		p_cmd_node->p_cmd_data = NULL ;
	}

	free(p_cmd_node) ;

	return 0 ;
}


int free_allCmdNode_inDev(device_node_info_t *p_dev_node)
{
	waiting_cmd_list_t *p_list = NULL ;
	waiting_cmd_list_t *p_next = NULL ;
	
	if(p_dev_node == NULL)
	{
		return -1 ;
	}

	//free dev cmd
	if(p_dev_node->p_devInfo_cmd != NULL)
	{
		free_specifiedCmd_inDevCmdList(p_dev_node->p_devInfo_cmd) ;
		p_dev_node->p_devInfo_cmd = NULL ;
	}

	//free senFmt cmd
	if(p_dev_node->p_senFmt_cmd != NULL)
	{
		free_specifiedCmd_inDevCmdList(p_dev_node->p_senFmt_cmd);
		p_dev_node->p_senFmt_cmd = NULL ;
	}

	//free senLimit cmd
	if(p_dev_node->p_senLimit_cmd != NULL)
	{
		free_specifiedCmd_inDevCmdList(p_dev_node->p_senLimit_cmd);
		p_dev_node->p_senLimit_cmd = NULL ;
	}

	p_list = p_dev_node->waiting_cmd_listHead.p_next_cmd ;
	while(p_list != NULL)
	{
		p_next = p_list->p_next_cmd ;

		free_cmdNode_inDevWaitingList(p_list,p_dev_node) ;

		p_list = p_next ;
	}

	return 0 ;
}

int send_cmd_toDevWaitingList(uint8_t *p_data,uint8_t data_len,uint8_t cmdID,device_node_info_t *p_dev_node)
{
	int serial_fd = -1 ;
	waiting_cmd_list_t *p_new_cmd = NULL ;
	uint8_t *p_cmd_data = NULL ;
	
	if(p_data == NULL || data_len <= 0 || p_dev_node == NULL)
	{
		debug("err input para when send_cmd_toDevWaitingList \n");
		return -1 ;
	}

	debug("begin to send_cmd_toDevWaitingList,dev=%s,cmdID=0x%x \n",p_dev_node->device_info.dev_serialNum,cmdID);
	switch(cmdID)
	{
		case GET_DEVINFO_V1_CMDID :
		case GET_DEVINFO_V2_CMDID :
			if(p_dev_node->p_devInfo_cmd != NULL)
			{
				debug("get_dev cmd has been in the dev-cmd-list,return now \n");
				return 0 ;
			}
			p_new_cmd = malloc(sizeof(waiting_cmd_list_t)) ;
			if(p_new_cmd == NULL)
			{
				debug("err malloc get-dev cmd when send_cmd_toDevWaitingList \n");
				return -1 ;
			}
			memset(p_new_cmd,0,sizeof(waiting_cmd_list_t));
			p_new_cmd->cmd_id = cmdID ;
			p_new_cmd->cmd_data_len = data_len ;
			p_new_cmd->dest_addr = p_dev_node->device_info.device_addr ;
			strlcpy(p_new_cmd->target_dev_serial,p_dev_node->device_info.dev_serialNum,sizeof(p_new_cmd->target_dev_serial));
			//malloc and copy cmd data
			p_cmd_data = malloc(data_len) ;
			if(p_cmd_data == NULL)
			{
				debug("err malloc get-dev cmd-data when send_cmd_toDevWaitingList \n");
				free(p_new_cmd) ;
				return -1 ;
			}
			memset(p_cmd_data,0,data_len);
			memcpy(p_cmd_data,p_data,data_len) ;

			p_new_cmd->p_cmd_data = p_cmd_data ;
			p_dev_node->p_devInfo_cmd = p_new_cmd ;
			
			break ;
		case GET_SEN_NAMEFORMAT_V1_CMDID :
		case GET_SEN_NAMEFORMAT_V2_CMDID :
			if(p_dev_node->p_senFmt_cmd != NULL)
			{
				debug("senFmt cmd has been in the dev-cmd-list,return now \n");
				return 0 ;
			}
			p_new_cmd = malloc(sizeof(waiting_cmd_list_t)) ;
			if(p_new_cmd == NULL)
			{
				debug("err malloc senFmt cmd when send_cmd_toDevWaitingList \n");
				return -1 ;
			}
			memset(p_new_cmd,0,sizeof(waiting_cmd_list_t));
			p_new_cmd->cmd_id = cmdID ;
			p_new_cmd->cmd_data_len = data_len ;
			p_new_cmd->dest_addr = p_dev_node->device_info.device_addr ;
			strlcpy(p_new_cmd->target_dev_serial,p_dev_node->device_info.dev_serialNum,sizeof(p_new_cmd->target_dev_serial));
			//malloc and copy cmd data
			p_cmd_data = malloc(data_len) ;
			if(p_cmd_data == NULL)
			{
				debug("err malloc senFmt cmd-data when send_cmd_toDevWaitingList \n");
				free(p_new_cmd) ;
				return -1 ;
			}
			memset(p_cmd_data,0,data_len);
			memcpy(p_cmd_data,p_data,data_len) ;

			p_new_cmd->p_cmd_data = p_cmd_data ;
			p_dev_node->p_senFmt_cmd = p_new_cmd ;
			
			break ;
		case GET_SEN_LIMIT_V1_CMDID :
		case GET_SEN_LIMIT_V2_CMDID :
			if(p_dev_node->p_senLimit_cmd != NULL)
			{
				debug("senLimit cmd has been in the dev-cmd-list,return now \n");
				return 0 ;
			}
			p_new_cmd = malloc(sizeof(waiting_cmd_list_t)) ;
			if(p_new_cmd == NULL)
			{
				debug("err malloc senLimit cmd when send_cmd_toDevWaitingList \n");
				return -1 ;
			}
			memset(p_new_cmd,0,sizeof(waiting_cmd_list_t));
			p_new_cmd->cmd_id = cmdID ;
			p_new_cmd->cmd_data_len = data_len ;
			p_new_cmd->dest_addr = p_dev_node->device_info.device_addr ;
			strlcpy(p_new_cmd->target_dev_serial,p_dev_node->device_info.dev_serialNum,sizeof(p_new_cmd->target_dev_serial));
			//malloc and copy cmd data
			p_cmd_data = malloc(data_len) ;
			if(p_cmd_data == NULL)
			{
				debug("err malloc senLimit cmd-data when send_cmd_toDevWaitingList \n");
				free(p_new_cmd) ;
				return -1 ;
			}
			memset(p_cmd_data,0,data_len);
			memcpy(p_cmd_data,p_data,data_len) ;

			p_new_cmd->p_cmd_data = p_cmd_data ;
			p_dev_node->p_senLimit_cmd = p_new_cmd ;
			
			break ;
		default :
			//any other cmd,insert into waiting list
			p_new_cmd = query_cmd_inDevWaitingList(cmdID,p_dev_node) ;
			if(p_new_cmd != NULL)
			{
				//for same cmdID,only one cmd in the list at the same time
				debug("cmdID=0x%x has been in waiting list,free old cmd now \n",cmdID);
				free_cmdNode_inDevWaitingList(p_new_cmd,p_dev_node) ;
				p_new_cmd = NULL ;
			}

			//create and insert new cmd ,maybe cmd-data changed,such as alarm-cmd data
			p_new_cmd = malloc(sizeof(waiting_cmd_list_t)) ;
			if(p_new_cmd == NULL)
			{
				debug("err malloc waiting cmd when send_cmd_toDevWaitingList \n");
				return -1 ;
			}
			memset(p_new_cmd,0,sizeof(waiting_cmd_list_t));
			p_new_cmd->cmd_id = cmdID ;
			p_new_cmd->cmd_data_len = data_len ;
			p_new_cmd->dest_addr = p_dev_node->device_info.device_addr ;
			strlcpy(p_new_cmd->target_dev_serial,p_dev_node->device_info.dev_serialNum,sizeof(p_new_cmd->target_dev_serial));
			//malloc and copy cmd data
			p_cmd_data = malloc(data_len) ;
			if(p_cmd_data == NULL)
			{
				debug("err malloc waiting cmd-data when send_cmd_toDevWaitingList \n");
				free(p_new_cmd) ;
				return -1 ;
			}
			memset(p_cmd_data,0,data_len);
			memcpy(p_cmd_data,p_data,data_len) ;
			p_new_cmd->p_cmd_data = p_cmd_data ;

			//insert into list
			insert_cmd_toDevWaitingList(p_new_cmd,p_dev_node) ;
			
			break ;
	}

	
	return 0 ;
}


int send_cmd_inDevCmdList(device_node_info_t *p_dev_node)
{
	time_t time_val = 0 ;
	uint32_t time_diff = 0 ;
	waiting_cmd_list_t *p_cmd_node = NULL ;
	uint8_t next_sen_index = 0 ;
	uint8_t cmd_data[32] ;
	uint8_t count = 0 ;
	uint16_t dest_addr = 0 ;
	uint8_t send_pak_period = 0 ;
	uint8_t cmd_id = 0 ;  //add by fxy

	if(p_dev_node == NULL)
	{
		return 0 ;
	}

	debug("send_cmd_inDevCmdList: dev waiting cmd num=%d \n",p_dev_node->waiting_cmd_listHead.total_cmd);

	//add get sen limit cmd if necessary
	next_sen_index = query_next_senLmt_index(p_dev_node) ;
	if(p_dev_node->device_info.sen_fmt_recv_flag == 1 && next_sen_index != ILLEGAL_SEN_INDEX)
	{
		//get next sen_limit data
		dest_addr = p_dev_node->device_info.device_addr ;
		cmd_data[0] = next_sen_index ;
		if (p_dev_node->device_info.cmd_ver==WSN_CMD_VER1){  //add by fxy
            cmd_id =  GET_SEN_LIMIT_V1_CMDID; 
			debug("send_cmd_inDevCmdList: GET_SEN_LIMIT_V1_CMDID\n");
			count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),cmd_id,cmd_data,1,local_addr,dest_addr);
		}else if(p_dev_node->device_info.cmd_ver==WSN_CMD_VER2){
            cmd_id = GET_SEN_LIMIT_V2_CMDID;  
			debug("send_cmd_inDevCmdList: GET_SEN_LIMIT_V2_CMDID\n");
			count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),cmd_id,cmd_data,1,local_addr,dest_addr); 
		}else{
			debug("send_cmd_inDevCmdList: SEN_LIMIT cm_id err\n");
		}
		//count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_LIMIT_V2_CMDID,cmd_data,1,local_addr,dest_addr);
		if(count > 0)
		{
			//send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
			send_cmd_toDevWaitingList(wsn_send_buf,count,cmd_id,p_dev_node) ;
		}
	}


	time(&time_val) ;

	if(p_dev_node->p_devInfo_cmd != NULL)
	{
		//send imidiately
		send_cmd_toRemote(p_dev_node->p_devInfo_cmd->p_cmd_data,p_dev_node->p_devInfo_cmd->cmd_data_len) ;

		if(p_dev_node != NULL)
		{
			//maybe this is update-dev-info cmd,update send-cmd timer
			p_dev_node->sendCmd_timer = time_val ;
		}
		
		//free this cmd mem
		free_specifiedCmd_inDevCmdList(p_dev_node->p_devInfo_cmd) ;
		p_dev_node->p_devInfo_cmd = NULL ;

		return 0 ;
	}

	//check and reset send_pak period
	send_pak_period = p_dev_node->device_info.data_interval ;
	if(recv_data_pak_flag == 1)
	{
		debug("recved data_pak,reset period == 0 \n") ;
		recv_data_pak_flag = 0 ;
		send_pak_period = 0 ;
	}
	else
	{
		//not data-pak,send nothing
		debug("recved not data-pak,not send pak to dev,return now \n");
		return 0 ;
	}

	#if 0
	//check the cmd-send-timer
	if(time_val >= p_dev_node->sendCmd_timer )
	{
		time_diff = time_val-p_dev_node->sendCmd_timer ;
		if( time_diff < send_pak_period)
		{
			debug("dev=%s,time-diff=%d too short,data-interval=%d when send_cmd_inDevCmdList\n",p_dev_node->device_info.dev_serialNum,time_diff, send_pak_period);
			return 0 ;
		}
	}
	else
	{
		//maybe time overflow?
		debug("maybe time overflow,send cmd and update the cmd-send-timer now \n");
	}
	#endif

	//send senFmt cmd if necessary
	if(p_dev_node->p_senFmt_cmd != NULL)
	{
		//send senFmt cmd
		send_cmd_toRemote(p_dev_node->p_senFmt_cmd->p_cmd_data,p_dev_node->p_senFmt_cmd->cmd_data_len) ;

		//update cmd-send-timer
		p_dev_node->sendCmd_timer = time_val ;

		//free this cmd
		free_specifiedCmd_inDevCmdList(p_dev_node->p_senFmt_cmd);
		p_dev_node->p_senFmt_cmd = NULL ;
		
		return 0 ;
	}


	//handle waiting list,only handle the first cmd node at one time
	p_cmd_node = p_dev_node->waiting_cmd_listHead.p_next_cmd ;
	if(p_cmd_node != NULL)
	{
		//send this cmd
		send_cmd_toRemote(p_cmd_node->p_cmd_data,p_cmd_node->cmd_data_len);

		//update cmd-send-timer
		p_dev_node->sendCmd_timer = time_val ;

		//free this cmd
		free_cmdNode_inDevWaitingList(p_cmd_node,p_dev_node) ;

		return 0 ;
	}

	//get limit data,lowest priority,get it only at free time
	if(p_dev_node->p_senLimit_cmd != NULL)
	{
		//send senLimit cmd
		send_cmd_toRemote(p_dev_node->p_senLimit_cmd->p_cmd_data,p_dev_node->p_senLimit_cmd->cmd_data_len) ;

		//update cmd-send-timer
		p_dev_node->sendCmd_timer = time_val ;

		//free this cmd
		free_specifiedCmd_inDevCmdList(p_dev_node->p_senLimit_cmd);
		p_dev_node->p_senLimit_cmd = NULL ;
		
		return 0 ;
	}
	
	return 0 ;
}

#if 0
int send_expectedResp_cmd_toRemote(uint8_t *p_data,uint8_t data_len)
{
	int serial_fd = -1 ;
	expected_resp_cmd_list_t *p_temp_cmd = NULL ;
	uint16_t dest_addr = 0xFFFF ;
	uint8_t cmd_id = 0xFF ;
	uint8_t cmdId_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN ;
	time_t time_val = 0 ;
	
	if(p_data == NULL || data_len <= 0)
	{
		debug("err input para when send_expectedResp_cmd_toRemote \n");
		return -1 ;
	}

	serial_fd = get_serial_fd() ;
	if(serial_fd < 0)
	{
		debug("get err fd when send_expectedResp_cmd_toRemote \n");
		return -1 ;
	}

	send_uint8_buf(serial_fd,p_data,data_len);

	//add this cmd to expected cmd list
	dest_addr = (p_data[3] << 8) | (p_data[4]) ;
	cmd_id = p_data[cmdId_offset] ;

	if(cmd_id == WSN_PING_DEV_MSGID)
	{
		//nothing to do
		return 0 ;
	}
	
	//debug("before insert cmd to expectedResp list,destaddr=%d,cmdid=%d \n",dest_addr,cmd_id);
	//time_val = time(NULL) ;
	time(&time_val) ;
	//debug("before insert cmd to expectedResp list,destaddr=%d,cmdid=%d ,resp_time=%u \n",dest_addr,cmd_id,time_val);
	
	p_temp_cmd = query_expected_cmd_list(dest_addr,cmd_id) ;
	if(p_temp_cmd != NULL)
	{
		//update the timer
		p_temp_cmd->resp_timer = time_val ;
		p_temp_cmd->no_resp_count++ ;
		return 0 ;
	}
	
	p_temp_cmd = malloc(sizeof(expected_resp_cmd_list_t)) ;
	if(p_temp_cmd == NULL)
	{
		debug("err malloc insert_expected_cmd_list buf failed in send_expectedResp_cmd_toRemote \n");
		return -1 ;
	}
	memset(p_temp_cmd,0,sizeof(expected_resp_cmd_list_t)) ;
	p_temp_cmd->resp_timer = time_val ;
	p_temp_cmd->cmd_id = cmd_id ;
	p_temp_cmd->dest_addr = dest_addr ;
	//p_temp_cmd->cmd_len = data_len ;
	//debug("sizeof p_temp-->cmd=%d \n",sizeof(p_temp_cmd->cmd)) ;
	if(data_len >= MAX_WSN_BUF_LEN)
	{
		memcpy(p_temp_cmd->cmd,p_data,MAX_WSN_BUF_LEN) ;
		p_temp_cmd->cmd_len = MAX_WSN_BUF_LEN ;
	}
	else
	{
		memcpy(p_temp_cmd->cmd,p_data,data_len) ;
		p_temp_cmd->cmd_len = data_len ;
	}
	p_temp_cmd->no_resp_count = 0 ; //begin to count
	p_temp_cmd->p_next = NULL ;
	insert_expected_cmd_list(p_temp_cmd) ;

	return 0 ;
}
#endif

void init_device_info_list(void)
{
	memset(&device_info_list_head,0,sizeof(device_info_list_head));
	device_info_list_head.device_info.device_addr = 0xFFFF ;

	memset(&offline_dev_info_list_head,0,sizeof(offline_dev_info_list_head));
	offline_dev_info_list_head.device_info.device_addr = 0xFFFF ;
	
	return ;
}

device_node_info_t *query_device_info_list_end(void)
{
	device_node_info_t *p_temp_device_info = NULL ;

	p_temp_device_info = &device_info_list_head ;

	while(p_temp_device_info->p_next_device != NULL)
	{
		p_temp_device_info = p_temp_device_info->p_next_device ;
	}

	return p_temp_device_info ;
}

device_node_info_t *query_device_info_by_serialNum(uint8_t *p_device_serialNum)
{
	device_node_info_t *p_temp_device_info = NULL ;

	if(p_device_serialNum == NULL || p_device_serialNum[0] == 0)
	{
		debug("err serialNum para when query_device_info_by_serialNum \n");
		return NULL ;
	}
	
	p_temp_device_info = device_info_list_head.p_next_device ;
	while(p_temp_device_info != NULL)
	{
		//if(memcmp(p_temp_device_info->device_info.dev_serialNum,p_device_serialNum,DEV_SERIALNUM_LEN) == 0)
		if(strcmp(p_temp_device_info->device_info.dev_serialNum,p_device_serialNum) == 0) //match
		{
			return p_temp_device_info ;
		}

		p_temp_device_info = p_temp_device_info->p_next_device ;
	}

	return NULL ;
}



device_node_info_t *query_device_info_by_deviceAddr(uint16_t dev_addr)
{
	device_node_info_t *p_temp_device_info = NULL ;

	if(dev_addr == 0xFFFF)
	{
		debug("err dev_addr == 0xFFFF when query_device_info_by_deviceAddr \n");
		return NULL ;
	}

	p_temp_device_info = device_info_list_head.p_next_device;
	while(p_temp_device_info != NULL)
	{
		if(p_temp_device_info->device_info.device_addr == dev_addr)
		{
			return p_temp_device_info ;
		}

		p_temp_device_info = p_temp_device_info->p_next_device ;
	}

	return NULL ;
}



int insert_device_info_to_list(device_node_info_t *p_device_info)
{
	device_node_info_t *p_device_info_end = NULL ;
	device_node_info_t *p_new_device_info = NULL ;
	device_node_info_t *p_node = NULL ;
	device_node_info_t *p_chk_addr_collision = NULL ;
	dev_diag_info_t temp_diag_info ;
	time_t time_val = 0 ;
	
	if(p_device_info == NULL)
	{
		debug("err p_device_info == NULL when insert_device_info_to_list \n");
		return -1 ;
	}

	debug("insert new dev into buf,dev_sn:%s  \n",p_device_info->device_info.dev_serialNum);

	p_node = p_device_info ;
	//set dev offline_flag when add new dev
	//p_node->device_info.dev_offline_flag = 1 ;
	
	memset(&temp_diag_info,0,sizeof(temp_diag_info));
	temp_diag_info.data_pak_flag = p_node->device_info.data_pak_recv_flag ;
	temp_diag_info.dev_addr = p_node->device_info.device_addr ;
	temp_diag_info.dev_info_flag = p_node->device_info.dev_info_recv_flag ;
	temp_diag_info.dev_online_flag = 0 ;
	
	memcpy(temp_diag_info.dev_sn,p_node->device_info.dev_serialNum,(sizeof(temp_diag_info.dev_sn)-1)) ;
	temp_diag_info.rssi_val = p_node->device_info.dev_rssi_val ;
	temp_diag_info.sen_format_flag = p_node->device_info.sen_fmt_recv_flag ;
	temp_diag_info.sen_limit_flag = p_node->device_info.sen_limit_recv_flag ;

	//check new node addr collision or not,del old node if necessary,keep from two devices have same addr in the list
	p_chk_addr_collision = query_device_info_by_deviceAddr(p_device_info->device_info.device_addr) ;
	if(p_chk_addr_collision != NULL && p_chk_addr_collision->device_info.dev_serialNum[0] != 0) 
	{
		//secret dev-info is empty-dev-info,no dev-serial
		if(strcmp(p_chk_addr_collision->device_info.dev_serialNum,p_device_info->device_info.dev_serialNum)) //serial not match,not the same dev
		{
			set_dev_offline_status(p_chk_addr_collision) ;
		}
	}

	p_device_info_end = query_device_info_by_serialNum(p_device_info->device_info.dev_serialNum) ;
	if(p_device_info_end != NULL) //update dev_info in the list
	{
		//free old sensor data format mem
		debug("dev:%s already in buf,renew it now\n",p_device_info_end->device_info.dev_serialNum);
		if(p_device_info_end->p_sendor_format_data_head != NULL)
		{
			free_sensor_format_data_mem(p_device_info_end->p_sendor_format_data_head) ;
			p_device_info_end->p_sendor_format_data_head = NULL ;
		}

		
		p_device_info->p_next_device = p_device_info_end->p_next_device ;

		//copy setcet info if necessary
		if(p_device_info_end->device_info.dev_support_encrypt)
		{
			p_device_info->device_info.dev_support_encrypt = 1 ;
			p_device_info->device_info.exchangeKey_success = p_device_info_end->device_info.exchangeKey_success ;
			p_device_info->device_info.gen_sk_time = p_device_info_end->device_info.gen_sk_time ;
			memcpy(p_device_info->device_info.session_key,p_device_info_end->device_info.session_key,32) ;
			memcpy(p_device_info->device_info.dev_dpsk,p_device_info_end->device_info.dev_dpsk,32);
		}

		//copy cmd list if necessary
		p_device_info->p_devInfo_cmd = p_device_info_end->p_devInfo_cmd ;
		p_device_info->p_senFmt_cmd = p_device_info_end->p_senFmt_cmd ;
		p_device_info->p_senLimit_cmd = p_device_info_end->p_senLimit_cmd ;
		p_device_info->waiting_cmd_listHead = p_device_info_end->waiting_cmd_listHead ;

		//copy groupAlarmRelay list,and data
		p_device_info->device_info.groupFault_list_head = p_device_info_end->device_info.groupFault_list_head ;
		p_device_info->device_info.groupHigh_list_head = p_device_info_end->device_info.groupHigh_list_head ;
		p_device_info->device_info.groupLow_list_head = p_device_info_end->device_info.groupLow_list_head ;
		p_device_info->device_info.groupRelay1_list_head = p_device_info_end->device_info.groupRelay1_list_head ;
		p_device_info->device_info.groupRelay2_list_head = p_device_info_end->device_info.groupRelay2_list_head ;
		p_device_info->device_info.groupRelay3_list_head = p_device_info_end->device_info.groupRelay3_list_head ;
		p_device_info->device_info.groupRelay4_list_head = p_device_info_end->device_info.groupRelay4_list_head ;
		p_device_info->device_info.groupRelay5_list_head = p_device_info_end->device_info.groupRelay5_list_head ;
		p_device_info->device_info.total_groupAlarm = p_device_info_end->device_info.total_groupAlarm ;
		p_device_info->device_info.total_groupRelay = p_device_info_end->device_info.total_groupRelay ;
		p_device_info->device_info.recved_groupAlarm = p_device_info_end->device_info.recved_groupAlarm ;
		
		memcpy(p_device_info_end,p_device_info,sizeof(device_node_info_t)) ;
		p_device_info_end->device_info.new_dev_flag = 0 ; // 0---will not change online_dev_count when set_dev_online

		//maybe info about this dev change,update diaginfo now;need not to update or add varName_sn_pair here
		nvram_update_dev_diaginfo(&temp_diag_info) ;

		//bak realDataRecord now
		bak_realDataRecord(p_device_info_end);
		
		return 0 ;
	}

	//add for secret dev
	if(p_device_info_end == NULL)
	{
		//secret dev-info is empty-dev-info,no dev-serial
		p_device_info_end = query_device_info_by_deviceAddr(p_device_info->device_info.device_addr) ;
		if(p_device_info_end != NULL && p_device_info_end->device_info.dev_support_encrypt)
		{
			debug("found dev-info by addr=0x%x,secret-flag=%d renew dev-info now\n",p_device_info->device_info.device_addr,
																			p_device_info_end->device_info.dev_support_encrypt);
			if(p_device_info_end->p_sendor_format_data_head != NULL)
			{
				free_sensor_format_data_mem(p_device_info_end->p_sendor_format_data_head) ;
				p_device_info_end->p_sendor_format_data_head = NULL ;
			}

			//copy secret-info
			p_device_info->device_info.dev_support_encrypt = 1 ;
			p_device_info->device_info.exchangeKey_success = p_device_info_end->device_info.exchangeKey_success ;
			p_device_info->device_info.gen_sk_time = p_device_info_end->device_info.gen_sk_time ;
			memcpy(p_device_info->device_info.session_key,p_device_info_end->device_info.session_key,32) ;
			memcpy(p_device_info->device_info.dev_dpsk,p_device_info_end->device_info.dev_dpsk,32);
			
			p_device_info->p_next_device = p_device_info_end->p_next_device ;
			memcpy(p_device_info_end,p_device_info,sizeof(device_node_info_t)) ;
			p_device_info_end->device_info.new_dev_flag = 1 ;


			//maybe info about this dev change,update diaginfo now;need not to update or add varName_sn_pair here
			//nvram_update_dev_diaginfo(&temp_diag_info) ;
			nvram_add_dev_diaginfo(&temp_diag_info) ; //update dev-info,add diagingo first-time

			return 0 ;
		}
	}

	//not found device info in the list
	debug("insert new dev: sn=%s \n",p_device_info->device_info.dev_serialNum);
	p_device_info_end = query_device_info_list_end();
	p_new_device_info = malloc(sizeof(device_node_info_t));

	if(p_new_device_info == NULL)
	{
		debug("err ! malloc device_info space failed when insert_device_info_to_list \n");
		return -1 ;
	}

	bzero(p_new_device_info,sizeof(device_node_info_t)) ;

	memcpy(p_new_device_info,p_device_info,sizeof(device_node_info_t)) ;
	p_new_device_info->device_info.dev_support_encrypt = p_device_info->device_info.dev_support_encrypt ;
	debug("insert new dev:dev-addr=0x%x,dev-encrypt_flag = %d \n",p_new_device_info->device_info.device_addr,p_new_device_info->device_info.dev_support_encrypt);

	//set new-dev flag,and total-dev-num
	p_new_device_info->device_info.new_dev_flag = 1 ;

	if(p_new_device_info->device_info.dev_serialNum[0] != 0 
			&& query_varName_sn_pair(p_new_device_info->device_info.dev_serialNum,strlen(p_new_device_info->device_info.dev_serialNum)) == NULL)
	{
		//we get dev_name_pair from nvram when app start,maybe dev already in nvram,but not in buf dev_list
		device_info_list_head.total_dev_count++ ;
	}
	p_new_device_info->p_next_device = NULL ;

	//atach into list
	p_device_info_end->p_next_device = p_new_device_info ;

	debug("success to add new_dev:%s into buf \n",p_new_device_info->device_info.dev_serialNum);

	//insert dev diaginfo into nvram
	if(temp_diag_info.dev_sn[0] != 0)
	{	
		//dev-sn not null
		nvram_add_dev_diaginfo(&temp_diag_info) ;
	}

	return 0 ;
}

//used for secret-dev
int del_deviceInfo_fromList_byAddr(device_node_info_t *p_node)
{
	device_node_info_t *p_prev = NULL ;
	device_node_info_t *p_current = NULL ;
	
	if(p_node == NULL)
	{
		return -1 ;
	}

	p_current = device_info_list_head.p_next_device ;
	p_prev = &device_info_list_head ;
	while(p_current != NULL)
	{
		//if(strncmp(p_current->device_info.dev_serialNum,p_node->device_info.dev_serialNum,sizeof(p_current->device_info.dev_serialNum)) == 0)
		if(p_current->device_info.device_addr == p_node->device_info.device_addr && p_current->device_info.dev_support_encrypt == 1)	
		{
			//match
			//detach from the list
			p_prev->p_next_device = p_current->p_next_device ;
			p_current->p_next_device = NULL ;

			#if 1
			//free sensor_format data
			free_sensor_format_data_mem(p_current->p_sendor_format_data_head) ;

			//free dev-node
			free(p_current) ;
			p_current = NULL ;
			#endif

			if(device_info_list_head.total_dev_count > 0)
			{
				device_info_list_head.total_dev_count-- ;
			}

			break ;
		}

		p_prev = p_current ;
		p_current = p_current->p_next_device ;
	}

	return 0 ;
}




int del_device_info_from_list(device_node_info_t *p_node)
{
	device_node_info_t *p_prev = NULL ;
	device_node_info_t *p_current = NULL ;
	
	if(p_node == NULL)
	{
		return -1 ;
	}

	p_current = device_info_list_head.p_next_device ;
	p_prev = &device_info_list_head ;
	while(p_current != NULL)
	{
		//if(strncmp(p_current->device_info.dev_serialNum,p_node->device_info.dev_serialNum,sizeof(p_current->device_info.dev_serialNum)) == 0)
		if(strcmp(p_current->device_info.dev_serialNum,p_node->device_info.dev_serialNum) == 0)	
		{
			//match
			//detach from the list
			p_prev->p_next_device = p_current->p_next_device ;
			p_current->p_next_device = NULL ;

			#if 1
			//free sensor_format data
			free_sensor_format_data_mem(p_current->p_sendor_format_data_head) ;

			//free dev-node
			free(p_current) ;
			p_current = NULL ;
			#endif

			if(device_info_list_head.total_dev_count > 0)
			{
				device_info_list_head.total_dev_count-- ;
			}

			break ;
		}

		p_prev = p_current ;
		p_current = p_current->p_next_device ;
	}

	return 0 ;
}


int detach_device_info_from_list(device_node_info_t *p_node)
{
	device_node_info_t *p_prev = NULL ;
	device_node_info_t *p_current = NULL ;
	
	if(p_node == NULL)
	{
		return -1 ;
	}

	p_current = device_info_list_head.p_next_device ;
	p_prev = &device_info_list_head ;
	while(p_current != NULL)
	{
		//if(strncmp(p_current->device_info.dev_serialNum,p_node->device_info.dev_serialNum,sizeof(p_current->device_info.dev_serialNum)) == 0)
		if(strcmp(p_current->device_info.dev_serialNum,p_node->device_info.dev_serialNum) == 0)
		{
			//match
			//detach from the list
			p_prev->p_next_device = p_current->p_next_device ;
			p_current->p_next_device = NULL ;

			#if 0
			//free sensor_format data
			free_sensor_format_data_mem(p_current->p_sendor_format_data_head) ;

			//free dev-node
			free(p_current) ;
			p_current = NULL ;
			#endif

			if(device_info_list_head.total_dev_count > 0)
			{
				device_info_list_head.total_dev_count-- ;
			}

			break ;
		}

		p_prev = p_current ;
		p_current = p_current->p_next_device ;
	}

	return 0 ;
}


int copy_device_info_toBuf(device_node_info_t *p_buf,uint8_t *p_wsn_data)
{
	int offset = 0 ;
	int cmd_data_offset = 0 ;
	time_t time_val = 0 ;
	
	if(p_buf == NULL || p_wsn_data == NULL)
	{
		debug("err input para when copy_device_info_toBuf \n");
		return -1 ;
	}


	//skip from sop to cmdID
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	//cmd_ver
	p_buf->device_info.cmd_ver= p_wsn_data[offset] ;
	offset += 1 ;

	//instr_family_id
	p_buf->device_info.instr_family_id = p_wsn_data[offset] ;
	offset += 1 ;
	p_buf->device_info.instr_model_id = p_wsn_data[offset] ;
	offset += 1 ;
	p_buf->device_info.instr_hw_id = p_wsn_data[offset] ;
	offset += 1 ;
	p_buf->device_info.instr_fw_id = p_wsn_data[offset] ;
	offset += 1 ;

	//serial_num
	memcpy(p_buf->device_info.dev_serialNum,p_wsn_data+offset,10) ;
	offset += 10 ;
	memcpy(p_buf->device_info.dev_serialNum+10,p_wsn_data+cmd_data_offset+39,2) ;

	//fw_ver
	memcpy(p_buf->device_info.dev_fw_ver,p_wsn_data+offset,6) ;
	offset += 6 ;

	//cfgSetCnt
	p_buf->device_info.cfgSet_cnt = p_wsn_data[offset] ;
	offset += 1 ;

	//hw_info
	p_buf->device_info.hw_info = p_wsn_data[offset] ;
	offset += 1 ;

	//modem type
	p_buf->device_info.modem_type = p_wsn_data[offset] ;
	offset += 1 ;

	//data_interval,second
	p_buf->device_info.data_interval = (p_wsn_data[offset] << 8) | p_wsn_data[offset+1] ;
	offset += 2 ;
	if(p_buf->device_info.data_interval <= 0)
	{
		p_buf->device_info.data_interval = 60 ; //default interval 60s
	}
	debug("parse dev-info,dev data-interval = %d \n",p_buf->device_info.data_interval);

	//sensor mask,one sensor match one bit,max 16 sensors in one device
	p_buf->device_info.sensor_mask = (p_wsn_data[offset] << 8) | p_wsn_data[offset+1] ;
	//p_buf->device_info.sensor_mask = 0x3F ; //only for debug!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	offset += 2 ;
	p_buf->device_info.active_sensor_num = calc_sensorNum_accordingToMask(p_buf->device_info.sensor_mask);
	debug("sen_mask =0x%02x,active_sen_num = %d \n",p_buf->device_info.sensor_mask,p_buf->device_info.active_sensor_num);

	//dio bank1 map
	p_buf->device_info.dioBank1_map = p_wsn_data[offset] ;
	offset += 1 ;
	p_buf->device_info.dioBank0_map = p_wsn_data[offset] ;
	offset += 1 ;

	//function info
	p_buf->device_info.function_info = p_wsn_data[offset] ;
	offset += 1 ;

	//userid
	memcpy(p_buf->device_info.userID,p_wsn_data+offset,8);


	//reset dev offline-timer
	debug("parse dev-info dev:%s ,update offline timer now \n",p_buf->device_info.dev_serialNum);
	time(&time_val) ;
	p_buf->device_info.dev_offline_timer = time_val ;
	p_buf->device_info.dev_info_recv_time = time_val ; // add for dev_v1 bug,dev-comm maybe err if interval too short 

	return 0 ;	
}

//add for offline-dev
device_node_info_t *query_offlineDev_info_by_serialNum(uint8_t *p_device_serialNum)
{
	device_node_info_t *p_temp_device_info = NULL ;

	if(p_device_serialNum == NULL || p_device_serialNum[0] == 0)
	{
		debug("err serialNum para when query_offlineDev_info_by_serialNum \n");
		return NULL ;
	}
	
	p_temp_device_info = offline_dev_info_list_head.p_next_device ;
	while(p_temp_device_info != NULL)
	{
		//if(memcmp(p_temp_device_info->device_info.dev_serialNum,p_device_serialNum,DEV_SERIALNUM_LEN) == 0)
		if(strcmp(p_temp_device_info->device_info.dev_serialNum,p_device_serialNum) == 0) //match
		{
			return p_temp_device_info ;
		}

		p_temp_device_info = p_temp_device_info->p_next_device ;
	}

	return NULL ;
}


int insert_dev_into_offlineList(device_node_info_t *p_dev_node)
{
	device_node_info_t *p_temp_dev = NULL ;
	//device_node_info_t *p_prev = NULL ;
	
	if(p_dev_node == NULL)
	{
		return -1 ;
	}

	p_temp_dev = query_offlineDev_info_by_serialNum(p_dev_node->device_info.dev_serialNum) ;
	if(p_temp_dev != NULL)
	{
		debug("dev=%s has been in the offlineList \n",p_dev_node->device_info.dev_serialNum);
		return 0 ;
	}

	p_temp_dev = offline_dev_info_list_head.p_next_device ;
	if(p_temp_dev == NULL)
	{
		//first node
		offline_dev_info_list_head.p_next_device = p_dev_node ;
		offline_dev_info_list_head.total_dev_count = 1 ;
		return 0 ;
	}
	//p_prev = offline_dev_info_list_head ;
	while(p_temp_dev != NULL)
	{
		if(p_temp_dev->p_next_device == NULL) 
		{
			//insert to the end
			p_temp_dev->p_next_device = p_dev_node ;
			p_dev_node->p_next_device = NULL ;
			offline_dev_info_list_head.total_dev_count++ ;

			break ;
		}

		//p_prev = p_temp_dev ;
		p_temp_dev = p_temp_dev->p_next_device ;
	}

	return 0 ;
}


int del_DevFmtInfo_from_offlineList(device_node_info_t *p_node)
{
	device_node_info_t *p_prev = NULL ;
	device_node_info_t *p_current = NULL ;
	
	if(p_node == NULL)
	{
		return -1 ;
	}

	p_current = offline_dev_info_list_head.p_next_device ;
	p_prev = &offline_dev_info_list_head ;
	while(p_current != NULL)
	{
		//if(strncmp(p_current->device_info.dev_serialNum,p_node->device_info.dev_serialNum,sizeof(p_current->device_info.dev_serialNum)) == 0)
		if(strcmp(p_current->device_info.dev_serialNum,p_node->device_info.dev_serialNum) == 0)
		{
			//match
			//detach from the list
			p_prev->p_next_device = p_current->p_next_device ;
			p_current->p_next_device = NULL ;

			#if 1
			//free sensor_format data
			free_sensor_format_data_mem(p_current->p_sendor_format_data_head) ;

			//free dev-node
			free(p_current) ;
			p_current = NULL ;
			#endif

			if(offline_dev_info_list_head.total_dev_count > 0)
			{
				offline_dev_info_list_head.total_dev_count-- ;
			}

			break ;
		}

		p_prev = p_current ;
		p_current = p_current->p_next_device ;
	}

	return 0 ;
}

//not free the senfmt mem if we copy the dev-info from the offline list
int del_deviceNode_from_offlineList(device_node_info_t *p_node)
{
	device_node_info_t *p_prev = NULL ;
	device_node_info_t *p_current = NULL ;
	
	if(p_node == NULL)
	{
		return -1 ;
	}

	p_current = offline_dev_info_list_head.p_next_device ;
	p_prev = &offline_dev_info_list_head ;
	while(p_current != NULL)
	{
		//if(strncmp(p_current->device_info.dev_serialNum,p_node->device_info.dev_serialNum,sizeof(p_current->device_info.dev_serialNum)) == 0)
		if(strcmp(p_current->device_info.dev_serialNum,p_node->device_info.dev_serialNum) == 0)
		{
			//match
			//detach from the list
			p_prev->p_next_device = p_current->p_next_device ;
			p_current->p_next_device = NULL ;

			#if 1
			//not free sensor_format data now,senFmt data used by online-dev
			//free_sensor_format_data_mem(p_current->p_sendor_format_data_head) ;

			//free dev-node
			free(p_current) ;
			p_current = NULL ;
			#endif

			if(offline_dev_info_list_head.total_dev_count > 0)
			{
				offline_dev_info_list_head.total_dev_count-- ;
			}

			break ;
		}

		p_prev = p_current ;
		p_current = p_current->p_next_device ;
	}

	return 0 ;
}






void free_sensor_format_data_mem(sensor_data_format_t *p_sensor_format_data_mem)
{
	sensor_data_format_t *p_temp = NULL ;
	sensor_data_format_t *p_next = NULL ;
	
	if(p_sensor_format_data_mem == NULL )
	{
		return ;
	}

	p_temp = p_sensor_format_data_mem ;
	while(p_temp != NULL)
	{
		p_next = p_temp->p_next_sensor_format ;
		free(p_temp) ;
		p_temp = p_next ;
	}

	return  ;
}

int insert_sensor_format_data(sensor_data_format_t *p_sensor_format_data,uint16_t device_addr)
{
	device_node_info_t *p_temp_node = NULL ;

	if(p_sensor_format_data == NULL)
	{
		debug("err p_sensor_format_data==NULL when insert_sensor_format_data \n");
		return -1 ;
	}

	p_temp_node = query_device_info_by_deviceAddr(device_addr) ;
	if(p_temp_node == NULL)
	{
		debug("err when insert_sensor_format_data,not found dev_addr match node \n");
		return -1 ;
	}

	debug("insert sen fmtdata:index=0x%x,devaddr=0x%x \n",p_sensor_format_data->sensor_index,device_addr);

	if(p_temp_node->p_sendor_format_data_head != NULL)
	{
		free_sensor_format_data_mem(p_temp_node->p_sendor_format_data_head);
	}
	p_temp_node->p_sendor_format_data_head = p_sensor_format_data ;
	p_temp_node->device_info.sen_limit_recvNum = 0 ; //reset senLimit recvNum,maybe renew the dev-info and senFmt now

	return 0 ;
}

int append_sensor_format_data(sensor_data_format_t *p_sensor_format_data,uint16_t device_addr)
{
	device_node_info_t *p_temp_node = NULL ;
	sensor_data_format_t *p_fmt = NULL ;

	if(p_sensor_format_data == NULL)
	{
		debug("err p_sensor_format_data==NULL when append_sensor_format_data \n");
		return -1 ;
	}

	p_temp_node = query_device_info_by_deviceAddr(device_addr) ;
	if(p_temp_node == NULL || p_temp_node->p_sendor_format_data_head == NULL)
	{
		debug("err when append_sensor_format_data,not found dev_addr match node \n");
		return -1 ;
	}

	debug("append sen fmtdata:index=0x%x,devaddr=0x%x \n",p_sensor_format_data->sensor_index,device_addr);

	p_fmt = p_temp_node->p_sendor_format_data_head ;
	while(p_fmt != NULL)
	{
		if(p_fmt ->p_next_sensor_format == NULL)
		{	
			p_fmt->p_next_sensor_format = p_sensor_format_data ;
			break ;
		}

		p_fmt = p_fmt->p_next_sensor_format ;
	}


	return 0 ;
}



int insert_sensor_limit_data(uint8_t * p_one_limit_data,uint8_t data_len,uint16_t source_addr,uint8_t sen_index)
{
	sensor_data_format_t *p_sen_format_data = NULL ;
	device_node_info_t *p_node_info = NULL ;
	
	if(p_one_limit_data == NULL || data_len <= 0)
	{
		debug("input para err when insert_sensor_limit_data \n");
		return -1 ;
	}

	p_node_info = query_device_info_by_deviceAddr(source_addr) ;
	if(p_node_info == NULL )
	{
		debug("cann't find dev addr=%d when  insert_sensor_limit_data \n",source_addr);
		return -1 ;
	}

	p_sen_format_data = query_sensor_format_data(source_addr,sen_index) ;
	if(p_sen_format_data == NULL)
	{
		debug("cann't find sen_format src_addr=%d,sen_index=%d when insert_sensor_limit_data \n",source_addr,sen_index) ;
		return -1 ;
	}

	if(data_len >= sizeof(p_sen_format_data->limit_data))
	{
		memcpy(p_sen_format_data->limit_data,p_one_limit_data,sizeof(p_sen_format_data->limit_data)) ;
	}
	else
	{
		memcpy(p_sen_format_data->limit_data,p_one_limit_data,data_len);
	}

	p_sen_format_data->current_senLimit_recvFlag = 1 ; // set current-sensor limit recvFlag
	p_node_info->device_info.sen_limit_recvNum++ ;

	return 0 ;
}


//add for cmd_v1
sensor_data_format_t *query_sensor_format_bySensorID(uint16_t device_addr,uint8_t sen_ID)
{
	device_node_info_t *p_temp_node = NULL ;
	sensor_data_format_t * p_temp_format = NULL ;

	if(device_addr == 0xFFFF )
	{
		debug("dev addr err == 0xFFFF when query_sensor_format_data \n") ;
		return NULL ;
	}

	p_temp_node = query_device_info_by_deviceAddr(device_addr) ;
	if(p_temp_node == NULL)
	{
		debug("err when query_sensor_format_data,not found dev_addr match node \n");
		return NULL ;
	}

	if(p_temp_node->p_sendor_format_data_head == NULL )
	{
		debug("sensor format data NULL when query_sensor_format_data \n");
		return NULL ;
	}

	p_temp_format = p_temp_node->p_sendor_format_data_head ;
	while(p_temp_format != NULL)
	{
		if(p_temp_format->sensor_id == sen_ID )
		{
			return p_temp_format ;
		}
		
		p_temp_format = p_temp_format->p_next_sensor_format ;
	}

	return NULL ;
}



// apply to cmd_v1 and cmd_v2
sensor_data_format_t *query_sensor_format_data(uint16_t device_addr,uint8_t sen_index)
{
	device_node_info_t *p_temp_node = NULL ;
	sensor_data_format_t * p_temp_format = NULL ;

	debug("query sensor fmt data,devaddr=0x%x,sen_index=0x%x \n",device_addr,sen_index);

	if(device_addr == 0xFFFF )
	{
		debug("dev addr err == 0xFFFF when query_sensor_format_data \n") ;
		return NULL ;
	}

	p_temp_node = query_device_info_by_deviceAddr(device_addr) ;
	if(p_temp_node == NULL)
	{
		debug("err when query_sensor_format_data,not found dev_addr match node \n");
		return NULL ;
	}

	if(p_temp_node->p_sendor_format_data_head == NULL )
	{
		debug("sensor format data NULL when query_sensor_format_data \n");
		return NULL ;
	}

	p_temp_format = p_temp_node->p_sendor_format_data_head ;
	while(p_temp_format != NULL)
	{
		if(p_temp_format->sensor_index == sen_index)
		{
			return p_temp_format ;
		}
		
		p_temp_format = p_temp_format->p_next_sensor_format ;
	}

	return NULL ;
}

int parse_sensor_format_dataV2(uint8_t *p_data,uint8_t data_len)
{
	sensor_data_format_t *p_sensor_format_head = NULL ;
	sensor_data_format_t *p_temp_sensor_fmt = NULL ;
	sensor_data_format_t *p_prev_sensor = NULL ;
	device_node_info_t *p_dev_node = NULL ;
	uint16_t pkt_source_addr = 0xFFFF ;
	uint16_t pkt_dest_addr = 0xFFFF ;
	uint8_t pkt_cfgSetCnt = 0 ;
	uint8_t active_sen_num = 0 ;
	uint8_t pkt_first_sen_index = 0 ;
	uint8_t sen_fmt_data_num = 0 ;
	uint8_t temp_sen_data_num = 0 ;
	uint8_t total_fmt_data_len = 0 ;
	uint8_t fmt_sensorNum = 0 ;
	uint8_t count = 0 ;
	uint8_t cmd_data_offset = 0 ;
	uint8_t temp_sen_data_len = 0 ;
	int ret_val = 0 ;
	uint8_t current_sen_arrayID = 0 ;
	uint8_t sen_fmt_seq = 0 ;
	uint8_t res_int = 0 ;
	sint8_t res_exp = 0 ;

	if(p_data == NULL || data_len <= 0 )
	{
		debug("err input para when parse_sensor_format_dataV2 \n") ;
		return -1 ;
	}

	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	total_fmt_data_len = data_len - (WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN+WSN_CHK_LEN+WSN_CMD_END_LEN) ;
	pkt_dest_addr = get_destAddr_fromPkt(p_data,data_len) ;
	temp_sen_data_num = p_data[cmd_data_offset+1] & 0x0F ; //low 4 bits are fmt-data-num of one sensor
	if(temp_sen_data_num == 0)
	{
		sen_fmt_data_num = 10 ;
	}
	else
	{
		sen_fmt_data_num = 8 ;
	}
	pkt_first_sen_index = p_data[cmd_data_offset+1]>>4 ; //high 4 bits are first sen-index
	pkt_cfgSetCnt = p_data[cmd_data_offset] ;
	pkt_source_addr = get_sourceAddr_fromPkt(p_data,data_len) ;
	if(pkt_source_addr == 0xFFFF)
	{
		debug("src=0xFFFF,not found addr info in this sensor-format pkt,discard it now \n") ;
		return -1 ;
	}

	p_dev_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
	if(p_dev_node == NULL)
	{
		debug("not found match dev info in the buf,discard this sen-format frame and re_get dev-info \n") ;
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
		if(count > 0)
		{
			send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
		}
		
		return -1 ;
	}

	
	//maybe RevNoAndIndex err?
	if(pkt_first_sen_index == 0)
	{
		pkt_first_sen_index = p_dev_node->device_info.active_sen_index[0] ;
	}
	

	//check cfgSet_cnt
	#if 0
	if(p_temp_node != NULL )
	{
		if(node_info.device_info.dev_info_recv_time >= p_temp_node->device_info.dev_info_recv_time &&
			(node_info.device_info.dev_info_recv_time-p_temp_node->device_info.dev_info_recv_time) <= 120)
		{
			debug("dev_info recv_time too short ,<=120s ,discard this dev-info pak now \n");
			return -1 ;
		}
	}
	#else
	//maybe cfgSet_cnt overlow? !!!!!!!!!!!!!!!!!!!!!,restart this app when overflow
	if(p_dev_node->device_info.sen_fmt_recv_flag && pkt_first_sen_index <= p_dev_node->device_info.active_sen_index[0])
	{
		//maybe sen_fmt_data is sub_pak,pkt_first_sen_index > p_dev_node->device_info.active_sen_index[0]
		if(pkt_cfgSetCnt <= p_dev_node->device_info.cfgSet_cnt)
		{
			debug("pkt_first_senindex=%d,cfgSet_cnt not renew,discard this sen_name_fmt pak now \n",pkt_first_sen_index);
			return -1 ;
		}
		
	}

	if(p_dev_node->device_info.sen_fmt_recv_flag && p_dev_node->device_info.sen_fmt_recvNO >= p_dev_node->device_info.active_sensor_num)
	{
		if(pkt_cfgSetCnt <= p_dev_node->device_info.cfgSet_cnt)
		{
			debug("sen_recvNo=%d,cfgSet_cnt not renew,discard this sen_name_fmt pak now \n",p_dev_node->device_info.sen_fmt_recvNO);
			return -1 ;
		}
	}
	#endif

	

	#if 1
	//check sen_fmt first sen_index
	if(pkt_first_sen_index != p_dev_node->device_info.active_sen_index[0] && p_dev_node->device_info.sen_fmt_recv_flag == 0)
	{
		debug("first sen_index not match,we need to re-get dev_info now \n");
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
		if(count > 0)
		{
			send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
		}
		return -1 ;
	}
	#endif
	
	active_sen_num = p_dev_node->device_info.active_sensor_num ;
	if(active_sen_num <= 0 || pkt_cfgSetCnt != p_dev_node->device_info.cfgSet_cnt)
	{
		debug("active sen-num == 0 or cfgsetcnt not match,discard this sen-format frame,re-get dev-info now \n");
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
		if(count > 0)
		{
			send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
		}
		return -1 ;
	}

	if(sen_fmt_data_num <= 0 || sen_fmt_data_num > 10)
	{
		debug("err sen_fmt_data_num = %d ,discard this frame \n",sen_fmt_data_num); //8 or 10
		return -1 ;
	}

	fmt_sensorNum = (total_fmt_data_len-2)/sen_fmt_data_num ; // -2 to skip CfgSet_Cnt and RevNoAndIndex
	debug("begin to handle sen fmt data,pkt_first_sen_index=%d,fmt_senNUm=%d \n",pkt_first_sen_index,fmt_sensorNum);
	current_sen_arrayID = get_current_senArrayID(pkt_source_addr,pkt_first_sen_index) ;
	if(current_sen_arrayID == ILLEGAL_SEN_INDEX)
	{
		debug("not found match sen_arrayID when parse_sensor_format_dataV2 \n");
		return -1 ;
	}

	if(fmt_sensorNum < active_sen_num)
	{
		//include subpackage in sen_fmt and data pak
		// set sub-data-pak flag
		p_dev_node->device_info.sub_data_pak = 1 ;
	}

	debug("current sen_arrayID=%d \n",current_sen_arrayID);
	for(count=current_sen_arrayID;count<(current_sen_arrayID+fmt_sensorNum);count++)
	{
		p_temp_sensor_fmt = malloc(sizeof(sensor_data_format_t)) ;
		if(p_temp_sensor_fmt == NULL)
		{
			debug("err when malloc sen_fromat mem \n");
			if(p_sensor_format_head != NULL)
			{
				free_sensor_format_data_mem(p_sensor_format_head) ;
			}
			return -1 ;
		}

		if(p_sensor_format_head == NULL)
		{
			p_sensor_format_head = p_temp_sensor_fmt ;
		}
		
		memset(p_temp_sensor_fmt,0,sizeof(sensor_data_format_t));
		p_temp_sensor_fmt->p_next_sensor_format = NULL ;
		if(p_prev_sensor != NULL)
		{
			p_prev_sensor->p_next_sensor_format = p_temp_sensor_fmt ;
		}
		

		//copy format data into the buf
		#if 0
		p_temp_sensor_fmt->sensor_index = p_dev_node->device_info.active_sen_index[count] ;
		p_temp_sensor_fmt->sensor_id = p_data[cmd_data_offset+2+sen_fmt_seq*sen_fmt_data_num] ; //+2 to skip cfgsetcnt and revNO
		p_temp_sensor_fmt->sensor_sub_id = p_data[cmd_data_offset+2+sen_fmt_seq*sen_fmt_data_num+1] ;
		p_temp_sensor_fmt->detection_mode = (p_data[cmd_data_offset+2+sen_fmt_seq*sen_fmt_data_num+2] >> 2) & 0x0F ; //byte2.bit2-bit5
		p_temp_sensor_fmt->gas_id = ((p_data[cmd_data_offset+2+sen_fmt_seq*sen_fmt_data_num+2] & 0x03) << 8) | p_data[cmd_data_offset+2+count*sen_fmt_data_num+3] ;
		p_temp_sensor_fmt->unit_id = (p_data[cmd_data_offset+2+sen_fmt_seq*sen_fmt_data_num+4]) & 0x7F ;
		temp_sen_data_len =  (p_data[cmd_data_offset+2+sen_fmt_seq*sen_fmt_data_num+5]) & 0x03 ; //bit0-bit1
		{
			if(temp_sen_data_len == 0)
			{
				p_temp_sensor_fmt->sensor_data_len = 1 ;
			}
			else if(temp_sen_data_len == 1)
			{
				p_temp_sensor_fmt->sensor_data_len = 2 ;
			}
			else
			{
				p_temp_sensor_fmt->sensor_data_len = 4 ;
			}
		}
		#endif
		p_temp_sensor_fmt->sensor_index = p_dev_node->device_info.active_sen_index[count] ;
		memcpy(p_temp_sensor_fmt->format_data,p_data+cmd_data_offset+2+sen_fmt_seq*sen_fmt_data_num,sen_fmt_data_num); // +2 to skip cfgsetcnt and revNO
		
		p_temp_sensor_fmt->sensor_id = p_temp_sensor_fmt->format_data[0] ; 
		p_temp_sensor_fmt->sensor_sub_id = p_temp_sensor_fmt->format_data[1] ;
		p_temp_sensor_fmt->detection_mode = (p_temp_sensor_fmt->format_data[2] >> 2) & 0x0F ; //byte2.bit2-bit5
		p_temp_sensor_fmt->gas_id = ((p_temp_sensor_fmt->format_data[2] & 0x03) << 8) | p_temp_sensor_fmt->format_data[3] ;
		p_temp_sensor_fmt->unit_id = p_temp_sensor_fmt->format_data[4] & 0x7F ;
		temp_sen_data_len = p_temp_sensor_fmt->format_data[5] & 0x03 ; // byte5:bit0-bit1
		if(temp_sen_data_len == 0)
		{
			p_temp_sensor_fmt->sensor_data_len = 1 ;
		}
		else if(temp_sen_data_len == 1)
		{
			p_temp_sensor_fmt->sensor_data_len = 2 ;
		}
		else
		{
			p_temp_sensor_fmt->sensor_data_len = 4 ;
		}
		
		debug("sen_index=%d,sen_data_len = %d \n",p_temp_sensor_fmt->sensor_index,p_temp_sensor_fmt->sensor_data_len);
		//calc sen_dp and sen_df
		p_temp_sensor_fmt->sensor_dp = (p_temp_sensor_fmt->format_data[5] >> 5) & 0x07 ; //byte[5],bit5-bit7
		p_temp_sensor_fmt->sensor_df = (p_temp_sensor_fmt->format_data[5] >> 2) & 0x07 ; //byte[5],bit2-bit4

		//sensor threshold
		p_temp_sensor_fmt->sensor_threshold_mask_high = p_temp_sensor_fmt->format_data[6] ;
		p_temp_sensor_fmt->sensor_threshold_mask_low = p_temp_sensor_fmt->format_data[7] ;

		//handle res
		if(sen_fmt_data_num == 10)
		{
			res_int = p_temp_sensor_fmt->format_data[8] ; //1-255
			res_exp = p_temp_sensor_fmt->format_data[9] ; //-4--4
			p_temp_sensor_fmt->res = res_int*pow(10,res_exp) ;
			if(res_exp < 0)
			{
				p_temp_sensor_fmt->res_dp = res_exp * (-1) ;
			}
			else
			{
				p_temp_sensor_fmt->res_dp = 0 ;
			}
			debug("sensor res = %f \n",p_temp_sensor_fmt->res);
		}

		p_prev_sensor = p_temp_sensor_fmt ;
		p_temp_sensor_fmt = NULL ; //==NULL
		sen_fmt_seq++; // handle next sen_fmt_data

	}

	if( p_dev_node->device_info.sen_fmt_recv_flag == 1 && p_dev_node->device_info.sen_fmt_recvNO <= p_dev_node->device_info.active_sensor_num)
	{
		//append sen_fmt data to the list end
		ret_val = append_sensor_format_data(p_sensor_format_head,pkt_source_addr) ;
		p_dev_node->device_info.sen_fmt_recvNO += fmt_sensorNum ;
		debug("append_sensor_format_data V2 sen_fmt_recvNO %d data_len %d\n",p_dev_node->device_info.sen_fmt_recvNO,data_len);
//fxy echoview		
		if(data_len<=sizeof(p_dev_node->sensor_format_buf[1])){
			memcpy(p_dev_node->sensor_format_buf[1],p_data,data_len) ;
			p_dev_node->sensor_format_buf_len[1]=data_len;
			p_dev_node->sensor_format_recv_finished = 1;
			p_dev_node->sensor_format_recv_num = 2;				
		}		
	}
	else
	{
		// insert new sen_fmt data
		ret_val = insert_sensor_format_data(p_sensor_format_head,pkt_source_addr) ;
		p_dev_node->device_info.sen_fmt_recvNO = fmt_sensorNum ;
//fxy echoview
        debug("insert_sensor_format_data V2 sen_fmt_recvNO %d data_len %d\n",p_dev_node->device_info.sen_fmt_recvNO,data_len);
		if(data_len<=sizeof(p_dev_node->sensor_format_buf[0])){
			memcpy(p_dev_node->sensor_format_buf[0],p_data,data_len) ;
			p_dev_node->sensor_format_buf_len[0]=data_len;
            if(p_dev_node->device_info.sen_fmt_recvNO < p_dev_node->device_info.active_sensor_num){
				p_dev_node->sensor_format_recv_finished = 0;
				p_dev_node->sensor_format_recv_num = 2;
			}else{
				p_dev_node->sensor_format_recv_finished = 1;
				p_dev_node->sensor_format_recv_num = 1;				
			}			
		}
	}
	if(ret_val != 0)
	{
		debug("err insert sen_format_info when parse_sensor_format_dataV2 \n");
		if(p_sensor_format_head != NULL)
		{
			free_sensor_format_data_mem(p_sensor_format_head) ;
		}

		return -1 ;
	}

	//p_dev_node->device_info.sen_fmt_recvNO += fmt_sensorNum ;
	p_dev_node->device_info.sen_fmt_recv_flag = 1 ;


	return 0 ;
	
}

int calc_cmd_chksum(uint8_t *data,uint16_t data_len)
{
	int chk_sum = 0 ;
	int temp_chk = 0 ;
	uint16_t cmd_data_count = 0 ;
	if(data == NULL || data_len <= 0)
	{
		debug("err input para when calc_cmd_chksum \n") ;
		return -1 ;
	}
	for(cmd_data_count = 0 ; cmd_data_count < data_len ; cmd_data_count++ )
	{
		temp_chk += data[cmd_data_count] ;
	}
	chk_sum = 0x01+(0xFF-temp_chk&0xFF) ;
	return chk_sum ;
}


int calc_mesh_cmd_chksum(uint8_t *data,uint16_t data_len,uint16_t cmd_data_len)
{
	int chk_sum = 0 ;
	int temp_chk = 0 ;
	uint16_t cmd_data_offset = 8 ; //skip from pkt-hdr to cmd
	uint16_t cmd_data_count = 0 ;

	if(data == NULL || data_len <= 0)
	{
		debug("err input para when calc_mesh_cmd_chksum \n") ;
		return -1 ;
	}

	for(cmd_data_count = 0 ; cmd_data_count < cmd_data_offset ; cmd_data_count++ )
	{
		temp_chk += data[cmd_data_count] ;
	}
	//temp_chk = data[0]+data[1]+data[2]+data[3]+data[4]+data[5]+data[6]+data[7] ; //from pkt hdr to cmd
	
	if(cmd_data_len > 0)
	{
		for(cmd_data_count=0; cmd_data_count < cmd_data_len; cmd_data_count++)
		{
			temp_chk = temp_chk + data[cmd_data_offset+cmd_data_count] ;
		}
	}

	chk_sum = 0x01+(0xFF-temp_chk&0xFF) ;

	return chk_sum ;
}

int fill_cmd_data(uint8_t *buf,uint16_t buf_len,uint8_t *cmd_data,uint16_t cmd_data_len)
{
	uint16_t count = 0 ;
	
	if(buf == NULL || buf_len <= 0 || cmd_data == NULL || cmd_data_len <= 0)
	{
		debug("err input para when fill_cmd_data \n");
		return -1 ;
	}

	if(cmd_data_len <= buf_len)
	{
		memcpy(buf,cmd_data,cmd_data_len) ;
		count = cmd_data_len ;
	}

	return count ;
}


//apply to cmd_v1 and cmd_v2
int build_wsn_cmdV2(uint8_t *buf,uint16_t buf_len,uint8_t cmd_id,uint8_t *p_cmd_data,uint16_t cmd_data_len,uint16_t source_addr,uint16_t dest_addr)
{
	int count = 0 ;
	int temp_count = 0 ;
	uint8_t cmd_val = 0 ;
	uint8_t cmd_ver = 0x38 ;
	uint8_t pkt_hdr = 0x7B ;
	uint8_t pkt_end = 0x7D ;
	uint16_t len_location = 0 ;
	uint16_t chk_location = 0 ;
	uint8_t encrypt_len = 0 ;
	uint8_t cmdID_offset = 0 ;
	uint8_t cmdData_offset = 0 ;
	device_node_info_t *p_dev_node = NULL ;
	uint8_t encrypt_buf[200] ;
	uint8_t encrypt_counter[16] ;
	uint32_t send_time = 0 ;
	uint8_t data2_buf[64] ;
	uint8_t data2_len = 0 ;
	int key_count = 0 ;
	uint8_t local_cmdData_len = 0 ;
	
	

	if( buf== NULL || buf_len < GET_DEVICE_INFO_CMD_LEN)
	{
		debug("err buf==NULL when build_get_device_info_cmdV2 \n");
		return -1 ;
	}

	local_cmdData_len = cmd_data_len ;

	//pkt hdr
	buf[count++] = pkt_hdr ;
	//cmd_ver
	buf[count++] = cmd_ver ;

	//len
	len_location = count ;
	buf[count++] = 0 ; //pkt-len,from dest to eop

	//dest addr
	buf[count++] = dest_addr >> 8 ; //high byre
	buf[count++] = dest_addr & 0xFF ; //low byte

	//source addr
	buf[count++] = source_addr >> 8 ;
	buf[count++] = source_addr & 0xFF ;

	//cmd val
	cmdID_offset = count ;
	buf[count++] = cmd_id ;
	encrypt_len++ ;
	

	//fill cmd_data
	cmdData_offset = count ;
	if(cmd_data_len > 0 && p_cmd_data != NULL && cmd_data_len <= (buf_len-count) )
	{
		//count += fill_cmd_data(&buf[count],buf_len-count,cmd_data,cmd_data_len) ;
		memcpy(&buf[count],p_cmd_data,cmd_data_len) ;
		count += cmd_data_len ;
		encrypt_len += cmd_data_len ;
		
	}

	//encrypt cmdID+cmdData if necessary
	p_dev_node = query_device_info_by_deviceAddr(dest_addr) ;
	if(p_dev_node != NULL && p_dev_node->device_info.dev_support_encrypt && cmd_id != WSN_EXCHANGE_KEY_CMDID )
	{
		memset(encrypt_buf,0,sizeof(encrypt_buf)) ;
		memset(encrypt_counter,0,sizeof(encrypt_counter)) ;
		time(&send_time) ;
		memcpy(encrypt_counter,&p_dev_node->device_info.session_key[16],12);//sk 17-28 
		//memcpy(encrypt_counter,&p_dev_node->device_info.dataPak_sk[16],12);//dataPak_sk 17-28 
		
		//memcpy(&encrypt_counter[12],&send_time,4) ;
		save_in_bigEndian(&encrypt_counter[12],(uint8_t *)&send_time,4,DATA_TYPE_INT) ;
		
		AES_ctr128_encrypt(&buf[cmdID_offset],&encrypt_buf[0],encrypt_len,p_dev_node->device_info.session_key,encrypt_counter); //session_key
		//AES_ctr128_encrypt(&buf[cmdID_offset],&encrypt_buf[0],encrypt_len,p_dev_node->device_info.dataPak_sk,encrypt_counter); //dataPak_sk

		#if 1
		debug("send data,encryptkey:");
		for(key_count=0;key_count<32;key_count++)
		{
			printf("%02x",p_dev_node->device_info.session_key[key_count]);
		}
		printf("\n");
		#endif

		//reset cmdID,cpy encrypt-data(include old-cmdID and cmd data)
		count = cmdID_offset ;
		buf[count++] = WSN_ENCRYPT_DATA_PAK_MSGID ; //set secret_mcdID
		//copy data1,Time Stamp + AES-CTR(New Data) with first 16 bytes of session key 
		save_in_bigEndian(&buf[count],(uint8_t *)&send_time,4,DATA_TYPE_INT);
		count += 4 ;
		memcpy(&buf[count],&encrypt_buf[0],encrypt_len) ;
		count += encrypt_len ;
		debug("send data,encrypt_len=%d \n",encrypt_len);

		//fill real pkt_len befor we calc data2
		buf[len_location] = (count+ENCRYPT_DATA2_LEN+WSN_CHK_LEN+WSN_CMD_END_LEN) - (WSN_CMD_HDR_LEN + WSN_CMD_VER_LEN + WSN_PKTLENGTH_LEN ) ; //from dest to EOP

		//data2
		memset(data2_buf,0,sizeof(data2_buf));
		hmac_sha256(&buf[0],count,data2_buf,&data2_len,p_dev_node->device_info.session_key,16); // input data: from sop to data1,session_key
		debug("send data,calc data2,total len=%d \n",count);
		//hmac_sha256(&buf[0],count,data2_buf,&data2_len,p_dev_node->device_info.dataPak_sk,16); // input data: from sop to data1,dataPak_sk
		
		//hmac_sha256(&buf[cmdData_offset],encrypt_len+4,data2_buf,&data2_len,p_dev_node->device_info.session_key,16); // input=data1
		//memcpy(&buf[count],data2_buf,ENCRYPT_DATA2_LEN) ;
		for(key_count=0;key_count<32;key_count++)
		{
			if((key_count%4) == 0)
			{
				buf[count++] = data2_buf[key_count] ;
			}
		}
		//count += ENCRYPT_DATA2_LEN ;

		local_cmdData_len = 4 + encrypt_len + ENCRYPT_DATA2_LEN ; // data1_len=timestamp_len + encrypt_len;data2_len=8
		debug("send pak: encrypt data2_len=%d local_cmdData_len=%d\n",data2_len,local_cmdData_len);
		
	}
	

	//chk_sum
	chk_location = count ;
	buf[count++] = 0 ;

	//pkt end
	buf[count++] = pkt_end ;

	//fill pkt len
	buf[len_location] = count - (WSN_CMD_HDR_LEN + WSN_CMD_VER_LEN + WSN_PKTLENGTH_LEN ) ; //from dest to EOP

	//fill chk_sum ;
	buf[chk_location] = calc_mesh_cmd_chksum(buf,count,local_cmdData_len) ;

	//print wsn cmd msg,only for debug
	debug("building send-to-remote-cmd,total wsn cmd len = %d \n",count);
	#if 0
	for(temp_count=0;temp_count<count;temp_count++)
	{
		printf("%02x ",buf[temp_count]) ;
	}
	debug("\n");
	#endif

	return count ;
	
}


int build_get_device_info_cmdV2(uint8_t *buf,uint16_t buf_len,uint8_t cmd_id,uint16_t cmd_data_len,uint16_t source_addr,uint16_t dest_addr)
{
	int count = 0 ;
	uint8_t cmd_val = 0x2E ;
	uint8_t cmd_ver = 0x38 ;
	uint8_t pkt_hdr = 0x7B ;
	uint8_t pkt_end = 0x7D ;
	uint16_t len_location = 0 ;
	uint16_t chk_location = 0 ;

	if( buf== NULL || buf_len < GET_DEVICE_INFO_CMD_LEN)
	{
		debug("err buf==NULL when build_get_device_info_cmdV2 \n");
		return -1 ;
	}

	//pkt hdr
	buf[count++] = pkt_hdr ;
	//cmd_ver
	buf[count++] = cmd_ver ;

	//len
	len_location = count ;
	buf[count++] = 0 ; //pkt-len,from dest to eop

	//dest addr
	buf[count++] = dest_addr >> 8 ; //high byre
	buf[count++] = dest_addr & 0xFF ; //low byte

	//source addr
	buf[count++] = source_addr >> 8 ;
	buf[count++] = source_addr & 0xFF ;

	//cmd val
	buf[count++] = cmd_val ;

	//chk_sum
	chk_location = count ;
	buf[count++] = 0 ;

	//pkt end
	buf[count++] = pkt_end ;

	//fill pkt len
	buf[len_location] = count - (WSN_CMD_HDR_LEN + WSN_CMD_VER_LEN + WSN_PKTLENGTH_LEN ) ;

	//fill chk_sum ;
	buf[chk_location] = calc_mesh_cmd_chksum(buf,count,cmd_data_len) ;

	return count ;
	
}

int build_get_nameSensor_format_cmdV2(uint8_t *buf,uint16_t buf_len,uint16_t source_addr,uint16_t dest_addr)
{
	if(buf == NULL || buf_len <= 0 )
	{
		debug("inuput para err when build_get_nameSensor_format_cmdV2 \n");
		return -1 ;
	}

	return 0 ;
}
#if 0
uint8_t * remove_rmci_hdr(uint8_t *p_data,uint16_t data_len)
{
	uint8_t pkt_hdr = 0x5B ;
	uint8_t *p_pkt_hdr = NULL ;
	uint16_t count = 0 ; 
	
	if(p_data == NULL || data_len <= 0 )
	{
		debug("err input para when remove_rmci_hdr \n") ;
		return NULL ;
	}

	for(count=0;count<data_len;count++)
	{
		if(p_data[count] == pkt_hdr )
		{
			p_pkt_hdr = &p_data[count] ;
			*p_skip_bytes = count ;
			break ;
		}
	}

	return p_pkt_hdr ;
}
uint8_t * find_pkt_rmci_hdr(uint8_t *p_data,uint16_t data_len,uint16_t *p_skip_bytes)
{
	uint8_t pkt_hdr = 0x5B ;
	uint8_t *p_pkt_hdr = NULL ;
	uint16_t count = 0 ; 
	
	if(p_data == NULL || data_len <= 0 || p_skip_bytes == NULL)
	{
		debug("err input para when find_pkt_rmci_hdr \n") ;
		return NULL ;
	}

	for(count=0;count<data_len;count++)
	{
		if(p_data[count] == pkt_hdr )
		{
			p_pkt_hdr = &p_data[count] ;
			*p_skip_bytes = count ;
			break ;
		}
	}

	return p_pkt_hdr ;
}
#endif


uint8_t * find_pkt_hdr(uint8_t *p_data,uint16_t data_len,uint16_t *p_skip_bytes)
{
	uint8_t pkt_hdr = 0x7B ;
	uint8_t *p_pkt_hdr = NULL ;
	uint16_t count = 0 ; 
	
	if(p_data == NULL || data_len <= 0 || p_skip_bytes == NULL)
	{
		debug("err input para when find_pkt_hdr \n") ;
		return NULL ;
	}

	for(count=0;count<data_len;count++)
	{
		if(p_data[count] == pkt_hdr )
		{
			p_pkt_hdr = &p_data[count] ;
			*p_skip_bytes = count ;
			break ;
		}
	}

	return p_pkt_hdr ;
}

int get_wsn_cmdID_fromPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t offset = 0 ;
	
	if(p_data == NULL )
	{
		return -1 ;
	}

	offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN ;
	return p_data[offset] ;
}

uint16_t get_sourceAddr_fromPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t source_offset = 5 ; //skip from sop to dest
	uint16_t source_addr =  0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_sourceAddr_fromPkt \n");
		return 0xFFFF ;
	}

	source_addr = (p_data[source_offset] << 8) | p_data[source_offset+1] ; // MSB first


	return source_addr ;
}

uint16_t get_destAddr_fromPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t dest_offset = 3 ; //skip from sop to len
	uint16_t dest_addr =  0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_destAddr_fromPkt \n");
		return 0xFFFF ;
	}

	dest_addr = (p_data[dest_offset] << 8) | p_data[dest_offset+1] ; // MSB first

	return dest_addr ;
}


//only used for data-pak
uint8_t get_senNum_fromDataPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	uint8_t temp_uintinfo = 0 ;
	uint8_t sen_num = 0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_senNum_fromPkt \n");
		return 0 ;
	}

	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	temp_uintinfo = p_data[cmd_data_offset+6] ; //cmd_data[6] == uintinfo 
	sen_num = (temp_uintinfo >> 2) & 0x0F ; // bit2-bit5 == sensor-num

	debug("data pkt include %d sensor-data \n",sen_num);

	return sen_num ;
}


//only used for dada-pak
uint8_t get_hwInfo_fromDataPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	uint8_t temp_hw_info = 0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_hwInfo_fromPkt \n");
		return 0 ;
	}
	
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	temp_hw_info = p_data[cmd_data_offset] ; //first byte of cmd-data is hwInfo


	return temp_hw_info ;
}

//only used for data-pak
uint8_t get_appAlarm_fromDataPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	uint8_t appAlarm_info = 0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_appAlarm_fromPkt \n");
		return 0 ;
	}
	
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	appAlarm_info = p_data[cmd_data_offset+1] ; //+1 to skip hwInfo


	return appAlarm_info ;
}



int get_comm_devData(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	
	if(p_data == NULL || data_len <= 0)
	{
		debug("err para when get_comm_devData \n");
		return -1 ;
	}

	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	comm_dev_data.hw_info = p_data[cmd_data_offset+0] ;
	comm_dev_data.app_alarm = p_data[cmd_data_offset+1] ;
	comm_dev_data.rm_debug_field = p_data[cmd_data_offset+2] ; //remote alarm info
	comm_dev_data.cfg_set_cnt = p_data[cmd_data_offset+3] ;
	comm_dev_data.alarm_set_cnt = p_data[cmd_data_offset+4] ;
	comm_dev_data.unit_err = p_data[cmd_data_offset+5] ;
	comm_dev_data.unit_info = p_data[cmd_data_offset+6] ;
	comm_dev_data.power_percent = p_data[cmd_data_offset+7] ;

	return 0 ;
}


uint8_t get_app_alarm(void)
{
	uint8_t app_alarm = 0 ;


	if(comm_dev_data.app_alarm & 0x04) //bit 2
	{
		app_alarm = 4 ;
	}
	else
	{
		app_alarm = comm_dev_data.app_alarm & 0x03 ; //bit0-bit1
	}
	
	return app_alarm ;
}

uint8_t get_sateliteNum(void)
{
	uint8_t num = 0 ;

	num = comm_dev_data.app_alarm & 0xF0 ; //bit4-bit7
	
	return num  ;
}

int get_app_alarmStr(uint8_t *p_str_buf,uint8_t buf_len)
{
	uint8_t app_alarm = 0 ;
	
	if(p_str_buf == NULL || buf_len <= 0)
	{
		return -1 ;
	}

	app_alarm = get_app_alarm() ;

	switch(app_alarm)
	{
		case 1 :
			strlcpy(p_str_buf,"Mandown Warning",buf_len) ;
			break ;
		case 2 :
			strlcpy(p_str_buf,"Mandown Alarm",buf_len) ;
			break ;
		case 3 :
			strlcpy(p_str_buf,"Super Alarm",buf_len) ;
			break ;
		case 4 :
			strlcpy(p_str_buf,"Panic Alarm",buf_len) ;
			break ;
		default :
			strlcpy(p_str_buf,"other App Alarm",buf_len) ;
			//debug("maybe no appAlarm now,app_alarm=%d \n",app_alarm);
			break ;
	}

	return 0 ;
}

//only used for data-pak
uint8_t get_rmDebugField_fromDataPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	uint8_t rmDebugField_info = 0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_rmDebugField_fromPkt \n");
		return 0 ;
	}
	
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	rmDebugField_info = p_data[cmd_data_offset+2] ; //+2 to skip hwInfo + appAlarm


	return rmDebugField_info ;
}


//only used for data-pak
uint8_t get_cfgsetCnt_fromDataPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	uint8_t cfgsetCnt_info = 0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_cfgsetCnt_fromPkt \n");
		return 0 ;
	}
	
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	cfgsetCnt_info = p_data[cmd_data_offset+3] ; //+3 to skip hwInfo + appAlarm + rmDebugField


	return cfgsetCnt_info ;
}


//only used for data-pak
uint8_t get_AlarmSetCnt_fromDataPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	uint8_t alarmSetCnt_info = 0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_AlarmSetCnt_fromPkt \n");
		return 0 ;
	}
	
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	alarmSetCnt_info = p_data[cmd_data_offset+4] ; //+4 to skip hwInfo + appAlarm + rmDebugField +cfgSetCnt


	return alarmSetCnt_info ;
}

//only used for data-pak
uint8_t get_unitErr_fromDataPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	uint8_t unitErr_info = 0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_unitErr_fromPkt \n");
		return 0 ;
	}
	
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	unitErr_info = p_data[cmd_data_offset+5] ; //+5 to skip hwInfo + appAlarm + rmDebugField +cfgSetCnt+alarmSetCnt


	return unitErr_info ;
}

//only used for data-pak
uint8_t get_unitInfo_fromDataPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	uint8_t unit_info = 0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_unitInfo_fromPkt \n");
		return 0 ;
	}
	
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	unit_info = p_data[cmd_data_offset+6] ; //+6 to skip hwInfo + appAlarm + rmDebugField +cfgSetCnt+alarmSetCnt+unitErr


	return unit_info ;
}


uint8_t get_powerPercent_fromDataPkt(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	uint8_t powerPercent_info = 0 ;

	if(p_data == NULL )
	{
		debug("err input para when get_unitInfo_fromPkt \n");
		return 0 ;
	}
	
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	powerPercent_info = p_data[cmd_data_offset+7] ; //+7to skip hwInfo + appAlarm + rmDebugField +cfgSetCnt+alarmSetCnt+unitErr+uintInfo


	return powerPercent_info ;
}


void init_expected_cmd_list(void)
{
	//memset(&expected_resp_cmd_head,0,sizeof(expected_resp_cmd_head));
	//expected_resp_cmd_head.cmd_id = 0xFF ;
	//expected_resp_cmd_head.dest_addr = 0xFFFF ;

	return ;
}

int insert_expected_cmd_list(expected_resp_cmd_list_t *p_expected_cmd)
{
	expected_resp_cmd_list_t *p_temp = NULL ;
	
	if(p_expected_cmd == NULL)
	{
		debug("err para when insert_expected_cmd_list \n");
		return -1 ;
	}


	if(p_expected_resp_cmd_head == NULL)
	{
		p_expected_resp_cmd_head = p_expected_cmd;
		p_expected_resp_cmd_head->p_next = NULL ;

		return 0 ;
	}
	
	p_temp = p_expected_resp_cmd_head ;
	while(p_temp != NULL)
	{
		if(p_temp->p_next == NULL)
		{
			p_temp->p_next = p_expected_cmd ;
			return 0 ;
		}

		p_temp = p_temp->p_next ;
	}

	return 0 ;
}


//no serial info in data-pak,so we can use source_addr info only
expected_resp_cmd_list_t *query_expected_cmd_list(uint16_t pkt_source_addr,uint8_t cmd_id)
{
	expected_resp_cmd_list_t *p_temp_list = NULL ;
	expected_resp_cmd_list_t *p_prev_list = NULL ;

	if(pkt_source_addr == 0xFFFF)
	{
		debug("dest addr = %d when  query_expected_cmd_list \n",pkt_source_addr) ;
		//return NULL ;
	}

	p_temp_list = p_expected_resp_cmd_head ;
	while(p_temp_list != NULL)
	{
		if(p_temp_list->dest_addr == pkt_source_addr && p_temp_list->cmd_id == cmd_id)
		{
			return p_temp_list ;
		}

		p_temp_list = p_temp_list->p_next ;
	}

	return NULL ;
}

#if 0
void check_and_reSend_timeout_cmd(void)
{
	expected_resp_cmd_list_t *p_temp = NULL ;
	expected_resp_cmd_list_t *p_temp_next = NULL ;
	device_node_info_t *p_node = NULL ;
	
	
	time_t time_val = 0 ;

	p_temp = p_expected_resp_cmd_head ;
	while(p_temp != NULL)
	{
		//del over MAX_NO_RESP_NUM cmd
		p_temp_next = p_temp->p_next ;
		if(p_temp->no_resp_count >= MAX_NO_RESP_NUM)
		{
			p_node = query_device_info_by_deviceAddr(p_temp->dest_addr) ;
			if(p_node != NULL )
			{
				debug("devaddr=%d,cmdid=%d no resp over 3 times,set this dev offline now when check_and_reSend_timeout_cmd \n",p_temp->dest_addr,p_temp->cmd_id);
				set_dev_offline_status(p_node) ;
				
			}
			del_expected_resp_cmd(p_temp->dest_addr,p_temp->cmd_id) ;
			
		}
		p_temp = p_temp_next ;
	}

	
	p_temp = p_expected_resp_cmd_head ;

	while(p_temp != NULL)
	{	
		//check and re-send cmd now
		time(&time_val);
		if(time_val >= p_temp->resp_timer)
		{
			
			if((time_val-p_temp->resp_timer) >= WSN_RESP_TIMEOUT)
			{
				// re-send cmd now
				debug("timeval=%u,resp_timer=%u \n",time_val,p_temp->resp_timer);
				debug("found resp-timeout cmd,re-send now,destaddr=%d,cmdid=%d when check_and_reSend_timeout_cmd\n",p_temp->dest_addr,p_temp->cmd_id) ;
				send_expectedResp_cmd_toRemote(p_temp->cmd,p_temp->cmd_len) ;
			}
		}
		else
		{
			//maybe timer overflow,update timer now
			p_temp->resp_timer = time_val ;
		}

		p_temp = p_temp->p_next ;
	}

	return ;
}


void check_and_reSend_cmd_byDevAddr(uint16_t dev_addr)
{
	expected_resp_cmd_list_t *p_temp = NULL ;
	expected_resp_cmd_list_t *p_temp_next = NULL ;
	device_node_info_t *p_node = NULL ;
	
	time_t time_val = 0 ;
	
	p_temp = p_expected_resp_cmd_head ;

	while(p_temp != NULL)
	{	
		//check and re-send cmd now
		if(dev_addr == p_temp->dest_addr)
		{
			// re-send cmd now
			debug("found no-resp cmd,re-send now,destaddr=%d,cmdid=%d when check_and_reSend_cmd_byAddr\n",p_temp->dest_addr,p_temp->cmd_id) ;
			send_expectedResp_cmd_toRemote(p_temp->cmd,p_temp->cmd_len) ;
			break ; //only handle one cmd
		}

		p_temp = p_temp->p_next ;
	}

	return ;
}
#endif



int del_expected_resp_cmd(uint16_t pkt_source_addr,uint8_t cmd_id)
{
	expected_resp_cmd_list_t *p_current = NULL ;
	expected_resp_cmd_list_t *p_prev = NULL ;
	
	if(pkt_source_addr == 0xFFFF)
	{
		debug("dest addr = %d when  del_expected_resp_cmd \n",pkt_source_addr) ;
		//return -1 ;
	}

	p_current = p_expected_resp_cmd_head ;
	while(p_current != NULL)
	{
		
		if( (p_current->dest_addr == pkt_source_addr || p_current->dest_addr == 0xFFFF) && p_current->cmd_id == cmd_id)
		{
			debug("del expected resp cmd,addr=%d,cmd_id=%d \n",p_current->dest_addr,p_current->cmd_id);
			if(p_prev == NULL ) // first node,p_temp_list == p_expected_resp_cmd_head
			{
				p_expected_resp_cmd_head = p_current->p_next ; // detached p_current
			}
			else
			{
				p_prev->p_next = p_current->p_next ; // detached p_current
			}

			free(p_current) ;
			p_current = NULL ;

			break ;
		}

		p_prev = p_current ;
		p_current = p_current->p_next ;
		
	}

	return 0 ;
}


int del_expected_resp_cmd_1(uint16_t pkt_source_addr,uint8_t cmd_id)
{
	expected_resp_cmd_list_t *p_current_node = NULL ;
	expected_resp_cmd_list_t *p_prev_node = NULL ;
	expected_resp_cmd_list_t *p_del_node = NULL ;
	
	if(pkt_source_addr == 0xFFFF)
	{
		debug("err addr = %d when  del_expected_resp_cmd \n",pkt_source_addr) ;
		return -1 ;
	}

	p_current_node = p_expected_resp_cmd_head ;
	
	while(p_current_node != NULL)
	{
	
		if(p_current_node->dest_addr == pkt_source_addr && p_current_node->cmd_id == cmd_id)
		{
			
			p_del_node = p_current_node ;
			if(p_del_node == p_expected_resp_cmd_head) 
			{
				//now at list-head,re-set p_expected_resp_cmd_head,maybe == NULL
				p_expected_resp_cmd_head =  p_expected_resp_cmd_head->p_next ; 
			}
			
			//detach p_del
			if(p_prev_node != NULL)
			{
				p_prev_node->p_next = p_del_node->p_next ;
			}
			else
			{
				//at list-head
				p_prev_node = p_del_node ;
			}

			p_current_node = p_prev_node ; //set current at prev if del p_current_node
			
		}
		p_prev_node = p_current_node ;
		if(p_prev_node != NULL)
		{
			p_current_node = p_prev_node->p_next ;
		}

		if(p_del_node != NULL)
		{
			debug("success to del expected resp cmd,addr=%d,cmd_id=%d \n",p_del_node->dest_addr,p_del_node->cmd_id);
			free(p_del_node) ;
			p_del_node = NULL ;
		}
		
	}

	return 0 ;
}






//maybe the recved-data include multi-pkts
uint8_t * delimit_data_packet(uint8_t *p_data,uint16_t data_len,uint8_t *pkt_num)
{
	//uint8_t pkt_hdr = 0x7B ;
	uint8_t pkt_end = 0x7D ;
	int count = 0 ;
	int temp_pkt_len = 0 ;
	uint8_t *p_first_pkt_hdr = NULL ;
	uint8_t *p_temp_hdr = NULL ;
	uint16_t temp_pktend_pos = 0 ;
	uint16_t skip_bytes_bf_hdr = 0 ;
	

	if(p_data == NULL || data_len <= 0 || pkt_num == NULL)
	{
		debug("err input para when find_pkt_hdr \n");
		return NULL ;
	}

	
	p_first_pkt_hdr = find_pkt_hdr(p_data,data_len,&skip_bytes_bf_hdr);

	if(p_first_pkt_hdr == NULL)
	{
		debug("not found pkt hdr in the input-data,discard this pkt now \n");
		return NULL ;
	}

	*pkt_num = 0 ;
	temp_pkt_len = 0 ;
	p_temp_hdr = p_first_pkt_hdr ;
	for(count=0;count<data_len;)
	{
		p_temp_hdr = find_pkt_hdr(&p_data[count],data_len-count,&skip_bytes_bf_hdr);
		if(p_temp_hdr == NULL)
		{
			break ;
		}

		temp_pkt_len = p_temp_hdr[2] ; //skip sop+ver ==2bytes
		temp_pktend_pos = temp_pkt_len+3-1 ; //temp_pkt_len+3==total pkt-len
		
		if(p_temp_hdr[temp_pktend_pos] == pkt_end ) 
		{
			
			*pkt_num += 1 ;
			count += skip_bytes_bf_hdr + temp_pkt_len+3 ;
		}
		else
		{
			debug("pkt end err when delimit_data_packet \n");
			break ;
		}
	}

	return p_first_pkt_hdr ;
}

//single precision 
int convert_bin_toFloat(uint8 *p_data,uint8_t data_len,float *p_float_data)
{
	uint8_t sign_bit = 0 ;
	int mantissa_val = 0 ;
	int temp_mantissa_val = 0 ;
	int exp_val = 0 ;
	int convert_num = 0 ;
	
	if(p_data == NULL || data_len > 4 )
	{
		debug("err para when convert_bin_toFloat \n");
		return -1 ;
	}

	convert_num = p_data[0] | (p_data[1]<<8) | (p_data[2]<<16) | (p_data[3]<<24) ;
	sign_bit = (p_data[0]>>7)&0x01 ; //0 or 1
	mantissa_val = convert_num & 0x007FFFFF ; //low 23 bits


	return 0;
}


void init_dev_data_buf(void)
{
	
	memset(&comm_dev_data,0,sizeof(comm_dev_data));
	memset(&dev_dio_data,0,sizeof(dev_dio_data));
	memset(&dev_zigbee_data,0,sizeof(dev_zigbee_data)) ;
	memset(&dev_gps_data,0,sizeof(dev_gps_data));
}

void free_dev_sensorData_mem(sensor_data_t *p_dev_sensor_data_buf)
{
	sensor_data_t *p_next = NULL ;
	sensor_data_t *p_current = NULL ;
	
	if(p_dev_sensor_data_buf == NULL)
	{
		debug("err para when free_dev_sensorData_mem \n");
		return  ;
	}

	p_current = p_dev_sensor_data_buf ;
	while(p_current != NULL)
	{
		p_next = p_current->p_next_sensor_data ;
		free(p_current) ;
		p_current = p_next ;
	}

	return ;
}

void free_dev_sensorData_memList(void)
{
	sensor_data_t *p_next = NULL ;
	sensor_data_t *p_current = NULL ;
	

	p_current = p_dev_sensorData_head ;
	while(p_current != NULL)
	{
		p_next = p_current->p_next_sensor_data ;
		free(p_current) ;
		p_current = p_next ;
	}

	p_dev_sensorData_head = NULL ;

	return ;
}

void free_specified_sensorData_memList(sensor_data_t *p_list)
{
	sensor_data_t *p_next = NULL ;
	sensor_data_t *p_current = NULL ;
	

	p_current = p_list ;
	while(p_current != NULL)
	{
		p_next = p_current->p_next_sensor_data ;
		free(p_current) ;
		p_current = p_next ;
	}

	return ;
}

sensor_data_t *query_sensorData_bySenIndex(sensor_data_t *p_list,uint8_t sen_index)
{
	sensor_data_t * p_temp = NULL ;

	p_temp = p_list ;
	while(p_temp != NULL)
	{
		if(p_temp->sensor_index == sen_index)
		{
			return p_temp ;
		}

		p_temp = p_temp->p_next_sensor_data ;
	}
	
	return NULL ;
}


int calc_sensor_Reading(sensor_data_format_t *p_sen_fmt,int sReading,uint32_t uReading,float *reading_out)
{
	float tmp_reading_out = 0.0 ;
	uint32_t amp = 0 ;
	uint32_t tmp_res = 0 ;
	uint32_t mod = 0 ;
	uint32_t rem = 0 ;
	uint32_t tmp_reading = 0 ;
	
	if(p_sen_fmt == NULL || reading_out == NULL)
	{
		debug("err para when calc_sensor_Reading \n");
		return -1 ;
	}

	if( (p_sen_fmt->format_data[2] & 0x80) ) //bit7 == 1`---signed int
	{
		if(sReading == 0)
		{
			debug("sReading=0 when calc_sensor_Reading \n");
			*reading_out = 0.0 ;
			return 0 ;
		}
		
		//calc sensor real data
		if(p_sen_fmt->res != 0.0)
		{
			amp = pow(10,p_sen_fmt->sensor_dp+p_sen_fmt->sensor_df) ;
			tmp_res = p_sen_fmt->res * amp ;
			mod = sReading/tmp_res ;
			rem = sReading%tmp_res ;
			tmp_reading = rem*2/tmp_res + mod ;
			tmp_reading = tmp_reading*tmp_res ;

			*reading_out = ((float)tmp_reading)/amp ;
			
			debug("res reading = %f \n",*reading_out);
		}
		else
		{
			*reading_out = ( (int)( (float) ( sReading/pow(10,p_sen_fmt->sensor_df))+0.5) )/pow(10,p_sen_fmt->sensor_dp) ;
		}

	}
	else
	{
		if(uReading == 0)
		{
			debug("uReading=0 when calc_sensor_Reading \n");
			*reading_out = 0.0 ;
			return 0 ;
		}
		
		//calc real val
		if(p_sen_fmt->res != 0.0)
		{
			amp = pow(10,p_sen_fmt->sensor_dp+p_sen_fmt->sensor_df) ;
			tmp_res = p_sen_fmt->res * amp ;
			mod = uReading/tmp_res ;
			rem = uReading%tmp_res ;
			tmp_reading = rem*2/tmp_res + mod ;
			tmp_reading = tmp_reading*tmp_res ;

			*reading_out = ((float)tmp_reading)/amp ;
			debug("res reading = %f \n",*reading_out);
		}
		else
		{
			*reading_out = ( (unsigned int)( (float) ( uReading/pow(10,p_sen_fmt->sensor_df))+0.5) )/pow(10,p_sen_fmt->sensor_dp) ;
		}
	}

	debug("reading_out=%f after calc_sensor_Reading \n",*reading_out);

	return 0 ;
}



int insert_dev_sensor_data(sensor_data_t *p_sensor_data)
{
	sensor_data_t *p_next = NULL ;
	sensor_data_t *p_current = NULL ;
	
	if(p_sensor_data == NULL)
	{
		debug("err para when insert_dev_sensor_data \n") ;
		return -1 ;
	}

	p_current = p_dev_sensorData_head ;
	if(p_current == NULL)
	{
		p_dev_sensorData_head = p_sensor_data ;
		return 0 ;
	}

	while(p_current != NULL)
	{
		//append p_sensor_data into list end
		if(p_current->p_next_sensor_data == NULL)
		{
			p_current->p_next_sensor_data = p_sensor_data ;
			break ;
		}

		p_current = p_current->p_next_sensor_data ;
	}

	return 0 ;
}

int get_dev_total_sensorErr(device_node_info_t *p_node)
{
	int count = 0 ;
	uint16_t total_senErr = 0 ;
	sensor_data_t *p_sensor_data = NULL ;
	dev_subDataPak_list_t *p_sub_data = NULL ;
	uint8_t sen_alarm = 0 ;
	uint8_t sen_alert = 0 ;
	
	if(p_node == NULL)
	{
		debug("err para when get_dev_total_sensorErr \n");
		return 0 ;
	}

	debug("sub_data_pak = %d \n",p_node->device_info.sub_data_pak);
	if(p_node->device_info.sub_data_pak == 1)
	{
		p_sub_data = query_subSenData_list(p_node);
		if(p_sub_data != NULL)
		{
			p_sensor_data = p_sub_data->p_sen_data_list ;
		}
		else
		{
			debug("sub_data == NULL when get_dev_total_sensorErr \n");
		}
	}
	else
	{
		p_sensor_data = p_dev_sensorData_head ;
	}

	while(p_sensor_data != NULL)
	{
		#if 0
		//why the ret_val wrong?
		total_senErr += p_sensor_data->sensor_err ;
		debug("senID=%d,senERR=%d,cur_totalERR=%d \n",p_sensor_data->sensor_index,p_sensor_data->sensor_err,total_senErr);
		#else
		if(p_sensor_data->sensor_err > 0 && p_sensor_data->sensor_err <= 63)
		{
			//debug("found senERR=%d when get_dev_total_sensorErr \n",p_sensor_data->sensor_err);
			//total_senErr = p_sensor_data->sensor_err ;
			//break ;
			sen_alert = 1 ;
		}
		else if(p_sensor_data->sensor_err >= 64)
		{
			//debug("found senERR=%d when get_dev_total_sensorErr \n",p_sensor_data->sensor_err);
			sen_alarm = 1 ;
		}
		#endif

		p_sensor_data = p_sensor_data->p_next_sensor_data ;
	}

	//3==alert,4==alarm,7=alert and alarm
	total_senErr = sen_alert*3 + sen_alarm*4 ;

	return total_senErr ;
}

//called when hub share-key changed
int reset_all_devSessionKey(void)
{
	device_node_info_t *p_dev_node = NULL ;
	int count = 0 ;

	//reset online-dev sk
	p_dev_node = device_info_list_head.p_next_device ;
	while(p_dev_node != NULL)
	{
		if(p_dev_node->device_info.dev_support_encrypt == 1)
		{
			for(count=0;count<32;count++)
			{
				//reset session_key
				p_dev_node->device_info.session_key[count] = 0 ;
			}
		}

		p_dev_node = p_dev_node->p_next_device ;
	}

	//reset offline-dev sk
	p_dev_node = offline_dev_info_list_head.p_next_device ;
	while(p_dev_node != NULL)
	{
		if(p_dev_node->device_info.dev_support_encrypt == 1)
		{
			for(count=0;count<32;count++)
			{
				//reset session_key
				p_dev_node->device_info.session_key[count] = 0 ;
			}
		}

		p_dev_node = p_dev_node->p_next_device ;
	}

	return 0 ;
}


int check_shareKey_change(void)
{
	if(get_shareKey_change() != 0)
	{
		debug("shareKey change,reset all dev sessionKey now \n");
		reset_shareKey_changFlag() ;
		reset_all_devSessionKey() ;
	}

	return 0 ;
}



int build_dev_exchangeKey_data(uint8_t *p_buf,uint8_t buf_len,device_node_info_t *p_dev_node)
{
	uint8_t temp_buf[128] ;
	uint8_t share_key[32] = {0} ;
	uint8_t dev_dpsk[64] ;
	uint32_t gen_sk_time = 0 ;
	int count = 0 ;
	uint8_t key_long = 0 ;
	uint8_t rand16[16] ;
	uint8_t session_key[40] ;
	uint8_t temp_exchangeKey_counter[16] ;
	uint8_t encrypt_result[32] ;
	uint8_t data1_buf[64] ;
	uint8_t data2_buf[32] ;
	uint8_t data2_len = 0 ;
	uint16_t total_len = 0 ;
	int key_count = 0 ;

	if(p_buf == NULL || buf_len <= 0 || p_dev_node == NULL)
	{
		debug("err para when build_dev_exchangeKey_data \n");
		return -1 ;
	}

	//reset dev support-encrypt-flag now
	//p_dev_node->device_info.dev_support_encrypt = 0 ;

	memset(temp_buf,0,sizeof(temp_buf)) ;
	memset(share_key,0,sizeof(share_key)) ;
	memset(dev_dpsk,0,sizeof(dev_dpsk));

	time(&gen_sk_time) ; //only get low 4 bytes
	//save gen_sk_time into dev-info
	p_dev_node->device_info.gen_sk_time = gen_sk_time ;

	// generate dev_dpsk,psk+nodeID
	get_share_key(share_key,sizeof(share_key)) ;
	//memset(share_key,0,sizeof(share_key)) ; //only for debug
	debug("sharekey[0][1][2]=0x%x 0x%x 0x%x \n",share_key[0],share_key[1],share_key[2]);
	
	#if 0
	for(key_count=0;key_count<32;key_count++)
	{
		debug("%x ",share_key[key_count]);
	}
	debug("\n");
	#endif
	
	key_long = sizeof(share_key) ;
	memcpy(temp_buf,share_key,key_long) ;
	//memcpy(&temp_buf[key_long],&gen_sk_time,sizeof(gen_sk_time)) ;
	save_in_bigEndian(&temp_buf[key_long],(uint8_t *)&p_dev_node->device_info.device_addr,2,DATA_TYPE_INT); //save in bigEndian
	sha256(temp_buf,key_long+2,dev_dpsk) ;
	//save dpsk into dev-info
	memcpy(p_dev_node->device_info.dev_dpsk,dev_dpsk,32) ;

	//exchangeKey_counter,dpsk[17-28]+timestamp
	memset(temp_exchangeKey_counter,0,sizeof(temp_exchangeKey_counter));
	memcpy(temp_exchangeKey_counter,&dev_dpsk[16],12) ; //17-28 bytes
	//memcpy(&temp_exchangeKey_counter[12],&gen_sk_time,4);
	save_in_bigEndian(&temp_exchangeKey_counter[12],(uint8_t *)&gen_sk_time,4,DATA_TYPE_INT) ;

	//generate session key,rand16+panID,
	memset(rand16,0,sizeof(rand16));
	rand_16bytes(rand16) ;
	memset(temp_buf,0,sizeof(temp_buf));
	memcpy(temp_buf,rand16,16) ;
	//memcpy(&temp_buf[16],&encrypt_panID,2) ;
	save_in_bigEndian(&temp_buf[16],(uint8_t *)&encrypt_panID,2,DATA_TYPE_INT);
	memset(session_key,0,sizeof(session_key)) ;
	sha256(temp_buf,18,session_key) ;
	//save into dev-info
	memcpy(p_dev_node->device_info.session_key,session_key,32) ;
	memset(p_dev_node->device_info.key_rand,0,sizeof(p_dev_node->device_info.key_rand));
	memcpy(p_dev_node->device_info.key_rand,rand16,16); //save k
	
		
	//generate data1,Time Stamp + AES-CTR(k, PAN ID, Node ID) with first 16 bytes of DPSK
	count = 0 ;
	memset(temp_buf,0,sizeof(temp_buf));
	memset(encrypt_result,0,sizeof(encrypt_result));
	memset(data1_buf,0,sizeof(data1_buf)) ;
	memset(data2_buf,0,sizeof(data2_buf)) ;
	
	memcpy(temp_buf,rand16,16);
	count += 16 ;
	//memcpy(&temp_buf[count],&encrypt_panID,sizeof(encrypt_panID)) ;
	save_in_bigEndian(&temp_buf[count],(uint8_t *)&encrypt_panID,sizeof(encrypt_panID),DATA_TYPE_INT) ;
	count += sizeof(encrypt_panID) ;
	//memcpy(&temp_buf[count],&p_dev_node->device_info.device_addr,sizeof(p_dev_node->device_info.device_addr));
	save_in_bigEndian(&temp_buf[count],(uint8_t *)&p_dev_node->device_info.device_addr,sizeof(p_dev_node->device_info.device_addr),DATA_TYPE_INT);
	count += sizeof(p_dev_node->device_info.device_addr) ;
	AES_ctr128_encrypt(temp_buf,encrypt_result,count,dev_dpsk,temp_exchangeKey_counter);
	//memcpy(data1_buf,&gen_sk_time,4) ;
	save_in_bigEndian(data1_buf,(uint8_t *)&gen_sk_time,4,DATA_TYPE_INT) ;
	memcpy(&data1_buf[4],encrypt_result,count) ;
	count += 4 ; //date1 len

	#if 1
	//generate data2,HMAC-SHA256(Data1) with first 16 bytes of DPSKi
	memset(data2_buf,0,sizeof(data2_buf)) ;
	hmac_sha256(data1_buf,count,data2_buf,&data2_len,dev_dpsk,16);
	debug("data2_len = %d \n",data2_len);
	#endif

	total_len = count+data2_len ;
	if( total_len <= buf_len)
	{
		memcpy(p_buf,data1_buf,count) ;
		memcpy(&p_buf[count],data2_buf,data2_len) ;
	}
	else
	{
		debug("data too big len=%d \n",total_len) ;
		return -1 ;
	}
	
	return total_len ;
}


int build_and_send_exchangeKey_pak(uint16_t dev_addr)
{
	device_node_info_t *p_dev_node = NULL ;
	device_node_info_t temp_node ;
	int ret_val = 0 ;
	uint8_t encrypt_cmd_data[200] ;
	uint8_t encrypt_cmdData_len = 0 ;
	int count = 0 ;
	time_t time_val = 0 ;

	p_dev_node = query_device_info_by_deviceAddr(dev_addr) ;
	if(p_dev_node == NULL)
	{
		debug("create empty-dev-node when build_and_send_exchangeKey_pak \n");
		//create empty-dev-node and insert into dev-info-list
		memset(&temp_node,0,sizeof(device_node_info_t)) ;

		temp_node.device_info.dev_info_recv_flag = 0 ;
		//set dev-addr
		temp_node.device_info.device_addr = dev_addr ;
		temp_node.device_info.dev_support_encrypt = 1 ; //set secret-flag
		//set default data_interval 60s
		temp_node.device_info.data_interval = 60 ;

		//set dev-offline timer
		time(&time_val) ;
		temp_node.device_info.dev_offline_timer = time_val ;
		
		ret_val = insert_device_info_to_list(&temp_node) ;
		if(ret_val == -1)
		{	
			debug("add empty-node-dev-info failed when build_and_send_exchangeKey_pak \n");
			return -1 ;
		}
	}

	p_dev_node = query_device_info_by_deviceAddr(dev_addr) ;
	if(p_dev_node != NULL)
	{
		//handle dev secret request
		debug("success to insert new empty_dev_node,dev_addr=0x%x \n",dev_addr);
		memset(encrypt_cmd_data,0,sizeof(encrypt_cmd_data));
		encrypt_cmdData_len = build_dev_exchangeKey_data(encrypt_cmd_data,sizeof(encrypt_cmd_data)-1,p_dev_node) ;
		if(encrypt_cmdData_len <= 0)
		{
			debug("build_dev_exchangeKey_data failed when build_and_send_exchangeKey_pak \n");
			return -1 ;
		}
		//build and send exchange_key cmd
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),WSN_EXCHANGE_KEY_CMDID,encrypt_cmd_data,encrypt_cmdData_len,local_addr,dev_addr);
		if(count > 0)
		{
			send_cmd_toRemote(wsn_send_buf,count); //send cmd imediately
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
		}
				
	}
	else
	{
		debug("insert new empty dev_node failed dev_addr=0x%x \n",dev_addr);
	}
	

	return 0 ;
}


int decrypt_data_pak(uint8_t *p_data,uint8_t *p_decrypt_buf,uint8_t buf_len,device_node_info_t *p_dev_node)
{
	uint8_t decrypt_len = 0 ;
	uint8_t temp_pkt_len = 0 ;
	uint8_t cmd_data_len = 0 ;
	uint8_t data2_len = ENCRYPT_DATA2_LEN ;
	uint8_t data1_len = 0 ;
	uint8_t data2_offset = 0 ;
	uint8_t cmdData_offset = 0 ;
	uint8_t cmdID_offset = 0 ;
	uint8_t pkt_time[4] ;
	uint8_t decrypt_counter[16] ;
	uint8_t temp_data2[32] ;
	int count = 0 ;
	int key_count = 0 ;
	uint8_t pkt_data2[ENCRYPT_DATA2_LEN] ;
	uint8_t calc_data2[ENCRYPT_DATA2_LEN] ;
	uint8_t data2_cnt = 0 ;
	
	
	
	if(p_data == NULL || p_decrypt_buf == NULL || p_dev_node == NULL)
	{
		debug("err para when decrypt_data_pak \n");
		return -1 ;
	}

	for(data2_cnt=0;data2_cnt<ENCRYPT_DATA2_LEN;data2_cnt++)
	{
		//clear data2 buf
		pkt_data2[data2_cnt] = 0 ;
		calc_data2[data2_cnt] = 0 ;
	}

	temp_pkt_len = p_data[2] ; //skip sop+ver
	if(temp_pkt_len == 0 || temp_pkt_len >= 255)
	{
		debug("err temp_pkt_len \n");
		return -1 ;
	}
	
	cmd_data_len = temp_pkt_len - (2+2+1+1+1) ; //skip dest+src+cmdID+chk+eop
	if(cmd_data_len <= 0)
	{
		debug("err cmd_data_len \n");
		return -1 ;
	}
	
	data1_len = cmd_data_len - data2_len ;
	if(data1_len <= 0 || data1_len >= 255)
	{
		debug("err data1_len \n");
		return -1 ;
	}
	
	decrypt_len = data1_len - 4 ; //-4 for skip timestamp
	debug("decrypt data-pak,pkt_len=%d,decrypt=%d \n",temp_pkt_len,decrypt_len);
	if(decrypt_len > buf_len || decrypt_len <= 0)
	{
		debug("maybe decrypt_len=%d wrong or buf_len=%d wrong when decrypt_data_pak \n",decrypt_len,buf_len);
		return -1 ;
	}

	cmdData_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	cmdID_offset =   WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN ;
	data2_offset =   WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN + data1_len ;
	if( data2_offset <= 0 || data2_offset >= 255)
	{
		debug("err data2_offset \n");
		return -1 ;
	}

	//calc data2
	memset(temp_data2,0,sizeof(temp_data2));
	hmac_sha256(&p_data[0],data2_offset,temp_data2,&data2_len,p_dev_node->device_info.session_key,16); //from sop to data1
	debug("calc data2: total_len=%d \n",data2_offset);
	//hmac_sha256(&p_data[cmdData_offset],data1_len,temp_data2,&data2_len,p_dev_node->device_info.session_key,16); //only data1
	debug("calc and print data2,len=8 bytes: \n");
	data2_cnt = 0 ;
	for(count=0;count<32;count++)
	{
		if((count%4) == 0)
		{
			calc_data2[data2_cnt++] = temp_data2[count] ;
			printf("%02x",temp_data2[count]);
		}
	}
	printf("\n");

	debug("print pkt data2: \n");
	for(count=0;count<ENCRYPT_DATA2_LEN;count++)
	{
		pkt_data2[count] = p_data[data2_offset+count] ;
		printf("%02x",p_data[data2_offset+count]);
	}
	printf("\n");

	//check data2 err or not?
	for(data2_cnt=0;data2_cnt<ENCRYPT_DATA2_LEN;data2_cnt++)
	{
		if(calc_data2[data2_cnt] != pkt_data2[data2_cnt])
		{
			//data2 not match,sessionKey err?
			debug("data2 check err when decrypt_data_pak,maybe we need to rebuild sessionKey!!! \n");
			return -1 ;
		}
	}

	memset(pkt_time,0,sizeof(pkt_time));
	memcpy(pkt_time,&p_data[cmdData_offset],4) ; //pkt send time,4 bytes,bigEndian
	//memset(wsn_decrypt_buf,0,sizeof(wsn_decrypt_buf));
	//memset(wsn_decrypt_data_buf,0,sizeof(wsn_decrypt_data_buf)) ;
	memset(decrypt_counter,0,sizeof(decrypt_counter)) ;
	memcpy(decrypt_counter,&p_dev_node->device_info.session_key[16],12) ;
	memcpy(&decrypt_counter[12],pkt_time,4) ;
	AES_ctr128_decrypt(&p_data[cmdData_offset+4],p_decrypt_buf,decrypt_len,p_dev_node->device_info.session_key,decrypt_counter); //+4 for skip timestamp

	#if 0
	debug("recv data,decryptkey:");
	for(key_count=0;key_count<32;key_count++)
	{
		printf("%02x",p_dev_node->device_info.session_key[key_count]);
	}
	printf("\n");
	

	debug("text after decrypt: \n");
	for(key_count=0;key_count<decrypt_len;key_count++)
	{	
		printf("%02x",p_decrypt_buf[key_count]);
	}
	printf("\n");
	#endif
	

	#if 0
	//calc data2
	memset(temp_data2,0,sizeof(temp_data2));
	//hmac_sha256(&p_data[0],data2_offset,temp_data2,&data2_len,p_dev_node->device_info.session_key,16); //from sop to data1
	hmac_sha256(&p_data[cmdData_offset],data1_len,temp_data2,&data2_len,p_dev_node->device_info.session_key,16); //only data1
	debug("calc data2: \n");
	for(count=0;count<32;count++)
	{
		printf("%02x",temp_data2[count]);
	}
	printf("\n");

	debug("pkt data2: \n");
	for(count=0;count<32;count++)
	{
		printf("%02x",p_data[data2_offset+count]);
	}
	printf("\n");
	#endif
	

	return 0 ;
	
}

int renew_dataPak_sk(device_node_info_t *p_dev_node)
{
	uint8_t temp_buf[64] ;
	uint8_t count = 0 ;
	//generate sk for encrypt data-pak
	if(p_dev_node == NULL)
	{
		debug("err para when renew_dataPak_sk \n");
		return -1 ;
	}

	memset(p_dev_node->device_info.dataPak_sk,0,sizeof(p_dev_node->device_info.dataPak_sk)) ;
	memset(temp_buf,0,sizeof(temp_buf)) ;
	memcpy(temp_buf,p_dev_node->device_info.key_rand,16); //k,rand16
	count += 16 ;
	save_in_bigEndian(&temp_buf[count],(uint8_t *)&encrypt_panID,2,DATA_TYPE_INT) ; //panID
	count += 2 ;
	save_in_bigEndian(&temp_buf[count],(uint8_t *)&p_dev_node->device_info.device_addr,2,DATA_TYPE_INT) ; //netID
	count += 2 ;
	sha256(temp_buf,count,p_dev_node->device_info.dataPak_sk) ;

	return 0 ;
}

int wsn_ping_response(uint16_t dev_addr)
{
	uint8_t cmd_data[32] ;
	uint8_t count = 0 ;
	uint8_t cmdData_len = 0 ;
	//uint16_t local_netID = 0x0001 ; // == local addr
	//uint16_t local_netID = local_addr ; // == local addr

	memset(cmd_data,0,sizeof(cmd_data));
	count = 0 ;
	cmdData_len = 0 ;

	//fill src addr
	cmd_data[cmdData_len++] = (local_addr >> 8) & 0x00FF ; //high byte
	cmd_data[cmdData_len++] = local_addr & 0x00FF ; //low byte

	//fill dest addr
	cmd_data[cmdData_len++] = (dev_addr >> 8) & 0x00FF ; //high byte
	cmd_data[cmdData_len++] = dev_addr & 0x00FF ; //low byte

	//send ping resp now
	count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),WSN_PING_DEV_MSGID,cmd_data,cmdData_len,local_addr,dev_addr);
	if(count > 0)
	{
		send_cmd_toRemote(wsn_send_buf,count);
		//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
	}

	return 0 ;
	
}


int handle_wsn_devEvt(uint8_t *p_data,uint8_t data_len)
{
	uint16_t pkt_dest_addr = 0 ; //for us
	uint16_t pkt_source_addr = 0 ;
	device_node_info_t *p_temp_node = NULL ;
	uint8_t evt_level = 0;
	uint8_t evtID = 0 ;
	uint8_t evt_reason = 0; 
	uint8_t evt_action = 0 ;
	uint8_t cmd_data_offset = 0 ;

	if(p_data == NULL || data_len <= 0)
	{
		debug("err para when handle_wsn_devEvt \n");
		return -1 ;
	}

	pkt_source_addr = get_sourceAddr_fromPkt(p_data,data_len) ;
	pkt_dest_addr = get_destAddr_fromPkt(p_data,data_len) ;
	
	//begin to handle event now
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	evt_level = p_data[cmd_data_offset] ;
	evtID = p_data[cmd_data_offset+1] ;
	evt_reason = p_data[cmd_data_offset+2] ;
	evt_action = p_data[cmd_data_offset+3] ;

	debug("handle dev evtent,level=%d,evtID=%d,reason=%d,evt_action=%d \n",evt_level,evtID,evt_reason,evt_action);
	if(evtID >= 1 && evtID <= 3) // only handle 1-3 now
	{
		//set dev offline now
		p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
		if(p_temp_node != NULL)
		{
			debug("recved wsn evt ,set dev=%s offline now \n",p_temp_node->device_info.dev_serialNum);
			set_dev_offline_status(p_temp_node) ;
		}
	}

	return 0 ;
}



void init_devAlarm_list(void)
{
	memset(&relay1_list_head,0,sizeof(relay1_list_head));
	memset(&relay2_list_head,0,sizeof(relay2_list_head));
	memset(&relay3_list_head,0,sizeof(relay3_list_head));
	memset(&relay4_list_head,0,sizeof(relay4_list_head));
	memset(&relay5_list_head,0,sizeof(relay5_list_head));

	memset(&high_list_head,0,sizeof(high_list_head));
	memset(&low_list_head,0,sizeof(low_list_head));
	memset(&fault_list_head,0,sizeof(fault_list_head));

	return ;
}


alarm_node_t *query_alarmNode_inList(alarm_node_t *p_list_head,uint8_t *p_dev_sn)
{
	alarm_node_t *p_temp_node = NULL ;
	
	if(p_list_head == NULL || p_dev_sn == NULL)
	{
		debug("err para when query_alarmNode_inList \n");
		return NULL ;
	}

	p_temp_node = (alarm_node_t *)p_list_head->p_next_alarm_node ;
	while(p_temp_node != NULL)
	{
		if(strcmp(p_temp_node->alarm_dev_sn,p_dev_sn) == 0)
		{
			//match
			return p_temp_node ;
		}

		p_temp_node = (alarm_node_t *)p_temp_node->p_next_alarm_node ;
	}

	return NULL ;
}


int append_alarmNode_intoList(alarm_node_t *p_list_head,uint8_t *p_dev_sn)
{
	alarm_node_t *p_alarm_node = NULL ;
	alarm_node_t *p_temp_node = NULL ;

	if(p_list_head == NULL || p_dev_sn == NULL)
	{
		debug("err para when append_alarmNode_intoList \n");
		return -1 ;
	}

	p_alarm_node = query_alarmNode_inList(p_list_head,p_dev_sn) ;
	if(p_alarm_node != NULL)
	{
		debug("alarm_node has been in the list,dev=%s \n",p_dev_sn);
		return 0 ;
	}

	//create new node and append into the list
	p_alarm_node = (alarm_node_t *)malloc(sizeof(alarm_node_t)) ;
	if(p_alarm_node == NULL)
	{
		debug("malloc failed when append_alarmNode_intoList,dev=%s \n",p_dev_sn);
		return -1 ;
	}

	memset(p_alarm_node,0,sizeof(alarm_node_t));
	if( strlen(p_dev_sn) >= sizeof(p_alarm_node->alarm_dev_sn) )
	{
		strncpy(p_alarm_node->alarm_dev_sn,p_dev_sn,(sizeof(p_alarm_node->alarm_dev_sn)-1) ) ;
	}
	else
	{
		strcpy(p_alarm_node->alarm_dev_sn,p_dev_sn);
	}

	//append into list end
	p_temp_node = p_list_head ;
	while(p_temp_node != NULL)
	{
		if(p_temp_node->p_next_alarm_node == NULL)
		{
			p_temp_node->p_next_alarm_node = p_alarm_node ;
			break ;
		}

		p_temp_node = (alarm_node_t *)p_temp_node->p_next_alarm_node ;
	}

	//inc the list count
	p_list_head->alarm_count++ ;

	debug("this alarm list count=%d \n",p_list_head->alarm_count) ;

	return 0;
}

int del_alarmNode_fromList(alarm_node_t *p_list_head,uint8_t *p_dev_sn)
{
	alarm_node_t *p_prev_node = NULL ;
	alarm_node_t *p_cur_node = NULL ;

	if(p_list_head == NULL || p_dev_sn == NULL)
	{
		debug("err para when del_alarmNode_fromList \n");
		return -1 ;
	}

	p_cur_node = query_alarmNode_inList(p_list_head,p_dev_sn) ;
	if(p_cur_node == NULL)
	{
		//debug("dev=%s  list when del_alarmNode_fromList,return now \n",p_dev_sn);
		return 0 ;
	}

	//debug("begin to del_alarmNode_fromList \n");
	p_cur_node = (alarm_node_t *)p_list_head->p_next_alarm_node ;
	p_prev_node = p_list_head ;
	while(p_cur_node != NULL)
	{
		if(strcmp(p_cur_node->alarm_dev_sn,p_dev_sn) == 0)
		{
			//debug("del_alarmNode match 1 \n") ;
			//match
			p_prev_node->p_next_alarm_node = p_cur_node->p_next_alarm_node ;

			free(p_cur_node) ;
			if(p_list_head->alarm_count > 0)
			{
				p_list_head->alarm_count-- ;
			}

			//debug("del_alarmNode match 2 \n") ;
			
			return 0 ;
		}

		p_prev_node = p_cur_node ;
		p_cur_node = (alarm_node_t *)p_cur_node->p_next_alarm_node ;
		//debug("del not match,continue \n");
	}

	return 0 ;
}


int free_specified_alarmList(alarm_node_t *p_list_head)
{
	alarm_node_t *p_next = NULL ;
	alarm_node_t *p_temp = NULL ;
	
	if(p_list_head == NULL)
	{
		return 0 ;
	}

	p_temp = p_list_head->p_next_alarm_node ;
	while(p_temp != NULL)
	{
		p_next = p_temp->p_next_alarm_node ;
		free(p_temp) ;

		p_temp = p_next ;
	}

	p_list_head->p_next_alarm_node = NULL ;

	return 0 ;
}


int free_allAlarmList_inDev(device_node_info_t *p_dev_node)
{
	if(p_dev_node == NULL)
	{
		return 0 ;
	}

	free_specified_alarmList(&p_dev_node->device_info.groupHigh_list_head);
	free_specified_alarmList(&p_dev_node->device_info.groupLow_list_head);
	free_specified_alarmList(&p_dev_node->device_info.groupFault_list_head);

	free_specified_alarmList(&p_dev_node->device_info.groupRelay1_list_head);
	free_specified_alarmList(&p_dev_node->device_info.groupRelay2_list_head);
	free_specified_alarmList(&p_dev_node->device_info.groupRelay3_list_head);
	free_specified_alarmList(&p_dev_node->device_info.groupRelay4_list_head);
	free_specified_alarmList(&p_dev_node->device_info.groupRelay5_list_head);

	//reset groupAlarm and groupRelay
	p_dev_node->device_info.total_groupAlarm = 0 ;
	p_dev_node->device_info.total_groupRelay = 0 ;
	
	return 0 ;
}


//renew local alarm list
int check_and_renew_alarmList(uint8_t cur_dev_alarmInfo,device_node_info_t *p_cur_dev_node)
{
	if(p_cur_dev_node == NULL )
	{
		debug("err para when check_and_renew_alarmList \n");
		return -1 ;
	}


	//high alarm list
	if((cur_dev_alarmInfo & 0x01) != 0) //bit0
	{
		//this dev has high alarm,append into high alarm list now
		//append_alarmNode_intoList(&device_info_list_head.high_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		append_alarmNode_intoList(&high_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
	}
	else
	{
		//this dev has not high alarm,del from high alarm list if necessary
		//del_alarmNode_fromList(&device_info_list_head.high_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		del_alarmNode_fromList(&high_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		if(high_list_head.alarm_count == 0)
		{
			//no dev in high alarm ,clear high alarm
			device_info_list_head.current_all_alarm &= 0xFE ; //reset bit0
		}
	}

	//low alarm list
	if( (cur_dev_alarmInfo & 0x02) != 0) //bit1
	{
		//this dev has low alarm,append into low alarm list now
		//append_alarmNode_intoList(&device_info_list_head.low_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		append_alarmNode_intoList(&low_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
	}
	else
	{
		//this dev has not low alarm,del from low alarm list if necessary
		//del_alarmNode_fromList(&device_info_list_head.low_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		del_alarmNode_fromList(&low_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		if(low_list_head.alarm_count == 0)
		{
			//no dev in low alarm,clear low alarm
			device_info_list_head.current_all_alarm &= 0xFD ; //reset bit1
		}
	}

	//fault alarm list
	if( (cur_dev_alarmInfo & 0x04) != 0) //bit2
	{
		//this dev has fault alarm,append into fault alarm list now
		append_alarmNode_intoList(&fault_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
	}
	else
	{
		//this dev has not fault alarm,del from fault alarm list if necessary
		del_alarmNode_fromList(&fault_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		if(fault_list_head.alarm_count == 0)
		{
			//no dev in fault alarm,clear fault alarm
			device_info_list_head.current_all_alarm &= 0xFB ; //reset bit2
		}
	}

	return 0 ;
}

//renew group alarm list
int check_and_renew_groupAlarmList(device_node_info_t *p_cur_dev_node,uint8_t groupAlarm,uint8_t *p_alarmDev_sn)
{
	if(p_cur_dev_node == NULL || p_alarmDev_sn == NULL || p_alarmDev_sn[0] == 0)
	{
		debug("err para when check_and_renew_groupAlarmList \n");
		return -1 ;
	}

	debug("begin to check_and_renew_groupAlarmList \n");
	//high alarm list
	if((groupAlarm & 0x01) != 0) //bit0
	{
		//this dev has high alarm,append into high alarm list now
		//append_alarmNode_intoList(&device_info_list_head.high_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		append_alarmNode_intoList(&p_cur_dev_node->device_info.groupHigh_list_head,p_alarmDev_sn) ;
	}
	else
	{
		//this dev has not high alarm,del from high alarm list if necessary
		//del_alarmNode_fromList(&device_info_list_head.high_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		del_alarmNode_fromList(&p_cur_dev_node->device_info.groupHigh_list_head,p_alarmDev_sn) ;
		if(p_cur_dev_node->device_info.groupHigh_list_head.alarm_count == 0)
		{
			//no dev in high alarm ,clear high alarm
			p_cur_dev_node->device_info.total_groupAlarm &= 0xFE ; //reset bit0
		}
	}

	//low alarm list
	if( (groupAlarm & 0x02) != 0) //bit1
	{
		//this dev has low alarm,append into low alarm list now
		//append_alarmNode_intoList(&device_info_list_head.low_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		append_alarmNode_intoList(&p_cur_dev_node->device_info.groupLow_list_head,p_alarmDev_sn) ;
	}
	else
	{
		//this dev has not low alarm,del from low alarm list if necessary
		//del_alarmNode_fromList(&device_info_list_head.low_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		del_alarmNode_fromList(&p_cur_dev_node->device_info.groupLow_list_head,p_alarmDev_sn) ;
		if(p_cur_dev_node->device_info.groupLow_list_head.alarm_count == 0)
		{
			//no dev in low alarm,clear low alarm
			p_cur_dev_node->device_info.total_groupAlarm &= 0xFD ; //reset bit1
		}
	}

	//fault alarm list
	if( (groupAlarm & 0x04) != 0) //bit2
	{
		//this dev has fault alarm,append into fault alarm list now
		append_alarmNode_intoList(&p_cur_dev_node->device_info.groupFault_list_head,p_alarmDev_sn) ;
	}
	else
	{
		//this dev has not fault alarm,del from fault alarm list if necessary
		del_alarmNode_fromList(&p_cur_dev_node->device_info.groupFault_list_head,p_alarmDev_sn) ;
		if(p_cur_dev_node->device_info.groupFault_list_head.alarm_count == 0)
		{
			//no dev in fault alarm,clear fault alarm
			p_cur_dev_node->device_info.total_groupAlarm &= 0xFB ; //reset bit2
		}
	}

	return 0 ;
}


//renew local relay list
int check_and_renew_relayList(uint8_t cur_dev_relay,device_node_info_t *p_cur_dev_node)
{
	if(p_cur_dev_node == NULL)
	{
		debug("err para when check_and_renew_relayList \n");
		return -1 ;
	}

	//debug("begin to relay1 \n");
	//bit0,relay1
	if((cur_dev_relay & 0x01) != 0)
	{
		//append_alarmNode_intoList(&device_info_list_head.relay1_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		append_alarmNode_intoList(&relay1_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
	}
	else
	{
		del_alarmNode_fromList(&relay1_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		if(relay1_list_head.alarm_count == 0)
		{
			//no dev in relay1 status,reset relay1 value
			device_info_list_head.relay_info &= 0xFE ; //reset bit0
		}
	}

	//debug("begin to relay2 \n");
	//bit1,relay2
	if((cur_dev_relay & 0x02) != 0)
	{
		append_alarmNode_intoList(&relay2_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
	}
	else
	{
		del_alarmNode_fromList(&relay2_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		if(relay2_list_head.alarm_count == 0)
		{
			//no dev in relay2 status,reset relay2 value
			device_info_list_head.relay_info &= 0xFD ; //reset bit1
		}
	}

	//debug("begin to relay3 \n");
	//bit2,relay3
	if((cur_dev_relay & 0x04) != 0)
	{
		append_alarmNode_intoList(&relay3_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
	}
	else
	{
		del_alarmNode_fromList(&relay3_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		if(relay3_list_head.alarm_count == 0)
		{
			//no dev in relay3 status,reset relay3 value
			device_info_list_head.relay_info &= 0xFB ; //reset bit2
		}
	}

	//debug("begin to relay4 \n");
	//bit3,relay4
	if((cur_dev_relay & 0x08) != 0)
	{
		append_alarmNode_intoList(&relay4_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
	}
	else
	{
		del_alarmNode_fromList(&relay4_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		if(relay4_list_head.alarm_count == 0)
		{
			//no dev in relay4 status,reset relay4 value
			device_info_list_head.relay_info &= 0xF7 ; //reset bit3
		}
	}

	//debug("begin to relay5 \n");
	//bit4,relay5
	if((cur_dev_relay & 0x10) != 0)
	{
		append_alarmNode_intoList(&relay5_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
	}
	else
	{
		del_alarmNode_fromList(&relay5_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		if(relay5_list_head.alarm_count == 0)
		{
			//no dev in relay5 status,reset relay5 value
			device_info_list_head.relay_info &= 0xEF ; //reset bit4
		}
	}

	return 0 ;
}

//renew group relay list
int check_and_renew_groupRelayList(device_node_info_t *p_cur_dev_node,uint8_t groupRelay,uint8_t *p_alarmDev_sn)
{
	if(p_cur_dev_node == NULL || p_alarmDev_sn == NULL || p_alarmDev_sn[0] == 0)
	{
		debug("err para when check_and_renew_groupRelayList \n");
		return -1 ;
	}

	debug("begin to check_and_renew_groupRelayList \n");
	//bit0,relay1
	if((groupRelay & 0x01) != 0)
	{
		//append_alarmNode_intoList(&device_info_list_head.relay1_list_head,p_cur_dev_node->device_info.dev_serialNum) ;
		append_alarmNode_intoList(&p_cur_dev_node->device_info.groupRelay1_list_head,p_alarmDev_sn) ;
	}
	else
	{
		del_alarmNode_fromList(&p_cur_dev_node->device_info.groupRelay1_list_head,p_alarmDev_sn) ;
		if(p_cur_dev_node->device_info.groupRelay1_list_head.alarm_count == 0)
		{
			//no dev in relay1 status,reset relay1 value
			p_cur_dev_node->device_info.total_groupRelay &= 0xFE ; //reset bit0
		}
	}

	//debug("begin to relay2 \n");
	//bit1,relay2
	if((groupRelay & 0x02) != 0)
	{
		append_alarmNode_intoList(&p_cur_dev_node->device_info.groupRelay2_list_head,p_alarmDev_sn) ;
	}
	else
	{
		del_alarmNode_fromList(&p_cur_dev_node->device_info.groupRelay2_list_head,p_alarmDev_sn) ;
		if(p_cur_dev_node->device_info.groupRelay2_list_head.alarm_count == 0)
		{
			//no dev in relay2 status,reset relay2 value
			p_cur_dev_node->device_info.total_groupRelay &= 0xFD ; //reset bit1
		}
	}

	//debug("begin to relay3 \n");
	//bit2,relay3
	if((groupRelay & 0x04) != 0)
	{
		append_alarmNode_intoList(&p_cur_dev_node->device_info.groupRelay3_list_head,p_alarmDev_sn) ;
	}
	else
	{
		del_alarmNode_fromList(&p_cur_dev_node->device_info.groupRelay3_list_head,p_alarmDev_sn) ;
		if(p_cur_dev_node->device_info.groupRelay3_list_head.alarm_count == 0)
		{
			//no dev in relay3 status,reset relay3 value
			p_cur_dev_node->device_info.total_groupRelay &= 0xFB ; //reset bit2
		}
	}

	//debug("begin to relay4 \n");
	//bit3,relay4
	if((groupRelay & 0x08) != 0)
	{
		append_alarmNode_intoList(&p_cur_dev_node->device_info.groupRelay4_list_head,p_alarmDev_sn) ;
	}
	else
	{
		del_alarmNode_fromList(&p_cur_dev_node->device_info.groupRelay4_list_head,p_alarmDev_sn) ;
		if(p_cur_dev_node->device_info.groupRelay4_list_head.alarm_count == 0)
		{
			//no dev in relay4 status,reset relay4 value
			p_cur_dev_node->device_info.total_groupRelay &= 0xF7 ; //reset bit3
		}
	}

	//debug("begin to relay5 \n");
	//bit4,relay5
	if((groupRelay & 0x10) != 0)
	{
		append_alarmNode_intoList(&p_cur_dev_node->device_info.groupRelay5_list_head,p_alarmDev_sn) ;
	}
	else
	{
		del_alarmNode_fromList(&p_cur_dev_node->device_info.groupRelay5_list_head,p_alarmDev_sn) ;
		if(p_cur_dev_node->device_info.groupRelay5_list_head.alarm_count == 0)
		{
			//no dev in relay5 status,reset relay5 value
			p_cur_dev_node->device_info.total_groupRelay &= 0xEF ; //reset bit4
		}
	}

	return 0 ;
}


//triger the dev alarm or relay when recved the data_pak
int triger_specifiedDev_alarm(device_node_info_t *p_cur_node)
{
	//device_node_info_t *p_temp_node = NULL ;
	uint8_t cmd_data[10] ;
	uint8_t send_cnt = 0 ;
	uint16_t productID = 0 ;
	uint8_t temp_alarm = 0 ;
	
	if(p_cur_node == NULL )
	{
		debug("err para when triger_specifiedDev_alarm \n");
		return -1 ;
	}

	productID = (p_cur_node->device_info.instr_family_id << 8) | p_cur_node->device_info.instr_model_id ;
	if(productID == RELAY_INSTRUMENT_ID)
	{
		debug("this dev is relay-dev when triger_specifiedDev_alarm \n");
		triger_relay_action(p_cur_node) ;
		return 0 ;
	}

	#if 0
	//need to clear remote_alarm status,so keep it
	if( (device_info_list_head.current_all_alarm & 0x07) == 0)
	{
		//no alarm now
		debug("no alarm, alarm_info=0x%x \n",device_info_list_head.current_all_alarm);
		return 0 ;
	}
	#endif

	//nvram_get_trigAlarm_conf(&wsn_net_conf_data.trig_alarm) ;
	//nvram_get_trigRelay_conf(&wsn_net_conf_data.trig_relay) ;

	if(wsn_net_conf_data.trig_alarm != 1)
	{
		//triger alarm not open
		debug("local alarm not open \n");
		return 0 ;
	}

	if( (p_cur_node->device_info.dev_remoteAlarm_info & 0x80) == 0)
	{
		//bit7,this dev not support remote alarm
		debug("this dev=%s not support remote alarm \n",p_cur_node->device_info.dev_serialNum);
		return 0 ;
	}

	#if 0
	if( (p_cur_node->device_info.dev_alarm_info & 0x07) == (device_info_list_head.current_all_alarm & 0x07) )
	{
		debug("dev_alarm has been trigered when triger_specifiedDev_alarm,return now \n");
		return 0 ;
	}
	#endif
	

	// alarm info,del dev-local alarm if necessary
	temp_alarm = device_info_list_head.current_all_alarm & 0x07 ;
	//check fault-alarm,bit2
	if( (p_cur_node->device_info.dev_local_alarm_info & 0x04) != 0)
	{
		if(fault_list_head.alarm_count <= 1)
		{
			//this fault-alarm generated by this node itself
			temp_alarm &= 0xFB ; //clear fault-alarm,reset bit-2
		}
	}
	//check low-alarm,bit1
	if( (p_cur_node->device_info.dev_local_alarm_info & 0x02) != 0 )
	{
		if(low_list_head.alarm_count <= 1)
		{
			temp_alarm &= 0xFD ; //clear low-alarm,reset bit1
		}
	}
	//check high-alarm,bit0
	if( (p_cur_node->device_info.dev_local_alarm_info & 0x01) != 0)
	{
		if(high_list_head.alarm_count <= 1)
		{
			temp_alarm &= 0xFE ; // clear high-alarm,reset bit0
		}
	}

	if( (p_cur_node->device_info.dev_remoteAlarm_info & 0x07) == (temp_alarm & 0x07) )
	{
		debug("dev_alarm has been trigered when triger_specifiedDev_alarm,return now \n");
		return 0 ;
	}

	debug("begin to send remote_alarm cmd:src=0x%x,dest=0x%x ,alarmCmd=0x%x \n",local_addr,p_cur_node->device_info.device_addr,temp_alarm);

	debug_notice("begin to send remote_alarm cmd to devSn=%s,devAddr=0x%x,alarmCmd=0x%x \n",p_cur_node->device_info.dev_serialNum,
																					p_cur_node->device_info.device_addr,
																					temp_alarm);
	cmd_data[0] = 0x03 ; //remote_alarm sub cmd
	cmd_data[1] = temp_alarm ;
	//or record high,low,fault num?
	send_cnt = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GENERAL_REQUEST_V2_CMDID,cmd_data,2,local_addr,p_cur_node->device_info.device_addr);
	if(send_cnt > 0)
	{
		//send_cmd_toRemote(wsn_send_buf,send_cnt); //this cmd no resp
		//send_expectedResp_cmd_toRemote(wsn_send_buf,send_cnt) ;
		send_cmd_toDevWaitingList(wsn_send_buf,send_cnt,GENERAL_REQUEST_V2_CMDID,p_cur_node) ;
	}

	return 0 ;
}


int triger_specifiedDev_cloudAlarm(device_node_info_t *p_cur_node)
{
	//device_node_info_t *p_temp_node = NULL ;
	uint8_t cmd_data[10] ;
	uint8_t send_cnt = 0 ;
	uint16_t productID = 0 ;
	
	if(p_cur_node == NULL )
	{
		debug("err para when triger_specifiedDev_cloudAlarm \n");
		return -1 ;
	}

	productID = (p_cur_node->device_info.instr_family_id << 8) | p_cur_node->device_info.instr_model_id ;
	if(productID == RELAY_INSTRUMENT_ID)
	{
		debug("this dev is relay-dev when triger_specifiedDev_cloudAlarm \n");
		triger_cloudRelay_action(p_cur_node) ;
		return 0 ;
	}


	if( (p_cur_node->device_info.dev_remoteAlarm_info & 0x80) == 0)
	{
		//bit7,this dev not support remote alarm
		debug("dev=%s,dev->remoteAlarm=0x%x not support remote alarm when triger_specifiedDev_cloudAlarm\n",p_cur_node->device_info.dev_serialNum,
																										p_cur_node->device_info.dev_remoteAlarm_info);
		return 0 ;
	}

	//dev_remoteAlarm_info only include remoteAlarm info,dev_alarm_info include remoteAlarm info and dev_local alarm info
	if( (p_cur_node->device_info.dev_remoteAlarm_info & 0x07) == (p_cur_node->device_info.total_groupAlarm & 0x07) )
	{
		debug("dev=%s dev_alarm has been trigered when triger_specifiedDev_cloudAlarm,return now \n",p_cur_node->device_info.dev_serialNum);
		return 0 ;
	}
	

	debug("begin to send cloud_groupAlarm cmd:src=0x%x,dest=0x%x \n",local_addr,p_cur_node->device_info.device_addr);
	debug_notice("begin to send groupAlarm cmd to devSn=%s,devAddr=0x%x,alarmCmd=0x%x \n",p_cur_node->device_info.dev_serialNum,
																					p_cur_node->device_info.device_addr,
																					p_cur_node->device_info.total_groupAlarm);

	cmd_data[0] = 0x03 ; //remote_alarm sub cmd
	cmd_data[1] = p_cur_node->device_info.total_groupAlarm & 0x07 ; // alarm info
	send_cnt = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GENERAL_REQUEST_V2_CMDID,cmd_data,2,local_addr,p_cur_node->device_info.device_addr);
	if(send_cnt > 0)
	{
		//send_cmd_toRemote(wsn_send_buf,send_cnt); //this cmd no resp
		//send_expectedResp_cmd_toRemote(wsn_send_buf,send_cnt) ;
		send_cmd_toDevWaitingList(wsn_send_buf,send_cnt,GENERAL_REQUEST_V2_CMDID,p_cur_node) ;
	}

	return 0 ;
}


#if 0
void check_and_clear_remoteAlarmRelay(void)
{
	time_t cur_time = 0 ;

	time(&cur_time);
	if( (device_info_list_head.current_all_alarm & 0x07) != 0)
	{
		if(cur_time >= device_info_list_head.cur_alarm_timer)
		{
			if( (cur_time - device_info_list_head.cur_alarm_timer) >= REMOTE_ALARM_PERIOD)
			{
				//reset all alarm info and timer
				device_info_list_head.current_all_alarm = 0 ;
				device_info_list_head.relay_info = 0 ;
				device_info_list_head.cur_alarm_timer = 0 ;
			}
		}
		else
		{
			//maybe overflow,reset alarm timer now
			device_info_list_head.cur_alarm_timer = cur_time ;
		}
	}

	return ;
}
#endif


int triger_relay_action(device_node_info_t *p_relay_node)
{
	uint8_t bank0_support = 0 ;
	uint8_t bank1_support = 0  ;

	if(p_relay_node == NULL )
	{
		debug("err para when triger_relay_action \n");
		return -1 ;
	}

	#if 0
	//need to clear remote_alarm status,so keep it
	if(device_info_list_head.relay_info == 0)
	{
		debug("no relay_info to triger when triger_relay_action,return now \n");
		return  0;
	}
	#endif

	//nvram_get_trigAlarm_conf(&wsn_net_conf_data.trig_alarm) ;
	//nvram_get_trigRelay_conf(&wsn_net_conf_data.trig_relay) ;

	if(wsn_net_conf_data.trig_relay != 1)
	{
		//triger relay not open 
		debug("local relay not open \n");
		return 0 ;
	}

	if( (p_relay_node->device_info.hw_info & 0x20) == 0) //bit5,not have DIO  ?
	{
		debug("dev=%s not support remoteRelay,dev-hw_info=0x%x \n",p_relay_node->device_info.dev_serialNum,p_relay_node->device_info.hw_info);
		return 0 ;
	}

	debug("this relayDev remotRelay0=0x%x,remotRelay1=0x%x \n",p_relay_node->device_info.relay_bank0,p_relay_node->device_info.relay_bank1);

	bank0_support = (p_relay_node->device_info.hw_info & 0x10) != 0 && p_relay_node->device_info.dioBank0_map != 0 ; //bank0 is DO and map not 0
	bank1_support = (p_relay_node->device_info.hw_info & 0x08) != 0 && p_relay_node->device_info.dioBank1_map != 0 ; //bank1 is DO and map not 0
	if( (bank0_support && p_relay_node->device_info.relay_bank0 != device_info_list_head.relay_info) || 
				(bank1_support && p_relay_node->device_info.relay_bank1 != device_info_list_head.relay_info) )
	{
		debug_notice("begin to send relay cmd to devSn=%s,devaddr=0x%x,relay_cmd=0x%x \n",p_relay_node->device_info.dev_serialNum,
																						p_relay_node->device_info.device_addr,
																						device_info_list_head.relay_info);
		build_and_send_remoteRelayCmd(p_relay_node,device_info_list_head.relay_info);
	}
	else
	{
		debug("curent localRelay=0x%x,this relayDev dioMap0=0x%x,dioMap1=0x%x \n",device_info_list_head.relay_info,
							p_relay_node->device_info.dioBank0_map,p_relay_node->device_info.dioBank1_map);
	}
	
	return 0 ;
}

int triger_cloudRelay_action(device_node_info_t *p_relay_node)
{
	uint8_t bank0_support = 0 ;
	uint8_t bank1_support = 0 ;
	
	if(p_relay_node == NULL )
	{
		debug("err para when triger_relay_action \n");
		return -1 ;
	}

	bank0_support = (p_relay_node->device_info.hw_info & 0x10) != 0 && p_relay_node->device_info.dioBank0_map != 0 ; //bank0 is DO and map not 0
	bank1_support = (p_relay_node->device_info.hw_info & 0x08) != 0 && p_relay_node->device_info.dioBank1_map != 0 ; //bank1 is DO and map not 0

	if( (p_relay_node->device_info.hw_info & 0x20) == 0 )
	{	
		debug("this dev=%s not support remote relay when triger_cloudRelay_action",p_relay_node->device_info.dev_serialNum) ;
		return 0 ;
	}

	if( (bank0_support != 0 && p_relay_node->device_info.relay_bank0 != p_relay_node->device_info.total_groupRelay) ||
		 (bank1_support != 0 && p_relay_node->device_info.relay_bank1 != p_relay_node->device_info.total_groupRelay) )
	{
		debug("begin to handle triger_cloud_relay_action \n");
	}
	else
	{
		debug("relay has been trigered when triger_cloud_relay_action,cloud_relay=0x%x \n",p_relay_node->device_info.total_groupRelay);
		return 0 ;
	}

	debug_notice("begin to send  cloud_groupRelay cmd to devSn=%s,devaddr=0x%x,relay_cmd=0x%x \n",p_relay_node->device_info.dev_serialNum,
																						p_relay_node->device_info.device_addr,
																						p_relay_node->device_info.total_groupRelay);

	build_and_send_remoteRelayCmd(p_relay_node,p_relay_node->device_info.total_groupRelay);	


	return 0 ;
}


int generate_dev_relay_byBankStatus(device_node_info_t *p_dev_node)
{
	uint16_t productID = 0 ;
	uint8_t count = 0 ;
	uint8_t bit_mask_count = 0 ;
	uint8_t relay_bit_mask[8] ; //0-4 is efective
	uint8_t temp_relay = 0 ;
	
	if(p_dev_node == NULL)
	{
		return 0 ;
	}

	memset(relay_bit_mask,0,sizeof(relay_bit_mask));

	//init relay bank0 and bank1
	p_dev_node->device_info.relay_bank0 = 0 ;
	p_dev_node->device_info.relay_bank1 = 0 ;

	productID = (p_dev_node->device_info.instr_family_id << 8) | p_dev_node->device_info.instr_model_id ;
	if(productID != RELAY_INSTRUMENT_ID)
	{
		//not relay dev
		debug("dev not relay when generate_dev_relay_byBankStatus \n");
		return 0 ;
	}


	if(p_dev_node->device_info.hw_info & 0x20) //bit5,have DIO or nor
	{
		if(p_dev_node->device_info.hw_info & 0x10) //bit4,bank0 is DO or not
		{
			bit_mask_count = 0 ;
			for(count=0;count<8;count++)
			{
				if( ((p_dev_node->device_info.dioBank0_map >> count) & 0x01) == 1 )
				{
					relay_bit_mask[bit_mask_count++] = count ;
				}
			}

			if(p_dev_node->device_info.dio_bank0_status == 0)
			{
				p_dev_node->device_info.relay_bank0 = 0 ;
			}
			else
			{
				for(count=0;count<5;count++) //only 0-4,relay1-relay5
				{
					temp_relay = p_dev_node->device_info.dio_bank0_status & (0x01<<relay_bit_mask[count]) ;
					//debug("temp_relay = 0x%x \n",temp_relay);
					if( temp_relay != 0 )
					{
						p_dev_node->device_info.relay_bank0 |= 0x01<<count ; //only bit0-bit4
					}
				}
			}
		}
		else
		{
			p_dev_node->device_info.relay_bank0 = 0 ;
		}

		if(p_dev_node->device_info.hw_info & 0x08) //bit3,bank1 is DO or not
		{
			bit_mask_count = 0 ;
			for(count=0;count<8;count++)
			{
				if( ((p_dev_node->device_info.dioBank1_map >> count) & 0x01) == 1 )
				{
					relay_bit_mask[bit_mask_count++] = count ;
				}
			}
			
			if(p_dev_node->device_info.dio_bank1_status == 0)
			{
				p_dev_node->device_info.relay_bank1 = 0 ;
			}
			else
			{
				for(count=0;count<5;count++) //only 0-4,relay1-relay5
				{
					temp_relay = p_dev_node->device_info.dio_bank1_status & (0x01<<relay_bit_mask[count]) ;
					//debug("temp_relay = 0x%x \n",temp_relay);
					if(temp_relay != 0)
					{
						p_dev_node->device_info.relay_bank1 |= 0x01<<count ; //only bit0-bit4
					}
				}
			}
		}
		else
		{
			p_dev_node->device_info.relay_bank1 = 0 ;
		}
	}

	debug("dev dioBank0=0x%x,dioBank1=0x%x \n",p_dev_node->device_info.dio_bank0_status,p_dev_node->device_info.dio_bank1_status);
	debug("dev=%s,relayBank0=0x%x,relayBank1=0x%x \n",p_dev_node->device_info.dev_serialNum,
														p_dev_node->device_info.relay_bank0,
														p_dev_node->device_info.relay_bank1 );

	return 0 ;
}

#if 0
int check_and_triger_remoteAlarm(device_node_info_t *p_cur_node,sensor_data_t *p_cur_sensor_data,uint8_t cur_dev_alarm)
{
	debug("check_and_triger_remoteAlarm,dur_dev_alarm=0x%x,all_alarm=0x%x \n",cur_dev_alarm,device_info_list_head.current_all_alarm);
	if( (cur_dev_alarm & 0x07) != 0)
	{
		//current dev alarm,triger other dev to alarm now
		triger_remote_alarm(cur_dev_alarm,p_cur_node,p_cur_sensor_data);
	}
	else
	{
		//this dev not alarm,triger it to alarm if necessary
		triger_specifiedDev_alarm(p_cur_node);
	}

	return 0 ;
}
#endif

int check_and_set_remoteAlarmInfo(device_node_info_t *p_cur_node,sensor_data_t *p_cur_sensor_data)
{
	sensor_data_t *p_temp_sen_data = NULL ;
	uint8_t cur_dev_alarm_info = 0 ;
	sensor_data_format_t *p_temp_sen_fmt = NULL ;
	uint8_t temp_sen_name[32] ;
	//time_t time_val ;
	
	if(p_cur_node == NULL || p_cur_sensor_data == NULL)
	{
		debug("err para when check_and_set_remoteAlarmInfo \n");
		return -1 ;
	}

	debug("begin to generate devself =%s alarmInfo according this dev-data \n",p_cur_node->device_info.dev_serialNum);
	if((comm_dev_data.app_alarm &0x07) != 0) //bit0-bit2
	{
		cur_dev_alarm_info |= 0x04 ; //bit2,fault alarm
	}

	if( (comm_dev_data.unit_err & 0x04) || (comm_dev_data.unit_err & 0x20) ) //bit2 or bit5
	{
		cur_dev_alarm_info |= 0x04 ; //bit2,fault alarm
	}

	p_temp_sen_data = p_cur_sensor_data ;
	while(p_temp_sen_data != NULL)
	{
		if(p_temp_sen_data->sensor_err != 0)
		{
			if(p_temp_sen_data->sensor_err >= 0x40 && p_temp_sen_data->sensor_err <= 0x43)
			{
				//max,over,high,highHigh
				cur_dev_alarm_info |= 0x01 ; // bit0,high alarm
				p_temp_sen_fmt = query_sensor_format_data(p_cur_node->device_info.device_addr,p_temp_sen_data->sensor_index) ;
				if(p_temp_sen_fmt != NULL)
				{
					memset(temp_sen_name,0,sizeof(temp_sen_name));
					get_sensorName_bysenIDandSubID(p_temp_sen_fmt->sensor_id,p_temp_sen_fmt->sensor_sub_id,temp_sen_name,(sizeof(temp_sen_name)-1) ) ;

					if(strcmp(temp_sen_name,"O2") != 0)
					{
						//not O2 sensor,triger low alarm
						cur_dev_alarm_info |= 0x02 ; //bit1,low alarm
					}
				}
			}
			else if(p_temp_sen_data->sensor_err >= 0x44 && p_temp_sen_data->sensor_err <= 0x45)
			{
				//low,lowLow
				cur_dev_alarm_info |= 0x02 ; //bit1,low alarm
			}
			else
			{
				//other sensor_err
				cur_dev_alarm_info |= 0x04 ; //bit2,fault alarm
			}
			
		}
		
		p_temp_sen_data = p_temp_sen_data->p_next_sensor_data ;
	}

	//append into current dev remote alarm info,include remoteAlarm and dev-local alarm
	p_cur_node->device_info.dev_alarm_info |= cur_dev_alarm_info ;

	//save dev-local alarm-info
	p_cur_node->device_info.dev_local_alarm_info = cur_dev_alarm_info ;


	//append into current_all_alarm
	if(cur_dev_alarm_info != 0)
	{
		//append new alarm,and renew the alarm timer
		device_info_list_head.current_all_alarm |= cur_dev_alarm_info ; // only bit0-bit2

		#if 0
		//renew remote_alarm timer now
		time(&time_val) ;
		device_info_list_head.cur_alarm_timer = time_val ;
		#endif
	}

	#if 1
	//maybe append dev into alarm list,or clear alarm status here
	check_and_renew_alarmList(cur_dev_alarm_info,p_cur_node) ;
	#endif
	

	debug("generated current devself alarm_info=0x%x,all_alarm=0x%x \n",cur_dev_alarm_info,device_info_list_head.current_all_alarm);

	return 0 ;
}

int check_and_set_relayInfo(device_node_info_t *p_alarm_node,sensor_data_t *p_alarm_sensor_data)
{
	sensor_data_t *p_temp_sen_data = NULL ;
	//uint8_t cur_dev_alarm_info = 0 ;
	sensor_data_format_t *p_temp_sen_fmt = NULL ;
	uint8_t temp_sen_name[32] ;
	uint8_t relay4_sen_name[10] = "LEL" ;
	uint8_t relay4_sen_name_1[10] = "CH4" ;
	uint8_t relay5_sen_name[10] = "H2S" ;
	uint8_t cur_dev_relay_info = 0 ;
	
	if(p_alarm_node == NULL || p_alarm_sensor_data == NULL)
	{
		debug("err para when check_and_set_relayInfo \n");
		return -1 ;
	}

	debug("begin to generate devself =%s relayInfo according this dev-data \n",p_alarm_node->device_info.dev_serialNum);
	
	if((comm_dev_data.app_alarm &0x07) != 0)
	{
		//device_info_list_head.relay_info |= 0x01 ; //relay1,bit0
		cur_dev_relay_info |= 0x01 ; //relay1,bit0
	}

	if( (comm_dev_data.unit_err & 0x04) || (comm_dev_data.unit_err & 0x20) ) //bit2 or bit5
	{
		cur_dev_relay_info |= 0x01 ; //relay1,bit0
	}

	
	p_temp_sen_data = p_alarm_sensor_data ;
	while( p_temp_sen_data != NULL )
	{
		//debug("handle relay info-- 1 \n");
		if(p_temp_sen_data->sensor_err != 0)
		{
			// set relay1,any alarm
			//device_info_list_head.relay_info |= 0x01 ; //relay1,bit0
			cur_dev_relay_info |= 0x01 ; //relay1,bit0
		

			//relay2,low,lowLow
			if(p_temp_sen_data->sensor_err >= 0x44 && p_temp_sen_data->sensor_err <= 0x45)
			{
				//device_info_list_head.relay_info |= 0x02 ; //relay2,bit1
				cur_dev_relay_info |= 0x02 ; //relay2,bit1
			}

			//relay3,max,over,high,highHigh
			if(p_temp_sen_data->sensor_err >= 0x40 && p_temp_sen_data->sensor_err <= 0x43)
			{
				//set relay3
				//device_info_list_head.relay_info |= 0x04 ; //relay3,bit2
				cur_dev_relay_info |= 0x04 ; //relay3,bit2

				//debug("handle relay info-- 2 \n");
				//it is lel,h2s or not?
				p_temp_sen_fmt = query_sensor_format_data(p_alarm_node->device_info.device_addr,p_temp_sen_data->sensor_index) ;
				if(p_temp_sen_fmt != NULL)
				{
					memset(temp_sen_name,0,sizeof(temp_sen_name));
					get_sensorName_bysenIDandSubID(p_temp_sen_fmt->sensor_id,p_temp_sen_fmt->sensor_sub_id,temp_sen_name,(sizeof(temp_sen_name)-1) ) ;
					if(strcmp(temp_sen_name,relay4_sen_name) == 0 || strcmp(temp_sen_name,relay4_sen_name_1) == 0 )
					{
						//match and set relay4,bit3
						//device_info_list_head.relay_info |= 0x08 ; //relay4,bit3
						cur_dev_relay_info |= 0x08 ; //relay4,bit3
					}
					else if(strcmp(temp_sen_name,relay5_sen_name) == 0)
					{
						//match and set relay5,bit4
						//device_info_list_head.relay_info |= 0x10 ; //relay5,bit4
						cur_dev_relay_info |= 0x10 ; //relay5,bit4
					}
				}
				//debug("handle relay info-- 3 \n");
			}
			
		}

		p_temp_sen_data = p_temp_sen_data->p_next_sensor_data ;
	}

	//append into current_all_relay
	if(cur_dev_relay_info != 0)
	{
		//append new alarm,and renew the alarm timer
		device_info_list_head.relay_info |= cur_dev_relay_info ; // only bit0-bit4,relay1-relay5

		#if 0
		//renew remote_alarm timer now
		time(&time_val) ;
		device_info_list_head.cur_alarm_timer = time_val ;
		#endif
	}

	#if 1
	//maybe append dev into alarm list,or clear alarm status here
	check_and_renew_relayList(cur_dev_relay_info,p_alarm_node) ;
	#endif

	debug("generated current devself relay_info=0x%x,all_relay_info=0x%x \n",cur_dev_relay_info,device_info_list_head.relay_info);

	return 0 ;
}

//called when triger-remoteAlarm  set == 0 and no groupAlarm cmd coming
int check_and_clear_RemoteAlarm(device_node_info_t *p_dev_node)
{
	uint8_t cmd_data[10] ;
	uint8_t send_cnt = 0 ;
	uint16_t productID = 0 ;
	
	if(p_dev_node == NULL)
	{
		return -1 ;
	}

	if(p_dev_node->device_info.recved_groupAlarm == 1)
	{
		debug("this dev has recved groupAlarm cmd,never clear the remoteAlarm,return now \n");
		return 0 ;
	}

	productID = (p_dev_node->device_info.instr_family_id << 8) | p_dev_node->device_info.instr_model_id ;
	if(productID == RELAY_INSTRUMENT_ID)
	{
		debug("this dev is relay-dev when check_and_clear_RemoteAlarm \n");
		check_and_clear_RemoteRelay(p_dev_node) ;
		return 0 ;
	}


	//if(wsn_net_conf_data.trig_alarm != 0 || wsn_net_conf_data.clear_remoteAlarmRelay != 1)
	if(wsn_net_conf_data.trig_alarm != 0 || query_dev_in_allGroupCmdList(p_dev_node) == 0 )
	{
		debug("triger-remoteAlarm == 1 or dev in groupAlarmCmd list when check_and_clear_RemoteAlarm ,nothing to do \n");
		return 0 ;
	}

	if( (p_dev_node->device_info.dev_remoteAlarm_info & 0x80) == 0)
	{
		//bit7,this dev not support remote alarm
		debug("this dev=%s not support remote alarm \n",p_dev_node->device_info.dev_serialNum);
		return 0 ;
	}

	if( (p_dev_node->device_info.dev_remoteAlarm_info & 0x07) == 0 ) //only check remoteAlarm,not include dev-local alarm
	{
		debug("dev_alarm has been cleared when check_and_clear_RemoteAlarm,return now \n");
		return 0 ;
	}

	build_and_send_remoteAlarmCmd(p_dev_node,0);

	return 0 ;
}

//called when triger-remoteRelay set == 0 and no groupAlarm cmd coming
int check_and_clear_RemoteRelay(device_node_info_t *p_dev_node)
{
	uint8_t bank0_support = 0 ;
	uint8_t bank1_support = 0 ;

	if(p_dev_node == NULL)
	{
		return -1 ;
	}

	if(wsn_net_conf_data.trig_relay != 0 || query_dev_in_allGroupCmdList(p_dev_node) == 0)
	{
		debug("triger-remoteRelay == 1 or dev in groupAlarmCmd list,nothing to do \n");
		return 0 ;
	}

	if( (p_dev_node->device_info.hw_info & 0x20) == 0) //bit5,not have DIO  ?
	{
		debug("dev=%s not support remoteRelay,dev-hw_info=0x%x \n",p_dev_node->device_info.dev_serialNum,p_dev_node->device_info.hw_info);
		return 0 ;
	}

	bank0_support = (p_dev_node->device_info.hw_info & 0x10) != 0 && p_dev_node->device_info.dioBank0_map != 0 ; //bank0 is DO and map not 0
	bank1_support = (p_dev_node->device_info.hw_info & 0x08) != 0 && p_dev_node->device_info.dioBank1_map != 0 ; //bank1 is DO and map not 0
	if( (bank0_support && p_dev_node->device_info.relay_bank0 != 0) || (bank1_support && p_dev_node->device_info.relay_bank1 != 0) )
	{
		build_and_send_remoteRelayCmd(p_dev_node,0);
	}
	
	return 0 ;
}

int build_and_send_remoteAlarmCmd(device_node_info_t *p_dev_node,uint8_t alarm_cmd)
{
	uint8_t cmd_data[10] ;
	uint8_t send_cnt = 0 ;
	
	if(p_dev_node == NULL)
	{
		debug("err para when build_and_send_devRemoteAlarmAction \n");
		return -1 ;
	}

	debug("begin to build remoteAlarm cmd,src_addr=0x%x,dest_addr=0x%x,alarm_cmd=0x%x \n",local_addr,p_dev_node->device_info.device_addr,alarm_cmd) ;
	cmd_data[0] = 0x03 ; //remote_alarm sub cmd
	cmd_data[1] = alarm_cmd ; // alarm info,set remoteAlarm
	send_cnt = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GENERAL_REQUEST_V2_CMDID,cmd_data,2,local_addr,p_dev_node->device_info.device_addr);
	if(send_cnt > 0)
	{
		//send_cmd_toRemote(wsn_send_buf,send_cnt); //this cmd no resp
		//send_expectedResp_cmd_toRemote(wsn_send_buf,send_cnt) ;
		send_cmd_toDevWaitingList(wsn_send_buf,send_cnt,GENERAL_REQUEST_V2_CMDID,p_dev_node) ;
	}
	
	return 0 ;
}

int build_and_send_remoteRelayCmd(device_node_info_t *p_dev_node,uint8_t relay_cmd)
{
	uint8_t relay_bit_mask[8] ; //0-4 is efective
	uint8_t relay_array[5] ;
	sensor_data_t *p_temp_sen_data = NULL ;
	sensor_data_format_t *p_temp_sen_fmt = NULL ;
	uint8_t temp_sen_name[MAX_SENSOR_NAME_LEN] ;
	uint8_t relay4_sen_name[10] = "LEL" ;
	uint8_t relay5_sen_name[10] = "H2S" ;
	uint8_t bank0_action = 0 ;
	uint8_t bank1_action = 0 ;
	uint8_t count = 0 ;
	uint8_t bit_mask_count = 0 ;
	uint8_t cmd_data[32] ;
	//uint8_t gw_sn[64] = "0123456789" ;
	uint8_t send_cnt = 0 ;
	uint8_t cmd_buf[64] ;
	uint8_t cmd_result[64] ;

	uint8_t bank0_support = 0 ;
	uint8_t bank1_support = 0 ;
	
	if(p_dev_node == NULL)
	{
		debug("err para when build_and_send_devRemoteAlarmAction \n");
		return -1 ;
	}

	debug("begin to build remoteRelay cmd,src=0x%x,dest=0x%x,relay_cmd=0x%x \n",local_addr,p_dev_node->device_info.device_addr,relay_cmd);
	memset(relay_bit_mask,0,sizeof(relay_bit_mask)) ;
	memset(relay_array,0,sizeof(relay_array));
	relay_array[0] = relay_cmd & 0x01 ; //bit0,relay1
	relay_array[1] = (relay_cmd>>1) & 0x01 ; // bit1,relay2
	relay_array[2] = (relay_cmd>>2) & 0x01 ; // bit2
	relay_array[3] = (relay_cmd>>3) & 0x01 ; // bit3
	relay_array[4] = (relay_cmd>>4) & 0x01 ; // bit4

	bank0_support = (p_dev_node->device_info.hw_info & 0x10) != 0 && p_dev_node->device_info.dioBank0_map != 0 ; //bank0 is DO and map not 0
	bank1_support = (p_dev_node->device_info.hw_info & 0x08) != 0 && p_dev_node->device_info.dioBank1_map != 0 ; //bank1 is DO and map not 0
	//generate bank action 
	if(p_dev_node->device_info.hw_info & 0x20) //bit5,have DIO or nor
	{
		//generate bank0_action
		bit_mask_count = 0 ;
		if(bank0_support) //bit4,bank0 is DO and map not 0
		{
			//bank0 is DO,generate bank0 action
			for(count=0;count<8;count++)
			{
				if( ((p_dev_node->device_info.dioBank0_map >> count) & 0x01) == 1 )
				{
					relay_bit_mask[bit_mask_count++] = count ;
				}
			}

			//only 5 bits efective,bit_mask value may be 0-7
			//if relay enableNum less than 5,we can use count<bit_mask_count,and if bit_mask_count >5,bit_mask_count=5;
			for(bit_mask_count=0;bit_mask_count<5;bit_mask_count++)
			{
				bank0_action |= (1*relay_array[bit_mask_count])<<(relay_bit_mask[bit_mask_count]) ;
			}
			
			debug("generated bank0_action=0x%x \n",bank0_action);
		}
		else
		{
			debug("bank0 is not DO \n");
			bank0_action = 0 ;
		}

		//generate bank1_action
		bit_mask_count = 0 ;
		if(bank1_support) //bit3,bank1 is DO or not
		{
			//bank1 is DO,generate bank1 action
			for(count=0;count<8;count++)
			{
				if( ((p_dev_node->device_info.dioBank1_map >> count) & 0x01) == 1 )
				{
					relay_bit_mask[bit_mask_count++] = count ;
				}
			}

			//only 5 bits efective,bit_mask value may be 0-7
			for(bit_mask_count=0;bit_mask_count<5;bit_mask_count++)
			{
				bank1_action |= (1*relay_array[bit_mask_count])<<(relay_bit_mask[bit_mask_count]) ;
			}
			
			debug("generated bank1_action=0x%x \n",bank1_action);
		}
		else
		{
			debug("bank1 is not DO \n");
			bank1_action = 0 ;
		}

		
	}


	count = 0 ;
	//bit4--DO,or bit3--DO
	#if 0
	if( ((p_dev_node->device_info.hw_info & 0x10) && p_dev_node->device_info.dio_bank0_status != bank0_action) || 
			( (p_dev_node->device_info.hw_info & 0x08) && p_dev_node->device_info.dio_bank1_status != bank1_action) )
	#endif
	{

		//debug("status not eq action,dio_bank0_status=0x%x,dio_bank1_status=0x%x \n",p_dev_node->device_info.dio_bank0_status,p_dev_node->device_info.dio_bank1_status);
		
		//build and send relay cmd 
		memset(cmd_data,0,sizeof(cmd_data)) ;

		//build cmd_data

		//SRC_EUI
		cmd_data[count++] = local_addr >> 8 ; //high byte
		cmd_data[count++] = local_addr & 0x00FF ;

		//SRC_LABEL
		strncpy(&cmd_data[count],gw_serial,10) ;
		count += 10 ;

		//pading,4 bytes
		memset(&cmd_data[count],0,4) ;
		count += 4 ;

		//bank1 action
		cmd_data[count++] = bank1_action ;
		//bank0 action
		cmd_data[count++] = bank0_action ;
		
		//chk data
		cmd_data[count++] = bank1_action ;
		cmd_data[count++] = bank0_action ;
		
		send_cnt = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),SET_REMOTE_DO_CMDID,cmd_data,count,local_addr,p_dev_node->device_info.device_addr);
		if(send_cnt > 0)
		{
			//send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,send_cnt) ;
			send_cmd_toDevWaitingList(wsn_send_buf,send_cnt,SET_REMOTE_DO_CMDID,p_dev_node) ;
		}
		
	}

	return 0 ;
}

uint16_t query_modbus_addr_bySerial(uint8_t *p_serial_num)
{
	uint16_t modbus_addr = 0xFFFF ; //illegal addr
	int ret_val = 0 ;

	//query nvram tab by dev-serial
	ret_val = nvram_get_devModbusAddr(p_serial_num) ;
	if(ret_val < 0)
	{
		modbus_addr = 0xFFFF ;
	}
	else
	{
		modbus_addr = ret_val ;
	}

	return modbus_addr ;
}

//add for modbusTcp
uint8_t query_instruID_byInstruName(uint8_t *p_instruName)
{
	uint8_t instruID = 0 ;  //unknown
	//char * p_subStr = NULL ;
	
	if(p_instruName == NULL || p_instruName[0] == 0)
	{
		return 0 ;
	}

	//if(strcmp(p_instruName,"MeshGuard") == 0)
	if(strstr(p_instruName,"MeshGuard") != NULL)
	{
		instruID = 0x01 ;
	}
	//else if(strcmp(p_instruName,"RAEWatch") == 0)
	if(strstr(p_instruName,"RAEWatch") != NULL)
	{
		instruID = 0x02 ;
	}
	//else if(strcmp(p_instruName,"RAETag") == 0)
	if(strstr(p_instruName,"RAETag") != NULL)
	{
		instruID = 0x03 ;
	}
	//else if(strcmp(p_instruName,"Router") == 0)
	if(strstr(p_instruName,"Router") != NULL)
	{
		instruID = 0x04 ;
	}
	//else if(strcmp(p_instruName,"ToxiRAE") == 0)
	if(strstr(p_instruName,"ToxiRAE") != NULL)
	{
		instruID = 0x05 ;
	}
	//else if(strcmp(p_instruName,"MultiRAE") == 0)
	if(strstr(p_instruName,"MultiRAE") != NULL)
	{
		instruID = 0x06 ;
	}
	//else if(strcmp(p_instruName,"QRAE3") == 0)
	if(strstr(p_instruName,"QRAE3") != NULL)
	{
		instruID = 0x07 ;
	}
	//else if(strcmp(p_instruName,"RAEPoint") == 0)
	if(strstr(p_instruName,"RAEPoint") != NULL)
	{
		instruID = 0x08 ;
	}
	//else if(strcmp(p_instruName,"RigRat") == 0)
	if(strstr(p_instruName,"RigRat") != NULL)
	{
		instruID = 0x09 ;
	}
	//else if(strcmp(p_instruName,"AreaRAE") == 0)
	if(strstr(p_instruName,"AreaRAE") != NULL)
	{
		instruID = 0x0A ;
	}

	return instruID ; //unknown
}


int insert_data_intoModbusLocalBuf(modbus_data_buf_t *p_data_node)
{
	modbus_data_buf_t *p_cur = NULL ;
	
	if(p_data_node == NULL)
	{
		return 0 ;
	}

	//insert data to the list end
	p_cur = p_wsn_modbus_dataList ;
	if(p_cur == NULL)
	{
		p_wsn_modbus_dataList = p_data_node ;
		return 0 ;
	}

	while(p_cur != NULL)
	{
		if(p_cur->p_next == NULL)
		{
			p_cur->p_next = p_data_node ;
			break ;
		}

		p_cur = p_cur->p_next ;
	}
	
	return 0 ;
}


int build_and_send_modbusData(device_node_info_t *p_dev_node,uint8_t *p_modbus_data_buf,uint16_t buf_len)
{
	uint16_t count = 0 ;
	uint16_t temp_count = 0 ;
	uint16_t reg_count = 0 ;
	uint8_t instru_name[32] ;
	uint32_t productID = 0 ;
	uint8_t modbus_intruID = 0 ;
	uint8_t sen_num = 0 ;
	sensor_data_format_t *p_sensor_format = NULL ;
	sensor_data_t *p_sensor_data = NULL ;
	//sensor_data_format_t *p_sen_fmt = NULL ;
	dev_subDataPak_list_t *p_subData_node = NULL ;
	uint8_t sensor_err = 0 ;
	uint16_t modbus_map_addr = 0 ;
	int sensor_reading = 0 ;
	int retry = 0 ;
	int retval = 0 ;
	modbus_data_buf_t *p_modbus_node = NULL ;
	uint8_t sensorName[MAX_SENSOR_NAME_LEN+4] ;
	
	
	if(p_dev_node == NULL || p_modbus_data_buf == NULL || buf_len < 32)
	{
		return -1 ;
	}

	modbus_map_addr = query_modbus_addr_bySerial(p_dev_node->device_info.dev_serialNum) ;
	if(modbus_map_addr == 0xFFFF)
	{
		//this dev not in the modbus tab,return now
		debug("dev=%s not in the modbus tab,return now \n",p_dev_node->device_info.dev_serialNum);

		//check and save data into modbus cache,if necessary
		retval = insert_devData_toModbusCacheList(p_wsn_modbus_dataList);
		if(retval != MODBUS_LOCK)
		{
			//success,reset p_wsn_modbus_dataList now
			p_wsn_modbus_dataList = NULL ;
		}

		return 0 ;
	}

	//hdr 16 registers,32bytes

	//reg0,MonitorIndex and SysOnlineNum
	p_modbus_data_buf[count++] = 0x01 ; //available
	//p_modbus_data_buf[count++] = p_dev_node->device_info.active_sensor_num ;
	p_modbus_data_buf[count++] = (device_info_list_head.online_dev_count & 0xFF ) ; // online dev num
	reg_count++ ;

	//reg1,RadioID high and low
	p_modbus_data_buf[count++] = (p_dev_node->device_info.device_addr >> 8) &0x00FF ; //high byte
	p_modbus_data_buf[count++] = p_dev_node->device_info.device_addr & 0x00FF ;
	reg_count++ ;

	//reg2-reg9,devSN,16 bytes,8 regs
	strlcpy(&p_modbus_data_buf[count],p_dev_node->device_info.dev_serialNum,16) ;
	count += 16 ;
	reg_count += 8 ;

	//reg 0x0A,InstrID and UnitErr
	memset(instru_name,0,sizeof(instru_name)) ;
	//productID = (p_dev_node->device_info.instr_family_id << 8) | p_dev_node->device_info.instr_model_id ;
	productID = (p_dev_node->device_info.instr_family_id << 24) | (p_dev_node->device_info.instr_model_id<<16) ;
	query_instrument_tab(productID,instru_name,sizeof(instru_name)-1 ) ;
	modbus_intruID = query_instruID_byInstruName(instru_name) ;
	p_modbus_data_buf[count++] = modbus_intruID ;
	p_modbus_data_buf[count++] = comm_dev_data.unit_err ;
	reg_count++ ;

	//reg 0x0B,senMask
	p_modbus_data_buf[count++] = (p_dev_node->device_info.sensor_mask >> 8) & 0x00FF ; //high byte
	p_modbus_data_buf[count++] = p_dev_node->device_info.sensor_mask & 0x00FF ; // low byte
	reg_count++ ;

	//reg 0x0C,dev dataInterval
	p_modbus_data_buf[count++] = (p_dev_node->device_info.data_interval >> 8) & 0x00FF ; //high byte
	p_modbus_data_buf[count++] = p_dev_node->device_info.data_interval & 0x00FF ; //low byte
	reg_count++ ;

	//reg 0x0D,PwrStatus and PwrPer
	p_modbus_data_buf[count++] = comm_dev_data.unit_info & 0xC0 ; //bit7,bit6
	p_modbus_data_buf[count++] = comm_dev_data.power_percent ;
	reg_count++ ;

	//reg 0x0E,DIO_Bank1_Settings and DIO_Bank0_Settings
	p_modbus_data_buf[count++] = dev_dio_data.dio_bank1_setting ;
	p_modbus_data_buf[count++] = dev_dio_data.dio_bank0_setting ;
	reg_count++ ;

	//reg 0x0F,DIO_Bank1_Status and DIO_Bank0_Status
	p_modbus_data_buf[count++] = dev_dio_data.dio_bank1_status ;
	p_modbus_data_buf[count++] = dev_dio_data.dio_bank0_status ;
	reg_count++ ;
	temp_count = count ;
	
	if(p_dev_node->device_info.active_sensor_num > 0)
	{
		//check buf overflow?
		if( (count+p_dev_node->device_info.active_sensor_num*4*2) > buf_len)
		{
			debug("buf overflow when build_and_send_modbusData \n");
			return -1 ;
		}

		if(p_dev_node->device_info.sub_data_pak)
		{
			p_subData_node = query_subSenData_list(p_dev_node) ;
			if(p_subData_node != NULL)
			{
				p_sensor_data = p_subData_node->p_sen_data_list ;
			}
		}
		else
		{
			p_sensor_data = p_dev_sensorData_head ;
		}

		//add sensor regs now
		while(p_sensor_data != NULL)
		{
			p_sensor_format = query_sensor_format_data(p_dev_node->device_info.device_addr,p_sensor_data->sensor_index) ;
			if(p_sensor_format == NULL)
			{
				debug("err not found senFmt when build_and_send_modbusData \n");
				return -1 ;
			}

			//senID
			memset(sensorName,0,sizeof(sensorName));
			get_sensorName_bysenIDandSubID(p_sensor_format->sensor_id,p_sensor_format->sensor_sub_id,sensorName,(sizeof(sensorName)-1) ) ;
			//p_modbus_data_buf[count++] = p_sensor_format->sensor_id ;
			p_modbus_data_buf[count++] = query_modbusSensorID(sensorName);

			//UnitID
			p_modbus_data_buf[count++] = p_sensor_format->unit_id ;

			//dataFmt
			if(p_dev_node->device_info.cmd_ver == WSN_CMD_VER2)
			{
				p_modbus_data_buf[count++] = p_sensor_format->format_data[5] ; // v2--byte5
			}
			else
			{
				p_modbus_data_buf[count++] = p_sensor_format->format_data[2] ; // v1--byte2
			}

			//senERR
			if(p_dev_node->device_info.cmd_ver == WSN_CMD_VER2)
			{
				sensor_err = sensorErr_tranV2toV1(p_sensor_data->sensor_err) ;
			}
			else
			{
				sensor_err = p_sensor_data->sensor_err ;
			}
			p_modbus_data_buf[count++] = sensor_err ;

			//reading,float,MSB first
			memcpy((uint8_t *)&sensor_reading,(uint8_t *)&p_sensor_data->sensor_real_data,4) ;
			p_modbus_data_buf[count++] = (sensor_reading >> 24) & 0xFF ;
			p_modbus_data_buf[count++] = (sensor_reading >> 16) & 0xFF ;
			p_modbus_data_buf[count++] = (sensor_reading >> 8) & 0xFF ;
			p_modbus_data_buf[count++] = sensor_reading & 0xFF ;

			//reg_count += 4 ;
			p_sensor_data = p_sensor_data->p_next_sensor_data ;
			
		}
		
	}

	//skip 16 sensors(16*4 regs) and 16*4*2=128 bytes
	reg_count += 64 ;
	count = temp_count + 128 ;

	if( (count+18) > buf_len) // 18 == userID and baudrate
	{
		debug("wr userID err,maybe buflen=%d err \n",buf_len);
		return -1 ;
	}

	//userID,or userName
	memcpy(&p_modbus_data_buf[count],p_dev_node->device_info.userID,8) ;

	count += 16 ; //userID total 16 bytes
	reg_count += 8 ;

	//baudRate
	p_modbus_data_buf[count++] = 0 ;
	p_modbus_data_buf[count++] = 0 ;
	reg_count++ ;


	//check total len
	if(count > 200)
	{
		debug("datalen err =%d when build_and_send_modbusData \n");
		return -1 ;
	}

	//save modbus data into the tab
	//modbus_map_addr = query_modbus_addr_bySerial(p_dev_node->device_info.dev_serialNum) ;
	if(modbus_map_addr != 0xFFFF)
	{
		p_modbus_node = modbus_dataBuf_malloc() ;
		if(p_modbus_node == NULL)
		{
			debug("modbus data cache overflow,maybe modbusTcp task occupy more time \n");
			//save to modbus cache list
			retval = insert_devData_toModbusCacheList(p_wsn_modbus_dataList);
			if(retval != MODBUS_LOCK)
			{
				//success,reset p_wsn_modbus_dataList now
				p_wsn_modbus_dataList = NULL ;
			}
			
			return -1 ;
		}

		
		memcpy(p_modbus_node->data,p_modbus_data_buf,count) ;
		p_modbus_node->busy = 1 ;
		p_modbus_node->data_size = count ;
		p_modbus_node->reg_count = reg_count ;
		p_modbus_node->modbus_addr = modbus_map_addr ;
		p_modbus_node->real_data_flag = 1 ; //real data
		p_modbus_node->p_next = NULL ;

		//insert into local list,p_wsn_modbus_dataList
		insert_data_intoModbusLocalBuf(p_modbus_node) ;

		//save to modbus cache list
		retval = insert_devData_toModbusCacheList(p_wsn_modbus_dataList);
		if(retval != MODBUS_LOCK)
		{
			//success,reset p_wsn_modbus_dataList now
			//debug("success save data to modbus cache \n");
			p_wsn_modbus_dataList = NULL ;
		}
		
	}
	
	return 0 ;
}


int build_and_send_modbusOfflineData(device_node_info_t *p_dev_node,uint8_t *p_modbus_offlineData_buf,uint16_t buf_len)
{
	uint16_t count = 0 ;
	uint16_t reg_count = 0 ;
	uint8_t instru_name[32] ;
	uint32_t productID = 0 ;
	uint8_t modbus_intruID = 0 ;
	uint8_t sen_num = 0 ;
	sensor_data_format_t *p_sensor_format = NULL ;
	sensor_data_t *p_sensor_data = NULL ;
	//sensor_data_format_t *p_sen_fmt = NULL ;
	dev_subDataPak_list_t *p_subData_node = NULL ;
	uint8_t sensor_err = 0 ;
	uint16_t modbus_map_addr = 0 ;
	int sensor_reading = 0 ;
	int retry = 0 ;
	int retval = 0 ;
	modbus_data_buf_t *p_modbus_node = NULL ;
	
	
	if(p_dev_node == NULL || p_modbus_offlineData_buf == NULL || buf_len < 32)
	{
		return -1 ;
	}

	modbus_map_addr = query_modbus_addr_bySerial(p_dev_node->device_info.dev_serialNum) ;
	if(modbus_map_addr == 0xFFFF)
	{
		//this dev not in the modbus tab,return now
		debug("dev=%s not in the modbus tab,return now \n",p_dev_node->device_info.dev_serialNum);

		//check and save data into modbus cache,if necessary
		retval = insert_devData_toModbusCacheList(p_wsn_modbus_dataList);
		if(retval != MODBUS_LOCK)
		{
			//success,reset p_wsn_modbus_dataList now
			p_wsn_modbus_dataList = NULL ;
		}

		return 0 ;
	}

	//hdr 16 registers,32bytes

	//reg0,MonitorIndex and SysOnlineNum
	p_modbus_offlineData_buf[count++] = 0x00 ; //available
	//p_modbus_data_buf[count++] = p_dev_node->device_info.active_sensor_num ;
	p_modbus_offlineData_buf[count++] = (device_info_list_head.online_dev_count & 0xFF ) ; // online dev num
	reg_count++ ;



	//check total len
	if(count > 256)
	{
		debug("datalen err =%d when build_and_send_modbusData \n");
		return -1 ;
	}

	//save modbus data into the tab
	//modbus_map_addr = query_modbus_addr_bySerial(p_dev_node->device_info.dev_serialNum) ;
	if(modbus_map_addr != 0xFFFF)
	{
		p_modbus_node = modbus_dataBuf_malloc() ;
		if(p_modbus_node == NULL)
		{
			debug("modbus data cache overflow,maybe modbusTcp task occupy more time \n");
			//save to modbus cache list
			retval = insert_devData_toModbusCacheList(p_wsn_modbus_dataList);
			if(retval != MODBUS_LOCK)
			{
				//success,reset p_wsn_modbus_dataList now
				p_wsn_modbus_dataList = NULL ;
			}
			
			return -1 ;
		}

		
		memcpy(p_modbus_node->data,p_modbus_offlineData_buf,count) ;
		p_modbus_node->busy = 1 ;
		//p_modbus_node->offline = 1 ; //not used
		p_modbus_node->data_size = count ;
		p_modbus_node->reg_count = reg_count ;
		p_modbus_node->modbus_addr = modbus_map_addr ;
		p_modbus_node->real_data_flag = 0 ; //offline data
		p_modbus_node->p_next = NULL ;

		//insert into local list,p_wsn_modbus_dataList
		insert_data_intoModbusLocalBuf(p_modbus_node) ;

		//save to modbus cache list
		retval = insert_devData_toModbusCacheList(p_wsn_modbus_dataList);
		if(retval != MODBUS_LOCK)
		{
			//success,reset p_wsn_modbus_dataList now
			//debug("success save data to modbus cache \n");
			p_wsn_modbus_dataList = NULL ;
		}
		
	}
	
	return 0 ;
}
int transfer_data_to_echoview(uint8_t *p_data,uint8_t data_len){
	uint8_t send_buff[256+8]={0};
	int serial_fd = -1 ;
	debug("transfer_data_to_echoview\n");
	if(p_data == NULL || data_len <= 0||data_len > 256-0x20-6)
	{
		debug("err input para when transfer_data_to_echoview \n");
		return -1 ;
	}
	if(g_module_config.module_lora1_mesh0!=1){
		debug("not support mesh when transfer_data_to_echoview \n");
		return -2 ;
	}
	serial_fd = get_serial_fd() ;
	if(serial_fd < 0)
	{
		debug("get err fd when transfer_data_to_echoview \n");
		return -4 ;
	}
    uint8_t rmci_pack[256];   //fxy 1
	for(uint8_t i=0;i<ECHOVIEW_NUM;i++){
		if(g_echoview_list[i].online==1){
			debug("transfer_data_to_echoview echoview_list[%d]\n",i);
			rmci_pack[0]=RMCI_SOP;
			rmci_pack[1]=0x20+data_len+6;
			rmci_pack[2]=RMCI_CMD_BY_HUB;
			rmci_pack[3]=g_echoview_list[i].addr_msb;
			rmci_pack[4]=g_echoview_list[i].addr_lsb;
			memcpy(&rmci_pack[5],p_data,data_len);
			rmci_pack[5+data_len]=RMCI_EOP;	
			send_uint8_buf(serial_fd,rmci_pack,6+data_len);
		    usleep(1*1000);
		}

	}
	return 0 ;
}

//except transfer DATA pack 
int transfer_devinfo_to_echoview(uint8_t *p_data,uint8_t data_len){
	uint8_t send_buff[256+8]={0};
	int serial_fd = -1 ;
	debug("transfer_devinfo_to_echoview data_len %d\n",data_len);
	if(p_data == NULL || data_len <= 0||data_len > 256-0x20-6)
	{
		debug("err input para when transfer_devinfo_to_echoview \n");
		return -1 ;
	}
	if(g_module_config.module_lora1_mesh0!=1){
		debug("not support mesh when transfer_devinfo_to_echoview \n");
		return -2 ;
	}
	serial_fd = get_serial_fd() ;
	if(serial_fd < 0)
	{
		debug("get err fd when transfer_devinfo_to_echoview \n");
		return -4 ;
	}
    uint8_t rmci_pack[256];   //fxy 1
	rmci_pack[0]=RMCI_SOP;
	rmci_pack[1]=0x20+data_len+6;
	rmci_pack[2]=RMCI_CMD_BY_HUB;
	rmci_pack[3]=(g_echoview.rmci_addr&0xFF00)>8;
    rmci_pack[4]= g_echoview.rmci_addr&0x00FF;//g_echoview.addr_lsb;
	memcpy(&rmci_pack[5],p_data,data_len);
	rmci_pack[5+data_len]=RMCI_EOP;	
    send_uint8_buf(serial_fd,rmci_pack,6+data_len);

	return 0 ;
}
//parse one wsn packet
int parse_wsn_data(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_id = 0 ;
	uint8_t hwinfo = 0 ;
	uint8_t sensor_num = 0 ;
	uint8_t cfgSet_cnt = 0 ;
	uint8_t alarmSet_cnt = 0 ;
	uint16_t pkt_dest_addr = 0 ; //for us
	uint16_t pkt_source_addr = 0 ;
	
	device_node_info_t node_info ;
	device_node_info_t *p_temp_node = NULL ;
    device_node_info_t *p_node_info_by_addr = NULL ;
	sensor_data_format_t *p_sensor_format = NULL ;
	sensor_data_t *p_sensor_data = NULL ;
	sensor_data_t *p_tmp_sensor_data = NULL ;
	int count = 0 ;
	int temp_count = 0 ;
	int serial_fd = 0 ;
	int ret_val = 0 ;
	uint8_t pkt_sen_index = 0 ;
	uint8_t next_sen_index = ILLEGAL_SEN_INDEX ;
	uint8_t cmd_data[32] ;
	uint8_t cmd_data_offset = 0 ;
	uint8_t limit_data_len = 0 ;
	//uint8_t buf_sen_index = 0 ;
	//uint8_t buf_sen_data_len = 0 ;
	uint16_t mesh_addr = 0 ;
	sint8_t rssi_val = 0 ;

	int sint_sensor_data = 0 ;
	uint32_t uint_sensor_data = 0 ;
	uint8_t sensor_data_count = 0 ;
	uint8_t sensor_df = 0 ; //Divide Factor
	uint8_t sensor_dp = 0 ; // Decimal Point,digits after the decimal point

	//cloud cmd para
	uint16_t temp_msgID = 0 ;
	uint16_t temp_msgType = 0 ;
	uint8_t temp_action_type = 0 ;

	uint8_t encrypt_cmd_data[200] ;
	uint8_t encrypt_cmdData_len = 0 ;
	//int ret_val = 0 ;

	uint8_t send_cnt = 0 ;
	uint16_t productID = 0 ;
	dev_subDataPak_list_t *p_sub_data_node = NULL ;

	int cloud_groupAlarm_ret = 0 ;
	time_t time_val = 0 ;
	
    uint8_t temp_chk_val = 0 ;
	
	
	if(p_data == NULL || data_len <= 0)
	{
		debug("err input para when parse_wsn_data \n");
		return -1 ;
	}

	memset(cmd_data,0,sizeof(cmd_data)) ;
	
	cmd_id = get_wsn_cmdID_fromPkt(p_data,data_len);
	
	
	//offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN ; //source addr offset
	pkt_source_addr = get_sourceAddr_fromPkt(p_data,data_len) ;
	if(pkt_source_addr == 0xFFFF)
	{
		debug("source addr==0xFFFF,err!!! \n");
		return 0 ;
	}
	
	pkt_dest_addr = get_destAddr_fromPkt(p_data,data_len) ;
	if(pkt_dest_addr != 0xFFFF) //not broadcast
	{
		//set local net-addr
		local_addr = pkt_dest_addr ;
	}
	debug("cmd_id 0x%2x rmci_addr %d pkt_dest_addr %d pkt_source_addr %d\n",cmd_id,g_echoview.rmci_addr,pkt_dest_addr,pkt_source_addr);
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	//del_expected_resp_cmd(pkt_source_addr,cmd_id) ;

	//add for sleep-dev
	//check_and_reSend_cmd_byDevAddr(pkt_source_addr);


	//check secret-dev exchangeKey success or not
	p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
	if(p_temp_node != NULL && p_temp_node->device_info.dev_support_encrypt == 1)
	{
		if(p_temp_node->device_info.exchangeKey_success != 1  && cmd_id != WSN_EXCHANGE_KEY_CMDID && cmd_id != WSN_SECRET_COMM_REQ )
		{
			debug("recved secret dev-pak,but exchangeKey not success,discard it now \n");
			set_dev_offline_status(p_temp_node) ;
			return 0 ;
		}
	}

	
	//check and update dev offline_timer,maybe dev not online now
	if(cmd_id == WSN_DATA_PAK_V1_MSGID || cmd_id == WSN_DATA_PAK_V2_MSGID)
	{
		p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
		if(p_temp_node != NULL && p_temp_node->device_info.dev_online_flag != 1)
		{
			//update dev offline timer
			time(&time_val) ;
			p_temp_node->device_info.dev_offline_timer = time_val ;
		}
	}

		
	
	switch(cmd_id)
	{
		case WSN_DATA_PAK_V1_MSGID :
			debug("recved data paket v1,handle  it now \n") ;
			recv_data_pak_flag = 1 ;

			parse_wsn_dataPakV1(p_data,data_len) ;
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ; //used for dev-v1,check and send cmd to dev,only when recved data-pak
			
			break ;
		case WSN_DATA_PAK_V2_MSGID :
			debug("recved WSN_DATA_PAK_V2_MSGID pkt,handle it now pkt_source_addr %d\n",pkt_source_addr);
			recv_data_pak_flag = 1 ;
			init_dev_data_buf();
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			
            //if(g_echoview.rmci_addr==g_echoview.fix_wsn_addr&&pkt_source_addr==g_echoview.rmci_addr){//from echo view
			if(p_temp_node != NULL){
			    //debug("module_lora1_mesh0 %d echoview_flag %d\n",g_module_config.module_lora1_mesh0,p_temp_node->device_info.echoview_flag);				
				if((g_module_config.module_lora1_mesh0==1)&&(p_temp_node->device_info.echoview_flag==1)){
					//p_temp_node->device_info.echoview_flag=1;
					//if(p_temp_node->device_info.dev_online_flag == 0){
					debug("recv WSN_DATA_PAK_V2_MSGID set_dev_online_status echoview \n");
					set_dev_online_status(p_temp_node) ;
					//}
					return 0 ;
				}				
			}
			
			hwinfo = get_hwInfo_fromDataPkt(p_data,data_len) ;
			sensor_num = get_senNum_fromDataPkt(p_data,data_len); //sensor number in this dataPak
			cfgSet_cnt = get_cfgsetCnt_fromDataPkt(p_data,data_len) ;
			if(p_temp_node == NULL || (p_temp_node->device_info.cfgSet_cnt != cfgSet_cnt) )
			{
				if(p_temp_node != NULL)
				{
					debug("cfgSetCnt change,reget dev-info and discard this data-pak now \n");
					set_dev_offline_status(p_temp_node) ;
				}
				else
				{
					//not found match device info in buf,discard this packet and send get_device_info now
					debug("not found devinfo in our buf,get dev-info and discard this data-pak now \n") ;
				}
				
				count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
				if(count > 0)
				{
					send_cmd_toRemote(wsn_send_buf,count);
					//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
				}

				//return,not send any other cmd to dev
				return 0 ;
			}


			//check senFmt and limit recv ok?
			if(p_temp_node->device_info.active_sensor_num > 0)
			{
				if(p_temp_node->device_info.sen_fmt_recv_flag != 1)
				{
					debug("dev=%s,senFmt not recved,get it now \n",p_temp_node->device_info.dev_serialNum) ;
					//not recved senFmt,get it now
					count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_NAMEFORMAT_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
					if(count > 0)
					{
						//send_cmd_toRemote(wsn_send_buf,count);
						//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
						send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_NAMEFORMAT_V2_CMDID,p_temp_node) ;
					}

					break ;
				}

				//not care limit data ,get limit data only at free time
				#if 0
				//not recved sen-limit-data over ? get it now
				if(p_temp_node->device_info.sen_limit_recv_flag != 1)
				{
					debug("dev=%s,senLimit not recved over,get it now \n",p_temp_node->device_info.dev_serialNum) ;
					//cmd_data[0] = p_temp_node->device_info.active_sen_index[0] ; 
					next_sen_index = query_next_senLmt_index(p_temp_node) ;
					if(next_sen_index != ILLEGAL_SEN_INDEX)
					{
						cmd_data[0] = next_sen_index ;
						send_cnt = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_LIMIT_V2_CMDID,cmd_data,1,local_addr,pkt_source_addr);
						if(send_cnt > 0)
						{
							//send_cmd_toRemote(wsn_send_buf,count);
							//send_expectedResp_cmd_toRemote(wsn_send_buf,send_cnt) ;
							send_cmd_toDevWaitingList(wsn_send_buf,send_cnt,GET_SEN_LIMIT_V2_CMDID,p_temp_node) ;
						}
					}

					break ;
				}
				#endif
			}

			// begin to handle this data-pak
			
			//skip from sop to cmdID
			count = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;

			//comm dev-data,hdr-8-bytes in data-pak
			get_comm_devData(p_data,data_len) ;
			//save dev remote alarm_info
			p_temp_node->device_info.dev_alarm_info = comm_dev_data.rm_debug_field ; //used for local-remote-alarm
			p_temp_node->device_info.dev_remoteAlarm_info = comm_dev_data.rm_debug_field ; //read only,never change it
			//debug("dev=%s,dev->remoteAlarm=0x%x \n",p_temp_node->device_info.dev_serialNum,p_temp_node->device_info.dev_alarm_info);
			
			count += 8 ; //skip hdr-8 bytes

			//check alarmSet_cnt change or not
			if(p_temp_node->device_info.active_sensor_num > 0 && comm_dev_data.alarm_set_cnt != p_temp_node->device_info.alarmSet_cnt)
			{
				//reset limit
				reset_dev_limitData(p_temp_node);
				//reget sensor limit data now
				debug("alarmSet_cnt change,old_alarmSet=%d,pkt_alarmSet=%d,reget sensor limit data \n",p_temp_node->device_info.alarmSet_cnt,comm_dev_data.alarm_set_cnt);
				cmd_data[0] = p_temp_node->device_info.active_sen_index[0] ; //first sen_index
				send_cnt = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_LIMIT_V2_CMDID,cmd_data,1,local_addr,pkt_source_addr);
				if(send_cnt > 0)
				{
					//send_cmd_toRemote(wsn_send_buf,count);
					//send_expectedResp_cmd_toRemote(wsn_send_buf,send_cnt) ;
					send_cmd_toDevWaitingList(wsn_send_buf,send_cnt,GET_SEN_LIMIT_V2_CMDID,p_temp_node) ;
				}

				//return 0;  //not return,continue to handle data
			}

			//dio data
			if(hwinfo & DIO_DATA_FLAG)
			{
				//handle dio data now
				debug("handle DIO_DATA now \n");
				dev_dio_data.new_data_flag = 1 ;
				dev_dio_data.dio_bank1_status = p_data[count] ;
				dev_dio_data.dio_bank0_status = p_data[count+1] ;
				dev_dio_data.dio_bank1_setting = p_data[count+2] ;
				dev_dio_data.dio_bank0_setting = p_data[count+3] ;

				//save the dio status if necessary
				productID = (p_temp_node->device_info.instr_family_id << 8) | p_temp_node->device_info.instr_model_id ;
				if(productID == RELAY_INSTRUMENT_ID)
				{
					p_temp_node->device_info.dio_bank0_status = dev_dio_data.dio_bank0_status ;
					p_temp_node->device_info.dio_bank1_status = dev_dio_data.dio_bank1_status ;
					generate_dev_relay_byBankStatus(p_temp_node);
				}
				
				count += DIO_DATA_LEN ;
			}

			//gps data
			if(hwinfo & GPS_DATA_FLAG)
			{
				//handle gps data,little endian
				debug("handle GPS_DATA now,sizeof(float)=%d \n",sizeof(float));
				dev_gps_data.new_data_flag = 1 ; //fxy
				//char tmp_x_val[4] = {0};
				//char tmp_y_val[4] = {0};
				memcpy(&dev_gps_data.x_val,p_data+count,4);
				memcpy(&dev_gps_data.y_val,p_data+count+4,4);
				//dev_gps_data.x_val = *((float *)tmp_x_val) ;
				//dev_gps_data.y_val = *((float *)tmp_y_val) ;
				if(dev_gps_data.y_val == -1 && dev_gps_data.x_val == -1)
				{
					//illegal value
					dev_gps_data.y_val = 0 ;
					dev_gps_data.x_val = 0 ;
				}
				else
				{
					// convert 
					dev_gps_data.y_val = dev_gps_data.y_val*180/3.14159265; //fxy
					dev_gps_data.x_val = dev_gps_data.x_val*180/3.14159265; //fxy
				}
				count += GPS_DATA_LEN ;
			}

			//zigbee data
			if(hwinfo & ZIGBEE_DATA_FLAG)
			{
				//handle zigbee data
				debug("handle zigbee data now \n");
				dev_zigbee_data.new_data_flag = 1 ;
				//rssi_val = p_data[count] ;
				p_temp_node->device_info.dev_rssi_val = p_data[count] ;
				dev_zigbee_data.rcm_signal = p_data[count] ;
				dev_zigbee_data.rcm_addr = (p_data[count+1]<<8) | p_data[count+2] ;
                if(p_temp_node->device_info.parent_device_addr!=dev_zigbee_data.rcm_addr){
					p_temp_node->device_info.parent_device_addr = dev_zigbee_data.rcm_addr;
					p_node_info_by_addr = query_device_info_by_deviceAddr(dev_zigbee_data.rcm_addr) ;
					if(p_node_info_by_addr != NULL)
					{
						strlcpy(p_temp_node->device_info.parent_dev_serialNum,p_node_info_by_addr->device_info.dev_serialNum, \
						    sizeof(p_temp_node->device_info.parent_dev_serialNum));
						debug("parent addr=%d sn %s \n",p_temp_node->device_info.parent_device_addr,p_temp_node->device_info.parent_dev_serialNum);
					}
				}
				count += ZIGBEE_DATA_LEN ;
			}

			//handle dev online here,dev-online status include the rcm_signal in the zigbee-data
			p_temp_node->device_info.data_pak_recv_flag = 1 ;
			debug("set dev online when recved data-pak,dev=%s \n",p_temp_node->device_info.dev_serialNum);
			set_dev_online_status(p_temp_node) ;
            
			transfer_data_to_echoview(p_data,data_len);  //fxy echoview 2

			//sensor data,handle sensor data and insert into p_dev_sensorData_head list
			if(sensor_num > 0 && count < data_len) 
			{
				//handle sendor data
				#if 0
				debug("handle sensor data now \n");
				printf("sensor-data: \n");
				for(temp_count=0;temp_count<sensor_num*ONE_SENSOR_DATA_LEN;temp_count++)
				{
					printf("%02x ",p_data[count+temp_count]); //+8 to skip the hdr-8 bytes in the cmd-data
				}
				printf("\n");
				#endif

				//free old sensor-data if necessory
				if(p_dev_sensorData_head != NULL)
				{
					free_dev_sensorData_memList() ;
				}
				
				for(temp_count=0;temp_count<sensor_num;temp_count++)
				{
					//init reading value
					sint_sensor_data = 0 ;
					uint_sensor_data = 0 ;
						
					//handle every sensor data now
					pkt_sen_index = p_data[count];
					p_sensor_format = query_sensor_format_data(pkt_source_addr,pkt_sen_index) ;
					if(p_sensor_format == NULL)
					{
						debug("not found match sen_index format data in the buf ,discard this data-pak and re_get dev_info now\n");
						#if 1
						//reget senfmt data,only renew senFmt and limit here
						p_temp_node->device_info.sen_fmt_recv_flag = 0 ;
						p_temp_node->device_info.sen_limit_recv_flag = 0 ;
						p_temp_node->device_info.sen_fmt_recvNO = 0 ;
						
						count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_NAMEFORMAT_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
						if(count > 0)
						{
							//send_cmd_toRemote(wsn_send_buf,count);
							//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
							send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_NAMEFORMAT_V2_CMDID,p_temp_node) ;
						}
						#endif

						init_dev_data_buf();
						free_dev_sensorData_memList();
						//free sub_senData node if necessary
						free_subSenData_node(p_temp_node->device_info.dev_serialNum) ;
						//maybe we need to send pak to dev here?
						send_cmd_inDevCmdList(p_temp_node) ;
						return -1 ;
					}

					
					p_sensor_data = malloc(sizeof(sensor_data_t));
					if(p_sensor_data == NULL)
					{
						debug("err malloc sensor-data buf when parse_wsn_data \n");
						free_dev_sensorData_memList();
						//free sub_senData node if necessary
						free_subSenData_node(p_temp_node->device_info.dev_serialNum) ;
						init_dev_data_buf();
						p_temp_node->device_info.sen_data_recvNO = 0 ;
						return -1 ;
						//break ;
					}
					memset(p_sensor_data,0,sizeof(sensor_data_t)) ;
					p_sensor_data->total_sensor = sensor_num ;
					p_sensor_data->sensor_index = pkt_sen_index ;
					p_sensor_data->sensor_err = p_data[count+1];
					if(p_sensor_format->sensor_data_len <= 4) //sensor-data len in 1,2,4
					{
						memcpy(p_sensor_data->sensor_data,&p_data[count+2],p_sensor_format->sensor_data_len);
					}
					else
					{
						debug("sensor index=%d,sensor-data len=%d err \n",pkt_sen_index,p_sensor_format->sensor_data_len) ;
						//maybe we need to reget dev,senfmt and limit now
						free_sensor_format_data_mem(p_temp_node->p_sendor_format_data_head) ;
						p_temp_node->p_sendor_format_data_head = NULL ;
						p_temp_node->device_info.sen_fmt_recv_flag = 0 ;
						p_temp_node->device_info.sen_fmt_recvNO = 0 ;
						p_temp_node->device_info.sen_limit_recv_flag = 0 ;
						p_temp_node->device_info.sen_limit_recvNum = 0 ;
						///////////////////////////////////////////////
						free_dev_sensorData_memList();
						init_dev_data_buf();
						free(p_sensor_data) ;
						p_temp_node->device_info.sen_data_recvNO = 0 ;
						//free sub_senData node if necessary
						free_subSenData_node(p_temp_node->device_info.dev_serialNum) ;
						//send get dev now
						count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
						if(count > 0)
						{
							send_cmd_toRemote(wsn_send_buf,count);
							//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
						}
						/////////////////////
						return -1 ;
						//break ;
					}
					//caculate sensor_data,formula :(float)( (int)( ( (float) data / 10^DF) + 0.5) ) / 10^DP 
					sensor_dp = (p_sensor_format->format_data[5] >> 5) & 0x07 ; //byte[5],bit5-bit7
					p_sensor_data->sensor_dp = sensor_dp ;
					sensor_df = (p_sensor_format->format_data[5] >> 2) & 0x07 ; //byte[5],bit2-bit4
					p_sensor_data->sensor_df = sensor_df ;

					//maybe overflow if save uint into int
					if( (p_sensor_format->format_data[2] & 0x80) ) //bit7 == 1`---signed int
					{
						for(sensor_data_count=0;sensor_data_count<p_sensor_format->sensor_data_len;sensor_data_count++)
						{
							sint_sensor_data += (p_sensor_data->sensor_data[sensor_data_count]) << ((p_sensor_format->sensor_data_len-1-sensor_data_count)*8) ;
						}
						//calc sensor real data
						if(sint_sensor_data == 0x8001 || sint_sensor_data == 0x80000001)
						{
							//specisl reading,sensor warming-up
							p_sensor_data->sensor_real_data = 0 ;
							p_sensor_data->specialReading = WarmingUp ;
						}
						else if(sint_sensor_data == 0x8000 || sint_sensor_data == 0x80000000)
						{
							//special reading,Sensor Disabled Temporary
							p_sensor_data->sensor_real_data = 0 ;
							p_sensor_data->specialReading = DisabledTemporary ;
						}
						else if(sint_sensor_data == 0x8002 || sint_sensor_data == 0x80000002)
						{
							//special reading,TubeSampling
							p_sensor_data->sensor_real_data = 0 ;
							p_sensor_data->specialReading = TubeSampling ;
						}
						else
						{
							//calc real-val
							//p_sensor_data->sensor_real_data = ( (int)( (float) ( sint_sensor_data/pow(10,sensor_df))+0.5) )/pow(10,sensor_dp) ;
							calc_sensor_Reading(p_sensor_format,sint_sensor_data,0,&p_sensor_data->sensor_real_data) ;
						}
					}
					else
					{
						for(sensor_data_count=0;sensor_data_count<p_sensor_format->sensor_data_len;sensor_data_count++)
						{
							uint_sensor_data += (p_sensor_data->sensor_data[sensor_data_count]) << ((p_sensor_format->sensor_data_len-1-sensor_data_count)*8) ;
						}
						//calc sensor real data
						if(uint_sensor_data == 0xFFFE || uint_sensor_data == 0xFFFFFFFE)
						{
							//specisl reading,sensor warming-up
							p_sensor_data->sensor_real_data = 0 ;
							p_sensor_data->specialReading = WarmingUp ;
						}
						else if(uint_sensor_data == 0xFFFF || uint_sensor_data == 0xFFFFFFFF)
						{
							//special reading,Sensor Disabled Temporary
							p_sensor_data->sensor_real_data = 0 ;
							p_sensor_data->specialReading = DisabledTemporary ;
						}
						else if(uint_sensor_data == 0xFFFD || uint_sensor_data == 0xFFFFFFFD)
						{
							//special reading,TubeSampling
							p_sensor_data->sensor_real_data = 0 ;
							p_sensor_data->specialReading = TubeSampling ;
						}
						else
						{
							//calc real val
							//p_sensor_data->sensor_real_data = ( (unsigned int)( (float) ( uint_sensor_data/pow(10,sensor_df))+0.5) )/pow(10,sensor_dp) ;
							calc_sensor_Reading(p_sensor_format,0,uint_sensor_data,&p_sensor_data->sensor_real_data) ;
						}
					}

					//insert into sensor-data list
					insert_dev_sensor_data(p_sensor_data) ;
					debug("sensor_index=%d,sen_real_data=%f,data[0]=0x%x,data[1]=0x%x \n",p_sensor_data->sensor_index,p_sensor_data->sensor_real_data,
																							p_sensor_data->sensor_data[0],p_sensor_data->sensor_data[1]);
					
					
					count += p_sensor_format->sensor_data_len + 2 ; //+2 to skip sen_index and sen_error
					if(count >= data_len)
					{
						// finish handle sensor data,maybe sensor_num err?
						break ;
					}
					
					
				}

				//count += sensor_num*ONE_SENSOR_DATA_LEN ;
			}

			if(p_temp_node->device_info.sub_data_pak == 1 )
			{
				// handle sub-data-pak
				debug("handle dev sub_dataPak now \n");

				//insert into subData_pak list
				insert_subSenData_list(p_dev_sensorData_head,sensor_num,p_temp_node) ;
				p_dev_sensorData_head = NULL ;

				// still to recv sub data-pak?
				if(p_temp_node->device_info.sen_data_recvNO < p_temp_node->device_info.active_sensor_num)
				{
					//continue to recv sub-data-pak

					//trig cloud group alarm if necessary
					cloud_groupAlarm_ret = query_and_handle_groupAlarmCmdList(p_temp_node);
					//triger current-dev local alarm if necessary
					if(cloud_groupAlarm_ret == -2)
					{
						// -2 not exist cloud groupAlarm
						triger_specifiedDev_alarm(p_temp_node);
					}
					break ;
				}
			}

			#if 1
			//handle alarm if necessary
			if(p_temp_node->device_info.sub_data_pak == 1)
			{
				p_sub_data_node = query_subSenData_list(p_temp_node) ;
				if(p_sub_data_node != NULL )
				{
					p_tmp_sensor_data = p_sub_data_node->p_sen_data_list ;
				}
				else
				{
					debug("dev sub_data==NULL before saveRealRcord \n");
				}
			}
			else
			{
				p_tmp_sensor_data = p_dev_sensorData_head ;
			}
			//check_and_triger_remoteAlarm(p_temp_node,p_tmp_sensor_data,p_temp_node->device_info.dev_alarm_info) ;
			check_and_set_remoteAlarmInfo(p_temp_node,p_tmp_sensor_data);
			check_and_set_relayInfo(p_temp_node,p_tmp_sensor_data); //maybe not need,relay-dev not include sensors
			#endif

			//triger cloud group-alarm
			debug("begin to handle cloud_groupAlarm now \n");
			cloud_groupAlarm_ret = query_and_handle_groupAlarmCmdList(p_temp_node);
			
			//triger current-dev local-alarm if necessary
			if(cloud_groupAlarm_ret == -2)
			{
				// -2 not exist cloud groupAlarm
				triger_specifiedDev_alarm(p_temp_node);
			}

			//clear dev-remoteAlarm info if necessary
			check_and_clear_RemoteAlarm(p_temp_node) ;
			
			//finish handling ,send data to north server
			debug("finish handling data,transfer data to remote manage-system now \n");

			//save real data record
			debug("begin to save_record_data \n");
			save_realDataRecord(p_temp_node) ;
			
			//begin to build real-data-json packet and send to cloud
			build_json_real_data(p_temp_node) ;
			send_json_realData_toCloud(p_temp_node);

			//save dev-data into modbus tab
			if(get_modbus_ena())
			{
				memset(modbus_data_buf,0,sizeof(modbus_data_buf)) ;
				build_and_send_modbusData(p_temp_node,modbus_data_buf,sizeof(modbus_data_buf)) ;
			}

			//finish handle ,free sensor_data mem now
			free_dev_sensorData_memList() ;
			free_subSenData_node(p_temp_node->device_info.dev_serialNum) ;	
			//reset senData recvNO
			p_temp_node->device_info.sen_data_recvNO = 0 ;
			break ;

		case GET_DEVINFO_V1_RESP :
			debug("recved GET_DEVINFO_V1_RESP,handle  it now \n");
            if((g_module_config.module_lora1_mesh0==1)&&g_echoview.rmci_addr!=pkt_source_addr){ 
				//get wsn dst addr,then transfer devinfo or senformat of this instrument of addr to echoview 
				debug("recved cmd from echoview for GET_DEVINFO_V1\n");
				p_temp_node = query_device_info_by_deviceAddr(pkt_dest_addr) ;  //use dest addr
				if(p_temp_node != NULL)  //fxy echoview
				{
					if(p_temp_node->devinfo_buf_len>0){
						transfer_devinfo_to_echoview(p_temp_node->devinfo_buf,p_temp_node->devinfo_buf_len) ;
					}
				}
				debug("recved echoview cmd\n");
				return 0;
			}			
			ret_val =parse_wsn_devInfo_v1(p_data,data_len) ;
			if(ret_val == -1)
			{
				debug("parse_wsn_devInfo_v1 data err \n");
				break ;
			}			
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			if(p_temp_node != NULL)  //fxy echoview
			{
				if(data_len<=sizeof(p_temp_node->devinfo_buf)){
				    memcpy(p_temp_node->devinfo_buf,p_data,data_len) ;
					p_temp_node->devinfo_buf_len=data_len;
				}
			}			
			break ;
		case GET_DEVINFO_V2_RESP :
			debug("recved GET_DEVINFO_V2_RESP,handle it now \n");
            if((g_module_config.module_lora1_mesh0==1)&&g_echoview.rmci_addr!=pkt_source_addr){ 
				//get wsn dst addr,then transfer devinfo or senformat of this instrument of addr to echoview 
				debug("recved cmd from echoview for GET_DEVINFO_V2\n"); 
				//get wsn dst addr,then transfer devinfo or senformat of this instrument of addr to echoview 
				p_temp_node = query_device_info_by_deviceAddr(pkt_dest_addr) ;  //use dest addr
				if(p_temp_node != NULL)  //fxy echoview
				{
					if(p_temp_node->devinfo_buf_len>0){
						transfer_devinfo_to_echoview(p_temp_node->devinfo_buf,p_temp_node->devinfo_buf_len) ;
					}
				}
				debug("recved echoview cmd\n");
				return 0;
			}			
			memset(&node_info,0,sizeof(device_node_info_t)) ;

			node_info.device_info.dev_info_recv_flag = 1 ;
			//set dev-addr
			node_info.device_info.device_addr = get_sourceAddr_fromPkt(p_data,data_len) ;

			copy_device_info_toBuf(&node_info,p_data);
			//add prdId compare to get echoview dev addr
			uint32_t newdev_product_id = (node_info.device_info.instr_family_id << 24) | (node_info.device_info.instr_model_id << 16 )
							| (node_info.device_info.instr_hw_id << 8) | node_info.device_info.instr_fw_id ;
			debug("newdev_product_id 0x%x g_echoview.id 0x%x\n",newdev_product_id,g_echoview.id);			
			//newdev_product_id = newdev_product_id & 0xFFFF0000 ;
			//debug("newdev_product_id 0x%x\n",newdev_product_id);	



			//check dev-info recv_time
			p_temp_node = query_device_info_by_serialNum(node_info.device_info.dev_serialNum) ;

			#if 0
			if(p_temp_node != NULL )
			{
				if(node_info.device_info.dev_info_recv_time >= p_temp_node->device_info.dev_info_recv_time &&
					(node_info.device_info.dev_info_recv_time-p_temp_node->device_info.dev_info_recv_time) <= 120)
				{
					debug("dev_info recv_time too short ,<=120s ,discard this dev-info pak now \n");
					return -1 ;
				}
			}
			#else
			//maybe cfgSet_cnt overlow? !!!!!!!!!!!!!!!!!!!!!
			if(p_temp_node != NULL && p_temp_node->device_info.dev_info_recv_flag)
			{
				if(node_info.device_info.cfgSet_cnt <= p_temp_node->device_info.cfgSet_cnt)
				{
					debug("cfgSet_cnt not renew,discard this dev-info pak now \n");
					return -1 ;
				}
			}
			#endif
			
			set_senIndex_inDevInfo(&node_info);
			//check dev-info in offline-list
			p_temp_node = query_offlineDev_info_by_serialNum(node_info.device_info.dev_serialNum);
			if(p_temp_node != NULL )
			{
				if(p_temp_node->device_info.cfgSet_cnt == node_info.device_info.cfgSet_cnt)
				{
					//copy the dev-info,fmt-info,limit-info from offline list
					debug("found dev-info in offline list,cfg match,copy now \n");
					p_temp_node->device_info.device_addr = node_info.device_info.device_addr ; //used new addr,maybe addr changed
					memcpy(&node_info,p_temp_node,sizeof(node_info)) ;
					for(uint8_t k=0;k<2;k++){
						if(node_info.sensor_format_buf_len[k]>2 &&node_info.sensor_format_buf_len[k]<=sizeof(node_info.sensor_format_buf[k])){
							node_info.sensor_format_buf[k][5] = (node_info.device_info.device_addr &0xFF00)>8; 
							node_info.sensor_format_buf[k][6] = node_info.device_info.device_addr&0x00FF;
							temp_chk_val = calc_cmd_chksum(node_info.sensor_format_buf[k],node_info.sensor_format_buf_len[k]-2); //temp_pkt_len+3==total-pkt-len
							node_info.sensor_format_buf[k][node_info.sensor_format_buf_len[k]-2]=temp_chk_val;
						}
					}

					if(node_info.sensor_limit_buf_len>2 &&node_info.sensor_limit_buf_len<=sizeof(node_info.sensor_limit_buf)){
						node_info.sensor_limit_buf[5] = (node_info.device_info.device_addr &0xFF00)>8; 
                        node_info.sensor_limit_buf[6] = node_info.device_info.device_addr&0x00FF;
		                temp_chk_val = calc_cmd_chksum(node_info.sensor_limit_buf,node_info.sensor_limit_buf_len-2); //temp_pkt_len+3==total-pkt-len
                        node_info.sensor_limit_buf[node_info.sensor_limit_buf_len-2]=temp_chk_val;
					}
					del_deviceNode_from_offlineList(p_temp_node) ; //not free senFmt mem,used by online dev
				}
				else
				{
					//cfg not match,free the dev-info in the offline-list
					del_DevFmtInfo_from_offlineList(p_temp_node) ;
				}
			}
			if((g_module_config.module_lora1_mesh0==1)&&ECHO_VIEW_ID ==newdev_product_id){
				// memcpy(g_echoview.device_info.dev_serialNum,node_info.device_info.dev_serialNum,sizeof(g_echoview.device_info.dev_serialNum)-1);
				// g_echoview.addr_lsb = node_info.device_info.device_addr &0x00FF;
				// g_echoview.addr_msb = (node_info.device_info.device_addr &0xFF00)>8;    //fxy echoview
				// g_echoview.fix_wsn_addr = node_info.device_info.device_addr;
				node_info.device_info.echoview_flag=1;   //20240510
				//debug("echoview_flag=1\n");
	            //add_dev_to_echoview_array(&node_info); //20240510
			}else{
				node_info.device_info.echoview_flag=0;   //20240510
			}			
			insert_device_info_to_list(&node_info) ;
			if((g_module_config.module_lora1_mesh0==1)&&ECHO_VIEW_ID ==newdev_product_id){
				debug("echoview GET_DEVINFO_V2_RESP after insert list then exit\n");
				return 0 ;				
			}
			p_temp_node = query_device_info_by_serialNum(node_info.device_info.dev_serialNum) ;
			if(p_temp_node == NULL)
			{
				debug("insert dev=%s failed \n",node_info.device_info.dev_serialNum);
				return -1 ;
			}
			
			if(node_info.device_info.active_sensor_num > 0 && p_temp_node->device_info.sen_fmt_recv_flag == 0) //maybe copy dev-info from offline list
			{
				
				//send GET_SEN_NAMEFORMAT_V2_CMDID now
				count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_NAMEFORMAT_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
				if(count > 0)
				{
					//send_cmd_toRemote(wsn_send_buf,count);
					//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
					send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_NAMEFORMAT_V2_CMDID,p_temp_node) ;
				}
			}
			else
			{
				//we need send dev_chk permission and device_cfg to cloud or not? if no sensor in this dev
				debug("active sensor num=%d \n",node_info.device_info.active_sensor_num);
				debug("update node config data to cloud server now \n");
				//p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
				if(p_temp_node != NULL)
				{
					build_json_config_data(p_temp_node) ;
					send_json_confData_toCloud(p_temp_node);
					if(p_temp_node->p_devInfo_cmd != NULL)
					{
						free_specifiedCmd_inDevCmdList(p_temp_node->p_devInfo_cmd);
						p_temp_node->p_devInfo_cmd = NULL ;
					}						
				}
			}

			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			if(p_temp_node != NULL)  //fxy echoview //dev info from not echoview then copy to buf for transfered to echoview
			{
				if(data_len<=sizeof(p_temp_node->devinfo_buf)){
				    memcpy(p_temp_node->devinfo_buf,p_data,data_len) ;
					p_temp_node->devinfo_buf_len=data_len;
				}
			}
			break ;

		case GET_SEN_NAMEFORMAT_V1_RESP :
			debug("recved GET_SEN_NAMEFORMAT_V1_RESP,handle it now \n");
            if((g_module_config.module_lora1_mesh0==1)&&g_echoview.rmci_addr!=pkt_source_addr){ 
				//get wsn dst addr,then transfer devinfo or senformat of this instrument of addr to echoview 
				p_temp_node = query_device_info_by_deviceAddr(pkt_dest_addr) ;  //use dest addr
				if(p_temp_node != NULL)  //fxy echoview
				{
					if(p_temp_node->sensor_format_buf_len[0]>0){
						transfer_devinfo_to_echoview(p_temp_node->sensor_format_buf[0],p_temp_node->sensor_format_buf_len[0]) ;
					}
				}
				debug("recved echoview cmd\n");
				return 0;
			}			
			ret_val = parse_wsn_senNameFmt_v1(p_data,data_len) ;
			if(ret_val == -1)
			{
				debug("parse_wsn_senNameFmt_v1 data err \n");
				break ;
			}			
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			if(p_temp_node != NULL)  //fxy echoview
			{
				if(data_len<=sizeof(p_temp_node->sensor_format_buf[0])){
				    memcpy(p_temp_node->sensor_format_buf[0],p_data,data_len) ;
					p_temp_node->sensor_format_buf_len[0]=data_len;
				}
				//free senFmt cmd
				if(p_temp_node->p_senFmt_cmd != NULL)
				{
					free_specifiedCmd_inDevCmdList(p_temp_node->p_senFmt_cmd);
					p_temp_node->p_senFmt_cmd = NULL ;
				}				
			}			
			break ;
		case GET_SEN_NAMEFORMAT_V2_RESP :
			debug("recved GET_SEN_NAMEFORMAT_V2_RESP,handle it now \n");
            if((g_module_config.module_lora1_mesh0==1)&&g_echoview.rmci_addr!=pkt_source_addr){ 
				//get wsn dst addr,then transfer devinfo or senformat of this instrument of addr to echoview 
				debug("recved echoview cmd\n");
				p_temp_node = query_device_info_by_deviceAddr(pkt_dest_addr) ;  //use dest addr
				if(p_temp_node != NULL)  //fxy echoview
				{
					if(p_temp_node->sensor_format_recv_num==2 && p_temp_node->sensor_format_recv_finished==1){
						if(p_temp_node->sensor_format_buf_len[0]>0){
							transfer_devinfo_to_echoview(p_temp_node->sensor_format_buf[0],p_temp_node->sensor_format_buf_len[0]) ;
						}
						usleep(20000);
						if(p_temp_node->sensor_format_buf_len[1]>0){
							transfer_devinfo_to_echoview(p_temp_node->sensor_format_buf[1],p_temp_node->sensor_format_buf_len[1]) ;
						}
				    }else if (p_temp_node->sensor_format_recv_num==1 && p_temp_node->sensor_format_recv_finished==1){
						if(p_temp_node->sensor_format_buf_len[0]>0){
							transfer_devinfo_to_echoview(p_temp_node->sensor_format_buf[0],p_temp_node->sensor_format_buf_len[0]) ;
						}
					}
				}
				
				return 0;
			}			
			ret_val = parse_sensor_format_dataV2(p_data,data_len);

			if(ret_val == -1)
			{
				debug("parse_sensor_format_dataV2 data err \n");
				break ;
			}

			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;

			#if 0
			if(p_temp_node != NULL)
			{
				p_temp_node->device_info.sen_fmt_recv_flag = 1 ;
			}
			#endif
			
			if(ret_val == 0 && p_temp_node != NULL && p_temp_node->device_info.sen_fmt_recvNO >= p_temp_node->device_info.active_sensor_num) //parse sen_format_data ok?
			{
				//p_temp_node->device_info.sen_fmt_recv_flag = 1 ;
				if(p_temp_node->device_info.active_sensor_num > 0)
				{
					//begin to get sen_limit_data now
					cmd_data[0] = p_temp_node->device_info.active_sen_index[0] ; //first sen_index
					count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_LIMIT_V2_CMDID,cmd_data,1,local_addr,pkt_source_addr);
					if(count > 0)
					{
						//send_cmd_toRemote(wsn_send_buf,count);
						//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
						send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_LIMIT_V2_CMDID,p_temp_node) ;
					}
				}

				////////////////////
				#if 1
				debug("recved fmt,send node config data to cloud server now \n");
				build_json_config_data(p_temp_node) ;
				send_json_confData_toCloud(p_temp_node);
				#endif
				////////////////////////////
				//free senFmt cmd
				if(p_temp_node->p_senFmt_cmd != NULL)
				{
					free_specifiedCmd_inDevCmdList(p_temp_node->p_senFmt_cmd);
					p_temp_node->p_senFmt_cmd = NULL ;
				}				
			}
			//p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			if(p_temp_node != NULL)  //fxy echoview
			{
			    debug("sensor_format_buf len0 %d len1 %d device_addr %2x\n",p_temp_node->sensor_format_buf_len[0],  \
				    p_temp_node->sensor_format_buf_len[1], p_temp_node->device_info.device_addr);
			}		

#if 0			
			if(p_temp_node != NULL)  //fxy echoview
			{
				if(data_len<=sizeof(p_temp_node->sensor_format_buf)){
				    memcpy(p_temp_node->sensor_format_buf,p_data,data_len) ;
					p_temp_node->sensor_format_buf_len=data_len;
				}
			}		
#endif				
			break ;

		case GET_SEN_LIMIT_V1_RESP :
			debug("recved GET_SEN_LIMIT_V1_RESP,handle it now \n");
            if((g_module_config.module_lora1_mesh0==1)&&g_echoview.rmci_addr!=pkt_source_addr){ 
				//get wsn dst addr,then transfer devinfo or senformat of this instrument of addr to echoview 
				p_temp_node = query_device_info_by_deviceAddr(pkt_dest_addr) ;  //use dest addr
				if(p_temp_node != NULL)  //fxy echoview
				{
					if(p_temp_node->sensor_limit_buf_len>0){
						transfer_devinfo_to_echoview(p_temp_node->sensor_limit_buf,p_temp_node->sensor_limit_buf_len) ;
					}
				}
				debug("recved echoview cmd and exit\n");
				return 0;
			}			
			parse_wsn_senLimit_v1(p_data,data_len) ;
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			if(p_temp_node != NULL)  //fxy echoview
			{
				if(data_len<=sizeof(p_temp_node->sensor_limit_buf)){
				    memcpy(p_temp_node->sensor_limit_buf,p_data,data_len) ;
					p_temp_node->sensor_limit_buf_len=data_len;
				}
			}	
			break ;
		case GET_SEN_LIMIT_V2_RESP :
			debug("recved GET_SEN_LIMIT_V2_RESP,handle it now \n");
            if((g_module_config.module_lora1_mesh0==1)&&g_echoview.rmci_addr!=pkt_source_addr){ 
				//get wsn dst addr,then transfer devinfo or senformat of this instrument of addr to echoview 
				p_temp_node = query_device_info_by_deviceAddr(pkt_dest_addr) ;  //use dest addr
				if(p_temp_node != NULL)  //fxy echoview
				{
					if(p_temp_node->sensor_limit_buf_len>0){
						transfer_devinfo_to_echoview(p_temp_node->sensor_limit_buf,p_temp_node->sensor_limit_buf_len) ;
					}
				}
				debug("recved echoview cmd and exit\n");
				return 0;
			}			

			alarmSet_cnt = p_data[cmd_data_offset] ;
			pkt_sen_index = p_data[cmd_data_offset+3] ;
			debug("parse GET_SEN_LIMIT_V2_RESP: pkt_sen_index = %d alarmSet_cnt=%d \n",pkt_sen_index,alarmSet_cnt);
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			if(p_temp_node == NULL)
			{
				debug("err not found dev_node in buf when parse GET_SEN_LIMIT_V2_RESP data,re-get dev_info now \n") ;
				count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
				if(count > 0)
				{
					send_cmd_toRemote(wsn_send_buf,count);
					//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
				}
				return 0 ; 
			}

			//check limit data renew or not
			p_sensor_format = query_sensor_format_data(pkt_source_addr,pkt_sen_index) ;
			if(p_sensor_format != NULL )
			{
				if(alarmSet_cnt <= p_temp_node->device_info.alarmSet_cnt && p_sensor_format->current_senLimit_recvFlag)
				{
					debug("recved senIndex=%d limit data and alarmSetCnt not renew,discart it now \n",pkt_sen_index);
					break ;
				}
			}

			//handle limit data now
			p_temp_node->device_info.alarmSet_cnt = alarmSet_cnt ; //set alarmSet_cnt
			//p_temp_node->device_info.sen_limit_recv_flag = 1 ;
			p_sensor_format = query_sensor_format_data(pkt_source_addr,pkt_sen_index) ;
			if(p_sensor_format != NULL)
			{
				limit_data_len = p_sensor_format->sensor_data_len + 7 ; //include limit data head 7 bytes
				insert_sensor_limit_data(&p_data[cmd_data_offset],limit_data_len,pkt_source_addr,pkt_sen_index) ;

				//next_sen_index = get_next_sen_index(pkt_source_addr,pkt_sen_index) ;
				next_sen_index = query_next_senLmt_index(p_temp_node) ;
				if(next_sen_index != ILLEGAL_SEN_INDEX)
				{
					//get next sen_limit data
					debug("curent sen-index=%d,get next sen limit \n",pkt_sen_index);
					cmd_data[0] = next_sen_index ;
					count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_LIMIT_V2_CMDID,cmd_data,1,local_addr,pkt_source_addr);
					if(count > 0)
					{
						//send_cmd_toRemote(wsn_send_buf,count);
						//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
						send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_LIMIT_V2_CMDID,p_temp_node) ;
					}
				}

			}
			else
			{
				//re-get sen_fmt data now
				count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_NAMEFORMAT_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
				if(count > 0)
				{
					//send_cmd_toRemote(wsn_send_buf,count);
					//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
					send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_NAMEFORMAT_V2_CMDID,p_temp_node) ;
				}
				
			}

			//all sen-limit-data recved ? send config to cloud now
			if(p_temp_node->device_info.active_sensor_num > 0 && p_temp_node->device_info.sen_limit_recvNum >= p_temp_node->device_info.active_sensor_num)
			{
				//set sen-limit-data recvFlag
				p_temp_node->device_info.sen_limit_recv_flag = 1 ;
				
				//finish recving sen-limit info,update to cloud server now
				debug("update node config data to cloud server now \n");
				build_json_config_data(p_temp_node) ;
				send_json_confData_toCloud(p_temp_node);
				//free sen-limit cmd
				if(p_temp_node->p_senLimit_cmd != NULL)  //fix bug repeat sending cmd
				{
					free_specifiedCmd_inDevCmdList(p_temp_node->p_senLimit_cmd);
					p_temp_node->p_senLimit_cmd = NULL ;
				}				
			}
			if(p_temp_node != NULL)  //fxy echoview
			{
				if(data_len<=sizeof(p_temp_node->sensor_limit_buf)){
				    memcpy(p_temp_node->sensor_limit_buf,p_data,data_len) ;
					p_temp_node->sensor_limit_buf_len=data_len;
				}
			}			
			
			break ;

		case LOCATION_PAK_MSGID :
			debug("recved LOCATION_PAK,discard now \n");
			break ;

		case WSN_SNDMSG_CMDID :
			//cmd maybe from cloud
			debug("begin to handle WSN_SNDMSG resp now \n");
			temp_msgID = (p_data[cmd_data_offset] << 8) | p_data[cmd_data_offset+1] ;
			temp_msgType = (p_data[cmd_data_offset+2] << 8) | p_data[cmd_data_offset+3] ;

			//send cmd_success to cloud if necessary
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			if(p_temp_node != NULL)
			{
				set_cloudCmd_result(CLOUD_CMD_SUCCESS) ;
				handle_cloud_cmdNode_bySnAndCmdID(p_temp_node->device_info.dev_serialNum,strlen(p_temp_node->device_info.dev_serialNum),WSN_SNDMSG_CMDID) ;
			}
			break ;

		case WSN_PUT_SLEEP_WAKEUP_CMDID :
			//cmd maybe from cloud
			debug("begin to handle WSN_PUT_SLEEP_WAKEUP resp now \n");
			temp_action_type = p_data[cmd_data_offset] ;
			
			//send cmd_success to cloud if necessary
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			if(p_temp_node != NULL)
			{
				set_cloudCmd_result(CLOUD_CMD_SUCCESS) ;
				handle_cloud_cmdNode_bySnAndCmdID(p_temp_node->device_info.dev_serialNum,strlen(p_temp_node->device_info.dev_serialNum),WSN_PUT_SLEEP_WAKEUP_CMDID) ;
			}
			
			break ;

		case WSN_SECRET_COMM_REQ :
			debug("recved dev secret comm request now \n");
			build_and_send_exchangeKey_pak(pkt_source_addr);
						
			break ;

		case WSN_EXCHANGE_KEY_CMDID :
			debug("recved key_exchange_resp,handle it now \n");
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			if(p_temp_node != NULL)
			{
				//set dev exchangeKey success
				p_temp_node->device_info.exchangeKey_success = 1 ;
				
				//decrypt_data_pak(p_data,wsn_decrypt_buf,sizeof(wsn_decrypt_buf),p_temp_node) ;
				
				//renew sk for encrypt data-pak
				renew_dataPak_sk(p_temp_node); //not used
				
				//p_temp_node->device_info.dev_support_encrypt = 1 ; //set dev_support_encrypt flag
				if(p_temp_node->device_info.dev_info_recv_flag == 0)
				{
					//get secret dev-info now
					debug("get secret dev-info now \n");
					count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V2_CMDID,NULL,0,local_addr,pkt_source_addr);
					if(count > 0)
					{
						send_cmd_toRemote(wsn_send_buf,count);
						//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
					}
					
				}
			}
			else
			{
				debug("recved dev_exchange_key resp,but not found dev-info in buf,discard it now \n");
			}
			break ;

		case WSN_PING_DEV_MSGID :
			debug("recved dev_ping msg,response now \n");
			wsn_ping_response(pkt_source_addr) ;
			break ;
		case WSN_DEV_EVENT_MSGID :
			debug("recved dev_event msg,handle it now \n");
			handle_wsn_devEvt(p_data,data_len) ;
			break ;

		case GENERAL_REQUEST_V2_CMDID :
			//this cmd no resp
			debug("recved set remote alarm-generalCMD response,dev_addr=0x%x \n",pkt_source_addr);
			break ;

		case SET_REMOTE_DO_RESP :
			debug("recved SET_REMOTE_DO_RESP relay resp,dev_addr=0x%x \n",pkt_source_addr);
			break ;
		
		default :
			debug("err msgid when parse_wsn_data ,id=0x%02x\n",cmd_id);
			p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
			//maybe exchange key err,or share_key change,del dev now
			if(p_temp_node != NULL && p_temp_node->device_info.dev_support_encrypt == 1)
			{
				set_dev_offline_status(p_temp_node) ;
			}
			break ;
	}


	//send cmd in dev-cmd-list if necessary
	send_cmd_inDevCmdList(p_temp_node) ;

	return 0 ;
}

int rm_rmci_hdr_packet(uint8_t *p_rmci_packet,int *p_offset,int data_len,uint8_t *p_rmci_payload,int *p_payload_data_len){
	uint8_t rmci_pkt_hdr = RMCI_SOP ;
	uint8_t *p_pkt_hdr = NULL ;
	uint16_t count = 0 ; 
	*p_payload_data_len = 0;
	g_echoview.req_transfer_flg=0;
	if(p_rmci_packet == NULL || data_len <= 0 ||p_rmci_payload == NULL)
	{
		debug("err input para when rm_rmci_hdr_packet \n") ;
		return NULL ;
	}
	for(count=0+*p_offset;count<data_len;count++)
	{
		if(p_rmci_packet[count] == rmci_pkt_hdr && p_rmci_packet[count+2]==RMCI_CMD_BY_INST)
		{
			g_echoview.rmci_addr= (p_rmci_packet[count+3]<<8)|p_rmci_packet[count+4];
			// if(g_echoview.online==1 &&g_echoview.rmci_addr==g_echoview.fix_wsn_addr){
			// 	g_echoview.req_transfer_flg=1;
			// }
			debug("rm_rmci_hdr_packet rmci_addr %d \n",g_echoview.rmci_addr);
			p_pkt_hdr = &p_rmci_packet[count] ;
			if(*(p_rmci_packet+count+1)>=0x26){
                *p_payload_data_len = *(p_rmci_packet+count+1)-0x20-6;
			}            
			if(*p_payload_data_len<=sizeof(wsn_recv_buf)){
			    memcpy(p_rmci_payload,p_rmci_packet+count+5,*p_payload_data_len); //5b len(1byte) c6 dest(2bytes) 7b     5d
				*p_offset=count + *p_payload_data_len;
				break ;
			}else{
				*p_payload_data_len = 0;
			}
		}
	}
}

int parse_wsn_packet(uint8_t *p_data,int data_len)
{
	uint8_t pkt_num = 0 ;
	uint8_t count = 0 ;
	uint16_t temp_count = 0 ;
	uint8_t *p_temp_pkt_hdr = NULL ;
	uint16_t skip_bytes = 0 ;
	uint16_t temp_pkt_len = 0 ;
	uint8_t temp_chk_val = 0 ;
	uint8_t cmd_data_len = 0 ;
	uint8_t cmd_id = 0 ;
	device_node_info_t *p_dev_node = NULL ;
	uint8_t cmdData_offset = 0 ;
	uint8_t cmdID_offset = 0 ;
	uint16_t pkt_src_addr =0 ;
	uint16_t pkt_dest_addr = 0 ;
	uint8_t encrypt_len = 0 ;
	uint8_t decrypt_counter[16] ;
	uint8_t pkt_time[4] ;
	uint8_t data2_len = ENCRYPT_DATA2_LEN ; //default len
	uint8_t chk_offset = 0 ;
	int ret_val =0 ;
	
	
	if(p_data == NULL || data_len <= 0)
	{
		debug("err input para when parse_wsn_packet \n");
		return -1 ;
	}

	p_temp_pkt_hdr = delimit_data_packet(p_data,data_len,&pkt_num) ;
	if(p_temp_pkt_hdr == NULL || pkt_num <= 0)
	{
		debug("not found wsn data in this frame,discard it now \n");
		return -1 ;
	}

	for(count=0;count<pkt_num;count++)
	{
		p_temp_pkt_hdr = find_pkt_hdr(p_data+temp_count,data_len-temp_count,&skip_bytes) ;
		//p_temp_pkt_hdr = &test_buf[0] ; //only for debug
		//debug("parse debug data now \n");
		if(p_temp_pkt_hdr == NULL)
		{
			debug("err not find pkt_hdr when parse_wsn_packet \n");
			return -1 ;
		}
		temp_pkt_len = p_temp_pkt_hdr[2] ; //skip sop+ver
		if(temp_pkt_len <= 0 || temp_pkt_len > 255)
		{
			debug("err pkt len \n");
			return -1 ;
		}
		cmd_data_len = temp_pkt_len - (2+2+1+1+1) ; //skip dest+src+cmd+chk+eop
		temp_chk_val = calc_mesh_cmd_chksum(p_temp_pkt_hdr,temp_pkt_len+3,cmd_data_len); //temp_pkt_len+3==total-pkt-len
		chk_offset = temp_pkt_len+3-2 ;
		if(chk_offset <= 0 || chk_offset >= 255)
		{
			debug("err chk_offset \n");
			return -1 ;
		}
		
		if( temp_chk_val == p_temp_pkt_hdr[temp_pkt_len+3-2] ) //chksum ok?
		{
			cmd_id = get_wsn_cmdID_fromPkt(p_temp_pkt_hdr,temp_pkt_len+3) ;
			if(cmd_id == WSN_ENCRYPT_DATA_PAK_MSGID)
			{
				//decrypt now
				pkt_src_addr = get_sourceAddr_fromPkt(p_temp_pkt_hdr,temp_pkt_len+3) ;
				if(pkt_src_addr == 0 || pkt_src_addr == 0xFFFF)
				{
					debug("err src_addr=0x%x \n",pkt_src_addr);
					return -1 ;
				}
				
				//del_expected_resp_cmd(pkt_src_addr,cmd_id) ;
				p_dev_node = query_device_info_by_deviceAddr(pkt_src_addr) ;
				debug("recved encrypt data-pak,decrypt now,dev_addr=0x%x \n",pkt_src_addr);
				if(p_dev_node != NULL)
				{
					debug("encrypt-dev encrypt_flag=%d \n",p_dev_node->device_info.dev_support_encrypt);
				}
				if(p_dev_node != NULL && p_dev_node->device_info.dev_support_encrypt)
				{
					#if 0
					cmdData_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
					cmdID_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN ;
					encrypt_len = temp_pkt_len -(WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN+WSN_CHK_LEN+WSN_CMD_END_LEN) - data2_len ; 
					memcpy(pkt_time,&p_temp_pkt_hdr[cmdData_offset],4) ; //pkt send time,4 bytes,bigEndian
					memset(wsn_decrypt_buf,0,sizeof(wsn_decrypt_buf));
					memset(wsn_decrypt_data_buf,0,sizeof(wsn_decrypt_data_buf)) ;
					memset(decrypt_counter,0,sizeof(decrypt_counter)) ;
					memcpy(decrypt_counter,&p_dev_node->device_info.session_key[16],12) ;
					memcpy(&decrypt_counter[12],pkt_time,4) ;
					AES_ctr128_decrypt(&p_temp_pkt_hdr[cmdData_offset],wsn_decrypt_buf,encrypt_len,p_dev_node->device_info.session_key,decrypt_counter);
					#endif
					encrypt_len = temp_pkt_len -(WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN+WSN_CHK_LEN+WSN_CMD_END_LEN) - data2_len -4; //-4 for skip timestamp
					if(encrypt_len <= 0 || encrypt_len >= 255)
					{
						debug("err encrypt_len=%d \n",encrypt_len);
						return -1 ;
					}
					memset(wsn_decrypt_buf,0,sizeof(wsn_decrypt_buf));
					memset(wsn_decrypt_data_buf,0,sizeof(wsn_decrypt_data_buf)) ;
					ret_val = decrypt_data_pak(p_temp_pkt_hdr,wsn_decrypt_buf,sizeof(wsn_decrypt_buf),p_dev_node); //del -1 
					if(ret_val != 0)
					{
						debug("dev=%s decrypt pak err,set dev offline and rebuild sk now \n",p_dev_node->device_info.dev_serialNum);
						set_dev_offline_status(p_dev_node) ;
						return -1 ;
					}

					memcpy(&wsn_decrypt_data_buf[0],&p_temp_pkt_hdr[0],7) ; //from sop to src_addr
					memcpy(&wsn_decrypt_data_buf[7],wsn_decrypt_buf,encrypt_len) ; // decrypt data,include cmdID+cmdData,we don't care chk and eop
					memcpy(&wsn_decrypt_data_buf[7+encrypt_len],&p_temp_pkt_hdr[chk_offset],2) ; //copy chk and eop

					//finish decrypt data,hand it now
					debug("finish decrypt data,handle it now \n");
					parse_wsn_data(wsn_decrypt_data_buf,7+encrypt_len+2) ; // 7+encrypt_len+2 == total_pkt_len,from sop to eop
				}
				else
				{
					debug("recved encrypt data,not found match dev-info or dev not support encrypt,discard it and send exchange_key req now \n") ;
					pkt_dest_addr = get_destAddr_fromPkt(p_temp_pkt_hdr,temp_pkt_len+3) ;
					if(pkt_dest_addr != 0xFFFF)
					{
						local_addr = pkt_dest_addr ; //maybe this is first pkt,and local_addr==0
					}
					build_and_send_exchangeKey_pak(pkt_src_addr) ;
				}
				
			}
			else
			{
				parse_wsn_data(p_temp_pkt_hdr,temp_pkt_len+3);
			}
		}
		else
		{
			debug("err chksum when parse_wsn_packet \n");
			return -1 ;
		}

		temp_count += skip_bytes+temp_pkt_len+3 ; //temp_pkt_len+3==total-pkt-len
		
	}

	return 0 ;
}

void check_dev_offline_status(void)
{
	device_node_info_t *p_node = NULL ;
	time_t time_val = 0 ;

	p_node = device_info_list_head.p_next_device ;

	while(p_node != NULL)
	{
		time(&time_val) ;
		if(time_val >= p_node->device_info.dev_offline_timer)
		{
			if((time_val-p_node->device_info.dev_offline_timer) >= (p_node->device_info.data_interval*MAX_DATA_TIMEOUT_NUM) )
			{
				debug("check_dev_offline_status dev->data_interval=%d, set dev:%s offline \n",p_node->device_info.data_interval,p_node->device_info.dev_serialNum);
				set_dev_offline_status(p_node) ;
			}
		}
		else
		{
			//maybe overflow,reset timer
			p_node->device_info.dev_offline_timer = time_val ;
		}

		p_node = p_node->p_next_device ;
	}


	return ;
}


void update_online_devNUM(void)
{
	device_node_info_t *p_node = NULL ;
	int online_cnt = 0 ;
	int offline_cnt = 0 ;
	
	p_node = device_info_list_head.p_next_device ;

	while(p_node != NULL)
	{
		if(p_node->device_info.dev_online_flag == 1)
		{
			online_cnt++ ;
		}
		else
		{
			
			offline_cnt++ ;
		}

		p_node = p_node->p_next_device ;
	}

	device_info_list_head.online_dev_count = online_cnt ;
	device_info_list_head.offline_dev_count = offline_cnt ;
	
	return ;
}

void set_dev_online_status(device_node_info_t *p_node)
{
	time_t time_val = 0 ;
	dev_diag_info_t temp_diag_info ;
	uint8_t need_set_online_flag = 0;
	
	if(p_node == NULL)
	{
		debug("err input para when set_dev_online_status \n");
		return  ;
	}

	debug("dev-sn=%s,dev-addr=0x%x,online now \n",p_node->device_info.dev_serialNum,p_node->device_info.device_addr);

	if(p_node->device_info.dev_online_flag == 0)
	{
		//only print one time
		debug_notice("dev-sn=%s,dev-addr=0x%x,online now \n",p_node->device_info.dev_serialNum,p_node->device_info.device_addr);
        if(p_node->device_info.echoview_flag !=1){		
		    need_set_online_flag = 1;
		}
	}

	#if 0
	if(p_node->device_info.new_dev_flag == 1)
	{
		//this is new-dev in the list
		device_info_list_head.online_dev_count++ ;
		//p_node->device_info.dev_offline_flag = 0 ;
		p_node->device_info.new_dev_flag = 0 ;
	}
	else
	{
		if(p_node->device_info.dev_offline_flag == 1)
		{
			if(device_info_list_head.offline_dev_count > 0)
			{
				device_info_list_head.offline_dev_count-- ;
			}
			
			device_info_list_head.online_dev_count++ ;
			//p_node->device_info.dev_offline_flag = 0 ;
		}
	}
	#endif

	p_node->data_pak_count++ ;
	//update dev-offline-timer
	time(&time_val) ;
	p_node->device_info.dev_offline_timer = time_val ;
	
	
	//update dev diaginfo
	memset(&temp_diag_info,0,sizeof(temp_diag_info));
	temp_diag_info.data_pak_flag = p_node->device_info.data_pak_recv_flag ;
	temp_diag_info.dev_addr = p_node->device_info.device_addr ;
	temp_diag_info.dev_info_flag = p_node->device_info.dev_info_recv_flag ;

	temp_diag_info.dev_online_flag = 1 ;

	memcpy(temp_diag_info.dev_sn,p_node->device_info.dev_serialNum,(sizeof(temp_diag_info.dev_sn)-1)) ;
	temp_diag_info.rssi_val = p_node->device_info.dev_rssi_val ;
	temp_diag_info.sen_format_flag = p_node->device_info.sen_fmt_recv_flag ;
	temp_diag_info.sen_limit_flag = p_node->device_info.sen_limit_recv_flag ;

	//dev not online or recved 5 data-paks
	if(p_node->device_info.dev_online_flag == 0 || (p_node->data_pak_count%5) == 0)
	{
        if((g_module_config.module_lora1_mesh0==1)&&p_node->device_info.echoview_flag ==1){
			if(p_node->device_info.dev_online_flag == 0){
                add_dev_to_echoview_array(p_node);  
			}  
	    }		
		//update online-dev num
		p_node->device_info.dev_online_flag = 1 ;
		p_node->device_info.dev_offline_flag = 0 ;
		update_online_devNUM();
		
		//update diaginfo in nvram
		nvram_update_dev_diaginfo(&temp_diag_info) ;

		//update online-dev-num in nvram
		nvram_update_online_devNum();
		if(need_set_online_flag==1){
            send_json_topology_toCloud();
		}
			
	}


	return ;
}

void set_dev_offline_status(device_node_info_t *p_node)
{
	dev_diag_info_t temp_diag_info ;
	time_t time_val = 0 ;
	
	if(p_node == NULL)
	{
		debug("err para when set_dev_offline_status \n");
		return ;
	}

	//only when exchange-key failed
	if(p_node->device_info.dev_serialNum[0] == 0)
	{
		//serial==NULL,this is secret dev,and decrypt err,we need to re-build-exchangeKey
		debug("maybe exchangeKey failed,del the dev from list now \n");
		del_deviceInfo_fromList_byAddr(p_node) ;
		return ;
	}

	debug("dev=%s offline now \n",p_node->device_info.dev_serialNum);
	debug_notice("dev=%s offline now \n",p_node->device_info.dev_serialNum);
// echoview
    if((g_module_config.module_lora1_mesh0==1)&&p_node->device_info.echoview_flag ==1){
        rm_dev_to_echoview_array(p_node);    
	}else{
		#if 0
		if(p_node->device_info.dev_offline_flag == 0)
		{
			device_info_list_head.offline_dev_count++ ;
			if(device_info_list_head.online_dev_count > 0)
			{
				device_info_list_head.online_dev_count-- ;
			}
		}
		#endif
		if(p_node->device_info.dev_online_flag ==1)
		{
			//send offline to cloud if dev has been online
			send_devOfflineData_toCloud(p_node) ;
			send_json_topology_toCloud();
		}

		//send offline modbus data
		if(get_modbus_ena())
		{
			memset(modbus_offlineData_buf,0,sizeof(modbus_offlineData_buf)) ;
			build_and_send_modbusOfflineData(p_node,modbus_offlineData_buf,sizeof(modbus_offlineData_buf)) ;
		}
	}
	p_node->device_info.dev_online_flag = 0 ;
	p_node->device_info.dev_offline_flag = 1 ;
	update_online_devNUM();

	//reset dev-offline-timer
	time(&time_val) ;
	p_node->device_info.dev_offline_timer = time_val ;

	#if 1
	//update dev diaginfo
	memset(&temp_diag_info,0,sizeof(temp_diag_info));
	temp_diag_info.data_pak_flag = p_node->device_info.data_pak_recv_flag ;
	temp_diag_info.dev_addr = p_node->device_info.device_addr ;
	temp_diag_info.dev_info_flag = p_node->device_info.dev_info_recv_flag ;

	temp_diag_info.dev_online_flag = 0 ;

	memcpy(temp_diag_info.dev_sn,p_node->device_info.dev_serialNum,(sizeof(temp_diag_info.dev_sn)-1)) ;
	temp_diag_info.rssi_val = 0 ; // reset rssi-val  //p_node->device_info.dev_rssi_val ;
	temp_diag_info.sen_format_flag = p_node->device_info.sen_fmt_recv_flag ;
	temp_diag_info.sen_limit_flag = p_node->device_info.sen_limit_recv_flag ;
	nvram_update_dev_diaginfo(&temp_diag_info) ;
	#endif

	//del diaginfo now
	//nvram_del_dev_diaginfo(p_node->device_info.dev_serialNum,strlen(p_node->device_info.dev_serialNum)) ;

	//update online-dev-num in nvram
	nvram_update_online_devNum();

	//handle cloud-cmd_list,send fail msg to cloud
	set_cloudCmd_result(CLOUD_CMD_FAILED) ;
	free_cloudCmdNode_byDevSn(p_node->device_info.dev_serialNum,strlen(p_node->device_info.dev_serialNum)) ;

	//del dev_alarm info in the alarm list if necessary
	check_and_renew_alarmList(0,p_node) ;
	check_and_renew_relayList(0,p_node) ;


	//handle groupAlarm cmdList when dev offline
	handle_offlineDev_groupAlarmCmdList(p_node) ;
	free_allAlarmList_inDev(p_node);

	//bak record file
    //bak_realDataRecord(p_node) ;
	//mv buff to SD
    //fxy

	uint8_t timenowstr[16] ;
	memset(timenowstr,0,sizeof(timenowstr));
	get_yearmonthday(timenowstr,sizeof(timenowstr));
  	uint8_t dev_sn[41];
    uint8_t a_buffer_filename[32]={0} ; //max 20+4+1
	uint8_t a_buffer_filename_escape[64]={0} ; //max 40+4+1
	memset(dev_sn,0,sizeof(dev_sn));
	transfer_dirChar_inSn(p_node->device_info.dev_serialNum,dev_sn,sizeof(dev_sn)) ;
    snprintf(a_buffer_filename_escape,sizeof(a_buffer_filename_escape),"%s.csv",dev_sn);
    snprintf(a_buffer_filename,sizeof(a_buffer_filename),"%s.csv",p_node->device_info.dev_serialNum);

    cp_csv_remainData_to_sd_and_del(a_buffer_filename,a_buffer_filename_escape,timenowstr);
    //end fxy


	//free waiting-cmd-list buf
	free_allCmdNode_inDev(p_node) ;

	//del dev_subData_node_info in the buf
	free_subSenData_node(p_node->device_info.dev_serialNum) ;

	//del_device_info_from_list(p_node) ;
	detach_device_info_from_list(p_node) ;
	insert_dev_into_offlineList(p_node);

	return ;
}

int wsn_recv_main_task(void)
{
	//uint8_t temp_wsn_data[MAX_WSN_BUF_LEN] ;
	int wsn_data_len = 0 ;
	int rmci_data_len = 0;
	int rmci_bytes_offset = 0;
	dev_diag_info_t temp_diaginfo ;
	nvram_dev_varname_sn_pair_t *p_pair = NULL ;
	int count = 0 ;
    g_echoview.id = ECHO_VIEW_ID;
	init_device_info_list();
	memset(wsn_recv_buf,0,sizeof(wsn_recv_buf)) ;
	memset(wsn_send_buf,0,sizeof(wsn_send_buf)) ;
	memset(&varname_sn_pair_head,0,sizeof(varname_sn_pair_head)) ;
	memset(&wsn_net_conf_data,0,sizeof(wsn_net_conf_data));
	init_devAlarm_list();
	memset(&dev_gps_data,0,sizeof(dev_gps_data));
	memset(&gw_gps_data,0,sizeof(dev_gps_data));

	//memset(&all_alarmRelay_list,0,sizeof(all_alarmRelay_list));

	//init modbus buf-pool
	init_modbus_bufPool();
	

	//init online_dev_num 0,and del diaginfo and varname_sn_pair,set total dev_node_num 0
	nvram_update_online_devNum(); // 0
	nvram_delete_dev_diaginfo_list();
	//nvram_del_varName_sn_pair_list() ;
	nvram_update_devNode_num(0);
	//////////////////////////////////////////
	

	

	//get wsn net conf data now
	nvram_get_devOffline_threshold(&wsn_net_conf_data.dev_offline_threshold) ;
	nvram_get_trigAlarm_conf(&wsn_net_conf_data.trig_alarm) ;
	nvram_get_trigRelay_conf(&wsn_net_conf_data.trig_relay) ;
	nvram_get_clearRemoteAlarm_conf(&wsn_net_conf_data.clear_remoteAlarmRelay);
	nvram_get_gw_gpsData();
	nvram_get_gwSn();
	debug("one dev mem=%d \n",sizeof(device_node_info_t));
	
	while(1)
	{
		if(get_uart_config_lock() != 0 || get_serial_fd() < 0)
		{
			debug("uart busy now \n") ;
			usleep(500*1000) ; //500 ms
			continue ;
		}


		handle_cloud_cmdList();
		//check_and_reSend_timeout_cmd();
		check_dev_offline_status();
		check_shareKey_change() ;
		
		if(g_module_config.module_lora1_mesh0==0){
			//memset(temp_wsn_data,0,sizeof(temp_wsn_data)) ;
			uart_recv_uint8_line(get_serial_fd(),wsn_recv_buf,sizeof(wsn_recv_buf),&wsn_data_len) ;

			if(wsn_data_len > 0)
			{
				debug("recv wsn data len: %d \n",wsn_data_len) ;
				parse_wsn_packet(wsn_recv_buf,wsn_data_len);
			}
			else
			{
				//debug("recv no data on serial fd=0x%x \n",get_serial_fd());
			}
		}else if(g_module_config.module_lora1_mesh0==1){
		//memset(temp_wsn_data,0,sizeof(temp_wsn_data)) ;
			uart_recv_uint8_line(get_serial_fd(),rmci_recv_buf,sizeof(rmci_recv_buf),&rmci_data_len) ;
	        rmci_bytes_offset = 0;
			if(rmci_data_len > 0)
			{
				debug("recv rmci data len: %d \n",rmci_data_len) ;
				do{
					rm_rmci_hdr_packet(rmci_recv_buf,&rmci_bytes_offset,rmci_data_len,wsn_recv_buf,&wsn_data_len);  //rm rmci and get wsn
					if(wsn_data_len > 0)
					{
						debug("recv wsn data len: %d  %x %x %x %x %x\n",wsn_data_len,wsn_recv_buf[0],wsn_recv_buf[1],wsn_recv_buf[2],wsn_recv_buf[3],wsn_recv_buf[4]) ;
						parse_wsn_packet(wsn_recv_buf,wsn_data_len);
					}
					else
					{
						//debug("recv no data on serial fd=0x%x \n",get_serial_fd());
					}
				}while(wsn_data_len>0);
			}
		}
		usleep(20*1000) ;
	}

	return 0 ;
}



//v1 dev not support sub-pak,not support remoteAlarm
//wsn cmd v1 funcs
int check_and_set_remoteAlarmInfo_v1(device_node_info_t *p_cur_node,sensor_data_t *p_cur_sensor_data)
{
	sensor_data_t *p_temp_sen_data = NULL ;
	uint8_t cur_dev_alarm_info = 0 ;
	sensor_data_format_t *p_temp_sen_fmt = NULL ;
	uint8_t temp_sen_name[32] ;
	time_t time_val = 0 ;
	
	if(p_cur_node == NULL || p_cur_sensor_data == NULL)
	{
		debug("err para when check_and_set_remoteAlarmInfo \n");
		return -1 ;
	}

	#if 0
	//v1 not support app_alarm
	if( (comm_dev_data.app_alarm &0x07) != 0)
	{
		cur_dev_alarm_info |= 0x04 ; //bit2,fault alarm
	}
	#endif

	debug("begin to handle v1_devself =%s alarmInfo \n",p_cur_node->device_info.dev_serialNum);

	p_temp_sen_data = p_cur_sensor_data ;
	while(p_temp_sen_data != NULL)
	{
		if(p_temp_sen_data->sensor_err != 0)
		{
			if( (p_temp_sen_data->sensor_err & 0x0B) != 0) //bit0,bit1,bit3
			{
				//over,max,high,highHigh,v1 only include over,max,high,bit0-over,bit1-max,bit3-high
				cur_dev_alarm_info |= 0x01 ; // bit0,high alarm
				p_temp_sen_fmt = query_sensor_format_data(p_cur_node->device_info.device_addr,p_temp_sen_data->sensor_index) ;
				if(p_temp_sen_fmt != NULL)
				{
					memset(temp_sen_name,0,sizeof(temp_sen_name));
					get_sensorName_bysenIDandSubID(p_temp_sen_fmt->sensor_id,p_temp_sen_fmt->sensor_sub_id,temp_sen_name,(sizeof(temp_sen_name)-1) ) ;

					if(strcmp(temp_sen_name,"O2") != 0)
					{
						//not O2 sensor,triger low alarm
						cur_dev_alarm_info |= 0x02 ; //bit1,low alarm
					}
				}
			}
			else if( (p_temp_sen_data->sensor_err & 0x10) != 0) //bit4
			{
				//low,lowLow,v1 only include low,bit4-low
				cur_dev_alarm_info |= 0x02 ; //bit1,low alarm
			}
			else
			{
				//other sensor_err
				cur_dev_alarm_info |= 0x04 ; //bit2,fault alarm
			}
			
		}
		
		p_temp_sen_data = p_temp_sen_data->p_next_sensor_data ;
	}


	//append into current dev remote alarm info
	p_cur_node->device_info.dev_alarm_info |= cur_dev_alarm_info ;

	//save dev-local alarm-info
	p_cur_node->device_info.dev_local_alarm_info = cur_dev_alarm_info ;

	//append into current_all_alarm
	if(cur_dev_alarm_info != 0)
	{
		//append new alarm,and renew the alarm timer
		device_info_list_head.current_all_alarm |= cur_dev_alarm_info ; // only bit0-bit2
	
		//renew remote_alarm timer now
		time(&time_val) ;
		device_info_list_head.cur_alarm_timer = time_val ;
	}

	//maybe append dev into alarm list,or clear alarm status here
	check_and_renew_alarmList(cur_dev_alarm_info,p_cur_node) ;

	debug("current v1_devselsf alarm info=0x%x,all_alarm=0x%x \n",cur_dev_alarm_info,device_info_list_head.current_all_alarm);

	return 0 ;
}

int check_and_set_relayInfo_v1(device_node_info_t *p_alarm_node,sensor_data_t *p_alarm_sensor_data)
{
	sensor_data_t *p_temp_sen_data = NULL ;
	//uint8_t cur_dev_alarm_info = 0 ;
	sensor_data_format_t *p_temp_sen_fmt = NULL ;
	uint8_t temp_sen_name[32] ;
	uint8_t relay4_sen_name[10] = "LEL" ;
	uint8_t relay5_sen_name[10] = "H2S" ;
	uint8_t cur_dev_relay_info = 0 ;
	
	if(p_alarm_node == NULL || p_alarm_sensor_data == NULL)
	{
		debug("err para when check_and_set_relayInfo \n");
		return -1 ;
	}

	#if 0
	//v1 not support app_alarm
	if((comm_dev_data.app_alarm &0x07) != 0)
	{
		device_info_list_head.relay_info |= 0x01 ; //relay1,bit0
	}
	#endif
	debug("begin to handle v1_devself =%s relayInfo \n",p_alarm_node->device_info.dev_serialNum);

	p_temp_sen_data = p_alarm_sensor_data ;
	while(p_temp_sen_data != NULL)
	{
		
		if(p_temp_sen_data->sensor_err != 0)
		{
			// set relay1,any alarm
			//device_info_list_head.relay_info |= 0x01 ; //relay1,bit0
			cur_dev_relay_info |= 0x01 ; //relay1,bit0
		

			//relay2,low,lowLow,v1 only include low
			if( (p_temp_sen_data->sensor_err & 0x10) != 0) //bit4
			{
				//device_info_list_head.relay_info |= 0x02 ; //relay2,bit1
				cur_dev_relay_info |= 0x02 ; //relay2,bit1
			}

			//relay3,max,over,high,highHigh,v1 only include over,max,high
			if( (p_temp_sen_data->sensor_err & 0x0B) != 0) //bit0,bit1,bit3
			{
				//set relay3
				//device_info_list_head.relay_info |= 0x04 ; //relay3,bit2
				cur_dev_relay_info |= 0x04 ; //relay3,bit2

				//it is lel,h2s or not?
				p_temp_sen_fmt = query_sensor_format_data(p_alarm_node->device_info.device_addr,p_temp_sen_data->sensor_index) ;
				if(p_temp_sen_fmt != NULL)
				{
					memset(temp_sen_name,0,sizeof(temp_sen_name));
					get_sensorName_bysenIDandSubID(p_temp_sen_fmt->sensor_id,p_temp_sen_fmt->sensor_sub_id,temp_sen_name,(sizeof(temp_sen_name)-1) ) ;
					if(strcmp(temp_sen_name,relay4_sen_name) == 0)
					{
						//match and set relay4,bit3
						//device_info_list_head.relay_info |= 0x08 ; //relay4,bit3
						cur_dev_relay_info |= 0x08 ; //relay4,bit3
					}
					else if(strcmp(temp_sen_name,relay5_sen_name) == 0)
					{
						//match and set relay5,bit4
						//device_info_list_head.relay_info |= 0x10 ; //relay5,bit4
						cur_dev_relay_info |= 0x10 ; //relay5,bit4
					}
				}
			}
			
		}

		p_temp_sen_data = p_temp_sen_data->p_next_sensor_data ;
	}

	//append into current_all_relay
	if(cur_dev_relay_info != 0)
	{
		//append new alarm,and renew the alarm timer
		device_info_list_head.relay_info |= cur_dev_relay_info ; // only bit0-bit4,relay1-relay5

		#if 0
		//renew remote_alarm timer now
		time(&time_val) ;
		device_info_list_head.cur_alarm_timer = time_val ;
		#endif
	}

	//maybe append dev into alarm list,or clear alarm status here
	check_and_renew_relayList(cur_dev_relay_info,p_alarm_node) ;

	debug("current devslef relay_info=0x%x,all_relay_info=0x%x \n",cur_dev_relay_info,device_info_list_head.relay_info);

	return 0 ;
}


int get_comm_devData_v1(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_data_offset = 0 ;
	
	if(p_data == NULL || data_len <= 0)
	{
		debug("err para when get_comm_devData_v1 \n");
		return -1 ;
	}

	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN + WSN_CMD_ID_LEN ;
	comm_dev_data.power_cfg = p_data[cmd_data_offset+0] ;
	comm_dev_data.cfg_set_cnt = p_data[cmd_data_offset+1] ;
	comm_dev_data.alarm_set_cnt =  p_data[cmd_data_offset+2] ;
	comm_dev_data.unit_info = p_data[cmd_data_offset+3] ; //cmd_v1 unit status
	comm_dev_data.unit_err = p_data[cmd_data_offset+4] ;
	comm_dev_data.power_percent = p_data[cmd_data_offset+5] ;

	return 0 ;
}


//v1-dev not support remote-alarm
int parse_wsn_dataPakV1(uint8_t *p_data,uint8_t data_len)
{
	uint8_t cmd_id = 0 ;
	uint8_t hwinfo = 0 ;
	uint8_t sensor_num = 0 ;
	uint8_t cfgSet_cnt = 0 ;
	uint8_t alarmSet_cnt = 0 ;
	uint16_t pkt_dest_addr = 0 ; //for us
	uint16_t pkt_source_addr = 0 ;

	device_node_info_t *p_temp_node = NULL,*p_node_info_by_addr =  NULL;
	sensor_data_format_t *p_sensor_format = NULL ;
	sensor_data_t *p_sensor_data = NULL ;
	sensor_data_t *p_tmp_sensor_data = NULL ;
	int count = 0 ;
	int temp_count = 0 ;
	int serial_fd = 0 ;
	int ret_val = 0 ;
	uint8_t pkt_sen_index = 0 ;
	uint8_t pkt_senID = 0 ;
	uint8_t next_sen_index = ILLEGAL_SEN_INDEX ;
	uint8_t cmd_data[32] ;
	uint8_t cmd_data_offset = 0 ;
	uint8_t limit_data_len = 0 ;
	//uint8_t buf_sen_index = 0 ;
	//uint8_t buf_sen_data_len = 0 ;
	uint16_t mesh_addr = 0 ;
	sint8_t rssi_val = 0 ;

	int sint_sensor_data = 0 ;
	uint32_t uint_sensor_data = 0 ;
	uint8_t sensor_data_count = 0 ;
	uint8_t sensor_df = 0 ; //Divide Factor
	uint8_t sensor_dp = 0 ; // Decimal Point,digits after the decimal point
	uint8_t gps_zigbee_data_flag = 0 ;
	
	if(p_data == NULL || data_len <= 0)
	{
		debug("err para when parse_wsn_dataPakV1 \n");
		return -1 ;
	}

	memset(cmd_data,0,sizeof(cmd_data)) ;
	cmd_id = get_wsn_cmdID_fromPkt(p_data,data_len);
	if(cmd_id != WSN_DATA_PAK_V1_MSGID)
	{
		debug("cmdID err when parse_wsn_dataPakV1 \n");
		return -1 ;
	}
	
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	pkt_source_addr = get_sourceAddr_fromPkt(p_data,data_len) ;
	if(pkt_source_addr == 0xFFFF)
	{
		debug("source addr==0xFFFF,err!!! \n");
		return 0 ;
	}
	
	pkt_dest_addr = get_destAddr_fromPkt(p_data,data_len) ;

	init_dev_data_buf();
	get_comm_devData_v1(p_data,data_len) ;
	sensor_num = (comm_dev_data.unit_info >> 2) & 0x0F ; //bit2-bit5,sensor num in this data_pak 
	cfgSet_cnt = comm_dev_data.cfg_set_cnt ;
			
	p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
	if(p_temp_node == NULL || (p_temp_node->device_info.cfgSet_cnt != cfgSet_cnt) )
	{
		if(p_temp_node != NULL)
		{
			debug("cfgSetCnt change,reget dev-info and discard this data-pak now \n") ;
			set_dev_offline_status(p_temp_node) ;
		}
		else
		{
			//not found match device info in buf,discard this packet and send get_device_info now
			debug("not found devinfo in our buf,get dev-info and discard this data-pak now \n") ;
		}
		
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
		if(count > 0)
		{
			send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
		}
		return 0 ;
	}

	//begin to handle data in this data_pak
	#if 1
	//check alarmSet_cnt
	if(p_temp_node->device_info.active_sensor_num > 0 && comm_dev_data.alarm_set_cnt != p_temp_node->device_info.alarmSet_cnt)
	{
		//reset limit
		reset_dev_limitData(p_temp_node);
		
		//reget sensor limit data now
		debug("alarmSet_cnt change,old_alarmSet=%d,pkt_alarmSet=%d,reget sensor limit data \n",p_temp_node->device_info.alarmSet_cnt,comm_dev_data.alarm_set_cnt);
		p_temp_node->device_info.alarmSet_cnt = comm_dev_data.alarm_set_cnt; //fxy 20240327 mod bug repeated sending limit requet
		cmd_data[0] = p_temp_node->device_info.active_sen_index[0] ; //first sen_index
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_LIMIT_V1_CMDID,cmd_data,1,local_addr,pkt_source_addr);
		if(count > 0)
		{
			//send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
			send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_LIMIT_V1_CMDID,p_temp_node);
		}

		//return 0; 
	}
	#endif

	//check senFmt and limit recv ok?
	if(p_temp_node->device_info.active_sensor_num > 0)
	{
		if(p_temp_node->device_info.sen_fmt_recv_flag != 1)
		{
			debug("dev=%s,senFmt not recved,get it now \n",p_temp_node->device_info.dev_serialNum) ;
			//not recved senFmt,get it now
			count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_NAMEFORMAT_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
			if(count > 0)
			{
				//send_cmd_toRemote(wsn_send_buf,count);
				//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
				send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_NAMEFORMAT_V1_CMDID,p_temp_node) ;
			}

			return 0 ;
		}

		//not care limit data,get limit data only at free time
		#if 0
		//not recved sen-limit-data over ? get it now
		if(p_temp_node->device_info.sen_limit_recv_flag != 1)
		{
			debug("dev=%s,senLimit not recved over,get it now \n",p_temp_node->device_info.dev_serialNum) ;
			//cmd_data[0] = p_temp_node->device_info.active_sen_index[0] ; 
			next_sen_index = query_next_senLmt_index(p_temp_node) ;
			if(next_sen_index != ILLEGAL_SEN_INDEX)
			{
				cmd_data[0] = next_sen_index ;
				count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_LIMIT_V1_CMDID,cmd_data,1,local_addr,pkt_source_addr);
				if(count > 0)
				{
					//send_cmd_toRemote(wsn_send_buf,count);
					//send_expectedResp_cmd_toRemote(wsn_send_buf,send_cnt) ;
					send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_LIMIT_V1_CMDID,p_temp_node) ;
				}
			}

			return 0 ;
		}
		#endif
	}

	// set dev-online status
	p_temp_node->device_info.data_pak_recv_flag = 1 ;
	debug("set dev online when recved data-pak,dev=%s \n",p_temp_node->device_info.dev_serialNum);
	set_dev_online_status(p_temp_node) ;
	
	transfer_data_to_echoview(p_data,data_len);  //fxy echoview
	
	count = cmd_data_offset ;

	count += 6 ; //skip hdr 6 bytes

	//handle sensor data now
	if(sensor_num > 0 && count < data_len)
	{
		//handle sendor data
				
		debug("handle sensor data now \n");
		#if 0
		printf("sensor-data: \n");
		for(temp_count=0;temp_count<sensor_num*ONE_SENSOR_DATA_LEN;temp_count++)
		{
			printf("%02x ",p_data[count+temp_count]); //+8 to skip the hdr-8 bytes in the cmd-data
		}
		printf("\n");
		#endif


		//free old sensor-data if necessory
		if(p_dev_sensorData_head != NULL)
		{
			free_dev_sensorData_memList() ;
		}
				
		for(temp_count=0;temp_count<sensor_num;temp_count++)
		{
			//init reading value
			sint_sensor_data = 0 ;
			uint_sensor_data = 0 ;
			
			//handle every sensor data now
			#if 0
			pkt_senID = p_data[count]; // use sen_index?maybe same senID in much sen_data 
			p_sensor_format = query_sensor_format_bySensorID(pkt_source_addr,pkt_senID) ;
			#else
			pkt_sen_index = p_temp_node->device_info.active_sen_index[temp_count] ;
			p_sensor_format = query_sensor_format_data(pkt_source_addr,pkt_sen_index) ;
			#endif
			
			if(p_sensor_format == NULL)
			{
				debug("not found match sen_index format data in the buf ,discard this data-pak and re_get dev_info now\n");
				count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_NAMEFORMAT_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
				if(count > 0)
				{
					//send_cmd_toRemote(wsn_send_buf,count);
					//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
					send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_NAMEFORMAT_V1_CMDID,p_temp_node) ;
				}

				init_dev_data_buf();
				free_dev_sensorData_memList();
				return -1 ;
			}

			p_sensor_data = malloc(sizeof(sensor_data_t));
			if(p_sensor_data == NULL)
			{
				debug("err malloc sensor-data buf when parse_wsn_data \n");
				free_dev_sensorData_memList();
				init_dev_data_buf();
				return -1 ;
				//break ;
			}
			memset(p_sensor_data,0,sizeof(sensor_data_t)) ;
			p_sensor_data->total_sensor = sensor_num ;
			p_sensor_data->sensor_index = p_sensor_format->sensor_index ;
			p_sensor_data->sensor_err = p_data[count+1];
			if(p_sensor_format->sensor_data_len <= 4) //sensor-data len in 1,2,4
			{
				memcpy(p_sensor_data->sensor_data,&p_data[count+2],p_sensor_format->sensor_data_len);
			}
			else
			{
				debug("sensor index=%d,sensor-data len=%d err \n",pkt_sen_index,p_sensor_format->sensor_data_len) ;
				//maybe we need to reget senfmt and limit now
				free_sensor_format_data_mem(p_temp_node->p_sendor_format_data_head) ;
				p_temp_node->p_sendor_format_data_head = NULL ;
				p_temp_node->device_info.sen_fmt_recv_flag = 0 ;
				p_temp_node->device_info.sen_fmt_recvNO = 0 ;
				p_temp_node->device_info.sen_limit_recv_flag = 0 ;
				p_temp_node->device_info.sen_limit_recvNum = 0 ;
				///////////////////////////////////////////////
				free_dev_sensorData_memList();
				init_dev_data_buf();
				free(p_sensor_data) ;
				//send get dev now
				count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
				if(count > 0)
				{
					send_cmd_toRemote(wsn_send_buf,count);
					//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
				}
				/////////////////////
				return -1 ;
				//break ;
			}
			//caculate sensor_data,formula :(float)( (int)( ( (float) data / 10^DF) + 0.5) ) / 10^DP 
			//sensor_dp = p_sensor_data->sensor_dp ; //calc dp and df when handle sen_fmt_data
			//sensor_df = p_sensor_data->sensor_df ;
			sensor_dp = p_sensor_format->sensor_dp ;
			sensor_df = p_sensor_format->sensor_df ;

			//maybe overflow if save uint into int
			if( 1 ) // cmd_v1 ,only use sint
			{
				for(sensor_data_count=0;sensor_data_count<p_sensor_format->sensor_data_len;sensor_data_count++)
				{
					sint_sensor_data += (p_sensor_data->sensor_data[sensor_data_count]) << ((p_sensor_format->sensor_data_len-1-sensor_data_count)*8) ;
				}
				//calc sensor real data
				p_sensor_data->sensor_real_data = ( (int)( (float) ( sint_sensor_data/pow(10,sensor_df))+0.5) )/pow(10,sensor_dp) ;
			}
			else
			{
				for(sensor_data_count=0;sensor_data_count<p_sensor_format->sensor_data_len;sensor_data_count++)
				{
					uint_sensor_data += (p_sensor_data->sensor_data[sensor_data_count]) << ((p_sensor_format->sensor_data_len-1-sensor_data_count)*8) ;
				}
				//calc sensor real data
				p_sensor_data->sensor_real_data = ( (unsigned int)( (float) ( uint_sensor_data/pow(10,sensor_df))+0.5) )/pow(10,sensor_dp) ;
			}

			//insert into sensor-data list
			insert_dev_sensor_data(p_sensor_data) ;
			debug("sensor_index=%d,sen_real_data=%f,data[0]=0x%x,data[1]=0x%x \n",p_sensor_data->sensor_index,p_sensor_data->sensor_real_data,
																							p_sensor_data->sensor_data[0],p_sensor_data->sensor_data[1]);
					
					
			count += p_sensor_format->sensor_data_len + 2 ; //+2 to skip sen_ID and sen_error
		
		}


	}

	//handle GPS data
	gps_zigbee_data_flag = comm_dev_data.unit_info & 0x03 ; //bit0-bit1
	if(gps_zigbee_data_flag == 1)
	{
		//handle gps data,little endian
		debug("handle GPS_DATA now,sizeof(float)=%d \n",sizeof(float));
		dev_gps_data.new_data_flag = 1 ; //fxy
		//dev_gps_data.x_val = *((float *)(p_data+count)) ;
		//dev_gps_data.y_val = *((float *)(p_data+count+4)) ;
                memcpy(&dev_gps_data.x_val,p_data+count,4);
		memcpy(&dev_gps_data.y_val,p_data+count+4,4);

		dev_gps_data.y_val = dev_gps_data.y_val*180/3.14159265; //fxy
		dev_gps_data.x_val = dev_gps_data.x_val*180/3.14159265; //fxy
		count += GPS_DATA_LEN ;
	}

	//handle zigbee data
	if(gps_zigbee_data_flag == 2)
	{
		//handle zigbee data
		debug("handle zigbee data now \n");
		dev_zigbee_data.new_data_flag = 1 ;
		//rssi_val = p_data[count] ;
		for(temp_count=0;temp_count<1;temp_count++)  //fxy 3->1
		{
			if(p_data[count] > 0) //siganl > 0,save zigbee data
			{
				p_temp_node->device_info.dev_rssi_val = p_data[count] ;
				dev_zigbee_data.rcm_signal = p_data[count] ;
				dev_zigbee_data.rcm_addr = (p_data[count+1]<<8) | p_data[count+2] ;
                if(p_temp_node->device_info.parent_device_addr!=dev_zigbee_data.rcm_addr){
					p_temp_node->device_info.parent_device_addr = dev_zigbee_data.rcm_addr;
					p_node_info_by_addr = query_device_info_by_deviceAddr(dev_zigbee_data.rcm_addr) ;
					if(p_node_info_by_addr != NULL)
					{
						strlcpy(p_temp_node->device_info.parent_dev_serialNum,p_node_info_by_addr->device_info.dev_serialNum, \
						    sizeof(p_temp_node->device_info.parent_dev_serialNum));
						debug("parent addr=%d sn %s \n",p_temp_node->device_info.parent_device_addr,p_temp_node->device_info.parent_dev_serialNum);
					}
				}				
			}

			count += ZIGBEE_DATA_LEN ; // 3 bytes
		}
	}

	//handle alarm if necessary

	p_tmp_sensor_data = p_dev_sensorData_head ;
	
	//check_and_triger_remoteAlarm(p_temp_node,p_tmp_sensor_data,p_temp_node->device_info.dev_alarm_info) ;
	check_and_set_remoteAlarmInfo_v1(p_temp_node,p_tmp_sensor_data);
	check_and_set_relayInfo_v1(p_temp_node,p_tmp_sensor_data); 

	debug("finish handling data,transfer data to remote manage-system now \n");

	//save real data record
	debug("begin to save_record_data \n");
	save_realDataRecord(p_temp_node) ;
			
	//begin to build real-data-json packet and send to cloud
	#if 1
	build_json_real_data(p_temp_node) ;
	send_json_realData_toCloud(p_temp_node);
	#endif

	//save dev-data into modbus tab
	if(get_modbus_ena())
	{
		memset(modbus_data_buf,0,sizeof(modbus_data_buf)) ;
		build_and_send_modbusData(p_temp_node,modbus_data_buf,sizeof(modbus_data_buf)) ;
	}
	//finish handle ,free sensor_data mem now
	if(p_dev_sensorData_head != NULL)
	{
		free_dev_sensorData_memList() ;
	}

	return 0 ;
}


int copy_deviceInfo_v1_toBuf(device_node_info_t *p_buf,uint8_t *p_wsn_data)
{
	int offset = 0 ;
	int cmd_data_offset = 0 ;
	time_t time_val = 0 ;
	
	if(p_buf == NULL || p_wsn_data == NULL)
	{
		debug("err input para when copy_device_info_toBuf \n");
		return -1 ;
	}

	p_buf->device_info.cmd_ver = WSN_CMD_VER1 ;

	//skip from sop to cmdID
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	
	//instrumentID
	p_buf->device_info.instr_model_id = p_wsn_data[offset] ; //instrumentID
	offset += 1 ;

	//serial_num
	memcpy(p_buf->device_info.dev_serialNum,p_wsn_data+offset,10) ;
	offset += 10 ;

	//fw_ver
	memcpy(p_buf->device_info.dev_fw_ver,p_wsn_data+offset,6) ;
	offset += 6 ;

	//powerCfg
	p_buf->device_info.powerCfg = p_wsn_data[offset] ;
	offset += 1 ;

	//data_interval,second
	p_buf->device_info.data_interval = (p_wsn_data[offset] << 8) | p_wsn_data[offset+1] ;
	offset += 2 ;
	if(p_buf->device_info.data_interval <= 0)
	{
		p_buf->device_info.data_interval = 60 ; //default interval 60s
	}

	//skt_mask
	p_buf->device_info.skt_mask = (p_wsn_data[offset] << 8) | p_wsn_data[offset+1] ;
	offset += 2 ;

	//sensor mask,one sensor match one bit,max 16 sensors in one device
	p_buf->device_info.sensor_mask = (p_wsn_data[offset] << 8) | p_wsn_data[offset+1] ;
	//p_buf->device_info.sensor_mask = 0x3F ; //only for debug!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	offset += 2 ;
	p_buf->device_info.active_sensor_num = calc_sensorNum_accordingToMask(p_buf->device_info.sensor_mask);
	debug("sen_mask =0x%02x,active_sen_num = %d \n",p_buf->device_info.sensor_mask,p_buf->device_info.active_sensor_num);


	//reset dev offline-timer
	debug("parse dev-info dev:%s ,update offline timer now \n",p_buf->device_info.dev_serialNum);
	time(&time_val) ;
	p_buf->device_info.dev_offline_timer = time_val ;
	p_buf->device_info.dev_info_recv_time = time_val ; // add for dev_v1 bug

	return 0 ;	
}



int parse_wsn_devInfo_v1(uint8_t *p_data,uint8_t data_len)
{
	device_node_info_t node_info ;
	device_node_info_t *p_temp_node = NULL ;
	uint16_t pkt_dest_addr = 0 ; //for us
	uint16_t pkt_source_addr = 0 ;
	uint8_t count = 0 ;
	time_t time_val = 0 ;
	uint8_t temp_chk_val = 0;
	
	if(p_data == NULL || data_len <= 0)
	{
		debug("err para when parse_wsn_devInfo_v1 \n");
		return -1 ;
	}

	debug("recved GET_DEVINFO_V1_RESP,handle it now \n");
	memset(&node_info,0,sizeof(device_node_info_t)) ;

	pkt_source_addr = get_sourceAddr_fromPkt(p_data,data_len) ;
	pkt_dest_addr = get_destAddr_fromPkt(p_data,data_len) ;

	node_info.device_info.dev_info_recv_flag = 1 ;

	
	//set dev-addr
	node_info.device_info.device_addr = get_sourceAddr_fromPkt(p_data,data_len) ;
	copy_deviceInfo_v1_toBuf(&node_info,p_data);

	//check dev-info recv_time
	p_temp_node = query_device_info_by_serialNum(node_info.device_info.dev_serialNum) ;
	if(p_temp_node != NULL )
	{
		// v1_dev_info not include cfgSet_cnt
		if(node_info.device_info.dev_info_recv_time >= p_temp_node->device_info.dev_info_recv_time &&
			(node_info.device_info.dev_info_recv_time-p_temp_node->device_info.dev_info_recv_time) <= 120)
		{
			debug("dev_info recv_time too short ,<=120s ,discard this dev-info pak now \n");
			return -1 ;
		}
	}
	
	set_senIndex_inDevInfo(&node_info);
	//check dev-info in offline-list
	p_temp_node = query_offlineDev_info_by_serialNum(node_info.device_info.dev_serialNum);
	if(p_temp_node != NULL )
	{
		if(p_temp_node->device_info.cfgSet_cnt == node_info.device_info.cfgSet_cnt)
		{
			//copy the dev-info,fmt-info,limit-info from offline list
			debug("found dev-info in offline list,cfg match,copy now \n");
			p_temp_node->device_info.device_addr = node_info.device_info.device_addr ; //used new addr,maybe addr changed
			memcpy(&node_info,p_temp_node,sizeof(node_info)) ;
			if(node_info.sensor_format_buf_len[0]>2 &&node_info.sensor_format_buf_len[0]<=sizeof(node_info.sensor_format_buf[0])){
				node_info.sensor_format_buf[0][5] = (node_info.device_info.device_addr &0xFF00)>8;
				node_info.sensor_format_buf[0][6] = node_info.device_info.device_addr&0x00FF;
				temp_chk_val = calc_cmd_chksum(node_info.sensor_format_buf[0],node_info.sensor_format_buf_len[0]-2); //temp_pkt_len+3==total-pkt-len
				node_info.sensor_format_buf[0][node_info.sensor_format_buf_len[0]-2]=temp_chk_val;
			}
			if(node_info.sensor_limit_buf_len>2 &&node_info.sensor_limit_buf_len<=sizeof(node_info.sensor_limit_buf)){
				node_info.sensor_limit_buf[5] = (node_info.device_info.device_addr &0xFF00)>8; 
				node_info.sensor_limit_buf[6] = node_info.device_info.device_addr&0x00FF;
				temp_chk_val = calc_cmd_chksum(node_info.sensor_limit_buf,node_info.sensor_limit_buf_len-2); //temp_pkt_len+3==total-pkt-len
				node_info.sensor_limit_buf[node_info.sensor_limit_buf_len-2]=temp_chk_val;
			}

			del_deviceNode_from_offlineList(p_temp_node) ; // not free senFmt mem,used by online-dev
		}
		else
		{
			//cfg not match,free the dev-info in the offline-list
			del_DevFmtInfo_from_offlineList(p_temp_node) ;
		}
	}
	insert_device_info_to_list(&node_info) ;
	p_temp_node = query_device_info_by_serialNum(node_info.device_info.dev_serialNum) ;
	if(p_temp_node == NULL)
	{
		debug("insert dev=%s failed \n",node_info.device_info.dev_serialNum);
		return -1 ;
	}
	
	if(node_info.device_info.active_sensor_num > 0 && p_temp_node->device_info.sen_fmt_recv_flag == 0) //maybe we copy dev-info from offline list
	{
		//send GET_SEN_NAMEFORMAT_V1_CMDID now
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_NAMEFORMAT_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
		if(count > 0)
		{
			//send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
			send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_NAMEFORMAT_V1_CMDID,p_temp_node) ;
		}
		
	}
	else
	{
		//p_temp_node = query_device_info_by_serialNum(node_info.device_info.dev_serialNum) ;
		//we need send dev_chk permission and device_cfg to cloud or not? if no sensor in this dev
		debug("active sensor num=%d \n",p_temp_node->device_info.active_sensor_num); 
		debug("update node config data to cloud server now \n");
		build_json_config_data(p_temp_node) ;
		send_json_confData_toCloud(p_temp_node);
		if(p_temp_node->p_devInfo_cmd != NULL)  //fix bug repeat send cmd
		{
			free_specifiedCmd_inDevCmdList(p_temp_node->p_devInfo_cmd);
			p_temp_node->p_devInfo_cmd = NULL ;
		}			

	}
			

	return 0 ;
}


int parse_wsn_senNameFmt_v1(uint8_t *p_data,uint8_t data_len)
{
	sensor_data_format_t *p_sensor_format_head = NULL ;
	sensor_data_format_t *p_temp_sensor_fmt = NULL ;
	sensor_data_format_t *p_prev_sensor = NULL ;
	device_node_info_t *p_dev_node = NULL ;
	uint16_t pkt_source_addr = 0xFFFF ;
	uint16_t pkt_dest_addr = 0xFFFF ;
	uint8_t pkt_cfgSetCnt = 0 ;
	uint8_t active_sen_num = 0 ;
	uint8_t pkt_first_sen_index = 0 ;
	uint8_t sen_fmt_data_num = 0 ;
	uint8_t temp_sen_data_num = 0 ;
	uint8_t count = 0 ;
	uint8_t cmd_data_offset = 0 ;
	uint8_t offset = 0 ;
	uint8_t temp_sen_data_len = 0 ;
	int ret_val = 0 ;
	uint8_t cmd_data[32] ;
	uint8_t sen_fmt_recvNO = 0 ;
	

	if(p_data == NULL || data_len <= 0 )
	{
		debug("err input para when parse_wsn_senNameFmt_v1 \n") ;
		return -1 ;
	}

	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	pkt_dest_addr = get_destAddr_fromPkt(p_data,data_len) ;
	sen_fmt_data_num = 4 ; // 4 bytes per sensor for cmd_v1
	
	pkt_source_addr = get_sourceAddr_fromPkt(p_data,data_len) ;
	if(pkt_source_addr == 0xFFFF)
	{
		debug("not found addr info in this sensor-format pkt,discard it now \n") ;
		return -1 ;
	}

	p_dev_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
	if(p_dev_node == NULL)
	{
		debug("not found match dev info in the buf,discard this sen-format frame and re_get dev-info \n") ;
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
		if(count > 0)
		{
			send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
		}
		
		return -1 ;
	}

	
	active_sen_num = p_dev_node->device_info.active_sensor_num ;
	if(active_sen_num <= 0 )
	{
		debug("active sen-num == 0 re-get dev-info now \n");
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
		if(count > 0)
		{
			send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
		}
		return -1 ;
	}

	if(sen_fmt_data_num <= 0 || sen_fmt_data_num > 10)
	{
		debug("err sen_fmt_data_num = %d ,discard this frame \n",sen_fmt_data_num); // v2 8 or 10,v1 4 bytes
		return -1 ;
	}

	//get cfg_cnt
	pkt_cfgSetCnt = p_data[cmd_data_offset] ;

	//check cfgSet_cnt
	#if 0
	if(p_temp_node != NULL )
	{
		if(node_info.device_info.dev_info_recv_time >= p_temp_node->device_info.dev_info_recv_time &&
			(node_info.device_info.dev_info_recv_time-p_temp_node->device_info.dev_info_recv_time) <= 120)
		{
			debug("dev_info recv_time too short ,<=120s ,discard this dev-info pak now \n");
			return -1 ;
		}
	}
	#else
	//maybe cfgSet_cnt overlow? !!!!!!!!!!!!!!!!!!!!!,we need restart this app if cfgSet_cnt overflow
	if(p_dev_node != NULL && p_dev_node->device_info.sen_fmt_recv_flag)
	{
		if(pkt_cfgSetCnt <= p_dev_node->device_info.cfgSet_cnt)
		{
			debug("cfgSet_cnt not renew,discard this sen_name_fmt pak now \n");
			return -1 ;
		}
		else
		{
			//renew dev-info now
			debug("handle dev-v1 senfmt data,cfgSet renew,reget dev-info now \n");
			count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
			if(count > 0)
			{
				send_cmd_toRemote(wsn_send_buf,count);
				//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
			}

			return 0 ;
		}
		
	}
	#endif

	//set sen_fmt_recv_flag
	p_dev_node->device_info.sen_fmt_recv_flag = 1 ;

	//set dev cfgSet_cnt now,because no cfgSet_cnt in dev_info_cmdV1 paket
	p_dev_node->device_info.cfgSet_cnt = pkt_cfgSetCnt ;

	//handle sensor formt data now
	offset = cmd_data_offset+1 ; // skip cfgSet_cnt
	for(count=0;count<active_sen_num;count++)
	{
		p_temp_sensor_fmt = malloc(sizeof(sensor_data_format_t)) ;
		if(p_temp_sensor_fmt == NULL)
		{
			debug("err when malloc sen_fromat mem \n");
			if(p_sensor_format_head != NULL)
			{
				free_sensor_format_data_mem(p_sensor_format_head) ;
			}
			return -1 ;
		}

		if(p_sensor_format_head == NULL)
		{
			p_sensor_format_head = p_temp_sensor_fmt ;
		}
		
		memset(p_temp_sensor_fmt,0,sizeof(sensor_data_format_t));
		p_temp_sensor_fmt->p_next_sensor_format = NULL ;
		if(p_prev_sensor != NULL)
		{
			p_prev_sensor->p_next_sensor_format = p_temp_sensor_fmt ;
		}

		//copy format data into the buf
		p_temp_sensor_fmt->sensor_index = p_dev_node->device_info.active_sen_index[count] ;
		p_temp_sensor_fmt->sensor_id = p_data[offset+count*sen_fmt_data_num] ; 
		p_temp_sensor_fmt->sensor_sub_id = 255 ; //no subID in cmd_v1,default 255
		p_temp_sensor_fmt->unit_id = p_data[offset+count*sen_fmt_data_num+1] ;
		//calc sen_dp and sen_df
		p_temp_sensor_fmt->sensor_dp = (p_data[offset+count*sen_fmt_data_num+2] >> 5) & 0x07 ; //byte[2],bit5-bit7
		p_temp_sensor_fmt->sensor_df = (p_data[offset+count*sen_fmt_data_num+2] >> 2) & 0x07 ; //byte[2],bit2-bit4
				
		temp_sen_data_len =  (p_data[offset+count*sen_fmt_data_num+2]) & 0x03 ; //byte[2],bit0-bit1
	
		if(temp_sen_data_len == 0)
		{
			p_temp_sensor_fmt->sensor_data_len = 1 ;
		}
		else if(temp_sen_data_len == 1)
		{
			p_temp_sensor_fmt->sensor_data_len = 2 ;
		}
		else
		{
			p_temp_sensor_fmt->sensor_data_len = 4 ;
		}

		//sensor threshold_mask
		p_temp_sensor_fmt->sensor_threshold_mask_v1 = p_data[offset+count*sen_fmt_data_num+3] ;
			
		memcpy(p_temp_sensor_fmt->format_data,p_data+offset+count*sen_fmt_data_num,sen_fmt_data_num);
		debug("sen_index=%d,sen_data_len = %d \n",p_temp_sensor_fmt->sensor_index,p_temp_sensor_fmt->sensor_data_len);

		p_prev_sensor = p_temp_sensor_fmt ;
		//p_temp_sensor_fmt = p_temp_sensor_fmt->p_next_sensor_format ; //==NULL
		sen_fmt_recvNO++ ;

	}

	ret_val = insert_sensor_format_data(p_sensor_format_head,pkt_source_addr) ;
	if(ret_val != 0)
	{
		debug("err insert sen_format_info when parse_sensor_format_dataV1 \n");
		if(p_sensor_format_head != NULL)
		{
			free_sensor_format_data_mem(p_sensor_format_head) ;
		}

		return -1 ;
	}

	p_dev_node->device_info.sen_fmt_recvNO = sen_fmt_recvNO ;

	//send config to cloud
	debug("recved fmt,send node config data to cloud server now \n");
	build_json_config_data(p_dev_node) ;
	send_json_confData_toCloud(p_dev_node);

	//get sensor limit data now
	if(p_dev_node->device_info.active_sensor_num > 0)
	{
		//begin to get sen_limit_data now
		cmd_data[0] = p_dev_node->device_info.active_sen_index[0] ; //first sen_index
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_LIMIT_V1_CMDID,cmd_data,1,local_addr,pkt_source_addr);
		if(count > 0)
		{
			//send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
			send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_LIMIT_V1_CMDID,p_dev_node) ;
		}
	}


	return 0 ;
}


int parse_wsn_senLimit_v1(uint8_t * p_data,uint8_t data_len)
{
	device_node_info_t *p_temp_node = NULL ;
	sensor_data_format_t *p_sensor_format = NULL ;
	uint8_t cfgSet_cnt = 0 ;
	uint8_t alarmSet_cnt = 0 ;
	uint16_t pkt_dest_addr = 0 ; //for us
	uint16_t pkt_source_addr = 0 ;
	uint8_t pkt_sen_index = 0 ;
	uint8_t next_sen_index = ILLEGAL_SEN_INDEX ;
	int count = 0 ;
	uint8_t cmd_data_offset = 0 ;
	uint8_t limit_data_len = 0 ;
	uint8_t cmd_data[32] ;
	
	if(p_data == NULL || data_len <= 0)
	{
		debug("err para when parse_wsn_senLimit_v1 \n");
		return -1 ;
	}

	debug("recved GET_SEN_LIMIT_V1_RESP,handle it now \n");
	cmd_data_offset = WSN_CMD_HDR_LEN+WSN_CMD_VER_LEN+WSN_PKTLENGTH_LEN+WSN_DEST_ADDR_LEN+WSN_SOURCE_ADDR_LEN+WSN_CMD_ID_LEN ;
	
	//alarmSet_cnt = p_data[cmd_data_offset] ; // cmd_v1 not include alarmSet_cnt
	pkt_sen_index = p_data[cmd_data_offset] ;
	pkt_source_addr = get_sourceAddr_fromPkt(p_data,data_len) ;
	
	debug("parse GET_SEN_LIMIT_V1_RESP: pkt_sen_index = %d alarmSet_cnt=%d \n",pkt_sen_index,alarmSet_cnt);
	p_temp_node = query_device_info_by_deviceAddr(pkt_source_addr) ;
	if(p_temp_node == NULL)
	{
		debug("err not found dev_node in buf when parse GET_SEN_LIMIT_V1_RESP data,re-get dev_info now \n") ;
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_DEVINFO_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
		if(count > 0)
		{
			send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
		}
		return 0 ; 
	}

	//check limit data renew or not
	p_sensor_format = query_sensor_format_data(pkt_source_addr,pkt_sen_index) ;
	if(p_sensor_format != NULL )
	{
		if(alarmSet_cnt <= p_temp_node->device_info.alarmSet_cnt && p_sensor_format->current_senLimit_recvFlag)
		{
			debug("recved senIndex=%d limit data and alarmSetCnt not renew,discart it now \n",pkt_sen_index);
			return 0 ;
		}
	}

	//p_temp_node->device_info.alarmSet_cnt = alarmSet_cnt ; //set alarmSet_cnt,cmd_v1 not include alarmset_cnt
	//p_temp_node->device_info.sen_limit_recv_flag = 1 ;
	p_sensor_format = query_sensor_format_data(pkt_source_addr,pkt_sen_index) ;
	if(p_sensor_format != NULL)
	{
		limit_data_len = p_sensor_format->sensor_data_len + 3 ; //include limit data head 3 bytes
		insert_sensor_limit_data(&p_data[cmd_data_offset],limit_data_len,pkt_source_addr,pkt_sen_index) ;

		//next_sen_index = get_next_sen_index(pkt_source_addr,pkt_sen_index) ;
		next_sen_index = query_next_senLmt_index(p_temp_node) ;
		if(next_sen_index != ILLEGAL_SEN_INDEX)
		{
			//get next sen_limit data
			debug("curent sen-index=%d,get next sen limit \n",pkt_sen_index);
			cmd_data[0] = next_sen_index ;
			count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_LIMIT_V1_CMDID,cmd_data,1,local_addr,pkt_source_addr);
			if(count > 0)
			{
				//send_cmd_toRemote(wsn_send_buf,count);
				//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
				send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_LIMIT_V1_CMDID,p_temp_node) ;
			}
		}
		
	}
	else
	{
		//re-get sen_fmt data now
		count = build_wsn_cmdV2(wsn_send_buf,sizeof(wsn_send_buf),GET_SEN_NAMEFORMAT_V1_CMDID,NULL,0,local_addr,pkt_source_addr);
		if(count > 0)
		{
			//send_cmd_toRemote(wsn_send_buf,count);
			//send_expectedResp_cmd_toRemote(wsn_send_buf,count) ;
			send_cmd_toDevWaitingList(wsn_send_buf,count,GET_SEN_NAMEFORMAT_V1_CMDID,p_temp_node) ;
		}
				
	}


	if(p_temp_node->device_info.active_sensor_num > 0 && p_temp_node->device_info.sen_limit_recvNum >= p_temp_node->device_info.active_sensor_num)
	{
		//set sen-limit-data recv flag
		p_temp_node->device_info.sen_limit_recv_flag = 1 ;
		
		//finish recving sen-limit info,update to cloud server now
		debug("recved over limit data,update dev config data to cloud server now \n");
		build_json_config_data(p_temp_node) ;
		send_json_confData_toCloud(p_temp_node);
		//free sen-limit cmd
		if(p_temp_node->p_senLimit_cmd != NULL)  //fix bug repeat sending cmd
		{
			free_specifiedCmd_inDevCmdList(p_temp_node->p_senLimit_cmd);
			p_temp_node->p_senLimit_cmd = NULL ;
		}		
	}


	return 0 ;
}


uint8_t sensorErr_transV1toV2(uint8_t sensor_errV1,uint8_t *sensor_name)
{
	uint8_t senErr_v2 = 0 ;
	uint8_t bit_cnt = 0 ;

	if(sensor_errV1 == 0)
	{
		return 0 ;
	}

	//only check the most significant bit (0-7)
	if(sensor_errV1 & 0x01) //bit0
	{
		// over
		senErr_v2 = 0x41 ;
	}
	else if(sensor_errV1 & 0x02 ) // bit1
	{
		// max
		senErr_v2 = 0x40 ;
	}
	else if(sensor_errV1 & 0x04 ) //bit2
	{
		// Lamp Alarm or sensor off
		if(strcmp(sensor_name,"PID") == 0)
		{
			senErr_v2 = 0x20 ; //Lamp Alarm
		}
		else if(strcmp(sensor_name,"LEL") == 0)
		{
			senErr_v2 = 0x21 ; //sensor off
		}
		else
		{
			senErr_v2 = 0x3F ; //General fail
		}
	}
	else if(sensor_errV1 & 0x08 ) //bit3
	{
		// High limit
		senErr_v2 = 0x43 ;
	}
	else if(sensor_errV1 & 0x10 ) //bit4
	{
		// low
		senErr_v2 = 0x44 ;
	}
	else if(sensor_errV1 & 0x20 ) //bit5
	{
		// STEL limit
		senErr_v2 = 0x46 ;
	}
	else if(sensor_errV1 & 0x40 ) //bit6
	{
		// TWA limit
		senErr_v2 = 0x47 ;
	}
	else if(sensor_errV1 & 0x80 ) //bit7
	{
		// Drift ---neg
		//senErr_v2 = 0x3F ; // drift not found in v2-tab
		senErr_v2 = 0x48 ; // drift not found in v2-tab
	}
	else
	{
		debug("maybe sensor_errV1=0x%x err \n",sensor_errV1);
		senErr_v2 = 0 ;
	}

	
	return senErr_v2 ;
}

uint8_t sensorErr_tranV2toV1(uint8_t sensor_errV2 )
{
	uint8_t senErr_v1 = 0 ;
	uint8_t bit_cnt = 0 ;

	if(sensor_errV2 == 0)
	{
		return 0 ; //no sensor err
	}

	if(sensor_errV2 == 0x41)
	{
		senErr_v1 |= 0x01 ; //over,bit0
	}
	else if(sensor_errV2 == 0x40)
	{
		senErr_v1 |= 0x02 ; //max,bit1
	}
	else if(sensor_errV2 == 0x20 || sensor_errV2 == 0x21 || sensor_errV2 == 0x3F)
	{
		senErr_v1 |= 0x04 ; //max,bit2,Lamp Alarm,or Lamp Fail,or general sensor fail
	}
	else if(sensor_errV2 == 0x43)
	{
		senErr_v1 |= 0x08 ; //high,bit3
	}
	else if(sensor_errV2 == 0x44)
	{
		senErr_v1 |= 0x10 ; //low,bit4
	}
	else if(sensor_errV2 == 0x46)
	{
		senErr_v1 |= 0x20 ; //stel limit,bit5
	}
	else if(sensor_errV2 == 0x47)
	{
		senErr_v1 |= 0x40 ; //TWA limit,bit6
	}
	else if(sensor_errV2 == 0x48)
	{
		senErr_v1 |= 0x80 ; //neg,bit7
	}
	else
	{
		// any other err,general sensor fail
		senErr_v1 |= 0x04 ;
	}

	return senErr_v1 ;
}


//end cmd_v1 funcs










