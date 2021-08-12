//========================================================================
//
// SplashOutputBase.cc
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
// Copyright (C) 2006 Stefan Schweizer <genstef@gentoo.org>
// Copyright (C) 2006-2020 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2006 Krzysztof Kowalczyk <kkowalczyk@gmail.com>
// Copyright (C) 2006 Scott Turner <scotty1024@mac.com>
// Copyright (C) 2007 Koji Otani <sho@bbr.jp>
// Copyright (C) 2009 Petr Gajdos <pgajdos@novell.com>
// Copyright (C) 2009-2016, 2020 Thomas Freitag <Thomas.Freitag@alfa.de>
// Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
// Copyright (C) 2009, 2014-2016, 2019 William Bader <williambader@hotmail.com>
// Copyright (C) 2010 Patrick Spendrin <ps_ml@gmx.de>
// Copyright (C) 2010 Brian Cameron <brian.cameron@oracle.com>
// Copyright (C) 2010 Paweł Wiejacha <pawel.wiejacha@gmail.com>
// Copyright (C) 2010 Christian Feuersänger <cfeuersaenger@googlemail.com>
// Copyright (C) 2011 Andreas Hartmetz <ahartmetz@gmail.com>
// Copyright (C) 2011 Andrea Canciani <ranma42@gmail.com>
// Copyright (C) 2011, 2012, 2017 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2013 Lu Wang <coolwanglu@gmail.com>
// Copyright (C) 2013 Li Junling <lijunling@sina.com>
// Copyright (C) 2014 Ed Porras <ed@moto-research.com>
// Copyright (C) 2014 Richard PALO <richard@netbsd.org>
// Copyright (C) 2015 Tamas Szekeres <szekerest@gmail.com>
// Copyright (C) 2015 Kenji Uno <ku@digitaldolphins.jp>
// Copyright (C) 2016 Takahiro Hashimoto <kenya888.en@gmail.com>
// Copyright (C) 2017 Even Rouault <even.rouault@spatialys.com>
// Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, <info@kdab.com>. Work sponsored by the LiMux project of the city of Munich
// Copyright (C) 2018, 2019 Stefan Brüns <stefan.bruens@rwth-aachen.de>
// Copyright (C) 2018 Adam Reichold <adam.reichold@t-online.de>
// Copyright (C) 2019 Christian Persch <chpe@src.gnome.org>
// Copyright (C) 2020 Oliver Sander <oliver.sander@tu-dresden.de>
// Copyright (C) 2020 Jon Howell <jonh@jonh.net>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#include <config.h>

#include <cstring>
#include <cmath>
#include <algorithm>
#include "goo/gfile.h"
#include "GlobalParams.h"
#include "Error.h"
#include "Object.h"
#include "Gfx.h"
#include "GfxFont.h"
#include "Page.h"
#include "PDFDoc.h"
#include "Link.h"
#include "FontEncodingTables.h"
#include "fofi/FoFiTrueType.h"
#include "splash/SplashBitmap.h"
#include "splash/SplashGlyphBitmap.h"
#include "splash/SplashPattern.h"
#include "splash/SplashScreen.h"
#include "splash/SplashPath.h"
#include "splash/SplashState.h"
#include "splash/SplashErrorCodes.h"
#include "splash/SplashFontEngine.h"
#include "splash/SplashFont.h"
#include "splash/SplashFontFile.h"
#include "splash/SplashFontFileID.h"
#include "splash/SplashMath.h"
#include "splash/Splash.h"
#include "SplashFonts.h"
#include "SplashOutputBase.h"

static const double s_minLineWidth = 0.0;

//------------------------------------------------------------------------
// SplashTransparencyGroup
//------------------------------------------------------------------------

struct SplashTransparencyGroup
{
    int tx, ty; // translation coordinates
    SplashBitmap *tBitmap; // bitmap for transparency group
    SplashBitmap *softmask; // bitmap for softmasks
    GfxColorSpace *blendingColorSpace;
    bool isolated;

    //----- for knockout
    SplashBitmap *shape;
    bool knockout;
    SplashCoord knockoutOpacity;
    bool fontAA;

    //----- saved state
    SplashBitmap *origBitmap;
    Splash *origSplash;

    SplashTransparencyGroup *next;
};

//------------------------------------------------------------------------
// SplashOutputBase
//------------------------------------------------------------------------

SplashOutputBase::SplashOutputBase(SplashColorMode colorModeA, int bitmapRowPadA, bool reverseVideoA, SplashColorPtr paperColorA, bool bitmapTopDownA, SplashThinLineMode thinLineMode, bool overprintPreviewA)
{
    colorMode = colorModeA;
    bitmapRowPad = bitmapRowPadA;
    bitmapTopDown = bitmapTopDownA;
    fontAntialias = true;
    vectorAntialias = true;
    overprintPreview = overprintPreviewA;
    enableFreeType = true;
    enableFreeTypeHinting = false;
    enableSlightHinting = false;
    setupScreenParams(72.0, 72.0);
    reverseVideo = reverseVideoA;
    if (paperColorA != nullptr) {
        splashColorCopy(paperColor, paperColorA);
    } else {
        splashClearColor(paperColor);
    }
    skipHorizText = false;
    skipRotatedText = false;
    keepAlphaChannel = paperColorA == nullptr;

    doc = nullptr;

    bitmap = new SplashBitmap(1, 1, bitmapRowPad, colorMode, colorMode != splashModeMono1, bitmapTopDown);
    splash = new Splash(bitmap, vectorAntialias, &screenParams);
    splash->setMinLineWidth(s_minLineWidth);
    splash->setThinLineMode(thinLineMode);
    splash->clear(paperColor, 0);

    fontEngine = nullptr;

    nT3Fonts = 0;
    t3GlyphStack = nullptr;

    font = nullptr;
    needFontUpdate = false;
    textClipPath = nullptr;
    xref = nullptr;
}

void SplashOutputBase::setupScreenParams(double hDPI, double vDPI)
{
    screenParams.size = -1;
    screenParams.dotRadius = -1;
    screenParams.gamma = (SplashCoord)1.0;
    screenParams.blackThreshold = (SplashCoord)0.0;
    screenParams.whiteThreshold = (SplashCoord)1.0;

    // use clustered dithering for resolution >= 300 dpi
    // (compare to 299.9 to avoid floating point issues)
    if (hDPI > 299.9 && vDPI > 299.9) {
        screenParams.type = splashScreenStochasticClustered;
        if (screenParams.size < 0) {
            screenParams.size = 64;
        }
        if (screenParams.dotRadius < 0) {
            screenParams.dotRadius = 2;
        }
    } else {
        screenParams.type = splashScreenDispersed;
        if (screenParams.size < 0) {
            screenParams.size = 4;
        }
    }
}

SplashOutputBase::~SplashOutputBase()
{
    int i;

    for (i = 0; i < nT3Fonts; ++i) {
        delete t3FontCache[i];
    }
    if (fontEngine) {
        delete fontEngine;
    }
    if (splash) {
        delete splash;
    }
    if (bitmap) {
        delete bitmap;
    }
    delete textClipPath;
}

void SplashOutputBase::startDoc(PDFDoc *docA)
{
    int i;

    doc = docA;
    if (fontEngine) {
        delete fontEngine;
    }
    fontEngine = new SplashFontEngine(enableFreeType, enableFreeTypeHinting, enableSlightHinting, getFontAntialias() && colorMode != splashModeMono1);
    for (i = 0; i < nT3Fonts; ++i) {
        delete t3FontCache[i];
    }
    nT3Fonts = 0;
}

void SplashOutputBase::startPage(int pageNum, GfxState *state, XRef *xrefA)
{
    int w, h;
    SplashCoord mat[6];
    SplashColor color;

    xref = xrefA;
    if (state) {
        setupScreenParams(state->getHDPI(), state->getVDPI());
        w = (int)(state->getPageWidth() + 0.5);
        if (w <= 0) {
            w = 1;
        }
        h = (int)(state->getPageHeight() + 0.5);
        if (h <= 0) {
            h = 1;
        }
    } else {
        w = h = 1;
    }
    SplashThinLineMode thinLineMode = splashThinLineDefault;
    if (splash) {
        thinLineMode = splash->getThinLineMode();
        delete splash;
        splash = nullptr;
    }
    if (!bitmap || w != bitmap->getWidth() || h != bitmap->getHeight()) {
        if (bitmap) {
            delete bitmap;
            bitmap = nullptr;
        }
        bitmap = new SplashBitmap(w, h, bitmapRowPad, colorMode, colorMode != splashModeMono1, bitmapTopDown);
        if (!bitmap->getDataPtr()) {
            delete bitmap;
            w = h = 1;
            bitmap = new SplashBitmap(w, h, bitmapRowPad, colorMode, colorMode != splashModeMono1, bitmapTopDown);
        }
    }
    splash = new Splash(bitmap, vectorAntialias, &screenParams);
    splash->setThinLineMode(thinLineMode);
    splash->setMinLineWidth(s_minLineWidth);
    if (state) {
        const double *ctm = state->getCTM();
        mat[0] = (SplashCoord)ctm[0];
        mat[1] = (SplashCoord)ctm[1];
        mat[2] = (SplashCoord)ctm[2];
        mat[3] = (SplashCoord)ctm[3];
        mat[4] = (SplashCoord)ctm[4];
        mat[5] = (SplashCoord)ctm[5];
        splash->setMatrix(mat);
    }
    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8:
        color[0] = 0;
        break;
    case splashModeXBGR8:
        color[3] = 255;
        // fallthrough
    case splashModeRGB8:
    case splashModeBGR8:
        color[0] = color[1] = color[2] = 0;
        break;
    case splashModeCMYK8:
        color[0] = color[1] = color[2] = color[3] = 0;
        break;
    case splashModeDeviceN8:
        splashClearColor(color);
        break;
    }
    splash->setStrokePattern(new SplashSolidColor(color));
    splash->setFillPattern(new SplashSolidColor(color));
    splash->setLineCap(splashLineCapButt);
    splash->setLineJoin(splashLineJoinMiter);
    splash->setLineDash(nullptr, 0, 0);
    splash->setMiterLimit(10);
    splash->setFlatness(1);
    // the SA parameter supposedly defaults to false, but Acrobat
    // apparently hardwires it to true
    splash->setStrokeAdjust(true);
    splash->clear(paperColor, 0);
}

