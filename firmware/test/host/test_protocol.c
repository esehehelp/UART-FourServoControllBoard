/* Host-side tests for firmware/src/protocol.c (parser, TTL forwarding,
 * error responses and ACKs). Build and run: make -C firmware/test/host */
#include "protocol.h"
#include "error_codes.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
extern int led1, led2, config_saves, flash_ok, pd_mv; extern uint16_t servo_pos[4];
extern uint8_t tx[8][512]; extern int txn[8]; extern unsigned char host_flash[256];
static int fails=0;
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } }while(0)
static void reset_tx(void){ memset(txn,0,sizeof txn); }
static void feed(Interface_t i, const uint8_t *b, int n){ for(int k=0;k<n;k++) Process_Byte(i,b[k]); }
static int build(uint8_t *o, uint8_t tgt, uint8_t ttl, uint8_t cmd, const uint8_t *d, int n){
  o[0]=0xAA;o[1]=tgt;o[2]=0x00;o[3]=ttl;o[4]=cmd;o[5]=(uint8_t)n; if(n) memcpy(o+6,d,n); o[6+n]=crc8(o,6+n); return 7+n; }
/* returns error code of the single 0xEE packet sent on iface, or -1 */
static int err_on(Interface_t i, uint8_t *orig){
  if(txn[i]!=9 || tx[i][4]!=0xEE) return -1;
  if(tx[i][8]!=crc8(tx[i],8)) return -2;
  if(orig) *orig=tx[i][6]; return tx[i][7]; }
