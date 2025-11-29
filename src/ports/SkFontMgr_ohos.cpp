// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/ports/SkFontMgr_ohos.h"

#include <unordered_set>

#include <native_drawing/drawing_text_font_descriptor.h>
#include <native_drawing/drawing_text_typography.h>

#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontScanner.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkStream.h"
#include "include/core/SkString.h"
#include "include/core/SkTypeface.h"
#include "include/private/base/SkTArray.h"
#include "src/core/SkFontDescriptor.h"
#include "src/core/SkOSFile.h"
#include "src/ports/SkFontScanner_FreeType_priv.h"
#include "src/ports/SkTypeface_FreeType.h"
#include "src/utils/SkOSPath.h"

class SkData;

SkTypeface_OHOS::SkTypeface_OHOS(const SkFontStyle& style,
                                 bool isFixedPitch,
                                 bool sysFont,
                                 const SkString familyName,
                                 const char path[],
                                 int index,
                                 AxisSet axisSet)
        : INHERITED(style, isFixedPitch)
        , fIsSysFont(sysFont)
        , fFamilyName(familyName)
        , fPath(path)
        , fIndex(index)
        , fAxisSet(axisSet) {}

bool SkTypeface_OHOS::isSysFont() const { return fIsSysFont; }

void SkTypeface_OHOS::onGetFamilyName(SkString* familyName) const { *familyName = fFamilyName; }

void SkTypeface_OHOS::onGetFontDescriptor(SkFontDescriptor* desc, bool* isLocal) const {
    if (desc) {
        desc->setFamilyName(fFamilyName.c_str());
        desc->setStyle(this->fontStyle());
        desc->setFactoryId(SkTypeface_FreeType::FactoryId);
    }
    if (isLocal) {
        *isLocal = !this->isSysFont();
    }
}

int SkTypeface_OHOS::getIndex() const { return fIndex; }

std::unique_ptr<SkStreamAsset> SkTypeface_OHOS::onOpenStream(int* ttcIndex) const {
    if (ttcIndex) {
        *ttcIndex = this->getIndex();
    }
    return SkStream::MakeFromFile(fPath.c_str());
}

sk_sp<SkTypeface> SkTypeface_OHOS::onMakeClone(const SkFontArguments& args) const {
    int ttcIndex = args.getCollectionIndex();
    std::unique_ptr<SkStreamAsset> stream(this->onOpenStream(&ttcIndex));

    unsigned int axisCount = args.getVariationDesignPosition().coordinateCount;
    if (axisCount > 0) {
        SkFontScanner_FreeType fontScanner;
        SkString familyName(fFamilyName);
        SkFontStyle style = this->fontStyle();
        bool isFixedPitch = this->isFixedPitch();
        SkFontScanner::AxisDefinitions axisDefs;
        SkFontScanner::VariationPosition current;
        if (!fontScanner.scanInstance(
                    stream.get(), ttcIndex, 0, &familyName,
                    &style, &isFixedPitch, &axisDefs, &current)) {
            SkDebugf("[SkTypeface_OHOS::onMakeClone] Failed to scan font, familyName:%s",
                     familyName.c_str());
            return nullptr;
        }
        const SkFontArguments::VariationPosition currentPos{current.data(), current.size()};
        if (axisDefs.size() > 0) {
            SkFixed axis[axisDefs.size()];
            fontScanner.computeAxisValues(
                    axisDefs, currentPos, args.getVariationDesignPosition(),
                    axis, familyName, &style);

            // setAxisSet
            AxisSet axisSet;
            for (int i = 0; i < axisCount; i++) {
                axisSet.axis.emplace_back(axis[i]);
                axisSet.range.emplace_back(axisDefs.data()[i]);
            }

            // computeFontStyle
            int weight = style.weight();
            int width = style.width();
            auto slant = style.slant();
            for (size_t i = 0; i < axisSet.axis.size(); i++) {
                auto value = SkFixedToScalar(axisSet.axis[i]);
                auto tag = axisSet.range[i].tag;
                if (tag == SkSetFourByteTag('w', 'g', 'h', 't')) {
                    weight = SkScalarFloorToInt(value);
                } else if (tag == SkSetFourByteTag('w', 'd', 't', 'h')) {
                    width = SkScalarFloorToInt(value);
                }
            }
            style = SkFontStyle(weight, width, slant);

            return sk_make_sp<SkTypeface_OHOS>(
                    style, isFixedPitch, true, familyName, fPath.c_str(), ttcIndex, axisSet);
        }
    }
    return sk_ref_sp(this);
}

