#ifndef __RS_REGION_H_INCLUDED__
#define __RS_REGION_H_INCLUDED__

#include <stdint.h>
#include <stdbool.h>

struct _RSRegion;
typedef struct _RSRegion RSRegion;

typedef enum
{
    RS_GZIP, /* RFC 1952 (usually unused) */
    RS_ZLIB, /* RFC 1950 */
    RS_UNKNOWN_COMPRESSION,
} RSCompressionType;

/* opening/closing */
RSRegion* rs_region_open(const char* path, bool write);
void rs_region_close(RSRegion* self);

/* low-level chunk reading */
uint32_t rs_region_get_chunk_timestamp(RSRegion* self, uint8_t x, uint8_t z);
uint32_t rs_region_get_chunk_length(RSRegion* self, uint8_t x, uint8_t z);
RSCompressionType rs_region_get_chunk_compression(RSRegion* self, uint8_t x, uint8_t z);
/* valid until region is closed, or flush called */
void* rs_region_get_chunk_data(RSRegion* self, uint8_t x, uint8_t z);

/* convenience */
#define rs_region_contains_chunk(self, x, z) (rs_region_get_chunk_timestamp((self), (x), (z)) == 0 ? false : true)

/* low-level chunk writing -- pointers to data must remain valid until
 * flush is called. Region must be opened with write == true.
 */
void rs_region_set_chunk_data(RSRegion* self, uint8_t x, uint8_t z, void* data, uint32_t len, RSCompressionType enc);
void rs_region_set_chunk_data_full(RSRegion* self, uint8_t x, uint8_t z, void* data, uint32_t len, RSCompressionType enc, uint32_t timestamp);
void rs_region_clear_chunk(RSRegion* self, uint8_t x, uint8_t z);

/* flushes all cached output, and re-reads file for input.
 * writes are cached until this is called
 */
void rs_region_flush(RSRegion* self);

#endif /* __RS_REGION_H_INCLUDED__ */
