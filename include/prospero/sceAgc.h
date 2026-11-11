#ifndef PROSPERO_SCE_AGC_H
#define PROSPERO_SCE_AGC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS5_AGC_INIT_VERSION 8u

int32_t sceAgcInit(uint32_t version);

struct ps5_agc_submit_description {
   void *words;
   uint32_t word_count;
   uint8_t flag;
   uint8_t padding[3];
};

int32_t sceAgcDriverSubmitDcb(struct ps5_agc_submit_description *description);

int32_t sceAgcSuspendPoint(void);

int32_t sceAgcDriverSetTFRing(uintptr_t address, uint32_t size);
int32_t sceAgcDriverGetTFRing(uintptr_t *address, uint32_t *size);

int32_t sceAgcDriverSetHsOffchipParam(uint32_t granularity, uint32_t buffering);
int32_t sceAgcDriverGetHsOffchipParam(uint16_t *granularity, uint16_t *buffering);

uint32_t sceAgcDriverGetWaitRenderingPacketSizeInDwords(void);
uint32_t sceAgcDriverWaitUntilSafeForRendering(uint32_t **up, uint32_t words, uint32_t reserved,
                                               uint32_t video, int buffer_index);

struct ps5_agc_command_buffer {
   uint32_t *bottom;
   uint32_t *top;
   uint32_t *up;
   uint32_t *down;
   uintptr_t callback;
   void *user_data;
   uint32_t reserved_dwords;
   uint32_t padding;
};

uint32_t *sceAgcDcbSetFlip(struct ps5_agc_command_buffer *buffer, uint32_t video,
                           int buffer_index, uint32_t mode, int64_t marker);

#ifdef __cplusplus
}
#endif

#endif 