void SplashOutputBase::endPage()
{
    if (colorMode != splashModeMono1 && !keepAlphaChannel) {
        splash->compositeBackground(paperColor);
    }
}

void SplashOutputBase::saveState(GfxState *state)
{
    splash->saveState();
    if (t3GlyphStack && !t3GlyphStack->haveDx) {
        t3GlyphStack->doNotCache = true;
        error(errSyntaxWarning, -1, "Save (q) operator before d0/d1 in Type 3 glyph");
    }
}

void SplashOutputBase::restoreState(GfxState *state)
{
    splash->restoreState();
    needFontUpdate = true;
    if (t3GlyphStack && !t3GlyphStack->haveDx) {
        t3GlyphStack->doNotCache = true;
        error(errSyntaxWarning, -1, "Restore (Q) operator before d0/d1 in Type 3 glyph");
    }
}

void SplashOutputBase::updateAll(GfxState *state)
{
    updateLineDash(state);
    updateLineJoin(state);
    updateLineCap(state);
    updateLineWidth(state);
    updateFlatness(state);
    updateMiterLimit(state);
    updateStrokeAdjust(state);
    updateFillColorSpace(state);
    updateFillColor(state);
    updateStrokeColorSpace(state);
    updateStrokeColor(state);
    needFontUpdate = true;
}

void SplashOutputBase::updateCTM(GfxState *state, double m11, double m12, double m21, double m22, double m31, double m32)
{
    SplashCoord mat[6];

    const double *ctm = state->getCTM();
    mat[0] = (SplashCoord)ctm[0];
    mat[1] = (SplashCoord)ctm[1];
    mat[2] = (SplashCoord)ctm[2];
    mat[3] = (SplashCoord)ctm[3];
    mat[4] = (SplashCoord)ctm[4];
    mat[5] = (SplashCoord)ctm[5];
    splash->setMatrix(mat);
}

void SplashOutputBase::updateLineDash(GfxState *state)
{
    double *dashPattern;
    int dashLength;
    double dashStart;
    SplashCoord dash[20];
    int i;

    state->getLineDash(&dashPattern, &dashLength, &dashStart);
    if (dashLength > 20) {
        dashLength = 20;
    }
    for (i = 0; i < dashLength; ++i) {
        dash[i] = (SplashCoord)dashPattern[i];
        if (dash[i] < 0) {
            dash[i] = 0;
        }
    }
    splash->setLineDash(dash, dashLength, (SplashCoord)dashStart);
}

void SplashOutputBase::updateFlatness(GfxState *state)
{
#if 0 // Acrobat ignores the flatness setting, and always renders curves
      // with a fairly small flatness value
   splash->setFlatness(state->getFlatness());
#endif
}

void SplashOutputBase::updateLineJoin(GfxState *state)
{
    splash->setLineJoin(state->getLineJoin());
}

void SplashOutputBase::updateLineCap(GfxState *state)
{
    splash->setLineCap(state->getLineCap());
}

void SplashOutputBase::updateMiterLimit(GfxState *state)
{
    splash->setMiterLimit(state->getMiterLimit());
}

void SplashOutputBase::updateLineWidth(GfxState *state)
{
    splash->setLineWidth(state->getLineWidth());
}

void SplashOutputBase::updateStrokeAdjust(GfxState * /*state*/)
{
#if 0 // the SA parameter supposedly defaults to false, but Acrobat
      // apparently hardwires it to true
  splash->setStrokeAdjust(state->getStrokeAdjust());
#endif
}

void SplashOutputBase::updateFillColor(GfxState *state)
{
    GfxGray gray;
    GfxRGB rgb;
    GfxCMYK cmyk;
    GfxColor deviceN;

    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8:
        state->getFillGray(&gray);
        splash->setFillPattern(getColor(gray));
        break;
    case splashModeXBGR8:
    case splashModeRGB8:
    case splashModeBGR8:
        state->getFillRGB(&rgb);
        splash->setFillPattern(getColor(&rgb));
        break;
    case splashModeCMYK8:
        state->getFillCMYK(&cmyk);
        splash->setFillPattern(getColor(&cmyk));
        break;
    case splashModeDeviceN8:
        state->getFillDeviceN(&deviceN);
        splash->setFillPattern(getColor(&deviceN));
        break;
    }
}

void SplashOutputBase::updateStrokeColor(GfxState *state)
{
    GfxGray gray;
    GfxRGB rgb;
    GfxCMYK cmyk;
    GfxColor deviceN;

    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8:
        state->getStrokeGray(&gray);
        splash->setStrokePattern(getColor(gray));
        break;
    case splashModeXBGR8:
    case splashModeRGB8:
    case splashModeBGR8:
        state->getStrokeRGB(&rgb);
        splash->setStrokePattern(getColor(&rgb));
        break;
    case splashModeCMYK8:
        state->getStrokeCMYK(&cmyk);
        splash->setStrokePattern(getColor(&cmyk));
        break;
    case splashModeDeviceN8:
        state->getStrokeDeviceN(&deviceN);
        splash->setStrokePattern(getColor(&deviceN));
        break;
    }
}

SplashPattern *SplashOutputBase::getColor(GfxGray gray)
{
    SplashColor color;

    if (reverseVideo) {
        gray = gfxColorComp1 - gray;
    }
    color[0] = colToByte(gray);
    return new SplashSolidColor(color);
}

SplashPattern *SplashOutputBase::getColor(GfxRGB *rgb)
{
    GfxColorComp r, g, b;
    SplashColor color;

    if (reverseVideo) {
        r = gfxColorComp1 - rgb->r;
        g = gfxColorComp1 - rgb->g;
        b = gfxColorComp1 - rgb->b;
    } else {
        r = rgb->r;
        g = rgb->g;
        b = rgb->b;
    }
    color[0] = colToByte(r);
    color[1] = colToByte(g);
    color[2] = colToByte(b);
    if (colorMode == splashModeXBGR8)
        color[3] = 255;
    return new SplashSolidColor(color);
}

SplashPattern *SplashOutputBase::getColor(GfxCMYK *cmyk)
{
    SplashColor color;

    color[0] = colToByte(cmyk->c);
    color[1] = colToByte(cmyk->m);
    color[2] = colToByte(cmyk->y);
    color[3] = colToByte(cmyk->k);
    return new SplashSolidColor(color);
}

SplashPattern *SplashOutputBase::getColor(GfxColor *deviceN)
{
    SplashColor color;

    for (int i = 0; i < 4 + SPOT_NCOMPS; i++)
        color[i] = colToByte(deviceN->c[i]);
    return new SplashSolidColor(color);
}

void SplashOutputBase::getMatteColor(SplashColorMode colorMode, GfxImageColorMap *colorMap, const GfxColor *matteColorIn, SplashColor matteColor)
{
    GfxGray gray;
    GfxRGB rgb;
    GfxCMYK cmyk;
    GfxColor deviceN;

    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8:
        colorMap->getColorSpace()->getGray(matteColorIn, &gray);
        matteColor[0] = colToByte(gray);
        break;
    case splashModeRGB8:
    case splashModeBGR8:
        colorMap->getColorSpace()->getRGB(matteColorIn, &rgb);
        matteColor[0] = colToByte(rgb.r);
        matteColor[1] = colToByte(rgb.g);
        matteColor[2] = colToByte(rgb.b);
        break;
    case splashModeXBGR8:
        colorMap->getColorSpace()->getRGB(matteColorIn, &rgb);
        matteColor[0] = colToByte(rgb.r);
        matteColor[1] = colToByte(rgb.g);
        matteColor[2] = colToByte(rgb.b);
        matteColor[3] = 255;
        break;
    case splashModeCMYK8:
        colorMap->getColorSpace()->getCMYK(matteColorIn, &cmyk);
        matteColor[0] = colToByte(cmyk.c);
        matteColor[1] = colToByte(cmyk.m);
        matteColor[2] = colToByte(cmyk.y);
        matteColor[3] = colToByte(cmyk.k);
        break;
    case splashModeDeviceN8:
        colorMap->getColorSpace()->getDeviceN(matteColorIn, &deviceN);
        for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
            matteColor[cp] = colToByte(deviceN.c[cp]);
        break;
    }
}

