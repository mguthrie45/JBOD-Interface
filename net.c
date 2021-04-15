#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  int n = 0;
  while (n < len) {
    int rd = read(fd, &buf[n], len-n);
    if (rd <= 0) {
      return false;
    }
    n += rd;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
  int n = 0;
  while (n < len) {
    int wt = write(fd, &buf[n], len-n);
    if (wt <= 0) {
      return false;
    }
    n += wt;
  }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint8_t header[HEADER_LEN];
  bool hrd = nread(fd, HEADER_LEN, header);
  if (!hrd) {printf("error recieving header.\n"); return false;}

  uint16_t length = (uint16_t)(header[0]<<8) + header[1];
  *op = (uint32_t)(header[2]<<24)+(header[3]<<16)+(header[4]<<8)+header[5];
  *ret = (uint16_t)(header[6]<<8)+header[7];

  printf("length: %d  opcode: %d  return: %d  \n", length, *op, *ret);

  int cmd = get_cmd_from_op(*op);
  if (cmd == JBOD_READ_BLOCK) {    
    bool brd = nread(fd, JBOD_BLOCK_SIZE, block);
    if (!brd) {printf("error recieving block.\n"); return false;}
  }
  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {  
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    return false;
  }
  cli_sd = sfd;

  struct sockaddr_in addr_info;
  addr_info.sin_family = AF_INET;
  addr_info.sin_port = htons(port);
  int aton_succ = inet_aton(ip, &(addr_info.sin_addr));
  if (aton_succ == -1) {
    return false;
  }
    
  int conn_succ = connect(cli_sd, (const struct sockaddr_in*)&addr_info, sizeof(addr_info));
  if (conn_succ == -1) {
    return false;
  }
  return true;
}

int get_cmd_from_op(uint32_t op) {
  return (int)op>>26;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
}
