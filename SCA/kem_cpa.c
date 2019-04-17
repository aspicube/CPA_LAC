/* Deterministic randombytes by Daniel J. Bernstein */
/* taken from SUPERCOP (https://bench.cr.yp.to)     */
/* main function modified for side channel attack   */

#include "api.h"
#include "stm32wrapper.h"
#include "randombytes.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>


typedef uint32_t uint32;
//flag_keypair = 0:keypair not ready =1:keypair ready
static unsigned int flag_keypair;

//actually, printbytes is sending hex format bytes, while print hex is sending origin bytes.

static void printbytes(const unsigned char *x, unsigned long long xlen)
{
  char outs[2*xlen+1];
  unsigned long long i;
  for(i=0;i<xlen;i++)
    sprintf(outs+2*i, "%02x", x[i]);
  outs[2*xlen] = 0;
  send_USART_str(outs);
}

static uint32 seed[32] = { 3,1,4,1,5,9,2,6,5,3,5,8,9,7,9,3,2,3,8,4,6,2,6,4,3,3,8,3,2,7,9,5 } ;
static uint32 in[12];
static uint32 out[8];
static int outleft = 0;

#define ROTATE(x,b) (((x) << (b)) | ((x) >> (32 - (b))))
#define MUSH(i,b) x = t[i] += (((x ^ seed[i]) + sum) ^ ROTATE(x,b));

static void surf(void)
{
  uint32 t[12]; uint32 x; uint32 sum = 0;
  int r; int i; int loop;

  for (i = 0;i < 12;++i) t[i] = in[i] ^ seed[12 + i];
  for (i = 0;i < 8;++i) out[i] = seed[24 + i];
  x = t[11];
  for (loop = 0;loop < 2;++loop) {
    for (r = 0;r < 16;++r) {
      sum += 0x9e3779b9;
      MUSH(0,5) MUSH(1,7) MUSH(2,9) MUSH(3,13)
      MUSH(4,5) MUSH(5,7) MUSH(6,9) MUSH(7,13)
      MUSH(8,5) MUSH(9,7) MUSH(10,9) MUSH(11,13)
    }
    for (i = 0;i < 8;++i) out[i] ^= t[i + 4];
  }
}

void randombytes(unsigned char *x,unsigned long long xlen)
{
  unsigned long long bak = xlen;
  unsigned char *xbak = x;

  while (xlen > 0) {
    if (!outleft) {
      if (!++in[0]) if (!++in[1]) if (!++in[2]) ++in[3];
      surf();
      outleft = 8;
    }
    *x = out[--outleft];
    ++x;
    --xlen;
  }
  //printbytes(xbak, bak);
}
// LAC128--------------------------------------
// CRYPTO_SECRETKEYBYTES = DIM_N+PK_LEN = 1056
// CRYPTO_PUBLICKEYBYTES = PK_LEN = 544
// CRYPTO_BYTES = MESSAGE_LEN = 32
// CRYPTO_CIPHERTEXTBYTES = CIPHER_LEN = 736
// --------------------------------------------
int main(void)
{

  unsigned char key_a[CRYPTO_BYTES], key_b[CRYPTO_BYTES];
  unsigned char pk[CRYPTO_PUBLICKEYBYTES];
  unsigned char sendb[CRYPTO_CIPHERTEXTBYTES];
  unsigned char sk_a[CRYPTO_SECRETKEYBYTES];

  unsigned char cmd_str[3];
  unsigned char ret_str[3];
  unsigned long long recv_param_length;
  unsigned long long send_param_length;
  flag_keypair = 0;
  //initialize--------------------------------------
  clock_setup(CLOCK_FAST);
  gpio_setup();
  gpio_ledsetup();
  usart_setup(115200);
  osctrig_reset();
  send_USART_str("STM32F407G-DISC1 initialized.\n");
  crypto_kem_keypair(pk, sk_a);
  send_USART_bytes(sk_a, CRYPTO_SECRETKEYBYTES);
  ledOFF();
  //------------------------------------------------
  //waiting for command
  while(1)
  {
    //ledOFF();
    recv_USART_bytes(cmd_str,3);
    ledON();
    switch (cmd_str[0])
    {
      case 0xC0: //keygen
      {
        crypto_kem_keypair(pk, sk_a);
        ret_str[0] = 0x00;
        ret_str[1] = CRYPTO_SECRETKEYBYTES / 256;
        ret_str[2] = CRYPTO_SECRETKEYBYTES % 256;
        send_USART_bytes(ret_str,3);
        send_USART_bytes(sk_a, CRYPTO_SECRETKEYBYTES);
        flag_keypair = 1;
        break;
      }
      case 0xC3: //key set from master
      {
        recv_param_length = cmd_str[1] * 256 + cmd_str[2];
        if(recv_param_length != CRYPTO_SECRETKEYBYTES)
        {
          ret_str[0] = 0xFF;
          ret_str[1] = 0x00;
          ret_str[2] = 0x00;
          send_USART_bytes(ret_str,3);
        }
        else
        {
          ret_str[0] = 0x00;
          ret_str[1] = 0x00;
          ret_str[2] = 0x00;
          send_USART_bytes(ret_str,3);
          recv_USART_bytes(sk_a,CRYPTO_SECRETKEYBYTES);
          //pk = sk_a + CRYPTO_SECRETKEYBYTES - CRYPTO_PUBLICKEYBYTES;
          memcpy(pk, sk_a + CRYPTO_SECRETKEYBYTES - CRYPTO_PUBLICKEYBYTES, CRYPTO_PUBLICKEYBYTES);
          flag_keypair = 1;
        }
        break;
      }
      case 0xCA://random enc and dec
      {
        if(flag_keypair == 0)
        {
          ret_str[0] = 0xFF;
          ret_str[1] = 0x00;
          ret_str[2] = 0x00;
          send_USART_bytes(ret_str,3);
        }else
        {
          crypto_kem_enc(sendb, key_b, pk);
          osctrig_set();
          crypto_kem_dec(key_a, sendb, sk_a);
          osctrig_reset();
          send_param_length = CRYPTO_CIPHERTEXTBYTES + CRYPTO_BYTES;
          ret_str[0] = 0x00;
          ret_str[1] = send_param_length / 256;
          ret_str[2] = send_param_length % 256;
          send_USART_bytes(ret_str,3);
          send_USART_bytes(key_a,CRYPTO_BYTES);
          send_USART_bytes(sendb,CRYPTO_CIPHERTEXTBYTES);
        }
        break;
      }
      case 0xCC://dec using data from USART
      {
        recv_param_length = cmd_str[1] * 256 + cmd_str[2];
        if((flag_keypair == 0) || (recv_param_length != CRYPTO_CIPHERTEXTBYTES))
        {
          ret_str[0] = 0xFF;
          ret_str[1] = 0x00;
          ret_str[2] = 0x00;
          send_USART_bytes(ret_str,3);
        }else
        {
          recv_USART_bytes(sendb, CRYPTO_CIPHERTEXTBYTES);
          osctrig_set();
          crypto_kem_dec(key_a, sendb, sk_a);
          osctrig_reset();
          send_param_length = CRYPTO_CIPHERTEXTBYTES + CRYPTO_BYTES;
          ret_str[0] = 0x00;
          ret_str[1] = send_param_length / 256;
          ret_str[2] = send_param_length % 256;
          send_USART_bytes(ret_str,3);
          send_USART_bytes(key_a,CRYPTO_BYTES);
          send_USART_bytes(sendb,CRYPTO_CIPHERTEXTBYTES);
        }
        break;
      }
    }
  }
  return 0;
}