void SplashOutputBase::setOverprintMask(GfxColorSpace *colorSpace, bool overprintFlag, int overprintMode, const GfxColor *singleColor, bool grayIndexed)
{
    unsigned int mask;
    GfxCMYK cmyk;
    bool additive = false;
    int i;

    if (colorSpace->getMode() == csIndexed) {
        setOverprintMask(((GfxIndexedColorSpace *)colorSpace)->getBase(), overprintFlag, overprintMode, singleColor, grayIndexed);
        return;
    }
    if (overprintFlag && overprintPreview) {
        mask = colorSpace->getOverprintMask();
        if (singleColor && overprintMode && colorSpace->getMode() == csDeviceCMYK) {
            colorSpace->getCMYK(singleColor, &cmyk);
            if (cmyk.c == 0) {
                mask &= ~1;
            }
            if (cmyk.m == 0) {
                mask &= ~2;
            }
            if (cmyk.y == 0) {
                mask &= ~4;
            }
            if (cmyk.k == 0) {
                mask &= ~8;
            }
        }
        if (grayIndexed) {
            mask &= ~7;
        } else if (colorSpace->getMode() == csSeparation) {
            GfxSeparationColorSpace *deviceSep = (GfxSeparationColorSpace *)colorSpace;
            additive = deviceSep->getName()->cmp("All") != 0 && mask == 0x0f && !deviceSep->isNonMarking();
        } else if (colorSpace->getMode() == csDeviceN) {
            GfxDeviceNColorSpace *deviceNCS = (GfxDeviceNColorSpace *)colorSpace;
            additive = mask == 0x0f && !deviceNCS->isNonMarking();
            for (i = 0; i < deviceNCS->getNComps() && additive; i++) {
                if (deviceNCS->getColorantName(i) == "Cyan") {
                    additive = false;
                } else if (deviceNCS->getColorantName(i) == "Magenta") {
                    additive = false;
                } else if (deviceNCS->getColorantName(i) == "Yellow") {
                    additive = false;
                } else if (deviceNCS->getColorantName(i) == "Black") {
                    additive = false;
                }
            }
        }
    } else {
        mask = 0xffffffff;
    }
    splash->setOverprintMask(mask, additive);
}

void SplashOutputBase::updateFont(GfxState * /*state*/)
{
    needFontUpdate = true;
}

void SplashOutputBase::doUpdateFont(GfxState *state)
{
    GfxFont *gfxFont;
    GfxFontLoc *fontLoc;
    GfxFontType fontType;
    SplashOutFontFileID *id = nullptr;
    SplashFontFile *fontFile;
    SplashFontSrc *fontsrc = nullptr;
    FoFiTrueType *ff;
    GooString *fileName;
    char *tmpBuf;
    int tmpBufLen;
    const double *textMat;
    double m11, m12, m21, m22, fontSize;
    int faceIndex = 0;
    SplashCoord mat[4];
    bool recreateFont = false;
    bool doAdjustFontMatrix = false;

    needFontUpdate = false;
    font = nullptr;
    fileName = nullptr;
    tmpBuf = nullptr;
    fontLoc = nullptr;

    if (!(gfxFont = state->getFont())) {
        goto err1;
    }
    fontType = gfxFont->getType();
    if (fontType == fontType3) {
        goto err1;
    }

    // sanity-check the font size - skip anything larger than 10 inches
    // (this avoids problems allocating memory for the font cache)
    if (state->getTransformedFontSize() > 10 * (state->getHDPI() + state->getVDPI())) {
        goto err1;
    }

    // check the font file cache
reload:
    delete id;
    delete fontLoc;
    fontLoc = nullptr;
    if (fontsrc && !fontsrc->isFile) {
        fontsrc->unref();
        fontsrc = nullptr;
    }

    id = new SplashOutFontFileID(gfxFont->getID());
    if ((fontFile = fontEngine->getFontFile(id))) {
        delete id;

    } else {

        if (!(fontLoc = gfxFont->locateFont((xref) ? xref : doc->getXRef(), nullptr))) {
            error(errSyntaxError, -1, "Couldn't find a font for '{0:s}'", gfxFont->getName() ? gfxFont->getName()->c_str() : "(unnamed)");
            goto err2;
        }

        // embedded font
        if (fontLoc->locType == gfxFontLocEmbedded) {
            // if there is an embedded font, read it to memory
            tmpBuf = gfxFont->readEmbFontFile((xref) ? xref : doc->getXRef(), &tmpBufLen);
            if (!tmpBuf)
                goto err2;

            // external font
        } else { // gfxFontLocExternal
            fileName = fontLoc->path;
            fontType = fontLoc->fontType;
            doAdjustFontMatrix = true;
        }

        fontsrc = new SplashFontSrc;
        if (fileName)
            fontsrc->setFile(fileName, false);
        else
            fontsrc->setBuf(tmpBuf, tmpBufLen, true);

        // load the font file
        switch (fontType) {
        case fontType1:
            if (!(fontFile = fontEngine->loadType1Font(id, fontsrc, (const char **)((Gfx8BitFont *)gfxFont)->getEncoding()))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'", gfxFont->getName() ? gfxFont->getName()->c_str() : "(unnamed)");
                if (gfxFont->invalidateEmbeddedFont())
                    goto reload;
                goto err2;
            }
            break;
        case fontType1C:
            if (!(fontFile = fontEngine->loadType1CFont(id, fontsrc, (const char **)((Gfx8BitFont *)gfxFont)->getEncoding()))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'", gfxFont->getName() ? gfxFont->getName()->c_str() : "(unnamed)");
                if (gfxFont->invalidateEmbeddedFont())
                    goto reload;
                goto err2;
            }
            break;
        case fontType1COT:
            if (!(fontFile = fontEngine->loadOpenTypeT1CFont(id, fontsrc, (const char **)((Gfx8BitFont *)gfxFont)->getEncoding()))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'", gfxFont->getName() ? gfxFont->getName()->c_str() : "(unnamed)");
                if (gfxFont->invalidateEmbeddedFont())
                    goto reload;
                goto err2;
            }
            break;
        case fontTrueType:
        case fontTrueTypeOT: {
            if (fileName)
                ff = FoFiTrueType::load(fileName->c_str());
            else
                ff = FoFiTrueType::make(tmpBuf, tmpBufLen);
            int *codeToGID;
            const int n = ff ? 256 : 0;
            if (ff) {
                codeToGID = ((Gfx8BitFont *)gfxFont)->getCodeToGIDMap(ff);
                delete ff;
                // if we're substituting for a non-TrueType font, we need to mark
                // all notdef codes as "do not draw" (rather than drawing TrueType
                // notdef glyphs)
                if (gfxFont->getType() != fontTrueType && gfxFont->getType() != fontTrueTypeOT) {
                    for (int i = 0; i < 256; ++i) {
                        if (codeToGID[i] == 0) {
                            codeToGID[i] = -1;
                        }
                    }
                }
            } else {
                codeToGID = nullptr;
            }
            if (!(fontFile = fontEngine->loadTrueTypeFont(id, fontsrc, codeToGID, n))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'", gfxFont->getName() ? gfxFont->getName()->c_str() : "(unnamed)");
                if (gfxFont->invalidateEmbeddedFont())
                    goto reload;
                goto err2;
            }
            break;
        }
        case fontCIDType0:
        case fontCIDType0C:
            if (!(fontFile = fontEngine->loadCIDFont(id, fontsrc))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'", gfxFont->getName() ? gfxFont->getName()->c_str() : "(unnamed)");
                if (gfxFont->invalidateEmbeddedFont())
                    goto reload;
                goto err2;
            }
            break;
        case fontCIDType0COT: {
            int *codeToGID;
            int n;
            if (((GfxCIDFont *)gfxFont)->getCIDToGID()) {
                n = ((GfxCIDFont *)gfxFont)->getCIDToGIDLen();
                codeToGID = (int *)gmallocn(n, sizeof(int));
                memcpy(codeToGID, ((GfxCIDFont *)gfxFont)->getCIDToGID(), n * sizeof(int));
            } else {
                codeToGID = nullptr;
                n = 0;
            }
            if (!(fontFile = fontEngine->loadOpenTypeCFFFont(id, fontsrc, codeToGID, n))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'", gfxFont->getName() ? gfxFont->getName()->c_str() : "(unnamed)");
                gfree(codeToGID);
                if (gfxFont->invalidateEmbeddedFont())
                    goto reload;
                goto err2;
            }
            break;
        }
        case fontCIDType2:
        case fontCIDType2OT: {
            int *codeToGID = nullptr;
            int n = 0;
            if (((GfxCIDFont *)gfxFont)->getCIDToGID()) {
                n = ((GfxCIDFont *)gfxFont)->getCIDToGIDLen();
                if (n) {
                    codeToGID = (int *)gmallocn(n, sizeof(int));
                    memcpy(codeToGID, ((GfxCIDFont *)gfxFont)->getCIDToGID(), n * sizeof(int));
                }
            } else {
                if (fileName)
                    ff = FoFiTrueType::load(fileName->c_str());
                else
                    ff = FoFiTrueType::make(tmpBuf, tmpBufLen);
                if (!ff) {
                    error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'", gfxFont->getName() ? gfxFont->getName()->c_str() : "(unnamed)");
                    goto err2;
                }
                codeToGID = ((GfxCIDFont *)gfxFont)->getCodeToGIDMap(ff, &n);
                delete ff;
            }
            if (!(fontFile = fontEngine->loadTrueTypeFont(id, fontsrc, codeToGID, n, faceIndex))) {
                error(errSyntaxError, -1, "Couldn't create a font for '{0:s}'", gfxFont->getName() ? gfxFont->getName()->c_str() : "(unnamed)");
                if (gfxFont->invalidateEmbeddedFont())
                    goto reload;
                goto err2;
            }
            break;
        }
        default:
            // this shouldn't happen
            goto err2;
        }
        fontFile->doAdjustMatrix = doAdjustFontMatrix;
    }

    // get the font matrix
    textMat = state->getTextMat();
    fontSize = state->getFontSize();
    m11 = textMat[0] * fontSize * state->getHorizScaling();
    m12 = textMat[1] * fontSize * state->getHorizScaling();
    m21 = textMat[2] * fontSize;
    m22 = textMat[3] * fontSize;

    // create the scaled font
    mat[0] = m11;
    mat[1] = m12;
    mat[2] = m21;
    mat[3] = m22;
    font = fontEngine->getFont(fontFile, mat, splash->getMatrix());

    // for substituted fonts: adjust the font matrix -- compare the
    // width of 'm' in the original font and the substituted font
    if (fontFile->doAdjustMatrix && !gfxFont->isCIDFont()) {
        double w1, w2, w3;
        CharCode code;
        const char *name;
        for (code = 0; code < 256; ++code) {
            if ((name = ((Gfx8BitFont *)gfxFont)->getCharName(code)) && name[0] == 'm' && name[1] == '\0') {
                break;
            }
        }
        if (code < 256) {
            w1 = ((Gfx8BitFont *)gfxFont)->getWidth(code);
            w2 = font->getGlyphAdvance(code);
            w3 = ((Gfx8BitFont *)gfxFont)->getWidth(0);
            if (!gfxFont->isSymbolic() && w2 > 0 && w1 > w3) {
                // if real font is substantially narrower than substituted
                // font, reduce the font size accordingly
                if (w1 > 0.01 && w1 < 0.9 * w2) {
                    w1 /= w2;
                    m11 *= w1;
                    m21 *= w1;
                    recreateFont = true;
                }
            }
        }
    }

    if (recreateFont) {
        mat[0] = m11;
        mat[1] = m12;
        mat[2] = m21;
        mat[3] = m22;
        font = fontEngine->getFont(fontFile, mat, splash->getMatrix());
    }

    delete fontLoc;
    if (fontsrc && !fontsrc->isFile)
        fontsrc->unref();
    return;

err2:
    delete id;
    delete fontLoc;
err1:
    if (fontsrc && !fontsrc->isFile)
        fontsrc->unref();
    return;
}

