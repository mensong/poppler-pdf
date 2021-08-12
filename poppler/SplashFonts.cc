#include "SplashFonts.h"

//------------------------------------------------------------------------
// Type 3 font cache size parameters
#define type3FontCacheAssoc 8
#define type3FontCacheMaxSets 8
#define type3FontCacheSize (128 * 1024)

SplashOutFontFileID::~SplashOutFontFileID() = default;

T3FontCache::T3FontCache(const Ref *fontIDA, double m11A, double m12A, double m21A, double m22A, int glyphXA, int glyphYA, int glyphWA, int glyphHA, bool validBBoxA, bool aa)
{

    fontID = *fontIDA;
    m11 = m11A;
    m12 = m12A;
    m21 = m21A;
    m22 = m22A;
    glyphX = glyphXA;
    glyphY = glyphYA;
    glyphW = glyphWA;
    glyphH = glyphHA;
    validBBox = validBBoxA;
    // sanity check for excessively large glyphs (which most likely
    // indicate an incorrect BBox)
    if (glyphW > INT_MAX / glyphH || glyphW <= 0 || glyphH <= 0 || glyphW * glyphH > 100000) {
        glyphW = glyphH = 100;
        validBBox = false;
    }
    if (aa) {
        glyphSize = glyphW * glyphH;
    } else {
        glyphSize = ((glyphW + 7) >> 3) * glyphH;
    }
    cacheAssoc = type3FontCacheAssoc;
    for (cacheSets = type3FontCacheMaxSets; cacheSets > 1 && cacheSets * cacheAssoc * glyphSize > type3FontCacheSize; cacheSets >>= 1)
        ;
    if (glyphSize < 10485760 / cacheAssoc / cacheSets) {
        cacheData = (unsigned char *)gmallocn_checkoverflow(cacheSets * cacheAssoc, glyphSize);
    } else {
        error(errSyntaxWarning, -1,
              "Not creating cacheData for T3FontCache, it asked for too much memory.\n"
              "       This could teoretically result in wrong rendering,\n"
              "       but most probably the document is bogus.\n"
              "       Please report a bug if you think the rendering may be wrong because of this.");
        cacheData = nullptr;
    }
    if (cacheData != nullptr) {
        cacheTags = (T3FontCacheTag *)gmallocn(cacheSets * cacheAssoc, sizeof(T3FontCacheTag));
        for (int i = 0; i < cacheSets * cacheAssoc; ++i) {
            cacheTags[i].mru = i & (cacheAssoc - 1);
        }
    } else {
        cacheTags = nullptr;
    }
}

T3FontCache::~T3FontCache()
{
    gfree(cacheData);
    gfree(cacheTags);
}
