#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "mdadm.h"
#include "jbod.h"
#include "cache.h"

int MOUNTED = 0;

uint32_t encode_op(jbod_cmd_t cmd, int disk_id, int block_id) {
  uint32_t c = cmd << 26;
  uint32_t did = disk_id << 22;
  uint32_t bid = block_id;
  uint32_t enc_op = c | did | bid;
  return enc_op;
}

int mdadm_mount(void) {
  if (MOUNTED) {
    return -1;
  }
  uint32_t jbod_op = encode_op(JBOD_MOUNT, 0, 0);
  jbod_operation(jbod_op, NULL);
  MOUNTED = 1;
  return 1;
}

int mdadm_unmount(void) {
  if (MOUNTED == 0) {
    return -1;
  }
  uint32_t jbod_op = encode_op(JBOD_UNMOUNT, 0, 0);
  jbod_operation(jbod_op, NULL);
  MOUNTED = 0;
  return 1;
}

void translate_addr(uint32_t lin_addr,
		    int *disk_num,
		    int *block_num,
		    int *offset) {
  *disk_num = lin_addr / JBOD_DISK_SIZE;
  *block_num = (lin_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  *offset = (lin_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
}

uint32_t linearize_addr(int disk_num, int block_num, int offset) {
  return (uint32_t) ((disk_num*JBOD_NUM_BLOCKS_PER_DISK+block_num)*JBOD_BLOCK_SIZE+offset);
}

int seek(int disk_num, int block_num) {
  uint32_t sd_op = encode_op(JBOD_SEEK_TO_DISK, disk_num, 0);
  uint32_t sb_op = encode_op(JBOD_SEEK_TO_BLOCK, 0, block_num);
  jbod_operation(sd_op, NULL);
  jbod_operation(sb_op, NULL);
  return 1;
}

void update_block_disk(int *disk_num, int *block_num) {
  ++*block_num;
  if (*block_num == JBOD_NUM_BLOCKS_PER_DISK) {
    *block_num = 0;
    ++*disk_num;
  }
}

int valid_read_write_params(uint32_t addr, int len, const uint8_t *buf) {
  if (MOUNTED == 0) {
    return -1;
  }
  else if (len > 1024) {
    return -1;
  }
  else if (buf == NULL) {
    return -1;
  }
  else if (addr+len > JBOD_DISK_SIZE*JBOD_NUM_DISKS) {
    return -1;
  }
  return 1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  if (len == 0) {
    return 0;
  }
  if (valid_read_write_params(addr, len, buf) == -1) {
    return -1;
  }
  
  int disk_num, block_num, offset, cache_hit;
  translate_addr(addr, &disk_num, &block_num, &offset);
  
  int block_reads = (int)((len+offset)/JBOD_BLOCK_SIZE+1);  
  if (len+offset <= 256) {
    block_reads = 1;
  }

  uint8_t large_buf[block_reads*JBOD_BLOCK_SIZE], block_buf[JBOD_BLOCK_SIZE];
  uint32_t rb_op;
  
  for (int rd = 0; rd < block_reads; ++rd) {
    cache_hit = cache_lookup(disk_num, block_num, block_buf);
    if (cache_hit == 1) {
      //printf("cache_hit in read (%i, %i) \n", disk_num, block_num);
    }
    seek(disk_num, block_num);
    
    if (cache_hit == -1) {
      rb_op = encode_op(JBOD_READ_BLOCK, 0, 0);
      jbod_operation(rb_op, block_buf);
      cache_insert(disk_num, block_num, block_buf);
      //printf("cache insert at (%i, %i) \n", disk_num, block_num);
    }
    update_block_disk(&disk_num, &block_num);    
    memcpy(&large_buf[rd*JBOD_BLOCK_SIZE], block_buf, JBOD_BLOCK_SIZE);
  }
  memcpy(buf, &large_buf[offset], len);
  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  if (len == 0) {
    return 0;
  }
  if (valid_read_write_params(addr, len, buf) == -1) {
    return -1;
  }
  
  int cache_hit, disk_num, block_num, dump, curr_size;
  int len_left=0, fake_offset=0,offset=addr%JBOD_BLOCK_SIZE;
  uint32_t op, current_addr = addr;
  uint8_t block_buf[JBOD_BLOCK_SIZE];
  
  
  while (current_addr < addr+len) {
    translate_addr(current_addr, &disk_num, &block_num, &dump);
    seek(disk_num, block_num);
    cache_hit = cache_lookup(disk_num, block_num, block_buf);

    if (cache_hit == -1) {
      op = encode_op(JBOD_READ_BLOCK, 0, 0);
      jbod_operation(op, block_buf);
    }

    curr_size = 0;
    len_left = addr+len-current_addr;
    curr_size = len_left;
    
    if (len_left+offset > JBOD_BLOCK_SIZE) {
      curr_size = JBOD_BLOCK_SIZE-offset;
    }

    memcpy(block_buf+offset, buf+fake_offset, curr_size);
    cache_update(disk_num, block_num, block_buf);
    seek(disk_num, block_num);
    
    op = encode_op(JBOD_WRITE_BLOCK, 0, 0);
    jbod_operation(op, block_buf);
    
    fake_offset += curr_size;
    current_addr += curr_size;
    offset = 0;
  }
  return len;
}
