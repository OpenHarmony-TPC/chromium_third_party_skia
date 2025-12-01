// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkFontMgr_ohos_DEFINED
#define SkFontMgr_ohos_DEFINED

#include "include/core/SkFontParameters.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontScanner.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkString.h"
#include "include/private/base/SkTArray.h"
#include "src/ports/SkFontScanner_FreeType_priv.h"
#include "src/ports/SkTypeface_FreeType.h"

#include <native_drawing/drawing_text_typography.h>
#include <vector>

class SkData;
class SkFontDescriptor;
class SkStreamAsset;
class SkTypeface;

// To manage the axis values for variable font
struct AxisSet {
    std::vector<SkFixed> axis;                         // the axis values
    std::vector<SkFontParameters::Variation::Axis> range;  // the axis ranges
};

class SkTypeface_OHOS : public SkTypeface_FreeType {
public:
    SkTypeface_OHOS(const SkFontStyle& style,
                    bool isFixedPitch,
                    bool sysFont,
                    const SkString familyName,
                    const char path[],
                    int index,
                    AxisSet axisSet);
    bool isSysFont() const;

protected:
    int getIndex() const;
    void onGetFamilyName(SkString* familyName) const override;
    void onGetFontDescriptor(SkFontDescriptor* desc, bool* isLocal) const override;
    sk_sp<SkTypeface> onMakeClone(const SkFontArguments& args) const override;
    std::unique_ptr<SkFontData> onMakeFontData() const override;
    std::unique_ptr<SkStreamAsset> onOpenStream(int* ttcIndex) const override;

private:
    const bool fIsSysFont;
    const SkString fFamilyName;
    const SkString fPath;
    const int fIndex;

    const AxisSet fAxisSet;  // the axis values for a variable font

    using INHERITED = SkTypeface_FreeType;
};

class SkFontStyleSet_OHOS : public SkFontStyleSet {
public:
    explicit SkFontStyleSet_OHOS(const SkString familyName);
    ~SkFontStyleSet_OHOS();

    int count() override;
    void getStyle(int index, SkFontStyle* style, SkString* name) override;
    sk_sp<SkTypeface> createTypeface(int index) override;
    sk_sp<SkTypeface> matchStyle(const SkFontStyle& pattern) override;

    /** Should only be called during the initial build phase. */
    void appendTypeface(sk_sp<SkTypeface> typeface);
    SkString getFamilyName();

private:
    friend class SkFontMgr_OHOS;

    skia_private::TArray<sk_sp<SkTypeface>> fStyles;
    SkString fFamilyName;

    sk_sp<SkTypeface> matchFontStyle(const SkFontStyle& pattern);
    uint32_t getFontStyleDifference(const SkFontStyle& dstStyle, const SkFontStyle& srcStyle);
};

class SkFontMgr_OHOS : public SkFontMgr {
public:
    typedef skia_private::TArray<sk_sp<SkFontStyleSet_OHOS>> Families;
    class SystemFontLoader_OHOS {
    public:
        SystemFontLoader_OHOS() = default;
        ~SystemFontLoader_OHOS() = default;
        void loadFonts(Families* families) const;

    private:
        static SkFontStyleSet_OHOS* find_family(SkFontMgr_OHOS::Families& families,
                                                const char familyName[]);
        void parse_face(const std::unique_ptr<SkStreamAsset>& stream,
                        const char* filename,
                        SkFontMgr_OHOS::Families* families) const;
        void parse_instance(const std::unique_ptr<SkStreamAsset>& stream,
                            const char* filename,
                            SkFontMgr_OHOS::Families* families,
                            int faceIndex) const;
        SkFontScanner_FreeType fScanner;
    };
    explicit SkFontMgr_OHOS(const SystemFontLoader_OHOS& loader);
    ~SkFontMgr_OHOS() override;

protected:
    int onCountFamilies() const override;
    void onGetFamilyName(int index, SkString* familyName) const override;
    sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override;
    sk_sp<SkFontStyleSet> onMatchFamily(const char familyName[]) const override;
    sk_sp<SkTypeface> onMatchFamilyStyle(const char familyName[],
                                         const SkFontStyle& fontStyle) const override;
    sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char familyName[],
                                                  const SkFontStyle&,
                                                  const char* bcp47[],
                                                  int bcp47Count,
                                                  SkUnichar character) const override;
    sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data, int ttcIndex) const override;
    sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset>,
                                            int ttcIndex) const override;
    sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset>,
                                           const SkFontArguments&) const override;
    sk_sp<SkTypeface> onMakeFromFile(const char path[], int ttcIndex) const override;
    sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[],
                                           SkFontStyle style) const override;

private:
    Families fFamilies;
    sk_sp<SkFontStyleSet> fDefaultFamily;
    OH_Drawing_FontConfigInfo* fontConfigInfo;

    sk_sp<SkTypeface> findTypeface(OH_Drawing_FontFallbackGroup& fallbackGroup,
                                   const SkFontStyle& style,
                                   const char* bcp47[],
                                   int bcp47Count,
                                   SkUnichar character) const;
    int compareLangs(const SkString& langs,
                     const char* bcp47[],
                     int bcp47Count,
                     const int tps[]) const;
    int findFallbackGroup(const char familyName[]) const;
    // match alias of generic fonts in OHOS
    sk_sp<SkTypeface> matchAlias(const char familyName[], SkFontStyle fontStyle) const;
};

SK_API sk_sp<SkFontMgr> SkFontMgr_New_OHOS();

#endif  // SkFontMgr_ohos_DEFINED