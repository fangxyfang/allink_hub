#ifndef __RMCI_H__
#define __RMCI_H__

#include "util.h"
// uart7
#define ATPORT "/dev/ttymxc6"

#define OTHER_BAUDRATE 19200
#define GTW_BAUDRATE 38400
#define LORA_BAUDRATE 115200

#define GTW 1
#define RTR 2
#define STD 4

#define TX_POWER_LOW "Low"
#define TX_POWER_MIDDLE "Middle"
#define TX_POWER_HIGH "High"

#define RMCI_CMD_SIZE 100
#define RMCI_RSP_SIZE 100
//Return the radio’s type: Mesh的Module type:9,10;9是900MHZ,10是2.4GHZ LORA的module type是42
#define CMD_GET_MODULE_TYPE 0x2A
#define CMD_GET_APPTYPE 0x3A
#define CMD_GET_FW_VERSION 0x4C
#define CMD_GET_NODE_INFO 0x60
#define CMD_GET_APPNODE_INFO 0x6C
#define CMD_GET_CHNMAPPING 0xB2

#define CMD_QUERY_SYS_CTRL_INFO 0x5A
#define CMD_QUERY_RECEIVER_STATUS 0xA2
#define CMD_QUERY_REGION 0xA6  

#define CMD_SET_MODULE_TYPE_GTW 0x30

#define CMD_SET_CMD_BATCH 0xAC

#define CMD_SET_CHANNEL 0x34
#define CMD_SET_TX_POWER 0x3C
#define CMD_SET_PAN_ID 0x44
#define CMD_SET_SYS_CTRL_INFO 0x58
#define CMD_SET_REGION 0xA4
#define CMD_SET_CHNMAPPING 0xB0



#define CMD_GET_ENA_MODULE_TYPE 0xAE


/////////////
#define MODULE_TYPE_MESH_9 9
#define MODULE_TYPE_MESH_10 10
#define MODULE_TYPE_LORA_42 42

  //-g 1: get modem type
  //-g 2: set region
  //-g 3: set channel
#define CMD_FUNCTION_GET_MODEM_TYPE "1"
#define CMD_FUNCTION_SET_CHNMAPPING_CMD  "2"
#define CMD_FUNCTION_SET_CHANNEL "3"

#define GET_CFG_ERR_PANID 1
#define GET_CFG_ERR_CHANNEL 2
#define GET_CFG_ERR_SET_CHNMAPPING 4
#define GET_CFG_ERR_TX_POWER 8
#define GET_CFG_ERR_REGION 16
#define GET_CFG_ERR_PSS 32  //pre_shared_secret
#define GET_CFG_ERR_PARA 64


#define CHG_SET_CFG_PANID 1
#define CHG_SET_CFG_CHANNEL 2
#define CHG_SET_CFG_SET_CHNMAPPING 4
#define CHG_SET_CFG_TX_POWER 8
#define CHG_SET_CFG_SYS_CTRL_INFO 16

#define DEFAULT_PANID_100 100
#define DEFAULT_PANID_999 999

enum RMCI_ERROR{

	 MODULE_TYPE_GOT_ERR = 10,
	 
     GET_CFG_ERR_PANID_ERR =20,
     GET_CFG_ERR_CHANNEL_ERR,
	 GET_CFG_ERR_SET_CHNMAPPING_ERR,
     GET_CFG_ERR_TX_POWER_ERR,
     GET_CFG_ERR_REGION_ERR,
	 GET_CFG_ERR_PSS_ERR,  //pre_shared_secret

	CHG_SET_CFG_PANID_ERR = 30,
	CHG_SET_CFG_CHANNEL_ERR,
	CHG_SET_CFG_SET_CHNMAPPING_ERR,
	CHG_SET_CFG_TX_POWER_ERR,
	CHG_SET_CFG_SYS_CTRL_INFO_ERR,
    MAX_ERR
};

typedef union
{
	uint8 byte;
	struct
	{
		unsigned bit0: 1;//roaming : 1;
		unsigned bit1 : 1;
		unsigned bit2 : 1;
		unsigned bit3: 1;//parallel_net : 1;
		unsigned bit4 : 1;
		unsigned bit5 : 1;
		unsigned bit6 : 1;
		unsigned bit7 : 1;  //byte3.7 1:wsn not encapuslated in rmci
	} bits;
} sys_ctrl_info_t;

typedef struct
{
	uint8 uid[8]; //only read
	uint8 channel;
	uint8 power;
	uint16 panid;
	uint8 power_str[8];
	uint8 channel_mask[16];
	sys_ctrl_info_t  sys_ctrl_info[4];
	char region_name[16];
	uint8 set_region_enable; //no use
	uint8 set_chnmapping_cmd[12+1];
	uint8 set_chnmapping_cmd_str[12*2+1];
	uint8 pre_shared_secret[32+1];
	//uint8 pre_shared_secret_str[32+1];
	// uint8 region_idx;
	uint8 alarmOfflineNumCfg;
	uint8 trigAlarmEnableCfg;
	uint8 trigRelayEnableCfg;
	uint8  locationCfg[32+1];
	uint8  gatewayNameCfg[32+1];
	uint8 ng;
	uint16 chg;
	uint8 module_type_got;     //fxy add
	uint8 module_lora1_mesh0;	
} module_config_t;

typedef struct
{
	char region_name[16];
	uint16 channel_mask;
	uint8 default_channel;
	char power_str[8];
	uint8 channelmap_payload[12];
} module_region_t;

typedef struct
{
	int baudrate;
	uint8 module_type; //Mesh的Module type:9,10;9是900MHZ,10是2.4GHZ;LORA的module type是42
	uint8 modem_application_type;
	module_config_t config;
	uint8 module_type_got;
	uint8 module_lora1_mesh0;
	uint8 cfg_chg_cnt;
} module_state_t;

int getconfig(uint8 *cmd_module_function_flag, module_config_t *p_module_config);
int getconfig_from_chg(uint8 *cmd_module_function_flag,uint8* chg_para, module_config_t *p_module_config);

int init_config_modem(uint8 *cmd_module_function_flag,uint8* cmd_para,module_config_t *p_module_config);
void reset_modem();
void restart_prog();
void set_nvram_or_fact(uint8 *p_nvram, uint8 *cmd_para,module_config_t *p_module_config);
void setconfig_ok(uint8 *cmd_function_flag,uint8 *cmd_para,module_config_t *p_module_config);
#endif