void SplashOutputBase::stroke(GfxState *state)
{
    if (state->getStrokeColorSpace()->isNonMarking()) {
        return;
    }
    setOverprintMask(state->getStrokeColorSpace(), state->getStrokeOverprint(), state->getOverprintMode(), state->getStrokeColor());
    SplashPath path = convertPath(state, state->getPath(), false);
    splash->stroke(&path);
}

void SplashOutputBase::fill(GfxState *state)
{
    if (state->getFillColorSpace()->isNonMarking()) {
        return;
    }
    setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(), state->getOverprintMode(), state->getFillColor());
    SplashPath path = convertPath(state, state->getPath(), true);
    splash->fill(&path, false);
}

void SplashOutputBase::eoFill(GfxState *state)
{
    if (state->getFillColorSpace()->isNonMarking()) {
        return;
    }
    setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(), state->getOverprintMode(), state->getFillColor());
    SplashPath path = convertPath(state, state->getPath(), true);
    splash->fill(&path, true);
}

void SplashOutputBase::clip(GfxState *state)
{
    SplashPath path = convertPath(state, state->getPath(), true);
    splash->clipToPath(&path, false);
}

void SplashOutputBase::eoClip(GfxState *state)
{
    SplashPath path = convertPath(state, state->getPath(), true);
    splash->clipToPath(&path, true);
}

void SplashOutputBase::clipToStrokePath(GfxState *state)
{
    SplashPath *path2;

    SplashPath path = convertPath(state, state->getPath(), false);
    path2 = splash->makeStrokePath(&path, state->getLineWidth());
    splash->clipToPath(path2, false);
    delete path2;
}

SplashPath SplashOutputBase::convertPath(GfxState *state, const GfxPath *path, bool dropEmptySubpaths)
{
    SplashPath sPath;
    int n, i, j;

    n = dropEmptySubpaths ? 1 : 0;
    for (i = 0; i < path->getNumSubpaths(); ++i) {
        const GfxSubpath *subpath = path->getSubpath(i);
        if (subpath->getNumPoints() > n) {
            sPath.reserve(subpath->getNumPoints() + 1);
            sPath.moveTo((SplashCoord)subpath->getX(0), (SplashCoord)subpath->getY(0));
            j = 1;
            while (j < subpath->getNumPoints()) {
                if (subpath->getCurve(j)) {
                    sPath.curveTo((SplashCoord)subpath->getX(j), (SplashCoord)subpath->getY(j), (SplashCoord)subpath->getX(j + 1), (SplashCoord)subpath->getY(j + 1), (SplashCoord)subpath->getX(j + 2), (SplashCoord)subpath->getY(j + 2));
                    j += 3;
                } else {
                    sPath.lineTo((SplashCoord)subpath->getX(j), (SplashCoord)subpath->getY(j));
                    ++j;
                }
            }
            if (subpath->isClosed()) {
                sPath.close();
            }
        }
    }
    return sPath;
}

void SplashOutputBase::drawChar(GfxState *state, double x, double y, double dx, double dy, double originX, double originY, CharCode code, int nBytes, const Unicode *u, int uLen)
{
    SplashPath *path;
    int render;
    bool doFill, doStroke, doClip, strokeAdjust;
    double m[4];
    bool horiz;

    if (skipHorizText || skipRotatedText) {
        state->getFontTransMat(&m[0], &m[1], &m[2], &m[3]);
        horiz = m[0] > 0 && fabs(m[1]) < 0.001 && fabs(m[2]) < 0.001 && m[3] < 0;
        if ((skipHorizText && horiz) || (skipRotatedText && !horiz)) {
            return;
        }
    }

    // check for invisible text -- this is used by Acrobat Capture
    render = state->getRender();
    if (render == 3) {
        return;
    }

    if (needFontUpdate) {
        doUpdateFont(state);
    }
    if (!font) {
        return;
    }

    x -= originX;
    y -= originY;

    doFill = !(render & 1) && !state->getFillColorSpace()->isNonMarking();
    doStroke = ((render & 3) == 1 || (render & 3) == 2) && !state->getStrokeColorSpace()->isNonMarking();
    doClip = render & 4;

    path = nullptr;
    SplashCoord lineWidth = splash->getLineWidth();
    if (doStroke && lineWidth == 0.0)
        splash->setLineWidth(1 / state->getVDPI());
    if (doStroke || doClip) {
        if ((path = font->getGlyphPath(code))) {
            path->offset((SplashCoord)x, (SplashCoord)y);
        }
    }

    // don't use stroke adjustment when stroking text -- the results
    // tend to be ugly (because characters with horizontal upper or
    // lower edges get misaligned relative to the other characters)
    strokeAdjust = false; // make gcc happy
    if (doStroke) {
        strokeAdjust = splash->getStrokeAdjust();
        splash->setStrokeAdjust(false);
    }

    // fill and stroke
    if (doFill && doStroke) {
        if (path) {
            setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(), state->getOverprintMode(), state->getFillColor());
            splash->fill(path, false);
            setOverprintMask(state->getStrokeColorSpace(), state->getStrokeOverprint(), state->getOverprintMode(), state->getStrokeColor());
            splash->stroke(path);
        }

        // fill
    } else if (doFill) {
        setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(), state->getOverprintMode(), state->getFillColor());
        splash->fillChar((SplashCoord)x, (SplashCoord)y, code, font);

        // stroke
    } else if (doStroke) {
        if (path) {
            setOverprintMask(state->getStrokeColorSpace(), state->getStrokeOverprint(), state->getOverprintMode(), state->getStrokeColor());
            splash->stroke(path);
        }
    }
    splash->setLineWidth(lineWidth);

    // clip
    if (doClip) {
        if (path) {
            if (textClipPath) {
                textClipPath->append(path);
            } else {
                textClipPath = path;
                path = nullptr;
            }
        }
    }

    if (doStroke) {
        splash->setStrokeAdjust(strokeAdjust);
    }

    if (path) {
        delete path;
    }
}

