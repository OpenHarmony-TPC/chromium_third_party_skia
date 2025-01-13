
/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SKHEIFDECODERAPI
#define SKHEIFDECODERAPI

#include <memory>
#include <stddef.h>
#include <stdint.h>

enum HeifColorFormat {
    kHeifColorFormat_RGB565,
    kHeifColorFormat_RGBA_8888,
    kHeifColorFormat_BGRA_8888,
    kHeifColorFormat_RGBA_1010102,
};

struct HeifFrameInfo {
    uint32_t mWidth;
    uint32_t mHeight;
    int32_t  mRotationAngle;           // Rotation angle, clockwise, should be multiple of 90
    uint32_t mBytesPerPixel;           // Number of bytes for one pixel
    int64_t mDurationUs;               // Duration of the frame in us
    std::vector<uint8_t> mIccData;     // ICC data array
};

namespace OHOS::NWeb {
class OhosImageDecoderAdapter;
}

class HeifDecoder {
public:
    bool Init(std::unique_ptr<SkStream> stream, HeifFrameInfo* heifInfo);
    bool Decode(HeifFrameInfo* heifInfo);
    bool SetOutputColor(HeifColorFormat colorFormat);
    void* GetDecodeData(uint64_t& size);
    void CloseDecodeData(void* ptr, uint64_t size);
    int32_t GetStride();

    bool getSequenceInfo(HeifFrameInfo* frameInfo, size_t *frameCount) { return false; }
    bool decode(HeifFrameInfo*) { return false; }
    bool decodeSequence(int frameIndex, HeifFrameInfo* frameInfo) { return false; }
    int skipScanlines(int) { return 0; }
    uint32_t getColorDepth() { return 0; }

private:
    static std::unique_ptr<OHOS::NWeb::OhosImageDecoderAdapter> decoder_adapter_;
    static OHOS::NWeb::OhosImageDecoderAdapter* GetDecoderAdapter();
    void SaveDataToFile(void* ptr, uint64_t size);
    HeifColorFormat color_format_;
    sk_sp<SkData> data_;
};

static inline HeifDecoder* createHeifDecoder() { return new HeifDecoder(); }

#endif // SKHEIFDECODERAPI