std::unique_ptr<SkFontData> SkTypeface_OHOS::onMakeFontData() const {
    int index;
    std::unique_ptr<SkStreamAsset> stream(this->onOpenStream(&index));
    if (!stream) {
        SkDebugf("[SkTypeface_OHOS::onMakeFontData] Failed to open stream from file, file name:%s",
                 this->fPath.c_str());
        return nullptr;
    }
    return std::make_unique<SkFontData>(
            std::move(stream), index, 0, fAxisSet.axis.data(), fAxisSet.axis.size(), nullptr, 0);
}

SkFontStyleSet_OHOS::SkFontStyleSet_OHOS(const SkString familyName) : fFamilyName(familyName) {}

SkFontStyleSet_OHOS::~SkFontStyleSet_OHOS() { fStyles.clear(); }

void SkFontStyleSet_OHOS::appendTypeface(sk_sp<SkTypeface> typeface) {
    fStyles.emplace_back(std::move(typeface));
}

int SkFontStyleSet_OHOS::count() { return fStyles.size(); }

void SkFontStyleSet_OHOS::getStyle(int index, SkFontStyle* style, SkString* name) {
    SkASSERT(index >= 0 && index < fStyles.size());
    if (style) {
        *style = fStyles[index]->fontStyle();
    }
    if (name) {
        static const char* names[] = {"invisible",
                                      "thin",
                                      "extralight",
                                      "light",
                                      "normal",
                                      "medium",
                                      "semibold",
                                      "bold",
                                      "extrabold",
                                      "black",
                                      "extrablack"};
        // the value of font weight is between 0 ~ 1000 (refer to SkFontStyle::Weight)
        // the weight is divided by 100 to get the matched name
        unsigned int i = fStyles[index]->fontStyle().weight() / 100;
        if (i < sizeof(names) / sizeof(char*)) {
            name->set(names[i]);
        } else {
            name->reset();
        }
    }
}

sk_sp<SkTypeface> SkFontStyleSet_OHOS::createTypeface(int index) {
    SkASSERT(index >= 0 && index < fStyles.size());
    return fStyles[index];
}

sk_sp<SkTypeface> SkFontStyleSet_OHOS::matchStyle(const SkFontStyle& pattern) {
    return this->matchFontStyle(pattern);
}

SkString SkFontStyleSet_OHOS::getFamilyName() { return fFamilyName; }

// Match the typeface with the closest style
sk_sp<SkTypeface> SkFontStyleSet_OHOS::matchFontStyle(const SkFontStyle& pattern) {
    if (fStyles.empty()) {
        return nullptr;
    }

    uint32_t minDiff = 0xFFFFFFFF;
    int index = 0;
    for (int i = 0; i < fStyles.size(); i++) {
        const SkFontStyle& fontStyle = fStyles[i]->fontStyle();
        uint32_t diff = getFontStyleDifference(pattern, fontStyle);
        if (diff < minDiff) {
            minDiff = diff;
            index = i;
        }
    }
    return fStyles[index];
}