bool SplashOutputBase::beginType3Char(GfxState *state, double x, double y, double dx, double dy, CharCode code, const Unicode *u, int uLen)
{
    GfxFont *gfxFont;
    const Ref *fontID;
    const double *ctm, *bbox;
    T3FontCache *t3Font;
    T3GlyphStack *t3gs;
    bool validBBox;
    double m[4];
    bool horiz;
    double x1, y1, xMin, yMin, xMax, yMax, xt, yt;
    int i, j;

    // check for invisible text -- this is used by Acrobat Capture
    if (state->getRender() == 3) {
        // this is a bit of cheating, we say yes, font is already on cache
        // so we actually skip the rendering of it
        return true;
    }

    if (skipHorizText || skipRotatedText) {
        state->getFontTransMat(&m[0], &m[1], &m[2], &m[3]);
        horiz = m[0] > 0 && fabs(m[1]) < 0.001 && fabs(m[2]) < 0.001 && m[3] < 0;
        if ((skipHorizText && horiz) || (skipRotatedText && !horiz)) {
            return true;
        }
    }

    if (!(gfxFont = state->getFont())) {
        return false;
    }
    fontID = gfxFont->getID();
    ctm = state->getCTM();
    state->transform(0, 0, &xt, &yt);

    // is it the first (MRU) font in the cache?
    if (!(nT3Fonts > 0 && t3FontCache[0]->matches(fontID, ctm[0], ctm[1], ctm[2], ctm[3]))) {

        // is the font elsewhere in the cache?
        for (i = 1; i < nT3Fonts; ++i) {
            if (t3FontCache[i]->matches(fontID, ctm[0], ctm[1], ctm[2], ctm[3])) {
                t3Font = t3FontCache[i];
                for (j = i; j > 0; --j) {
                    t3FontCache[j] = t3FontCache[j - 1];
                }
                t3FontCache[0] = t3Font;
                break;
            }
        }
        if (i >= nT3Fonts) {

            // create new entry in the font cache
            if (nT3Fonts == splashOutT3FontCacheSize) {
                t3gs = t3GlyphStack;
                while (t3gs != nullptr) {
                    if (t3gs->cache == t3FontCache[nT3Fonts - 1]) {
                        error(errSyntaxWarning, -1, "t3FontCache reaches limit but font still on stack in SplashOutputBase::beginType3Char");
                        return true;
                    }
                    t3gs = t3gs->next;
                }
                delete t3FontCache[nT3Fonts - 1];
                --nT3Fonts;
            }
            for (j = nT3Fonts; j > 0; --j) {
                t3FontCache[j] = t3FontCache[j - 1];
            }
            ++nT3Fonts;
            bbox = gfxFont->getFontBBox();
            if (bbox[0] == 0 && bbox[1] == 0 && bbox[2] == 0 && bbox[3] == 0) {
                // unspecified bounding box -- just take a guess
                xMin = xt - 5;
                xMax = xMin + 30;
                yMax = yt + 15;
                yMin = yMax - 45;
                validBBox = false;
            } else {
                state->transform(bbox[0], bbox[1], &x1, &y1);
                xMin = xMax = x1;
                yMin = yMax = y1;
                state->transform(bbox[0], bbox[3], &x1, &y1);
                if (x1 < xMin) {
                    xMin = x1;
                } else if (x1 > xMax) {
                    xMax = x1;
                }
                if (y1 < yMin) {
                    yMin = y1;
                } else if (y1 > yMax) {
                    yMax = y1;
                }
                state->transform(bbox[2], bbox[1], &x1, &y1);
                if (x1 < xMin) {
                    xMin = x1;
                } else if (x1 > xMax) {
                    xMax = x1;
                }
                if (y1 < yMin) {
                    yMin = y1;
                } else if (y1 > yMax) {
                    yMax = y1;
                }
                state->transform(bbox[2], bbox[3], &x1, &y1);
                if (x1 < xMin) {
                    xMin = x1;
                } else if (x1 > xMax) {
                    xMax = x1;
                }
                if (y1 < yMin) {
                    yMin = y1;
                } else if (y1 > yMax) {
                    yMax = y1;
                }
                validBBox = true;
            }
            t3FontCache[0] = new T3FontCache(fontID, ctm[0], ctm[1], ctm[2], ctm[3], (int)floor(xMin - xt) - 2, (int)floor(yMin - yt) - 2, (int)ceil(xMax) - (int)floor(xMin) + 4, (int)ceil(yMax) - (int)floor(yMin) + 4, validBBox,
                                             colorMode != splashModeMono1);
        }
    }
    t3Font = t3FontCache[0];

    // is the glyph in the cache?
    i = (code & (t3Font->cacheSets - 1)) * t3Font->cacheAssoc;
    for (j = 0; j < t3Font->cacheAssoc; ++j) {
        if (t3Font->cacheTags != nullptr) {
            if ((t3Font->cacheTags[i + j].mru & 0x8000) && t3Font->cacheTags[i + j].code == code) {
                drawType3Glyph(state, t3Font, &t3Font->cacheTags[i + j], t3Font->cacheData + (i + j) * t3Font->glyphSize);
                return true;
            }
        }
    }

    // push a new Type 3 glyph record
    t3gs = new T3GlyphStack();
    t3gs->next = t3GlyphStack;
    t3GlyphStack = t3gs;
    t3GlyphStack->code = code;
    t3GlyphStack->cache = t3Font;
    t3GlyphStack->cacheTag = nullptr;
    t3GlyphStack->cacheData = nullptr;
    t3GlyphStack->haveDx = false;
    t3GlyphStack->doNotCache = false;

    return false;
}

void SplashOutputBase::endType3Char(GfxState *state)
{
    T3GlyphStack *t3gs;

    if (t3GlyphStack->cacheTag) {
        memcpy(t3GlyphStack->cacheData, bitmap->getDataPtr(), t3GlyphStack->cache->glyphSize);
        delete bitmap;
        delete splash;
        bitmap = t3GlyphStack->origBitmap;
        splash = t3GlyphStack->origSplash;
        const double *ctm = state->getCTM();
        state->setCTM(ctm[0], ctm[1], ctm[2], ctm[3], t3GlyphStack->origCTM4, t3GlyphStack->origCTM5);
        updateCTM(state, 0, 0, 0, 0, 0, 0);
        drawType3Glyph(state, t3GlyphStack->cache, t3GlyphStack->cacheTag, t3GlyphStack->cacheData);
    }
    t3gs = t3GlyphStack;
    t3GlyphStack = t3gs->next;
    delete t3gs;
}

void SplashOutputBase::type3D0(GfxState *state, double wx, double wy)
{
    if (likely(t3GlyphStack != nullptr)) {
        t3GlyphStack->haveDx = true;
    } else {
        error(errSyntaxWarning, -1, "t3GlyphStack was null in SplashOutputBase::type3D0");
    }
}

void SplashOutputBase::type3D1(GfxState *state, double wx, double wy, double llx, double lly, double urx, double ury)
{
    T3FontCache *t3Font;
    SplashColor color;
    double xt, yt, xMin, xMax, yMin, yMax, x1, y1;
    int i, j;

    // ignore multiple d0/d1 operators
    if (!t3GlyphStack || t3GlyphStack->haveDx) {
        return;
    }
    t3GlyphStack->haveDx = true;
    // don't cache if we got a gsave/grestore before the d1
    if (t3GlyphStack->doNotCache) {
        return;
    }

    if (unlikely(t3GlyphStack == nullptr)) {
        error(errSyntaxWarning, -1, "t3GlyphStack was null in SplashOutputBase::type3D1");
        return;
    }

    if (unlikely(t3GlyphStack->origBitmap != nullptr)) {
        error(errSyntaxWarning, -1, "t3GlyphStack origBitmap was not null in SplashOutputBase::type3D1");
        return;
    }

    if (unlikely(t3GlyphStack->origSplash != nullptr)) {
        error(errSyntaxWarning, -1, "t3GlyphStack origSplash was not null in SplashOutputBase::type3D1");
        return;
    }

    t3Font = t3GlyphStack->cache;

    // check for a valid bbox
    state->transform(0, 0, &xt, &yt);
    state->transform(llx, lly, &x1, &y1);
    xMin = xMax = x1;
    yMin = yMax = y1;
    state->transform(llx, ury, &x1, &y1);
    if (x1 < xMin) {
        xMin = x1;
    } else if (x1 > xMax) {
        xMax = x1;
    }
    if (y1 < yMin) {
        yMin = y1;
    } else if (y1 > yMax) {
        yMax = y1;
    }
    state->transform(urx, lly, &x1, &y1);
    if (x1 < xMin) {
        xMin = x1;
    } else if (x1 > xMax) {
        xMax = x1;
    }
    if (y1 < yMin) {
        yMin = y1;
    } else if (y1 > yMax) {
        yMax = y1;
    }
    state->transform(urx, ury, &x1, &y1);
    if (x1 < xMin) {
        xMin = x1;
    } else if (x1 > xMax) {
        xMax = x1;
    }
    if (y1 < yMin) {
        yMin = y1;
    } else if (y1 > yMax) {
        yMax = y1;
    }
    if (xMin - xt < t3Font->glyphX || yMin - yt < t3Font->glyphY || xMax - xt > t3Font->glyphX + t3Font->glyphW || yMax - yt > t3Font->glyphY + t3Font->glyphH) {
        if (t3Font->validBBox) {
            error(errSyntaxWarning, -1, "Bad bounding box in Type 3 glyph");
        }
        return;
    }

    if (t3Font->cacheTags == nullptr)
        return;

    // allocate a cache entry
    i = (t3GlyphStack->code & (t3Font->cacheSets - 1)) * t3Font->cacheAssoc;
    for (j = 0; j < t3Font->cacheAssoc; ++j) {
        if ((t3Font->cacheTags[i + j].mru & 0x7fff) == t3Font->cacheAssoc - 1) {
            t3Font->cacheTags[i + j].mru = 0x8000;
            t3Font->cacheTags[i + j].code = t3GlyphStack->code;
            t3GlyphStack->cacheTag = &t3Font->cacheTags[i + j];
            t3GlyphStack->cacheData = t3Font->cacheData + (i + j) * t3Font->glyphSize;
        } else {
            ++t3Font->cacheTags[i + j].mru;
        }
    }

    // save state
    t3GlyphStack->origBitmap = bitmap;
    t3GlyphStack->origSplash = splash;
    const double *ctm = state->getCTM();
    t3GlyphStack->origCTM4 = ctm[4];
    t3GlyphStack->origCTM5 = ctm[5];

    // create the temporary bitmap
    if (colorMode == splashModeMono1) {
        bitmap = new SplashBitmap(t3Font->glyphW, t3Font->glyphH, 1, splashModeMono1, false);
        splash = new Splash(bitmap, false, t3GlyphStack->origSplash->getScreen());
        color[0] = 0;
        splash->clear(color);
        color[0] = 0xff;
    } else {
        bitmap = new SplashBitmap(t3Font->glyphW, t3Font->glyphH, 1, splashModeMono8, false);
        splash = new Splash(bitmap, vectorAntialias, t3GlyphStack->origSplash->getScreen());
        color[0] = 0x00;
        splash->clear(color);
        color[0] = 0xff;
    }
    splash->setMinLineWidth(s_minLineWidth);
    splash->setThinLineMode(splashThinLineDefault);
    splash->setFillPattern(new SplashSolidColor(color));
    splash->setStrokePattern(new SplashSolidColor(color));
    //~ this should copy other state from t3GlyphStack->origSplash?
    state->setCTM(ctm[0], ctm[1], ctm[2], ctm[3], -t3Font->glyphX, -t3Font->glyphY);
    updateCTM(state, 0, 0, 0, 0, 0, 0);
}