int main(void){
  setvbuf(stdout, NULL, _IONBF, 0);
  uint8_t pkt[300]; int n; uint8_t oc;
  Protocol_Init(1);
  /* LED 0x30 */
  uint8_t led[2]={1,20}; reset_tx(); n=build(pkt,1,16,0x30,led,2); feed(IF_USB,pkt,n);
  CHECK(led2==20 && txn[IF_USB]==0);
  /* bad LED channel -> ERR_BAD_CHANNEL */
  led[0]=5; reset_tx(); n=build(pkt,1,16,0x30,led,2); feed(IF_USB,pkt,n);
  CHECK(err_on(IF_USB,&oc)==ERR_BAD_CHANNEL && oc==0x30);
  /* retired 0x05 -> unknown */
  reset_tx(); n=build(pkt,1,16,0x05,led,2); feed(IF_USB,pkt,n);
  CHECK(err_on(IF_USB,NULL)==ERR_UNKNOWN_CMD);
  /* broadcast unknown: no error */
  reset_tx(); n=build(pkt,0xFF,16,0x05,led,2); feed(IF_USB,pkt,n);
  CHECK(txn[IF_USB]==0);
  /* servo out of range -> BAD_VALUE, not moved */
  uint8_t sv[3]={0,0x0B,0xB8}; servo_pos[0]=0; reset_tx(); n=build(pkt,1,16,0x01,sv,3); feed(IF_USB,pkt,n);
  CHECK(err_on(IF_USB,NULL)==ERR_BAD_VALUE && servo_pos[0]==0);
  sv[1]=0x05; sv[2]=0xDC; reset_tx(); n=build(pkt,1,16,0x01,sv,3); feed(IF_USB,pkt,n);
  CHECK(txn[IF_USB]==0 && servo_pos[0]==1500);
  /* short length */
  reset_tx(); n=build(pkt,1,16,0x01,sv,2); feed(IF_USB,pkt,n);
  CHECK(err_on(IF_USB,NULL)==ERR_BAD_LENGTH);
  /* PD: 20 V rejected, 9 V ACKed */
  uint8_t pd[2]={0x4E,0x20}; pd_mv=-1; reset_tx(); n=build(pkt,1,16,0x06,pd,2); feed(IF_USB,pkt,n);
  CHECK(err_on(IF_USB,NULL)==ERR_BAD_VALUE && pd_mv==-1);
  pd[0]=0x23; pd[1]=0x28; reset_tx(); n=build(pkt,1,16,0x06,pd,2); feed(IF_USB,pkt,n);
  CHECK(pd_mv==9000 && txn[IF_USB]==9 && tx[IF_USB][4]==0x86);
  /* cal save: invalid range, flash fail, ok */
  uint8_t c[13]={0}; c[0]=1; c[9]=0x09;c[10]=0xC4; c[11]=0x01;c[12]=0xF4;
  reset_tx(); n=build(pkt,1,16,0x07,c,13); feed(IF_USB,pkt,n); CHECK(err_on(IF_USB,NULL)==ERR_CAL_INVALID);
  c[9]=0x01;c[10]=0xF4; c[11]=0x09;c[12]=0xC4; flash_ok=0;
  reset_tx(); n=build(pkt,1,16,0x07,c,13); feed(IF_USB,pkt,n); CHECK(err_on(IF_USB,NULL)==ERR_FLASH_WRITE);
  flash_ok=1; reset_tx(); n=build(pkt,1,16,0x07,c,13); feed(IF_USB,pkt,n);
  CHECK(txn[IF_USB]==8 && tx[IF_USB][4]==0x87 && tx[IF_USB][6]==1);
  /* cfg write: device id 0xFF rejected */
  uint8_t cf[2]={1,0xFF}; reset_tx(); n=build(pkt,1,16,0x04,cf,2); feed(IF_USB,pkt,n); CHECK(err_on(IF_USB,NULL)==ERR_BAD_VALUE);
  /* forwarding: packet for id 5 from UART2 goes out UART4 with TTL-1 and valid CRC */
  reset_tx(); n=build(pkt,5,3,0x02,(uint8_t[]){0},1); feed(IF_UART2,pkt,n);
  CHECK(txn[IF_UART4]==n && tx[IF_UART4][3]==2 && tx[IF_UART4][n-1]==crc8(tx[IF_UART4],n-1));
  /* TTL 0: not forwarded, error back on the receiving interface */
  reset_tx(); n=build(pkt,5,0,0x02,(uint8_t[]){0},1); feed(IF_UART2,pkt,n);
  CHECK(txn[IF_UART4]==0 && err_on(IF_UART2,NULL)==ERR_TTL_EXPIRED);
  /* TTL 0 error packet: dropped silently */
  reset_tx(); n=build(pkt,5,0,0xEE,(uint8_t[]){2,0x13},2); feed(IF_UART2,pkt,n);
  CHECK(txn[IF_UART4]==0 && txn[IF_UART2]==0);
  /* oversize length dropped, parser resyncs */
  uint8_t big[200]={0}; reset_tx(); n=build(pkt,1,16,0x30,big,PKT_MAX_DATA_LEN+1); feed(IF_USB,pkt,n);
  led[0]=0; led[1]=77; n=build(pkt,1,16,0x30,led,2); feed(IF_USB,pkt,n); CHECK(led1==77);
  /* ---- #49 config write/read (0x20 / 0x21 / legacy 0x04) ---- */
  { uint8_t w[20]; int saves;
    /* name write + read back */
    w[0]=0x03; memcpy(w+1,"ArmLeft",7); reset_tx(); n=build(pkt,1,16,0x20,w,8); feed(IF_USB,pkt,n);
    CHECK(txn[IF_USB]==8 && tx[IF_USB][4]==0x84 && tx[IF_USB][6]==0x03);
    CHECK(strcmp(g_config.device.name,"ArmLeft")==0);
    CHECK(memcmp(host_flash, &g_config, sizeof g_config)==0); /* persisted */
    reset_tx(); n=build(pkt,1,16,0x21,(uint8_t[]){0x03},1); feed(IF_USB,pkt,n);
    CHECK(tx[IF_USB][4]==0x85 && tx[IF_USB][5]==8 && memcmp(tx[IF_USB]+7,"ArmLeft",7)==0);
    /* servo default pulse CH2 = 1500, read back */
    uint8_t dp[4]={0x10,2,0x05,0xDC}; reset_tx(); n=build(pkt,1,16,0x20,dp,4); feed(IF_USB,pkt,n);
    CHECK(tx[IF_USB][4]==0x84 && tx[IF_USB][5]==2 && g_config.servo[2].default_pulse==1500);
    reset_tx(); n=build(pkt,1,16,0x21,(uint8_t[]){0x10,2},2); feed(IF_USB,pkt,n);
    CHECK(tx[IF_USB][4]==0x85 && tx[IF_USB][7]==2 && tx[IF_USB][8]==0x05 && tx[IF_USB][9]==0xDC);
    /* default pulse outside range -> CONFIG_INVALID, unchanged */
    dp[2]=0x0B; dp[3]=0xB8; reset_tx(); n=build(pkt,1,16,0x20,dp,4); feed(IF_USB,pkt,n);
    CHECK(err_on(IF_USB,NULL)==ERR_CONFIG_INVALID && g_config.servo[2].default_pulse==1500);
    /* min >= max -> CONFIG_INVALID */
    uint8_t mn[4]={0x11,0,0x09,0xC4}; reset_tx(); n=build(pkt,1,16,0x20,mn,4); feed(IF_USB,pkt,n);
    CHECK(err_on(IF_USB,NULL)==ERR_CONFIG_INVALID && g_config.servo[0].min_pulse==500);
    /* bad channel, unknown tag, read-only tag, bad length */
    dp[1]=5; reset_tx(); n=build(pkt,1,16,0x20,dp,4); feed(IF_USB,pkt,n); CHECK(err_on(IF_USB,NULL)==ERR_BAD_CHANNEL);
    reset_tx(); n=build(pkt,1,16,0x20,(uint8_t[]){0x77,1},2); feed(IF_USB,pkt,n); CHECK(err_on(IF_USB,NULL)==ERR_BAD_VALUE);
    reset_tx(); n=build(pkt,1,16,0x20,(uint8_t[]){0xF1,9},2); feed(IF_USB,pkt,n); CHECK(err_on(IF_USB,NULL)==ERR_BAD_VALUE);
    reset_tx(); n=build(pkt,1,16,0x20,(uint8_t[]){0x31,1},2); feed(IF_USB,pkt,n); CHECK(err_on(IF_USB,NULL)==ERR_BAD_LENGTH);
    /* protection threshold write */
    reset_tx(); n=build(pkt,1,16,0x20,(uint8_t[]){0x31,0x0F,0xA0},3); feed(IF_USB,pkt,n);
    CHECK(tx[IF_USB][4]==0x84 && g_config.prot.max_current_ma==4000);
    /* unknown feature bits rejected */
    reset_tx(); n=build(pkt,1,16,0x20,(uint8_t[]){0x30,0x80},2); feed(IF_USB,pkt,n); CHECK(err_on(IF_USB,NULL)==ERR_BAD_VALUE);
    /* flash failure: RAM rolled back */
    flash_ok=0; saves=config_saves;
    reset_tx(); n=build(pkt,1,16,0x20,(uint8_t[]){0x31,0x03,0xE8},3); feed(IF_USB,pkt,n);
    CHECK(err_on(IF_USB,NULL)==ERR_FLASH_WRITE && g_config.prot.max_current_ma==4000 && config_saves==saves+1);
    flash_ok=1;
    /* firmware version */
    reset_tx(); n=build(pkt,1,16,0x21,(uint8_t[]){0xF0},1); feed(IF_USB,pkt,n);
    CHECK(tx[IF_USB][4]==0x85 && tx[IF_USB][7]==FW_VERSION_MAJOR && tx[IF_USB][8]==FW_VERSION_MINOR && tx[IF_USB][9]==FW_VERSION_PATCH);
    /* legacy 0x04: change device id -> ACK comes from the new id */
    reset_tx(); n=build(pkt,1,16,0x04,(uint8_t[]){0x01,0x07},2); feed(IF_USB,pkt,n);
    CHECK(tx[IF_USB][4]==0x84 && tx[IF_USB][2]==0x07 && g_config.device.device_id==7);
    reset_tx(); n=build(pkt,7,16,0x04,(uint8_t[]){0x01,0x01},2); feed(IF_USB,pkt,n);
    CHECK(g_config.device.device_id==1);
  }
  /* ---- #47: servo commands blocked while a protection fault persists ---- */
  { extern uint8_t prot_fault; uint8_t sv2[3]={0,0x05,0xDC};
    prot_fault=ERR_OVERHEAT; servo_pos[0]=0; reset_tx(); n=build(pkt,1,16,0x01,sv2,3); feed(IF_USB,pkt,n);
    CHECK(err_on(IF_USB,NULL)==ERR_OVERHEAT && servo_pos[0]==0);
    prot_fault=0; reset_tx(); n=build(pkt,1,16,0x01,sv2,3); feed(IF_USB,pkt,n);
    CHECK(txn[IF_USB]==0 && servo_pos[0]==1500); }
  printf(fails? "FAILED (%d)\n" : "ALL OK\n", fails); return fails!=0; }