uint32_t SkFontStyleSet_OHOS::getFontStyleDifference(const SkFontStyle& dstStyle,
                                                     const SkFontStyle& srcStyle) {
    int normalWidth = SkFontStyle::kNormal_Width;
    int dstWidth = dstStyle.width();
    int srcWidth = srcStyle.width();

    uint32_t widthDiff = 0;
    // The maximum font width is kUltraExpanded_Width i.e. '9'.
    // If dstWidth <= kNormal_Width (5), first check narrower values, then wider values.
    // If dstWidth > kNormal_Width, first check wider values, then narrower values.
    // When dstWidth and srcWidth are at different side of kNormal_Width,
    // the width difference between them should be more than 5 (9/2+1)
    if (dstWidth <= normalWidth) {
        if (srcWidth <= dstWidth) {
            widthDiff = dstWidth - srcWidth;
        } else {
            widthDiff = srcWidth - dstWidth + 5;
        }
    } else {
        if (srcWidth >= dstWidth) {
            widthDiff = srcWidth - dstWidth;
        } else {
            widthDiff = dstWidth - srcWidth + 5;
        }
    }

    // The first index of diffSlantValue indicates the slant of dstStyle, the second index represent
    // the slant of srcStyle, [0] means upright, [1] means italic, [2] means Oblique.
    // The value of diffSlantValue represents the difference between the slant value of the dstStyle
    // and srcStyle, the larger the difference, the lower the matching priority.
    // For example, if  dstStyle's slant is upright, match srcStyle with upright slant first, then
    // oblique, and finally italic, so diffSlantValue [0] = {0, 2, 1}
    int diffSlantValue[3][3] = {{0, 2, 1}, {2, 0, 1}, {2, 1, 0}};
    uint32_t slantDiff = diffSlantValue[dstStyle.slant()][srcStyle.slant()];

    int dstWeight = dstStyle.weight();
    int srcWeight = srcStyle.weight();
    uint32_t weightDiff = 0;
    // If dstWeight == kNormal_Weight (400), first check kMedium_Weight (500), then smaller values,
    // and then bigger values.
    // If dstWeight == kMedium_Weight, first check kNormal_Weight, then bigger values, and then
    // smaller values. so we set the difference of kNormal_Weight and kMedium_Weight is 50 in order
    // to differ from other cases. If dstWeight < kNormal_Weight, first check smaller values, then
    // bigger values. If dstWeight > kNormal_Weight, first check bigger values, then smaller values.
    // The maximum weight is kExtraBlack_Weight (1000), when dstWeight and srcWeight are at the
    // different side of kNormal_Weight, the weight difference between them should be more than 500
    // (1000/2)
    if ((dstWeight == SkFontStyle::kNormal_Weight && srcWeight == SkFontStyle::kMedium_Weight) ||
        (dstWeight == SkFontStyle::kMedium_Weight && srcWeight == SkFontStyle::kNormal_Weight)) {
        weightDiff = 50;
    } else if (dstWeight <= SkFontStyle::kNormal_Weight) {
        if (srcWeight <= dstWeight) {
            weightDiff = dstWeight - srcWeight;
        } else {
            weightDiff = srcWeight - dstWeight + 500;
        }
    } else if (dstWeight > SkFontStyle::kNormal_Weight) {
        if (srcWeight >= dstWeight) {
            weightDiff = srcWeight - dstWeight;
        } else {
            weightDiff = dstWeight - srcWeight + 500;
        }
    }
    // The first 2 bytes to save weight difference, the third byte to save slant difference,
    // and the fourth byte to save width difference
    uint32_t diff = (widthDiff << 24) + (slantDiff << 16) + weightDiff;
    return diff;
}

