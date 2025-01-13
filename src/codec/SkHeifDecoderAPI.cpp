
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

#ifdef SK_HAS_HEIF_LIBRARY
#include <sys/mman.h>
#include <fstream>

#include "base/logging.h"
#include "base/containers/span.h"
#include "include/core/SkStream.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"

#include "SkHeifDecoderAPI.h"

std::unique_ptr<OHOS::NWeb::OhosImageDecoderAdapter> HeifDecoder::decoder_adapter_;

#define HEIF_BYTES_PER_PIXEL_RGBA_8888 4

void HeifDecoder::SaveDataToFile(void* ptr, uint64_t size)
{
    static const std::string SANDBOX = "/data/storage/el2/base/files/";

    std::string fileName = SANDBOX + "heif";
    std::string mapString = "_w_" + std::to_string(GetDecoderAdapter()->GetImageWidth()) +
                            "_h_" + std::to_string(GetDecoderAdapter()->GetImageHeight()) +
                            "_stride_" + std::to_string(GetDecoderAdapter()->GetStride()) +
                            "_size_" + std::to_string(size);
    fileName += mapString + ".dat";

    std::ofstream outFile(fileName, std::ofstream::out);
    if (!outFile.is_open()) {
        LOG(ERROR) << "[HeifSupport] HeifDecoder::SaveDataToFile open " << fileName << " failed.";
        outFile.close();
        return;
    }

    outFile.write(reinterpret_cast<const char*>(ptr), size);
    if (outFile.fail()) {
        LOG(ERROR) << "[HeifSupport] HeifDecoder::SaveDataToFile write " << fileName << " failed.";
    }
    LOG(DEBUG) << "[HeifSupport] HeifDecoder::SaveDataToFile close " << fileName;
    outFile.close();
}

bool HeifDecoder::Init(std::unique_ptr<SkStream> stream, HeifFrameInfo* heifInfo)
{
    auto skData = SkData::MakeFromStream(stream.get(), stream->getLength());
    if (skData == nullptr) {
        LOG(ERROR) << "[HeifSupport] HeifDecoder::Init skData is null.";
        return false;
    }

    base::span<const uint8_t> encodedData = base::make_span(skData->bytes(), skData->size());
    if (!GetDecoderAdapter()->ParseImageInfo(encodedData.data(), (uint32_t)encodedData.size())) {
        LOG(ERROR) << "[HeifSupport] HeifDecoder::Init ParseImageInfo failed.";
        return false;
    }

    heifInfo->mHeight = GetDecoderAdapter()->GetImageHeight();
    heifInfo->mWidth = GetDecoderAdapter()->GetImageWidth();

    data_ = std::move(skData);
    return true;
}

bool HeifDecoder::Decode(HeifFrameInfo* heifInfo)
{
    bool useYuv = (color_format_ == kHeifColorFormat_RGBA_8888) ? false : true;
    if (!useYuv) {
        heifInfo->mBytesPerPixel = HEIF_BYTES_PER_PIXEL_RGBA_8888;
    }

    return GetDecoderAdapter()->Decode((const uint8_t*)data_->bytes(), (uint32_t)data_->size(),
                                       OHOS::NWeb::AllocatorType::kDmaAlloc, useYuv);
}

bool HeifDecoder::SetOutputColor(HeifColorFormat colorFormat)
{
    color_format_ = colorFormat;
    return true;
}

void* HeifDecoder::GetDecodeData(uint64_t& size)
{
    size = GetDecoderAdapter()->GetSize();
    void* ptr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE,
                     GetDecoderAdapter()->GetFd(), GetDecoderAdapter()->GetOffset());
    if (ptr == MAP_FAILED) {
        return nullptr;
    }
    return ptr;
}

int32_t HeifDecoder::GetStride()
{
    return GetDecoderAdapter()->GetStride();
}

void HeifDecoder::CloseDecodeData(void* ptr, uint64_t size)
{
    munmap(ptr, size);
    return;
}

OHOS::NWeb::OhosImageDecoderAdapter* HeifDecoder::GetDecoderAdapter() {
    if (!decoder_adapter_) {
        decoder_adapter_ = OHOS::NWeb::OhosAdapterHelper::GetInstance().CreateOhosImageDecoderAdapter();
    }

    return decoder_adapter_.get();
}

#endif // SK_HAS_HEIF_LIBRARY