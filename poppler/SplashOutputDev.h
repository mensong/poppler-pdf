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
// Copyright (C) 2012, 2015, 2018-2021 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2015, 2016 William Bader <williambader@hotmail.com>
// Copyright (C) 2018 Stefan Brüns <stefan.bruens@rwth-aachen.de>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#ifndef SPLASHOUTPUTDEV_H
#define SPLASHOUTPUTDEV_H

#include "poppler-config.h"
#include "poppler_private_export.h"
#include "OutputDev.h"
#include "GlobalParams.h"
#include "SplashOutputBase.h"

struct SplashTransparencyGroup;

//------------------------------------------------------------------------
// SplashOutputDev
//------------------------------------------------------------------------

class POPPLER_PRIVATE_EXPORT SplashOutputDev : public SplashOutputBase
{
public:
    // Constructor.
    SplashOutputDev(SplashColorMode colorModeA, int bitmapRowPadA, bool reverseVideoA, SplashColorPtr paperColorA, bool bitmapTopDownA = true, SplashThinLineMode thinLineMode = splashThinLineDefault,
                    bool overprintPreviewA = globalParams->getOverprintPreview());

    // Destructor.
    ~SplashOutputDev() override;

    //----- get info about output device

    // Does this device use tilingPatternFill()?  If this returns false,
    // tiling pattern fills will be reduced to a series of other drawing
    // operations.
    bool useTilingPatternFill() override { return true; }

    //----- update graphics state
    void updateFillColorSpace(GfxState *state) override;
    void updateStrokeColorSpace(GfxState *state) override;
    void updateBlendMode(GfxState *state) override;
    void updateFillOpacity(GfxState *state) override;
    void updateStrokeOpacity(GfxState *state) override;
    void updatePatternOpacity(GfxState *state) override;
    void clearPatternOpacity(GfxState *state) override;
    void updateFillOverprint(GfxState *state) override;
    void updateStrokeOverprint(GfxState *state) override;
    void updateOverprintMode(GfxState *state) override;
    void updateTransfer(GfxState *state) override;

    //----- path painting
    bool tilingPatternFill(GfxState *state, Gfx *gfx, Catalog *catalog, GfxTilingPattern *tPat, const double *mat, int x0, int y0, int x1, int y1, double xStep, double yStep) override;
    bool functionShadedFill(GfxState *state, GfxFunctionShading *shading) override;
    bool axialShadedFill(GfxState *state, GfxAxialShading *shading, double tMin, double tMax) override;
    bool radialShadedFill(GfxState *state, GfxRadialShading *shading, double tMin, double tMax) override;
    bool gouraudTriangleShadedFill(GfxState *state, GfxGouraudTriangleShading *shading) override;

    //----- image drawing
    void setSoftMaskFromImageMask(GfxState *state, Object *ref, Stream *str, int width, int height, bool invert, bool inlineImg, double *baseMatrix) override;
    void unsetSoftMaskFromImageMask(GfxState *state, double *baseMatrix) override;
    void drawMaskedImage(GfxState *state, Object *ref, Stream *str, int width, int height, GfxImageColorMap *colorMap, bool interpolate, Stream *maskStr, int maskWidth, int maskHeight, bool maskInvert, bool maskInterpolate) override;
    void drawSoftMaskedImage(GfxState *state, Object *ref, Stream *str, int width, int height, GfxImageColorMap *colorMap, bool interpolate, Stream *maskStr, int maskWidth, int maskHeight, GfxImageColorMap *maskColorMap,
                             bool maskInterpolate) override;

    //----- transparency groups and soft masks
    bool checkTransparencyGroup(GfxState *state, bool knockout) override;
    void beginTransparencyGroup(GfxState *state, const double *bbox, GfxColorSpace *blendingColorSpace, bool isolated, bool knockout, bool forSoftMask) override;
    void endTransparencyGroup(GfxState *state) override;
    void paintTransparencyGroup(GfxState *state, const double *bbox) override;
    void setSoftMask(GfxState *state, const double *bbox, bool alpha, Function *transferFunc, GfxColor *backdropColor) override;
    void clearSoftMask(GfxState *state) override;

    // If <skipTextA> is true, don't draw horizontal text.
    // If <skipRotatedTextA> is true, don't draw rotated (non-horizontal) text.
    void setSkipText(bool skipHorizTextA, bool skipRotatedTextA)
    {
        skipHorizText = skipHorizTextA;
        skipRotatedText = skipRotatedTextA;
    }

private:
    bool univariateShadedFill(GfxState *state, SplashUnivariatePattern *pattern, double tMin, double tMax);
    static bool tilingBitmapSrc(void *data, SplashColorPtr line, unsigned char *alphaLine);

    SplashTransparencyGroup * // transparency group stack
            transpGroupStack;
};

#endif
