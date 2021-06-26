//========================================================================
//
// SplashOutputDev.h
//
// Copyright 2003 Glyph & Cog, LLC
//
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2005 Takashi Iwai <tiwai@suse.de>
// Copyright (C) 2009-2016 Thomas Freitag <Thomas.Freitag@alfa.de>
// Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
// Copyright (C) 2010 Christian Feuersänger <cfeuersaenger@googlemail.com>
// Copyright (C) 2011 Andreas Hartmetz <ahartmetz@gmail.com>
// Copyright (C) 2011 Andrea Canciani <ranma42@gmail.com>
// Copyright (C) 2011, 2017 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2012, 2015, 2018-2020 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2015, 2016 William Bader <williambader@hotmail.com>
// Copyright (C) 2018 Stefan Brüns <stefan.bruens@rwth-aachen.de>
// Copyright (C) 2020 Jon Howell <jonh@jonh.net>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#ifndef VECTOROUTPUTDEV_H
#define VECTOROUTPUTDEV_H

#include "splash/SplashTypes.h"
#include "splash/SplashPattern.h"
#include "poppler-config.h"
#include "OutputDev.h"
#include "GfxState.h"
#include "GlobalParams.h"
#include "SplashDynamicPatterns.h"

class PDFDoc;
class SplashBitmap;
class Splash;
class SplashPath;
class SplashFontEngine;
class SplashFont;

class T3FontCache;
struct T3FontCacheTag;
struct T3GlyphStack;

// number of Type 3 fonts to cache
#define splashOutT3FontCacheSize 8

struct SplashOutImageMaskData
{
    ImageStream *imgStr;
    bool invert;
    int width, height, y;
};

struct SplashOutImageData
{
    ImageStream *imgStr;
    GfxImageColorMap *colorMap;
    SplashColorPtr lookup;
    const int *maskColors;
    SplashColorMode colorMode;
    int width, height, y;
    ImageStream *maskStr;
    GfxImageColorMap *maskColorMap;
    SplashColor matteColor;
};

struct SplashOutMaskedImageData
{
    ImageStream *imgStr;
    GfxImageColorMap *colorMap;
    SplashBitmap *mask;
    SplashColorPtr lookup;
    SplashColorMode colorMode;
    int width, height, y;
};

//------------------------------------------------------------------------
// SplashOutputBase
//------------------------------------------------------------------------

// Superclass shared by SplashOutputDev and StrokeOutputDev.
class POPPLER_PRIVATE_EXPORT SplashOutputBase : public OutputDev
{
public:
    // Constructor.
    SplashOutputBase(SplashColorMode colorModeA, int bitmapRowPadA, bool reverseVideoA, SplashColorPtr paperColorA, bool bitmapTopDownA = true, SplashThinLineMode thinLineMode = splashThinLineDefault,
                     bool overprintPreviewA = globalParams->getOverprintPreview());

    // Destructor.
    ~SplashOutputBase() override;

    //----- get info about output device

    // Does this device use tilingPatternFill()?  If this returns false,
    // tiling pattern fills will be reduced to a series of other drawing
    // operations.
    bool useTilingPatternFill() override { return false; }

    // Does this device use functionShadedFill(), axialShadedFill(), and
    // radialShadedFill()?  If this returns false, these shaded fills
    // will be reduced to a series of other drawing operations.
    bool useShadedFills(int type) override { return (type >= 1 && type <= 5) ? true : false; }

    // Does this device use upside-down coordinates?
    // (Upside-down means (0,0) is the top left corner of the page.)
    bool upsideDown() override { return bitmapTopDown; }

    // Does this device use drawChar() or drawString()?
    bool useDrawChar() override { return true; }

    // Does this device use beginType3Char/endType3Char?  Otherwise,
    // text in Type 3 fonts will be drawn with drawChar/drawString.
    bool interpretType3Chars() override { return true; }

    //----- initialization and control

    // Start a page.
    void startPage(int pageNum, GfxState *state, XRef *xref) override;

    // End a page.
    void endPage() override;

    //----- save/restore graphics state
    void saveState(GfxState *state) override;
    void restoreState(GfxState *state) override;

    //----- update graphics state
    void updateAll(GfxState *state) override;
    void updateCTM(GfxState *state, double m11, double m12, double m21, double m22, double m31, double m32) override;
    void updateLineDash(GfxState *state) override;
    void updateFlatness(GfxState *state) override;
    void updateLineJoin(GfxState *state) override;
    void updateLineCap(GfxState *state) override;
    void updateMiterLimit(GfxState *state) override;
    void updateLineWidth(GfxState *state) override;
    void updateStrokeAdjust(GfxState *state) override;
    void updateFillColor(GfxState *state) override;
    void updateStrokeColor(GfxState *state) override;

    //----- update text state
    void updateFont(GfxState *state) override;

    //----- path painting
    void stroke(GfxState *state) override;
    void fill(GfxState *state) override;
    void eoFill(GfxState *state) override;