SkFontMgr_OHOS::SkFontMgr_OHOS(const SystemFontLoader_OHOS& loader)
        : fDefaultFamily(nullptr), fontConfigInfo(nullptr) {
    loader.loadFonts(&fFamilies);
    OH_Drawing_FontConfigInfoErrorCode code;
    fontConfigInfo = OH_Drawing_GetSystemFontConfigInfo(&code);
    if (code != OH_Drawing_FontConfigInfoErrorCode::SUCCESS_FONT_CONFIG_INFO || !fontConfigInfo) {
        SkDebugf("[SkFontMgr_OHOS] call OH_Drawing_GetSystemFontConfigInfo failed, error code:%d",
                 static_cast<int>(code));
    } else if (fontConfigInfo->fontGenericInfoSize > 0) {
        fDefaultFamily = this->onMatchFamily(fontConfigInfo->fontGenericInfoSet[0].familyName);
    }
}

SkFontMgr_OHOS::~SkFontMgr_OHOS() {
    fFamilies.clear();
    if (!fontConfigInfo) {
        OH_Drawing_DestroySystemFontConfigInfo(fontConfigInfo);
    }
}

int SkFontMgr_OHOS::onCountFamilies() const { return fFamilies.size(); }

void SkFontMgr_OHOS::onGetFamilyName(int index, SkString* familyName) const {
    SkASSERT(index >= 0 && index < fFamilies.size());
    if (!familyName) {
        SkDebugf("[SkFontMgr_OHOS::onGetFamilyName] param familyName is a nullptr");
        return;
    }
    familyName->set(fFamilies[index]->getFamilyName());
}

sk_sp<SkFontStyleSet> SkFontMgr_OHOS::onCreateStyleSet(int index) const {
    SkASSERT(index >= 0 && index < fFamilies.size());
    return fFamilies[index];
}

sk_sp<SkFontStyleSet> SkFontMgr_OHOS::onMatchFamily(const char familyName[]) const {
    if (familyName == nullptr) {
        return fDefaultFamily;
    }
    for (int i = 0; i < fFamilies.size(); ++i) {
        if (fFamilies[i]->getFamilyName().equals(familyName)) {
            return fFamilies[i];
        }
    }
    return nullptr;
}

sk_sp<SkTypeface> SkFontMgr_OHOS::onMatchFamilyStyle(const char familyName[],
                                                     const SkFontStyle& fontStyle) const {
    sk_sp<SkTypeface> tf = matchAlias(familyName, fontStyle);
    if (tf != nullptr) {
        return tf;
    }

    sk_sp<SkFontStyleSet> sset(this->matchFamily(familyName));
    if (sset) {
        return sset->matchStyle(fontStyle);
    }
    return nullptr;
}

sk_sp<SkTypeface> SkFontMgr_OHOS::onMatchFamilyStyleCharacter(const char familyName[],
                                                              const SkFontStyle& style,
                                                              const char* bcp47[],
                                                              int bcp47Count,
                                                              SkUnichar character) const {
    if (!fontConfigInfo) {
        return nullptr;
    }

    int matchIndex = findFallbackGroup(familyName);
    int defaultIndex = findFallbackGroup(nullptr);
    sk_sp<SkTypeface> retTp;
    OH_Drawing_FontFallbackGroup group;
    if (bcp47Count > 0) {
        for (int index : {matchIndex, defaultIndex}) {
            if (index < 0) {
                continue;
            }
            group = fontConfigInfo->fallbackGroupSet[index];
            retTp = findTypeface(group, style, bcp47, bcp47Count, character);
            if (retTp != nullptr) {
                return retTp;
            }
        }
    }
    for (int index : {matchIndex, defaultIndex}) {
        if (index < 0) {
            continue;
        }
        group = fontConfigInfo->fallbackGroupSet[index];
        for (unsigned int i = 0; i < group.fallbackInfoSize; i++) {
            sk_sp<SkFontStyleSet> sset(this->matchFamily(group.fallbackInfoSet[i].familyName));
            if (sset == nullptr) {
                continue;
            }
            for (int j = 0; j < sset->count(); j++) {
                retTp = sset->createTypeface(j);
                if (retTp->unicharToGlyph(character) != 0) {
                    return sset->matchStyle(style);
                }
            }
        }
    }
    SkDebugf(
            "[SkFontMgr_OHOS::onMatchFamilyStyleCharacter] Failed to match character, "
            "character SkUnichar:%d",
            character);
    return nullptr;
}

