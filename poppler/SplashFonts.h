#ifndef VECTORFONTS_H
#define VECTORFONTS_H

#include "splash/SplashBitmap.h"
#include "splash/SplashFontFileID.h"
#include "splash/Splash.h"
#include "Object.h"

//------------------------------------------------------------------------
// SplashOutFontFileID
//------------------------------------------------------------------------

class SplashOutFontFileID : public SplashFontFileID
{
public:
    SplashOutFontFileID(const Ref *rA) { r = *rA; }

    ~SplashOutFontFileID() override;

    bool matches(SplashFontFileID *id) override { return ((SplashOutFontFileID *)id)->r == r; }

private:
    Ref r;
};

//------------------------------------------------------------------------
// T3FontCache
//------------------------------------------------------------------------

struct T3FontCacheTag
{
    unsigned short code;
    unsigned short mru; // valid bit (0x8000) and MRU index
};

class T3FontCache
{
public:
    T3FontCache(const Ref *fontID, double m11A, double m12A, double m21A, double m22A, int glyphXA, int glyphYA, int glyphWA, int glyphHA, bool validBBoxA, bool aa);
    ~T3FontCache();
    T3FontCache(const T3FontCache &) = delete;
    T3FontCache &operator=(const T3FontCache &) = delete;
    bool matches(const Ref *idA, double m11A, double m12A, double m21A, double m22A) { return fontID == *idA && m11 == m11A && m12 == m12A && m21 == m21A && m22 == m22A; }

    Ref fontID; // PDF font ID
    double m11, m12, m21, m22; // transform matrix
    int glyphX, glyphY; // pixel offset of glyph bitmaps
    int glyphW, glyphH; // size of glyph bitmaps, in pixels
    bool validBBox; // false if the bbox was [0 0 0 0]
    int glyphSize; // size of glyph bitmaps, in bytes
    int cacheSets; // number of sets in cache
    int cacheAssoc; // cache associativity (glyphs per set)
    unsigned char *cacheData; // glyph pixmap cache
    T3FontCacheTag *cacheTags; // cache tags, i.e., char codes
};

struct T3GlyphStack
{
    unsigned short code; // character code

    bool haveDx; // set after seeing a d0/d1 operator
    bool doNotCache; // set if we see a gsave/grestore before
                     //   the d0/d1

    //----- cache info
    T3FontCache *cache; // font cache for the current font
    T3FontCacheTag *cacheTag; // pointer to cache tag for the glyph
    unsigned char *cacheData; // pointer to cache data for the glyph

    //----- saved state
    SplashBitmap *origBitmap;
    Splash *origSplash;
    double origCTM4, origCTM5;

    T3GlyphStack *next; // next object on stack
};

#endif // VECTORFONTS_H