    //----- path clipping
    void clip(GfxState *state) override;
    void eoClip(GfxState *state) override;
    void clipToStrokePath(GfxState *state) override;

    //----- text drawing
    void drawChar(GfxState *state, double x, double y, double dx, double dy, double originX, double originY, CharCode code, int nBytes, const Unicode *u, int uLen) override;
    bool beginType3Char(GfxState *state, double x, double y, double dx, double dy, CharCode code, const Unicode *u, int uLen) override;
    void endType3Char(GfxState *state) override;
    void beginTextObject(GfxState *state) override;
    void endTextObject(GfxState *state) override;

    //----- image drawing
    void drawImageMask(GfxState *state, Object *ref, Stream *str, int width, int height, bool invert, bool interpolate, bool inlineImg) override;
    void drawImage(GfxState *state, Object *ref, Stream *str, int width, int height, GfxImageColorMap *colorMap, bool interpolate, const int *maskColors, bool inlineImg) override;

    //----- Type 3 font operators
    void type3D0(GfxState *state, double wx, double wy) override;
    void type3D1(GfxState *state, double wx, double wy, double llx, double lly, double urx, double ury) override;

    //----- special access

    // Called to indicate that a new PDF document has been loaded.
    void startDoc(PDFDoc *docA);

    void setPaperColor(SplashColorPtr paperColorA);

    bool isReverseVideo() { return reverseVideo; }
    void setReverseVideo(bool reverseVideoA) { reverseVideo = reverseVideoA; }

    // Get the bitmap and its size.
    SplashBitmap *getBitmap() { return bitmap; }
    int getBitmapWidth();
    int getBitmapHeight();

    // Returns the last rasterized bitmap, transferring ownership to the
    // caller.
    SplashBitmap *takeBitmap();

    // Get the Splash object.
    Splash *getSplash() { return splash; }

    SplashFont *getCurrentFont() { return font; }

#if 1 //~tmp: turn off anti-aliasing temporarily
    bool getVectorAntialias() override;
    void setVectorAntialias(bool vaa) override;
#endif

    bool getFontAntialias() { return fontAntialias; }
    void setFontAntialias(bool anti) { fontAntialias = anti; }

    void setFreeTypeHinting(bool enable, bool enableSlightHinting);
    void setEnableFreeType(bool enable) { enableFreeType = enable; }

protected:
    static SplashPath convertPath(GfxState *state, const GfxPath *path, bool dropEmptySubpaths);
    void doUpdateFont(GfxState *state);
    bool needFontUpdate; // set when the font needs to be updated
    SplashFont *font; // current font

    void setupScreenParams(double hDPI, double vDPI);
    SplashPattern *getColor(GfxGray gray);
    SplashPattern *getColor(GfxRGB *rgb);
    SplashPattern *getColor(GfxCMYK *cmyk);
    SplashPattern *getColor(GfxColor *deviceN);
    static void getMatteColor(SplashColorMode colorMode, GfxImageColorMap *colorMap, const GfxColor *matteColor, SplashColor splashMatteColor);
    void setOverprintMask(GfxColorSpace *colorSpace, bool overprintFlag, int overprintMode, const GfxColor *singleColor, bool grayIndexed = false);
    void drawType3Glyph(GfxState *state, T3FontCache *t3Font, T3FontCacheTag *tag, unsigned char *data);
#ifdef USE_CMS
    bool useIccImageSrc(void *data);
    static void iccTransform(void *data, SplashBitmap *bitmap);
    static bool iccImageSrc(void *data, SplashColorPtr colorLine, unsigned char *alphaLine);
#endif
    static bool imageMaskSrc(void *data, SplashColorPtr line);
    static bool imageSrc(void *data, SplashColorPtr colorLine, unsigned char *alphaLine);
    static bool alphaImageSrc(void *data, SplashColorPtr line, unsigned char *alphaLine);
    static bool maskedImageSrc(void *data, SplashColorPtr line, unsigned char *alphaLine);

    bool keepAlphaChannel; // don't fill with paper color, keep alpha channel

    SplashColorMode colorMode;
    int bitmapRowPad;
    bool bitmapTopDown;
    bool fontAntialias;
    bool vectorAntialias;
    bool overprintPreview;
    bool enableFreeType;
    bool enableFreeTypeHinting;
    bool enableSlightHinting;
    bool reverseVideo; // reverse video mode
    SplashColor paperColor; // paper color
    SplashScreenParams screenParams;
    bool skipHorizText;
    bool skipRotatedText;

    PDFDoc *doc; // the current document
    XRef *xref; // the xref of the current document

    SplashBitmap *bitmap;
    Splash *splash;
    SplashFontEngine *fontEngine;

    T3FontCache * // Type 3 font cache
            t3FontCache[splashOutT3FontCacheSize];
    int nT3Fonts; // number of valid entries in t3FontCache
    T3GlyphStack *t3GlyphStack; // Type 3 glyph context stack

    SplashPath *textClipPath; // clipping path built with text object
};

#endif // VECTOROUTPUTDEV_H
