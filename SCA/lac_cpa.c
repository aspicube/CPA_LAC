/* Deterministic randombytes by Daniel J. Bernstein */
/* taken from SUPERCOP (https://bench.cr.yp.to)     */
/* main function modified for side channel attack   */

#include "api.h"
#include "stm32wrapper.h"
#include "randombytes.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define NTESTS 2

#define STATE_INITING 0
#define STATE_INITED 1
#define STATE_WAITING 2
#define STATE_WORKING 3

typedef uint32_t uint32;
static unsigned int program_state;
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
  printbytes(xbak, bak);
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
  int i,j;

  program_state = STATE_INITING;
  //initialize
  clock_setup(CLOCK_FAST);
  gpio_setup();
  usart_setup(115200);

  send_USART_str("==========================");
  //
  program_state = STATE_INITED;

  for(i=0;i<NTESTS;i++)
  {
    // Key-pair generation
    crypto_kem_keypair(pk, sk_a);

    printbytes(pk,CRYPTO_PUBLICKEYBYTES);
    printbytes(sk_a,CRYPTO_SECRETKEYBYTES);

    // Encapsulation
    crypto_kem_enc(sendb, key_b, pk);

    printbytes(sendb,CRYPTO_CIPHERTEXTBYTES);
    printbytes(key_b,CRYPTO_BYTES);

    // Decapsulation
    crypto_kem_dec(key_a, sendb, sk_a);

    printbytes(key_a,CRYPTO_BYTES);

    for(j=0;j<CRYPTO_BYTES;j++)
    {
      if(key_a[j] != key_b[j])
      {
        send_USART_str("ERROR");
        send_USART_str("#");
        return -1;
      }
    }
  }

  send_USART_str("#");
  while(1);
  return 0;
}
// RXNE: RX not empty
// TXE: TX empty
void usart2_isr(void)
{
  static uint8_t data = 'A';
  /* Check if we were called because of RXNE. */
	if (((USART_CR1(USART2) & USART_CR1_RXNEIE) != 0) &&
	    ((USART_SR(USART2) & USART_SR_RXNE) != 0)) {

		/* Indicate that we got data. */
		gpio_toggle(GPIOD, GPIO12);

		/* Retrieve the data from the peripheral. */
		data = usart_recv(USART2);

		/* Enable transmit interrupt so it sends back the data. */
		usart_enable_tx_interrupt(USART2);
	}

	/* Check if we were called because of TXE. */
	if (((USART_CR1(USART2) & USART_CR1_TXEIE) != 0) &&
	    ((USART_SR(USART2) & USART_SR_TXE) != 0)) {

		/* Put data into the transmit register. */
		usart_send(USART2, data);

		/* Disable the TXE interrupt as we don't need it anymore. */
		usart_disable_tx_interrupt(USART2);
	}
}