void SplashOutputBase::drawType3Glyph(GfxState *state, T3FontCache *t3Font, T3FontCacheTag * /*tag*/, unsigned char *data)
{
    SplashGlyphBitmap glyph;

    setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(), state->getOverprintMode(), state->getFillColor());
    glyph.x = -t3Font->glyphX;
    glyph.y = -t3Font->glyphY;
    glyph.w = t3Font->glyphW;
    glyph.h = t3Font->glyphH;
    glyph.aa = colorMode != splashModeMono1;
    glyph.data = data;
    glyph.freeData = false;
    splash->fillGlyph(0, 0, &glyph);
}

void SplashOutputBase::beginTextObject(GfxState *state) { }

void SplashOutputBase::endTextObject(GfxState *state)
{
    if (textClipPath) {
        splash->clipToPath(textClipPath, false);
        delete textClipPath;
        textClipPath = nullptr;
    }
}

bool SplashOutputBase::imageMaskSrc(void *data, SplashColorPtr line)
{
    SplashOutImageMaskData *imgMaskData = (SplashOutImageMaskData *)data;
    unsigned char *p;
    SplashColorPtr q;
    int x;

    if (imgMaskData->y == imgMaskData->height) {
        return false;
    }
    if (!(p = imgMaskData->imgStr->getLine())) {
        return false;
    }
    for (x = 0, q = line; x < imgMaskData->width; ++x) {
        *q++ = *p++ ^ imgMaskData->invert;
    }
    ++imgMaskData->y;
    return true;
}

void SplashOutputBase::drawImageMask(GfxState *state, Object *ref, Stream *str, int width, int height, bool invert, bool interpolate, bool inlineImg)
{
    SplashCoord mat[6];
    SplashOutImageMaskData imgMaskData;

    if (state->getFillColorSpace()->isNonMarking()) {
        return;
    }
    setOverprintMask(state->getFillColorSpace(), state->getFillOverprint(), state->getOverprintMode(), state->getFillColor());

    const double *ctm = state->getCTM();
    for (int i = 0; i < 6; ++i) {
        if (!std::isfinite(ctm[i]))
            return;
    }
    mat[0] = ctm[0];
    mat[1] = ctm[1];
    mat[2] = -ctm[2];
    mat[3] = -ctm[3];
    mat[4] = ctm[2] + ctm[4];
    mat[5] = ctm[3] + ctm[5];

    imgMaskData.imgStr = new ImageStream(str, width, 1, 1);
    imgMaskData.imgStr->reset();
    imgMaskData.invert = invert ? false : true;
    imgMaskData.width = width;
    imgMaskData.height = height;
    imgMaskData.y = 0;

    splash->fillImageMask(&imageMaskSrc, &imgMaskData, width, height, mat, t3GlyphStack != nullptr);
    if (inlineImg) {
        while (imgMaskData.y < height) {
            imgMaskData.imgStr->getLine();
            ++imgMaskData.y;
        }
    }

    delete imgMaskData.imgStr;
    str->close();
}

#ifdef USE_CMS
bool SplashOutputBase::useIccImageSrc(void *data)
{
    SplashOutImageData *imgData = (SplashOutImageData *)data;

    if (!imgData->lookup && imgData->colorMap->getColorSpace()->getMode() == csICCBased && imgData->colorMap->getBits() != 1) {
        GfxICCBasedColorSpace *colorSpace = (GfxICCBasedColorSpace *)imgData->colorMap->getColorSpace();
        switch (imgData->colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            if (colorSpace->getAlt() != nullptr && colorSpace->getAlt()->getMode() == csDeviceGray)
                return true;
            break;
        case splashModeXBGR8:
        case splashModeRGB8:
        case splashModeBGR8:
            if (colorSpace->getAlt() != nullptr && colorSpace->getAlt()->getMode() == csDeviceRGB)
                return true;
            break;
        case splashModeCMYK8:
            if (colorSpace->getAlt() != nullptr && colorSpace->getAlt()->getMode() == csDeviceCMYK)
                return true;
            break;
        case splashModeDeviceN8:
            if (colorSpace->getAlt() != nullptr && colorSpace->getAlt()->getMode() == csDeviceN)
                return true;
            break;
        }
    }

    return false;
}
#endif

// Clip x to lie in [0, 255].
static inline unsigned char clip255(int x)
{
    return x < 0 ? 0 : x > 255 ? 255 : x;
}