sk_sp<SkTypeface> SkFontMgr_OHOS::onMakeFromData(sk_sp<SkData> data, int ttcIndex) const {
    return this->makeFromStream(std::make_unique<SkMemoryStream>(std::move(data)), ttcIndex);
}

sk_sp<SkTypeface> SkFontMgr_OHOS::onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream,
                                                        int ttcIndex) const {
    return this->makeFromStream(std::move(stream), SkFontArguments().setCollectionIndex(ttcIndex));
}

sk_sp<SkTypeface> SkFontMgr_OHOS::onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream,
                                                       const SkFontArguments& args) const {
    return SkTypeface_FreeType::MakeFromStream(std::move(stream), args);
}

sk_sp<SkTypeface> SkFontMgr_OHOS::onMakeFromFile(const char path[], int ttcIndex) const {
    std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(path);
    return stream ? this->makeFromStream(std::move(stream), ttcIndex) : nullptr;
}

sk_sp<SkTypeface> SkFontMgr_OHOS::onLegacyMakeTypeface(const char familyName[],
                                                       SkFontStyle style) const {
    sk_sp<SkTypeface> typeface = this->matchFamilyStyle(familyName, style);
    // if familyName is not found, then try the default family
    if (typeface == nullptr && familyName != nullptr) {
        typeface = this->matchFamilyStyle(nullptr, style);
    }
    if (typeface) {
        return typeface;
    }
    SkDebugf("[SkFontMgr_OHOS::onLegacyMakeTypeface] match familyName failed, familyName is %s",
             familyName);
    return nullptr;
}

sk_sp<SkTypeface> SkFontMgr_OHOS::findTypeface(OH_Drawing_FontFallbackGroup& fallbackGroup,
                                               const SkFontStyle& style,
                                               const char* bcp47[],
                                               int bcp47Count,
                                               SkUnichar character) const {
    if (bcp47Count == 0 || fallbackGroup.fallbackInfoSize == 0) {
        return nullptr;
    }

    // example bcp47 code : 'zh-Hans' : ('zh' : iso639 code, 'Hans' : iso15924 code)
    // iso639 code will be taken from bcp47 code, so that we can try to match
    // bcp47 or only iso639. Therefore totalCount need to be 'bcp47Count * 2'
    int totalCount = bcp47Count * 2;
    int tps[totalCount];
    for (int i = 0; i < totalCount; i++) {
        tps[i] = -1;
    }
    // find the families matching the bcp47 list
    for (unsigned int i = 0; i < fallbackGroup.fallbackInfoSize; i++) {
        int ret = compareLangs(
                SkString(fallbackGroup.fallbackInfoSet[i].language), bcp47, bcp47Count, tps);
        if (ret == -1) {
            continue;
        }
        tps[ret] = i;
    }
    // match typeface in families
    // tps[bcp47Count,totalCount-1] is the result of matching the complete bcp47 code
    // tps[0,bcp47Count-1] is the result of matching iso639 code only
    // bcp47[0] is the least significant fallback, bcp47[bcp47Count-1] is the most significant.
    for (int i = totalCount - 1; i >= 0; i--) {
        if (tps[i] == -1) {
            continue;
        }
        sk_sp<SkFontStyleSet> sset(
                this->matchFamily(fallbackGroup.fallbackInfoSet[tps[i]].familyName));
        if (sset == nullptr) {
            continue;
        }
        for (int i = 0; i < sset->count(); i++) {
            sk_sp<SkTypeface> tp = sset->createTypeface(i);
            if (tp->unicharToGlyph(character) != 0) {
                return sset->matchStyle(style);
            }
        }
    }
    return nullptr;
}

