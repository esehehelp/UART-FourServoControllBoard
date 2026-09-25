/* Host-side tests for firmware/src/protocol.c (parser, TTL forwarding,
 * error responses and ACKs). Build and run: make -C firmware/test/host */
#include "protocol.h"
#include "error_codes.h"
#include <stdio.h>
#include <string.h>
extern int led1, led2, config_saves, flash_ok, pd_mv; extern uint16_t servo_pos[4];
extern uint8_t tx[8][512]; extern int txn[8];
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
  printf(fails? "FAILED (%d)\n" : "ALL OK\n", fails); return fails!=0; }
