#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

static bool cache_active = false;

int valid_cache_params(int disk_num, int block_num, const uint8_t *buf) {
  if (!cache_active) {
    return -1;
  }
  if (buf == NULL) {
    return -1;
  }
  if (disk_num > JBOD_NUM_DISKS-1 || disk_num < 0) {
    return -1;
  }
  if (block_num > JBOD_NUM_BLOCKS_PER_DISK*JBOD_NUM_DISKS-1 || block_num < 0) {
    return -1;
  }
  return 1;
}

int cache_create(int num_entries) {
  if (cache_active) {
    return -1;
  }
  if (num_entries < 2 || num_entries > 4096) {
    return -1;
  }

  cache_size = num_entries;
  cache = calloc(num_entries, sizeof(cache_entry_t));
  clock = 0;
  cache_active = 1;
  return 1;
}

int cache_destroy(void) {
  if (!cache_active) {
    return -1;
  }
  free(cache);
  cache = NULL;
  cache_size = 0;
  cache_active = 0;  
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  int valid_params = valid_cache_params(disk_num, block_num, buf);
  if (valid_params == -1) {
    return -1;
  }  
  ++num_queries;
  ++clock;

  for (cache_entry_t *entry = cache; entry < cache+cache_size; ++entry) {
    if (entry -> valid) {
      if (disk_num == entry -> disk_num && block_num == entry -> block_num) {
	memcpy(buf, entry -> block, JBOD_BLOCK_SIZE);
	entry -> access_time = clock;
	++num_hits;
	return 1;
      }
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  //++num_queries;
  ++clock;
  
  for (cache_entry_t *entry = cache; entry < cache+cache_size; ++entry) {
    if (entry -> valid) {
      if (disk_num == entry -> disk_num && block_num == entry -> block_num) {
	memcpy(entry -> block, buf, JBOD_BLOCK_SIZE);
	entry -> access_time = clock;
	//++num_hits;
	break;
      }
    }
  }
}

cache_entry_t *get_min_timestamp_entry() {
  double min_time = INFINITY;
  cache_entry_t *min_access_time_entry;
  for (cache_entry_t *entry = cache; entry < cache+cache_size; ++entry) {
    if ((double) entry -> access_time < min_time) {
      min_time = (double) entry -> access_time;
      min_access_time_entry = entry;
    }
  }
  return min_access_time_entry;
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  int valid_params = valid_cache_params(disk_num, block_num, buf);
  if (valid_params == -1) {
    return -1;
  }
  uint8_t lookup_buf[JBOD_BLOCK_SIZE];
  int exists_already = cache_lookup(disk_num, block_num, lookup_buf);
  if (exists_already == 1) {
    return -1;
  }
  //++num_queries;
  ++clock;

  bool available_space = false;
  for (cache_entry_t *entry = cache; entry < cache+cache_size; ++entry) {    
    if (!(entry -> valid)) {
      memcpy(entry -> block, buf, JBOD_BLOCK_SIZE);
      entry -> disk_num = disk_num;
      entry -> block_num = block_num;
      entry -> access_time = clock;
      entry -> valid = true;
      available_space = true;
      break;
    }
  }

  if (!available_space) {
    cache_entry_t *lru_entry = get_min_timestamp_entry();
    memcpy(lru_entry -> block, buf, JBOD_BLOCK_SIZE);
    lru_entry -> access_time = clock;
    lru_entry -> disk_num = disk_num;
    lru_entry -> block_num = block_num;
  }
  
  return 1;
}

bool cache_enabled(void) {
  return cache_active;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