int SkFontMgr_OHOS::compareLangs(const SkString& langs,
                                 const char* bcp47[],
                                 int bcp47Count,
                                 const int tps[]) const {
    if (bcp47 == nullptr || bcp47Count == 0) {
        return -1;
    }
    for (int i = bcp47Count - 1; i >= 0; i--) {
        if (tps[i] != -1) {
            continue;
        }
        if (langs.find(bcp47[i]) != -1 ||
            (strcmp(bcp47[i], "zh-CN") == 0 && langs.find("zh-Hans") != -1)) {
            return i + bcp47Count;
        } else {
            const char* iso15924 = strrchr(bcp47[i], '-');
            if (iso15924 == nullptr) {
                continue;
            }
            iso15924++;
            int len = iso15924 - 1 - bcp47[i];
            SkString country(bcp47[i], len);
            if (langs.find(iso15924) != -1 ||
                (strncmp(bcp47[i], "und", strlen("und")) && langs.find(country.c_str()) != -1)) {
                return i;
            }
        }
    }
    return -1;
}

int SkFontMgr_OHOS::findFallbackGroup(const char familyName[]) const {
    if (fontConfigInfo == nullptr) {
        return -1;
    }
    for (size_t i = 0; i < fontConfigInfo->fallbackGroupSize; ++i) {
        if (fontConfigInfo->fallbackGroupSet[i].groupName == familyName) {
            return i;
        }
    }
    return -1;
}

sk_sp<SkTypeface> SkFontMgr_OHOS::matchAlias(const char familyName[], SkFontStyle fontStyle) const {
    if (!fontConfigInfo) {
        return nullptr;
    }

    for (size_t i = 0; i < fontConfigInfo->fontGenericInfoSize; i++) {
        OH_Drawing_FontGenericInfo fontGenericInfo = fontConfigInfo->fontGenericInfoSet[i];
        for (size_t j = 0; j < fontGenericInfo.aliasInfoSize; j++) {
            OH_Drawing_FontAliasInfo aliasInfo = fontGenericInfo.aliasInfoSet[j];
            if (aliasInfo.familyName == nullptr || familyName == nullptr ||
                strcmp(aliasInfo.familyName, familyName) != 0) {
                continue;
            }
            sk_sp<SkFontStyleSet> sset(this->matchFamily(fontGenericInfo.familyName));
            // When the weight value is greater than 0,the font set contains only fonts with the
            // specified weight.When the weight value is equal to 0, the font set contains all
            // fonts.
            if (aliasInfo.weight) {
                fontStyle = SkFontStyle(aliasInfo.weight, fontStyle.width(), fontStyle.slant());
            }
            return sset->matchStyle(fontStyle);
        }
    }
    return nullptr;
}

void SkFontMgr_OHOS::SystemFontLoader_OHOS::loadFonts(SkFontMgr_OHOS::Families* families) const {
    std::unordered_set<std::string> filenameSet;

    OH_Drawing_Array* array =
            OH_Drawing_GetSystemFontFullNamesByType(OH_Drawing_SystemFontType::ALL);
    if (!array) {
        SkDebugf("[loadFonts] Failed to get system font full names by type.");
        return;
    }

    size_t arraySize = OH_Drawing_GetDrawingArraySize(array);
    for (size_t i = 0; i < arraySize; i++) {
        const OH_Drawing_String* strInfo = OH_Drawing_GetSystemFontFullNameByIndex(array, i);
        if (!strInfo) {
            SkDebugf("[loadFonts] Failed to get system font full name by index, index: %d", i);
            continue;
        }

        OH_Drawing_FontDescriptor* fontDesc =
                OH_Drawing_GetFontDescriptorByFullName(strInfo, OH_Drawing_SystemFontType::ALL);
        if (!fontDesc || !fontDesc->path) {
            SkDebugf("[loadFonts] Failed to get font description or path by string info, index: %d",
                     i);
            continue;
        }

        filenameSet.insert(std::string(fontDesc->path));
    }
    OH_Drawing_DestroySystemFontFullNames(array);

    for (std::string filename : filenameSet) {
        std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(filename.c_str());
        if (!stream) {
            SkDebugf("[loadFonts] Failed to make stream from font file, file name:%s",
                     filename.c_str());
            continue;
        }
        parse_face(stream, filename.c_str(), families);
    }
}