bool SplashOutputBase::imageSrc(void *data, SplashColorPtr colorLine, unsigned char * /*alphaLine*/)
{
    SplashOutImageData *imgData = (SplashOutImageData *)data;
    unsigned char *p;
    SplashColorPtr q, col;
    GfxRGB rgb;
    GfxGray gray;
    GfxCMYK cmyk;
    GfxColor deviceN;
    int nComps, x;

    if (imgData->y == imgData->height) {
        return false;
    }
    if (!(p = imgData->imgStr->getLine())) {
        int destComps = 1;
        if (imgData->colorMode == splashModeRGB8 || imgData->colorMode == splashModeBGR8)
            destComps = 3;
        else if (imgData->colorMode == splashModeXBGR8)
            destComps = 4;
        else if (imgData->colorMode == splashModeCMYK8)
            destComps = 4;
        else if (imgData->colorMode == splashModeDeviceN8)
            destComps = SPOT_NCOMPS + 4;
        memset(colorLine, 0, imgData->width * destComps);
        return false;
    }

    nComps = imgData->colorMap->getNumPixelComps();

    if (imgData->lookup) {
        switch (imgData->colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            for (x = 0, q = colorLine; x < imgData->width; ++x, ++p) {
                *q++ = imgData->lookup[*p];
            }
            break;
        case splashModeRGB8:
        case splashModeBGR8:
            for (x = 0, q = colorLine; x < imgData->width; ++x, ++p) {
                col = &imgData->lookup[3 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
            }
            break;
        case splashModeXBGR8:
            for (x = 0, q = colorLine; x < imgData->width; ++x, ++p) {
                col = &imgData->lookup[4 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                *q++ = col[3];
            }
            break;
        case splashModeCMYK8:
            for (x = 0, q = colorLine; x < imgData->width; ++x, ++p) {
                col = &imgData->lookup[4 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                *q++ = col[3];
            }
            break;
        case splashModeDeviceN8:
            for (x = 0, q = colorLine; x < imgData->width; ++x, ++p) {
                col = &imgData->lookup[(SPOT_NCOMPS + 4) * *p];
                for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                    *q++ = col[cp];
            }
            break;
        }
    } else {
        switch (imgData->colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            for (x = 0, q = colorLine; x < imgData->width; ++x, p += nComps) {
                imgData->colorMap->getGray(p, &gray);
                *q++ = colToByte(gray);
            }
            break;
        case splashModeRGB8:
        case splashModeBGR8:
            if (imgData->colorMap->useRGBLine()) {
                imgData->colorMap->getRGBLine(p, (unsigned char *)colorLine, imgData->width);
            } else {
                for (x = 0, q = colorLine; x < imgData->width; ++x, p += nComps) {
                    imgData->colorMap->getRGB(p, &rgb);
                    *q++ = colToByte(rgb.r);
                    *q++ = colToByte(rgb.g);
                    *q++ = colToByte(rgb.b);
                }
            }
            break;
        case splashModeXBGR8:
            if (imgData->colorMap->useRGBLine()) {
                imgData->colorMap->getRGBXLine(p, (unsigned char *)colorLine, imgData->width);
            } else {
                for (x = 0, q = colorLine; x < imgData->width; ++x, p += nComps) {
                    imgData->colorMap->getRGB(p, &rgb);
                    *q++ = colToByte(rgb.r);
                    *q++ = colToByte(rgb.g);
                    *q++ = colToByte(rgb.b);
                    *q++ = 255;
                }
            }
            break;
        case splashModeCMYK8:
            if (imgData->colorMap->useCMYKLine()) {
                imgData->colorMap->getCMYKLine(p, (unsigned char *)colorLine, imgData->width);
            } else {
                for (x = 0, q = colorLine; x < imgData->width; ++x, p += nComps) {
                    imgData->colorMap->getCMYK(p, &cmyk);
                    *q++ = colToByte(cmyk.c);
                    *q++ = colToByte(cmyk.m);
                    *q++ = colToByte(cmyk.y);
                    *q++ = colToByte(cmyk.k);
                }
            }
            break;
        case splashModeDeviceN8:
            if (imgData->colorMap->useDeviceNLine()) {
                imgData->colorMap->getDeviceNLine(p, (unsigned char *)colorLine, imgData->width);
            } else {
                for (x = 0, q = colorLine; x < imgData->width; ++x, p += nComps) {
                    imgData->colorMap->getDeviceN(p, &deviceN);
                    for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                        *q++ = colToByte(deviceN.c[cp]);
                }
            }
            break;
        }
    }

    if (imgData->maskStr != nullptr && (p = imgData->maskStr->getLine()) != nullptr) {
        int destComps = splashColorModeNComps[imgData->colorMode];
        int convComps = (imgData->colorMode == splashModeXBGR8) ? 3 : destComps;
        imgData->maskColorMap->getGrayLine(p, p, imgData->width);
        for (x = 0, q = colorLine; x < imgData->width; ++x, p++, q += destComps) {
            for (int cp = 0; cp < convComps; cp++) {
                q[cp] = (*p) ? clip255(imgData->matteColor[cp] + (int)(q[cp] - imgData->matteColor[cp]) * 255 / *p) : imgData->matteColor[cp];
            }
        }
    }
    ++imgData->y;
    return true;
}

#ifdef USE_CMS
bool SplashOutputBase::iccImageSrc(void *data, SplashColorPtr colorLine, unsigned char * /*alphaLine*/)
{
    SplashOutImageData *imgData = (SplashOutImageData *)data;
    unsigned char *p;
    int nComps;

    if (imgData->y == imgData->height) {
        return false;
    }
    if (!(p = imgData->imgStr->getLine())) {
        int destComps = 1;
        if (imgData->colorMode == splashModeRGB8 || imgData->colorMode == splashModeBGR8)
            destComps = 3;
        else if (imgData->colorMode == splashModeXBGR8)
            destComps = 4;
        else if (imgData->colorMode == splashModeCMYK8)
            destComps = 4;
        else if (imgData->colorMode == splashModeDeviceN8)
            destComps = SPOT_NCOMPS + 4;
        memset(colorLine, 0, imgData->width * destComps);
        return false;
    }

    if (imgData->colorMode == splashModeXBGR8) {
        SplashColorPtr q;
        int x;
        for (x = 0, q = colorLine; x < imgData->width; ++x) {
            *q++ = *p++;
            *q++ = *p++;
            *q++ = *p++;
            *q++ = 255;
        }
    } else {
        nComps = imgData->colorMap->getNumPixelComps();
        memcpy(colorLine, p, imgData->width * nComps);
    }

    ++imgData->y;
    return true;
}

void SplashOutputBase::iccTransform(void *data, SplashBitmap *bitmap)
{
    SplashOutImageData *imgData = (SplashOutImageData *)data;
    int nComps = imgData->colorMap->getNumPixelComps();

    unsigned char *colorLine = (unsigned char *)gmalloc(nComps * bitmap->getWidth());
    unsigned char *rgbxLine = (imgData->colorMode == splashModeXBGR8) ? (unsigned char *)gmalloc(3 * bitmap->getWidth()) : nullptr;
    for (int i = 0; i < bitmap->getHeight(); i++) {
        unsigned char *p = bitmap->getDataPtr() + i * bitmap->getRowSize();
        switch (imgData->colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            imgData->colorMap->getGrayLine(p, colorLine, bitmap->getWidth());
            memcpy(p, colorLine, nComps * bitmap->getWidth());
            break;
        case splashModeRGB8:
        case splashModeBGR8:
            imgData->colorMap->getRGBLine(p, colorLine, bitmap->getWidth());
            memcpy(p, colorLine, nComps * bitmap->getWidth());
            break;
        case splashModeCMYK8:
            imgData->colorMap->getCMYKLine(p, colorLine, bitmap->getWidth());
            memcpy(p, colorLine, nComps * bitmap->getWidth());
            break;
        case splashModeDeviceN8:
            imgData->colorMap->getDeviceNLine(p, colorLine, bitmap->getWidth());
            memcpy(p, colorLine, nComps * bitmap->getWidth());
            break;
        case splashModeXBGR8:
            unsigned char *q;
            unsigned char *b = p;
            int x;
            for (x = 0, q = rgbxLine; x < bitmap->getWidth(); ++x, b += 4) {
                *q++ = b[2];
                *q++ = b[1];
                *q++ = b[0];
            }
            imgData->colorMap->getRGBLine(rgbxLine, colorLine, bitmap->getWidth());
            b = p;
            for (x = 0, q = colorLine; x < bitmap->getWidth(); ++x, b += 4) {
                b[2] = *q++;
                b[1] = *q++;
                b[0] = *q++;
            }
            break;
        }
    }
    gfree(colorLine);
    if (rgbxLine != nullptr)
        gfree(rgbxLine);
}
#endif

bool SplashOutputBase::alphaImageSrc(void *data, SplashColorPtr colorLine, unsigned char *alphaLine)
{
    SplashOutImageData *imgData = (SplashOutImageData *)data;
    unsigned char *p, *aq;
    SplashColorPtr q, col;
    GfxRGB rgb;
    GfxGray gray;
    GfxCMYK cmyk;
    GfxColor deviceN;
    unsigned char alpha;
    int nComps, x, i;

    if (imgData->y == imgData->height) {
        return false;
    }
    if (!(p = imgData->imgStr->getLine())) {
        return false;
    }

    nComps = imgData->colorMap->getNumPixelComps();

    for (x = 0, q = colorLine, aq = alphaLine; x < imgData->width; ++x, p += nComps) {
        alpha = 0;
        for (i = 0; i < nComps; ++i) {
            if (p[i] < imgData->maskColors[2 * i] || p[i] > imgData->maskColors[2 * i + 1]) {
                alpha = 0xff;
                break;
            }
        }
        if (imgData->lookup) {
            switch (imgData->colorMode) {
            case splashModeMono1:
            case splashModeMono8:
                *q++ = imgData->lookup[*p];
                break;
            case splashModeRGB8:
            case splashModeBGR8:
                col = &imgData->lookup[3 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                break;
            case splashModeXBGR8:
                col = &imgData->lookup[4 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                *q++ = 255;
                break;
            case splashModeCMYK8:
                col = &imgData->lookup[4 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                *q++ = col[3];
                break;
            case splashModeDeviceN8:
                col = &imgData->lookup[(SPOT_NCOMPS + 4) * *p];
                for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                    *q++ = col[cp];
                break;
            }
            *aq++ = alpha;
        } else {
            switch (imgData->colorMode) {
            case splashModeMono1:
            case splashModeMono8:
                imgData->colorMap->getGray(p, &gray);
                *q++ = colToByte(gray);
                break;
            case splashModeXBGR8:
            case splashModeRGB8:
            case splashModeBGR8:
                imgData->colorMap->getRGB(p, &rgb);
                *q++ = colToByte(rgb.r);
                *q++ = colToByte(rgb.g);
                *q++ = colToByte(rgb.b);
                if (imgData->colorMode == splashModeXBGR8)
                    *q++ = 255;
                break;
            case splashModeCMYK8:
                imgData->colorMap->getCMYK(p, &cmyk);
                *q++ = colToByte(cmyk.c);
                *q++ = colToByte(cmyk.m);
                *q++ = colToByte(cmyk.y);
                *q++ = colToByte(cmyk.k);
                break;
            case splashModeDeviceN8:
                imgData->colorMap->getDeviceN(p, &deviceN);
                for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                    *q++ = colToByte(deviceN.c[cp]);
                break;
            }
            *aq++ = alpha;
        }
    }

    ++imgData->y;
    return true;
}

void SplashOutputBase::drawImage(GfxState *state, Object *ref, Stream *str, int width, int height, GfxImageColorMap *colorMap, bool interpolate, const int *maskColors, bool inlineImg)
{
    SplashCoord mat[6];
    SplashOutImageData imgData;
    SplashColorMode srcMode;
    SplashImageSource src;
    SplashICCTransform tf;
    GfxGray gray;
    GfxRGB rgb;
    GfxCMYK cmyk;
    bool grayIndexed = false;
    GfxColor deviceN;
    unsigned char pix;
    int n, i;

    const double *ctm = state->getCTM();
    for (i = 0; i < 6; ++i) {
        if (!std::isfinite(ctm[i]))
            return;
    }
    mat[0] = ctm[0];
    mat[1] = ctm[1];
    mat[2] = -ctm[2];
    mat[3] = -ctm[3];
    mat[4] = ctm[2] + ctm[4];
    mat[5] = ctm[3] + ctm[5];

    imgData.imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
    imgData.imgStr->reset();
    imgData.colorMap = colorMap;
    imgData.maskColors = maskColors;
    imgData.colorMode = colorMode;
    imgData.width = width;
    imgData.height = height;
    imgData.maskStr = nullptr;
    imgData.maskColorMap = nullptr;
    imgData.y = 0;

    // special case for one-channel (monochrome/gray/separation) images:
    // build a lookup table here
    imgData.lookup = nullptr;
    if (colorMap->getNumPixelComps() == 1) {
        n = 1 << colorMap->getBits();
        switch (colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            imgData.lookup = (SplashColorPtr)gmalloc(n);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getGray(&pix, &gray);
                imgData.lookup[i] = colToByte(gray);
            }
            break;
        case splashModeRGB8:
        case splashModeBGR8:
            imgData.lookup = (SplashColorPtr)gmallocn(n, 3);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getRGB(&pix, &rgb);
                imgData.lookup[3 * i] = colToByte(rgb.r);
                imgData.lookup[3 * i + 1] = colToByte(rgb.g);
                imgData.lookup[3 * i + 2] = colToByte(rgb.b);
            }
            break;
        case splashModeXBGR8:
            imgData.lookup = (SplashColorPtr)gmallocn_checkoverflow(n, 4);
            if (likely(imgData.lookup != nullptr)) {
                for (i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getRGB(&pix, &rgb);
                    imgData.lookup[4 * i] = colToByte(rgb.r);
                    imgData.lookup[4 * i + 1] = colToByte(rgb.g);
                    imgData.lookup[4 * i + 2] = colToByte(rgb.b);
                    imgData.lookup[4 * i + 3] = 255;
                }
            }
            break;
        case splashModeCMYK8:
            grayIndexed = colorMap->getColorSpace()->getMode() != csDeviceGray;
            imgData.lookup = (SplashColorPtr)gmallocn(n, 4);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getCMYK(&pix, &cmyk);
                if (cmyk.c != 0 || cmyk.m != 0 || cmyk.y != 0) {
                    grayIndexed = false;
                }
                imgData.lookup[4 * i] = colToByte(cmyk.c);
                imgData.lookup[4 * i + 1] = colToByte(cmyk.m);
                imgData.lookup[4 * i + 2] = colToByte(cmyk.y);
                imgData.lookup[4 * i + 3] = colToByte(cmyk.k);
            }
            break;
        case splashModeDeviceN8:
            colorMap->getColorSpace()->createMapping(bitmap->getSeparationList(), SPOT_NCOMPS);
            grayIndexed = colorMap->getColorSpace()->getMode() != csDeviceGray;
            imgData.lookup = (SplashColorPtr)gmallocn(n, SPOT_NCOMPS + 4);
            for (i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getCMYK(&pix, &cmyk);
                if (cmyk.c != 0 || cmyk.m != 0 || cmyk.y != 0) {
                    grayIndexed = false;
                }
                colorMap->getDeviceN(&pix, &deviceN);
                for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                    imgData.lookup[(SPOT_NCOMPS + 4) * i + cp] = colToByte(deviceN.c[cp]);
            }
            break;
        }
    }

    setOverprintMask(colorMap->getColorSpace(), state->getFillOverprint(), state->getOverprintMode(), nullptr, grayIndexed);

    if (colorMode == splashModeMono1) {
        srcMode = splashModeMono8;
    } else {
        srcMode = colorMode;
    }
#ifdef USE_CMS
    src = maskColors ? &alphaImageSrc : useIccImageSrc(&imgData) ? &iccImageSrc : &imageSrc;
    tf = maskColors == nullptr && useIccImageSrc(&imgData) ? &iccTransform : nullptr;
#else
    src = maskColors ? &alphaImageSrc : &imageSrc;
    tf = nullptr;
#endif
    splash->drawImage(src, tf, &imgData, srcMode, maskColors ? true : false, width, height, mat, interpolate);
    if (inlineImg) {
        while (imgData.y < height) {
            imgData.imgStr->getLine();
            ++imgData.y;
        }
    }

    gfree(imgData.lookup);
    delete imgData.imgStr;
    str->close();
}

bool SplashOutputBase::maskedImageSrc(void *data, SplashColorPtr colorLine, unsigned char *alphaLine)
{
    SplashOutMaskedImageData *imgData = (SplashOutMaskedImageData *)data;
    unsigned char *p, *aq;
    SplashColorPtr q, col;
    GfxRGB rgb;
    GfxGray gray;
    GfxCMYK cmyk;
    GfxColor deviceN;
    unsigned char alpha;
    unsigned char *maskPtr;
    int maskBit;
    int nComps, x;

    if (imgData->y == imgData->height) {
        return false;
    }
    if (!(p = imgData->imgStr->getLine())) {
        return false;
    }

    nComps = imgData->colorMap->getNumPixelComps();

    maskPtr = imgData->mask->getDataPtr() + imgData->y * imgData->mask->getRowSize();
    maskBit = 0x80;
    for (x = 0, q = colorLine, aq = alphaLine; x < imgData->width; ++x, p += nComps) {
        alpha = (*maskPtr & maskBit) ? 0xff : 0x00;
        if (!(maskBit >>= 1)) {
            ++maskPtr;
            maskBit = 0x80;
        }
        if (imgData->lookup) {
            switch (imgData->colorMode) {
            case splashModeMono1:
            case splashModeMono8:
                *q++ = imgData->lookup[*p];
                break;
            case splashModeRGB8:
            case splashModeBGR8:
                col = &imgData->lookup[3 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                break;
            case splashModeXBGR8:
                col = &imgData->lookup[4 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                *q++ = 255;
                break;
            case splashModeCMYK8:
                col = &imgData->lookup[4 * *p];
                *q++ = col[0];
                *q++ = col[1];
                *q++ = col[2];
                *q++ = col[3];
                break;
            case splashModeDeviceN8:
                col = &imgData->lookup[(SPOT_NCOMPS + 4) * *p];
                for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                    *q++ = col[cp];
                break;
            }
            *aq++ = alpha;
        } else {
            switch (imgData->colorMode) {
            case splashModeMono1:
            case splashModeMono8:
                imgData->colorMap->getGray(p, &gray);
                *q++ = colToByte(gray);
                break;
            case splashModeXBGR8:
            case splashModeRGB8:
            case splashModeBGR8:
                imgData->colorMap->getRGB(p, &rgb);
                *q++ = colToByte(rgb.r);
                *q++ = colToByte(rgb.g);
                *q++ = colToByte(rgb.b);
                if (imgData->colorMode == splashModeXBGR8)
                    *q++ = 255;
                break;
            case splashModeCMYK8:
                imgData->colorMap->getCMYK(p, &cmyk);
                *q++ = colToByte(cmyk.c);
                *q++ = colToByte(cmyk.m);
                *q++ = colToByte(cmyk.y);
                *q++ = colToByte(cmyk.k);
                break;
            case splashModeDeviceN8:
                imgData->colorMap->getDeviceN(p, &deviceN);
                for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                    *q++ = colToByte(deviceN.c[cp]);
                break;
            }
            *aq++ = alpha;
        }
    }

    ++imgData->y;
    return true;
}

void SplashOutputBase::setPaperColor(SplashColorPtr paperColorA)
{
    splashColorCopy(paperColor, paperColorA);
}

int SplashOutputBase::getBitmapWidth()
{
    return bitmap->getWidth();
}

int SplashOutputBase::getBitmapHeight()
{
    return bitmap->getHeight();
}

SplashBitmap *SplashOutputBase::takeBitmap()
{
    SplashBitmap *ret;

    ret = bitmap;
    bitmap = new SplashBitmap(1, 1, bitmapRowPad, colorMode, colorMode != splashModeMono1, bitmapTopDown);
    return ret;
}

#if 1 //~tmp: turn off anti-aliasing temporarily
bool SplashOutputBase::getVectorAntialias()
{
    return splash->getVectorAntialias();
}

void SplashOutputBase::setVectorAntialias(bool vaa)
{
    vaa = vaa && colorMode != splashModeMono1;
    vectorAntialias = vaa;
    splash->setVectorAntialias(vaa);
}
#endif

void SplashOutputBase::setFreeTypeHinting(bool enable, bool enableSlightHintingA)
{
    enableFreeTypeHinting = enable;
    enableSlightHinting = enableSlightHintingA;
}
