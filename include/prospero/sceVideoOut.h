#ifndef PROSPERO_SCE_VIDEOOUT_H
#define PROSPERO_SCE_VIDEOOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS5_VIDEO_OUT_USER_SYSTEM 0xff

#define PS5_VIDEO_OUT_PIXEL_FORMAT_B8G8R8A8_SDR UINT64_C(0x8000000000000000)

#define PS5_VIDEO_OUT_TILING_64KB_R_X 0u

#define PS5_VIDEO_OUT_ATTRIBUTE_BYTES 80

struct ps5_video_out_buffer {
   void *data;
   void *metadata;
   void *reserved[2];
};

typedef struct SceVideoOutOutputStatus {
	uint32_t resolution;
	uint32_t dynamicRange;
	uint64_t refreshRate;
	uint64_t flags;
	uint64_t _reserved[3];
} SceVideoOutOutputStatus;

#define PS5_VIDEO_OUT_FLIP_STATUS_WORDS 16
#define PS5_VIDEO_OUT_FLIP_STATUS_SHOWN_ARGUMENT 3

#define PS5_VIDEO_OUT_FLIP_VSYNC 1

#define PS5_VIDEO_OUT_ERROR_BUSY ((int)0x80290009)

#define PS5_VIDEO_OUT_MODE_HIGH_FRAME_RATE 15u
#define PS5_VIDEO_OUT_MODE_RESTORE 1u

int sceVideoOutOpen(int32_t user, int32_t bus, int32_t index, const void *parameter);
int sceVideoOutClose(int32_t handle);

int sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
void sceVideoOutSetBufferAttribute2(void *attribute, uint64_t pixel_format, uint32_t tiling,
                                    uint32_t width, uint32_t height, uint64_t reserved1,
                                    uint32_t reserved2, uint64_t reserved3);

int sceVideoOutRegisterBuffers2(int32_t handle, int32_t set, int32_t start,
                                struct ps5_video_out_buffer *buffers, int32_t count,
                                void *attribute, int32_t reserved, void *option);
int sceVideoOutUnregisterBuffers(int32_t handle, int32_t set);

int sceVideoOutSubmitFlip(int32_t handle, int32_t buffer_index, uint32_t mode, int64_t argument);
int sceVideoOutGetFlipStatus(int32_t handle, uint64_t status[PS5_VIDEO_OUT_FLIP_STATUS_WORDS]);

int sceVideoOutIsFlipPending(int32_t handle);
int sceVideoOutWaitVblank(int32_t handle);

int sceVideoOutIsOutputSupported(int32_t handle, uint32_t mode, const void *reserved1,
                                 const void *reserved2, const void *reserved3);
int sceVideoOutConfigureOutput(int32_t handle, uint32_t mode, const void *reserved1,
                               const void *reserved2, const void *reserved3);
int32_t sceVideoOutGetOutputStatus(int32_t handle, SceVideoOutOutputStatus *status);

#ifdef __cplusplus
}
#endif

#endif 