SkFontStyleSet_OHOS* SkFontMgr_OHOS::SystemFontLoader_OHOS::find_family(
        SkFontMgr_OHOS::Families& families, const char familyName[]) {
    for (int i = 0; i < families.size(); ++i) {
        if (families[i]->getFamilyName().equals(familyName)) {
            return families[i].get();
        }
    }
    return nullptr;
}

void SkFontMgr_OHOS::SystemFontLoader_OHOS::parse_face(const std::unique_ptr<SkStreamAsset>& stream,
                                                       const char* filename,
                                                       SkFontMgr_OHOS::Families* families) const {
    int numFaces;
    if (!fScanner.scanFile(stream.get(), &numFaces)) {
        SkDebugf("[parse_typeface] Failed to parse font file stream, file name:%s", filename);
        return;
    }

    for (int faceIndex = 0; faceIndex < numFaces; ++faceIndex) {
        parse_instance(stream, filename, families, faceIndex);
    }
}

void SkFontMgr_OHOS::SystemFontLoader_OHOS::parse_instance(
        const std::unique_ptr<SkStreamAsset>& stream,
        const char* filename,
        SkFontMgr_OHOS::Families* families,
        int faceIndex) const {
    int numInstances;
    if (!fScanner.scanFace(stream.get(), faceIndex, &numInstances)) {
        SkDebugf("[parse_instance] Failed to parse font file stream, file name:%s", filename);
        return;
    }
    for (int i = 0; i <= numInstances; ++i) {
        bool isFixedPitch;
        SkString realname;
        SkFontStyle style = SkFontStyle();
        SkFontScanner::AxisDefinitions axisDefs;
        SkFontScanner::VariationPosition current;
        if (!fScanner.scanInstance(
                    stream.get(), faceIndex, i, &realname,
                    &style,&isFixedPitch, &axisDefs, &current)) {
            SkDebugf("[parse_instance] Failed to open file as a font, file name: %s", filename);
            continue;
        }
        SkFontStyleSet_OHOS* addTo = find_family(*families, realname.c_str());
        if (addTo == nullptr) {
            addTo = new SkFontStyleSet_OHOS(realname);
            families->push_back().reset(addTo);
        }
        AxisSet axisSet;
        sk_sp<SkTypeface> typeface = sk_make_sp<SkTypeface_OHOS>(
                style, isFixedPitch, true, realname, filename, (i << 16) + faceIndex, axisSet);
        addTo->appendTypeface(typeface);

        SkTypeface::LocalizedString localizedStr;
        auto iter = typeface->createFamilyNameIterator();
        if (!iter) {
            continue;
        }
        while (iter->next(&localizedStr)) {
            SkString localName = localizedStr.fString;
            if (localName.equals(realname) || localName.isEmpty()) {
                continue;
            }
            SkFontStyleSet_OHOS* localizedAddTo = find_family(*families, localName.c_str());
            if (localizedAddTo == nullptr) {
                localizedAddTo = new SkFontStyleSet_OHOS(localName);
                families->push_back().reset(localizedAddTo);
            }
            sk_sp<SkTypeface> localTypeface = sk_make_sp<SkTypeface_OHOS>(
                    style, isFixedPitch, true, localName, filename, (i << 16) + faceIndex, axisSet);
            localizedAddTo->appendTypeface(localTypeface);
        }
    }
}

SK_API sk_sp<SkFontMgr> SkFontMgr_New_OHOS() {
    return sk_make_sp<SkFontMgr_OHOS>(SkFontMgr_OHOS::SystemFontLoader_OHOS());
}
