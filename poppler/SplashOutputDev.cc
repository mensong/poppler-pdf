//========================================================================
//
// SplashOutputDev.cc
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
// Copyright (C) 2006-2021 Albert Astals Cid <aacid@kde.org>
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
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#include <config.h>

#include <cstring>
#include <cmath>
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
#include "SplashOutputDev.h"
#include <algorithm>
#include "SplashFonts.h"

static const double s_minLineWidth = 0.0;

//------------------------------------------------------------------------
// Divide a 16-bit value (in [0, 255*255]) by 255, returning an 8-bit result.
static inline unsigned char div255(int x)
{
    return (unsigned char)((x + (x >> 8) + 0x80) >> 8);
}

//------------------------------------------------------------------------
// Blend functions
//------------------------------------------------------------------------

static void splashOutBlendMultiply(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            blend[i] = (dest[i] * src[i]) / 255;
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
}

static void splashOutBlendScreen(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            blend[i] = dest[i] + src[i] - (dest[i] * src[i]) / 255;
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
}

static void splashOutBlendOverlay(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            blend[i] = dest[i] < 0x80 ? (src[i] * 2 * dest[i]) / 255 : 255 - 2 * ((255 - src[i]) * (255 - dest[i])) / 255;
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
}

static void splashOutBlendDarken(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            blend[i] = dest[i] < src[i] ? dest[i] : src[i];
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
}

static void splashOutBlendLighten(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            blend[i] = dest[i] > src[i] ? dest[i] : src[i];
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
}

static void splashOutBlendColorDodge(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i, x;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            if (src[i] == 255) {
                blend[i] = 255;
            } else {
                x = (dest[i] * 255) / (255 - src[i]);
                blend[i] = x <= 255 ? x : 255;
            }
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
}

static void splashOutBlendColorBurn(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i, x;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            if (src[i] == 0) {
                blend[i] = 0;
            } else {
                x = ((255 - dest[i]) * 255) / src[i];
                blend[i] = x <= 255 ? 255 - x : 0;
            }
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
}

static void splashOutBlendHardLight(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            blend[i] = src[i] < 0x80 ? (dest[i] * 2 * src[i]) / 255 : 255 - 2 * ((255 - dest[i]) * (255 - src[i])) / 255;
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
}

static void splashOutBlendSoftLight(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i, x;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            if (src[i] < 0x80) {
                blend[i] = dest[i] - (255 - 2 * src[i]) * dest[i] * (255 - dest[i]) / (255 * 255);
            } else {
                if (dest[i] < 0x40) {
                    x = (((((16 * dest[i] - 12 * 255) * dest[i]) / 255) + 4 * 255) * dest[i]) / 255;
                } else {
                    x = (int)sqrt(255.0 * dest[i]);
                }
                blend[i] = dest[i] + (2 * src[i] - 255) * (x - dest[i]) / 255;
            }
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
}

static void splashOutBlendDifference(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            blend[i] = dest[i] < src[i] ? src[i] - dest[i] : dest[i] - src[i];
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
    if (cm == splashModeDeviceN8) {
        for (i = 4; i < splashColorModeNComps[cm]; ++i) {
            if (dest[i] == 0 && src[i] == 0)
                blend[i] = 0;
        }
    }
}

static void splashOutBlendExclusion(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    int i;

    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
        }
    }
    {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            blend[i] = dest[i] + src[i] - (2 * dest[i] * src[i]) / 255;
        }
    }
    if (cm == splashModeCMYK8 || cm == splashModeDeviceN8) {
        for (i = 0; i < splashColorModeNComps[cm]; ++i) {
            dest[i] = 255 - dest[i];
            src[i] = 255 - src[i];
            blend[i] = 255 - blend[i];
        }
    }
    if (cm == splashModeDeviceN8) {
        for (i = 4; i < splashColorModeNComps[cm]; ++i) {
            if (dest[i] == 0 && src[i] == 0)
                blend[i] = 0;
        }
    }
}

static int getLum(int r, int g, int b)
{
    return (int)(0.3 * r + 0.59 * g + 0.11 * b);
}

static int getSat(int r, int g, int b)
{
    int rgbMin, rgbMax;

    rgbMin = rgbMax = r;
    if (g < rgbMin) {
        rgbMin = g;
    } else if (g > rgbMax) {
        rgbMax = g;
    }
    if (b < rgbMin) {
        rgbMin = b;
    } else if (b > rgbMax) {
        rgbMax = b;
    }
    return rgbMax - rgbMin;
}

static void clipColor(int rIn, int gIn, int bIn, unsigned char *rOut, unsigned char *gOut, unsigned char *bOut)
{
    int lum, rgbMin, rgbMax;

    lum = getLum(rIn, gIn, bIn);
    rgbMin = rgbMax = rIn;
    if (gIn < rgbMin) {
        rgbMin = gIn;
    } else if (gIn > rgbMax) {
        rgbMax = gIn;
    }
    if (bIn < rgbMin) {
        rgbMin = bIn;
    } else if (bIn > rgbMax) {
        rgbMax = bIn;
    }
    if (rgbMin < 0) {
        *rOut = (unsigned char)(lum + ((rIn - lum) * lum) / (lum - rgbMin));
        *gOut = (unsigned char)(lum + ((gIn - lum) * lum) / (lum - rgbMin));
        *bOut = (unsigned char)(lum + ((bIn - lum) * lum) / (lum - rgbMin));
    } else if (rgbMax > 255) {
        *rOut = (unsigned char)(lum + ((rIn - lum) * (255 - lum)) / (rgbMax - lum));
        *gOut = (unsigned char)(lum + ((gIn - lum) * (255 - lum)) / (rgbMax - lum));
        *bOut = (unsigned char)(lum + ((bIn - lum) * (255 - lum)) / (rgbMax - lum));
    } else {
        *rOut = rIn;
        *gOut = gIn;
        *bOut = bIn;
    }
}

static void setLum(unsigned char rIn, unsigned char gIn, unsigned char bIn, int lum, unsigned char *rOut, unsigned char *gOut, unsigned char *bOut)
{
    int d;

    d = lum - getLum(rIn, gIn, bIn);
    clipColor(rIn + d, gIn + d, bIn + d, rOut, gOut, bOut);
}

static void setSat(unsigned char rIn, unsigned char gIn, unsigned char bIn, int sat, unsigned char *rOut, unsigned char *gOut, unsigned char *bOut)
{
    int rgbMin, rgbMid, rgbMax;
    unsigned char *minOut, *midOut, *maxOut;

    if (rIn < gIn) {
        rgbMin = rIn;
        minOut = rOut;
        rgbMid = gIn;
        midOut = gOut;
    } else {
        rgbMin = gIn;
        minOut = gOut;
        rgbMid = rIn;
        midOut = rOut;
    }
    if (bIn > rgbMid) {
        rgbMax = bIn;
        maxOut = bOut;
    } else if (bIn > rgbMin) {
        rgbMax = rgbMid;
        maxOut = midOut;
        rgbMid = bIn;
        midOut = bOut;
    } else {
        rgbMax = rgbMid;
        maxOut = midOut;
        rgbMid = rgbMin;
        midOut = minOut;
        rgbMin = bIn;
        minOut = bOut;
    }
    if (rgbMax > rgbMin) {
        *midOut = (unsigned char)((rgbMid - rgbMin) * sat) / (rgbMax - rgbMin);
        *maxOut = (unsigned char)sat;
    } else {
        *midOut = *maxOut = 0;
    }
    *minOut = 0;
}

static void splashOutBlendHue(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    unsigned char r0, g0, b0;
    unsigned char r1, g1, b1;
    int i;
    SplashColor src2, dest2;

    switch (cm) {
    case splashModeMono1:
    case splashModeMono8:
        blend[0] = dest[0];
        break;
    case splashModeXBGR8:
        src[3] = 255;
        // fallthrough
    case splashModeRGB8:
    case splashModeBGR8:
        setSat(src[0], src[1], src[2], getSat(dest[0], dest[1], dest[2]), &r0, &g0, &b0);
        setLum(r0, g0, b0, getLum(dest[0], dest[1], dest[2]), &blend[0], &blend[1], &blend[2]);
        break;
    case splashModeCMYK8:
    case splashModeDeviceN8:
        for (i = 0; i < 4; i++) {
            // convert to additive
            src2[i] = 0xff - src[i];
            dest2[i] = 0xff - dest[i];
        }
        // NB: inputs have already been converted to additive mode
        setSat(src2[0], src2[1], src2[2], getSat(dest2[0], dest2[1], dest2[2]), &r0, &g0, &b0);
        setLum(r0, g0, b0, getLum(dest2[0], dest2[1], dest2[2]), &r1, &g1, &b1);
        blend[0] = r1;
        blend[1] = g1;
        blend[2] = b1;
        blend[3] = dest2[3];
        for (i = 0; i < 4; i++) {
            // convert back to subtractive
            blend[i] = 0xff - blend[i];
        }
        break;
    }
}

static void splashOutBlendSaturation(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    unsigned char r0, g0, b0;
    unsigned char r1, g1, b1;
    int i;
    SplashColor src2, dest2;

    switch (cm) {
    case splashModeMono1:
    case splashModeMono8:
        blend[0] = dest[0];
        break;
    case splashModeXBGR8:
        src[3] = 255;
        // fallthrough
    case splashModeRGB8:
    case splashModeBGR8:
        setSat(dest[0], dest[1], dest[2], getSat(src[0], src[1], src[2]), &r0, &g0, &b0);
        setLum(r0, g0, b0, getLum(dest[0], dest[1], dest[2]), &blend[0], &blend[1], &blend[2]);
        break;
    case splashModeCMYK8:
    case splashModeDeviceN8:
        for (i = 0; i < 4; i++) {
            // convert to additive
            src2[i] = 0xff - src[i];
            dest2[i] = 0xff - dest[i];
        }
        setSat(dest2[0], dest2[1], dest2[2], getSat(src2[0], src2[1], src2[2]), &r0, &g0, &b0);
        setLum(r0, g0, b0, getLum(dest2[0], dest2[1], dest2[2]), &r1, &g1, &b1);
        blend[0] = r1;
        blend[1] = g1;
        blend[2] = b1;
        blend[3] = dest2[3];
        for (i = 0; i < 4; i++) {
            // convert back to subtractive
            blend[i] = 0xff - blend[i];
        }
        break;
    }
}

static void splashOutBlendColor(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    unsigned char r, g, b;
    int i;
    SplashColor src2, dest2;

    switch (cm) {
    case splashModeMono1:
    case splashModeMono8:
        blend[0] = dest[0];
        break;
    case splashModeXBGR8:
        src[3] = 255;
        // fallthrough
    case splashModeRGB8:
    case splashModeBGR8:
        setLum(src[0], src[1], src[2], getLum(dest[0], dest[1], dest[2]), &blend[0], &blend[1], &blend[2]);
        break;
    case splashModeCMYK8:
    case splashModeDeviceN8:
        for (i = 0; i < 4; i++) {
            // convert to additive
            src2[i] = 0xff - src[i];
            dest2[i] = 0xff - dest[i];
        }
        setLum(src2[0], src2[1], src2[2], getLum(dest2[0], dest2[1], dest2[2]), &r, &g, &b);
        blend[0] = r;
        blend[1] = g;
        blend[2] = b;
        blend[3] = dest2[3];
        for (i = 0; i < 4; i++) {
            // convert back to subtractive
            blend[i] = 0xff - blend[i];
        }
        break;
    }
}

static void splashOutBlendLuminosity(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm)
{
    unsigned char r, g, b;
    int i;
    SplashColor src2, dest2;

    switch (cm) {
    case splashModeMono1:
    case splashModeMono8:
        blend[0] = dest[0];
        break;
    case splashModeXBGR8:
        src[3] = 255;
        // fallthrough
    case splashModeRGB8:
    case splashModeBGR8:
        setLum(dest[0], dest[1], dest[2], getLum(src[0], src[1], src[2]), &blend[0], &blend[1], &blend[2]);
        break;
    case splashModeCMYK8:
    case splashModeDeviceN8:
        for (i = 0; i < 4; i++) {
            // convert to additive
            src2[i] = 0xff - src[i];
            dest2[i] = 0xff - dest[i];
        }
        setLum(dest2[0], dest2[1], dest2[2], getLum(src2[0], src2[1], src2[2]), &r, &g, &b);
        blend[0] = r;
        blend[1] = g;
        blend[2] = b;
        blend[3] = src2[3];
        for (i = 0; i < 4; i++) {
            // convert back to subtractive
            blend[i] = 0xff - blend[i];
        }
        break;
    }
}

// NB: This must match the GfxBlendMode enum defined in GfxState.h.
static const SplashBlendFunc splashOutBlendFuncs[] = { nullptr,
                                                       &splashOutBlendMultiply,
                                                       &splashOutBlendScreen,
                                                       &splashOutBlendOverlay,
                                                       &splashOutBlendDarken,
                                                       &splashOutBlendLighten,
                                                       &splashOutBlendColorDodge,
                                                       &splashOutBlendColorBurn,
                                                       &splashOutBlendHardLight,
                                                       &splashOutBlendSoftLight,
                                                       &splashOutBlendDifference,
                                                       &splashOutBlendExclusion,
                                                       &splashOutBlendHue,
                                                       &splashOutBlendSaturation,
                                                       &splashOutBlendColor,
                                                       &splashOutBlendLuminosity };

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
// SplashOutputDev
//------------------------------------------------------------------------

SplashOutputDev::SplashOutputDev(SplashColorMode colorModeA, int bitmapRowPadA, bool reverseVideoA, SplashColorPtr paperColorA, bool bitmapTopDownA, SplashThinLineMode thinLineMode, bool overprintPreviewA)
    : SplashOutputBase(colorModeA, bitmapRowPadA, reverseVideoA, paperColorA, bitmapTopDownA, thinLineMode, overprintPreviewA)
{
    transpGroupStack = nullptr;
}

SplashOutputDev::~SplashOutputDev() { }

void SplashOutputDev::updateFillColorSpace(GfxState *state)
{
    if (colorMode == splashModeDeviceN8)
        state->getFillColorSpace()->createMapping(bitmap->getSeparationList(), SPOT_NCOMPS);
}

void SplashOutputDev::updateStrokeColorSpace(GfxState *state)
{
    if (colorMode == splashModeDeviceN8)
        state->getStrokeColorSpace()->createMapping(bitmap->getSeparationList(), SPOT_NCOMPS);
}

void SplashOutputDev::updateBlendMode(GfxState *state)
{
    splash->setBlendFunc(splashOutBlendFuncs[state->getBlendMode()]);
}

void SplashOutputDev::updateFillOpacity(GfxState *state)
{
    splash->setFillAlpha((SplashCoord)state->getFillOpacity());
    if (transpGroupStack != nullptr && (SplashCoord)state->getFillOpacity() < transpGroupStack->knockoutOpacity) {
        transpGroupStack->knockoutOpacity = (SplashCoord)state->getFillOpacity();
    }
}

void SplashOutputDev::updateStrokeOpacity(GfxState *state)
{
    splash->setStrokeAlpha((SplashCoord)state->getStrokeOpacity());
    if (transpGroupStack != nullptr && (SplashCoord)state->getStrokeOpacity() < transpGroupStack->knockoutOpacity) {
        transpGroupStack->knockoutOpacity = (SplashCoord)state->getStrokeOpacity();
    }
}

void SplashOutputDev::updatePatternOpacity(GfxState *state)
{
    splash->setPatternAlpha((SplashCoord)state->getStrokeOpacity(), (SplashCoord)state->getFillOpacity());
}

void SplashOutputDev::clearPatternOpacity(GfxState *state)
{
    splash->clearPatternAlpha();
}

void SplashOutputDev::updateFillOverprint(GfxState *state)
{
    splash->setFillOverprint(state->getFillOverprint());
}

void SplashOutputDev::updateStrokeOverprint(GfxState *state)
{
    splash->setStrokeOverprint(state->getStrokeOverprint());
}

void SplashOutputDev::updateOverprintMode(GfxState *state)
{
    splash->setOverprintMode(state->getOverprintMode());
}

void SplashOutputDev::updateTransfer(GfxState *state)
{
    Function **transfer;
    unsigned char red[256], green[256], blue[256], gray[256];
    double x, y;
    int i;

    transfer = state->getTransfer();
    if (transfer[0] && transfer[0]->getInputSize() == 1 && transfer[0]->getOutputSize() == 1) {
        if (transfer[1] && transfer[1]->getInputSize() == 1 && transfer[1]->getOutputSize() == 1 && transfer[2] && transfer[2]->getInputSize() == 1 && transfer[2]->getOutputSize() == 1 && transfer[3] && transfer[3]->getInputSize() == 1
            && transfer[3]->getOutputSize() == 1) {
            for (i = 0; i < 256; ++i) {
                x = i / 255.0;
                transfer[0]->transform(&x, &y);
                red[i] = (unsigned char)(y * 255.0 + 0.5);
                transfer[1]->transform(&x, &y);
                green[i] = (unsigned char)(y * 255.0 + 0.5);
                transfer[2]->transform(&x, &y);
                blue[i] = (unsigned char)(y * 255.0 + 0.5);
                transfer[3]->transform(&x, &y);
                gray[i] = (unsigned char)(y * 255.0 + 0.5);
            }
        } else {
            for (i = 0; i < 256; ++i) {
                x = i / 255.0;
                transfer[0]->transform(&x, &y);
                red[i] = green[i] = blue[i] = gray[i] = (unsigned char)(y * 255.0 + 0.5);
            }
        }
    } else {
        for (i = 0; i < 256; ++i) {
            red[i] = green[i] = blue[i] = gray[i] = (unsigned char)i;
        }
    }
    splash->setTransfer(red, green, blue, gray);
}

void SplashOutputDev::setSoftMaskFromImageMask(GfxState *state, Object *ref, Stream *str, int width, int height, bool invert, bool inlineImg, double *baseMatrix)
{
    const double *ctm;
    SplashCoord mat[6];
    SplashOutImageMaskData imgMaskData;
    Splash *maskSplash;
    SplashColor maskColor;
    double bbox[4] = { 0, 0, 1, 1 }; // default;

    if (state->getFillColorSpace()->isNonMarking()) {
        return;
    }

    ctm = state->getCTM();
    for (int i = 0; i < 6; ++i) {
        if (!std::isfinite(ctm[i]))
            return;
    }

    beginTransparencyGroup(state, bbox, nullptr, false, false, false);
    baseMatrix[4] -= transpGroupStack->tx;
    baseMatrix[5] -= transpGroupStack->ty;

    ctm = state->getCTM();
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

    transpGroupStack->softmask = new SplashBitmap(bitmap->getWidth(), bitmap->getHeight(), 1, splashModeMono8, false);
    maskSplash = new Splash(transpGroupStack->softmask, vectorAntialias);
    maskColor[0] = 0;
    maskSplash->clear(maskColor);
    maskColor[0] = 0xff;
    maskSplash->setFillPattern(new SplashSolidColor(maskColor));
    maskSplash->fillImageMask(&imageMaskSrc, &imgMaskData, width, height, mat, t3GlyphStack != nullptr);
    delete maskSplash;
    delete imgMaskData.imgStr;
    str->close();
}

void SplashOutputDev::unsetSoftMaskFromImageMask(GfxState *state, double *baseMatrix)
{
    double bbox[4] = { 0, 0, 1, 1 }; // dummy

    if (!transpGroupStack) {
        return;
    }

    /* transfer mask to alpha channel! */
    // memcpy(maskBitmap->getAlphaPtr(), maskBitmap->getDataPtr(), bitmap->getRowSize() * bitmap->getHeight());
    // memset(maskBitmap->getDataPtr(), 0, bitmap->getRowSize() * bitmap->getHeight());
    if (transpGroupStack->softmask != nullptr) {
        unsigned char *dest = bitmap->getAlphaPtr();
        unsigned char *src = transpGroupStack->softmask->getDataPtr();
        for (int c = 0; c < transpGroupStack->softmask->getRowSize() * transpGroupStack->softmask->getHeight(); c++) {
            dest[c] = src[c];
        }
        delete transpGroupStack->softmask;
        transpGroupStack->softmask = nullptr;
    }
    endTransparencyGroup(state);
    baseMatrix[4] += transpGroupStack->tx;
    baseMatrix[5] += transpGroupStack->ty;
    paintTransparencyGroup(state, bbox);
}

struct TilingSplashOutBitmap
{
    SplashBitmap *bitmap;
    SplashPattern *pattern;
    SplashColorMode colorMode;
    int paintType;
    int repeatX;
    int repeatY;
    int y;
};

bool SplashOutputDev::tilingBitmapSrc(void *data, SplashColorPtr colorLine, unsigned char *alphaLine)
{
    TilingSplashOutBitmap *imgData = (TilingSplashOutBitmap *)data;

    if (imgData->y == imgData->bitmap->getHeight()) {
        imgData->repeatY--;
        if (imgData->repeatY == 0)
            return false;
        imgData->y = 0;
    }

    if (imgData->paintType == 1) {
        const SplashColorMode cMode = imgData->bitmap->getMode();
        SplashColorPtr q = colorLine;
        // For splashModeBGR8 and splashModeXBGR8 we need to use getPixel
        // for the others we can use raw access
        if (cMode == splashModeBGR8 || cMode == splashModeXBGR8) {
            for (int m = 0; m < imgData->repeatX; m++) {
                for (int x = 0; x < imgData->bitmap->getWidth(); x++) {
                    imgData->bitmap->getPixel(x, imgData->y, q);
                    q += splashColorModeNComps[cMode];
                }
            }
        } else {
            const int n = imgData->bitmap->getRowSize();
            SplashColorPtr p;
            for (int m = 0; m < imgData->repeatX; m++) {
                p = imgData->bitmap->getDataPtr() + imgData->y * imgData->bitmap->getRowSize();
                for (int x = 0; x < n; ++x) {
                    *q++ = *p++;
                }
            }
        }
        if (alphaLine != nullptr) {
            SplashColorPtr aq = alphaLine;
            SplashColorPtr p;
            const int n = imgData->bitmap->getWidth() - 1;
            for (int m = 0; m < imgData->repeatX; m++) {
                p = imgData->bitmap->getAlphaPtr() + imgData->y * imgData->bitmap->getWidth();
                for (int x = 0; x < n; ++x) {
                    *aq++ = *p++;
                }
                // This is a hack, because of how Splash antialias works if we overwrite the
                // last alpha pixel of the tile most/all of the files look much better
                *aq++ = (n == 0) ? *p : *(p - 1);
            }
        }
    } else {
        SplashColor col, pat;
        SplashColorPtr dest = colorLine;
        for (int m = 0; m < imgData->repeatX; m++) {
            for (int x = 0; x < imgData->bitmap->getWidth(); x++) {
                imgData->bitmap->getPixel(x, imgData->y, col);
                imgData->pattern->getColor(x, imgData->y, pat);
                for (int i = 0; i < splashColorModeNComps[imgData->colorMode]; ++i) {
                    if (imgData->colorMode == splashModeCMYK8 || imgData->colorMode == splashModeDeviceN8)
                        dest[i] = div255(pat[i] * (255 - col[0]));
                    else
                        dest[i] = 255 - div255((255 - pat[i]) * (255 - col[0]));
                }
                dest += splashColorModeNComps[imgData->colorMode];
            }
        }
        if (alphaLine != nullptr) {
            const int y = (imgData->y == imgData->bitmap->getHeight() - 1 && imgData->y > 50) ? imgData->y - 1 : imgData->y;
            SplashColorPtr aq = alphaLine;
            SplashColorPtr p;
            const int n = imgData->bitmap->getWidth();
            for (int m = 0; m < imgData->repeatX; m++) {
                p = imgData->bitmap->getAlphaPtr() + y * imgData->bitmap->getWidth();
                for (int x = 0; x < n; ++x) {
                    *aq++ = *p++;
                }
            }
        }
    }
    ++imgData->y;
    return true;
}

void SplashOutputDev::drawMaskedImage(GfxState *state, Object *ref, Stream *str, int width, int height, GfxImageColorMap *colorMap, bool interpolate, Stream *maskStr, int maskWidth, int maskHeight, bool maskInvert, bool maskInterpolate)
{
    GfxImageColorMap *maskColorMap;
    SplashCoord mat[6];
    SplashOutMaskedImageData imgData;
    SplashOutImageMaskData imgMaskData;
    SplashColorMode srcMode;
    SplashBitmap *maskBitmap;
    Splash *maskSplash;
    SplashColor maskColor;
    GfxGray gray;
    GfxRGB rgb;
    GfxCMYK cmyk;
    GfxColor deviceN;
    unsigned char pix;
    int n, i;

    colorMap->getColorSpace()->createMapping(bitmap->getSeparationList(), SPOT_NCOMPS);
    setOverprintMask(colorMap->getColorSpace(), state->getFillOverprint(), state->getOverprintMode(), nullptr);

    // If the mask is higher resolution than the image, use
    // drawSoftMaskedImage() instead.
    if (maskWidth > width || maskHeight > height) {
        Object maskDecode(new Array((xref) ? xref : doc->getXRef()));
        maskDecode.arrayAdd(Object(maskInvert ? 0 : 1));
        maskDecode.arrayAdd(Object(maskInvert ? 1 : 0));
        maskColorMap = new GfxImageColorMap(1, &maskDecode, new GfxDeviceGrayColorSpace());
        drawSoftMaskedImage(state, ref, str, width, height, colorMap, interpolate, maskStr, maskWidth, maskHeight, maskColorMap, maskInterpolate);
        delete maskColorMap;

    } else {
        //----- scale the mask image to the same size as the source image

        mat[0] = (SplashCoord)width;
        mat[1] = 0;
        mat[2] = 0;
        mat[3] = (SplashCoord)height;
        mat[4] = 0;
        mat[5] = 0;
        imgMaskData.imgStr = new ImageStream(maskStr, maskWidth, 1, 1);
        imgMaskData.imgStr->reset();
        imgMaskData.invert = maskInvert ? false : true;
        imgMaskData.width = maskWidth;
        imgMaskData.height = maskHeight;
        imgMaskData.y = 0;
        maskBitmap = new SplashBitmap(width, height, 1, splashModeMono1, false);
        if (!maskBitmap->getDataPtr()) {
            delete maskBitmap;
            width = height = 1;
            maskBitmap = new SplashBitmap(width, height, 1, splashModeMono1, false);
        }
        maskSplash = new Splash(maskBitmap, false);
        maskColor[0] = 0;
        maskSplash->clear(maskColor);
        maskColor[0] = 0xff;
        maskSplash->setFillPattern(new SplashSolidColor(maskColor));
        maskSplash->fillImageMask(&imageMaskSrc, &imgMaskData, maskWidth, maskHeight, mat, false);
        delete imgMaskData.imgStr;
        maskStr->close();
        delete maskSplash;

        //----- draw the source image

        const double *ctm = state->getCTM();
        for (i = 0; i < 6; ++i) {
            if (!std::isfinite(ctm[i])) {
                delete maskBitmap;
                return;
            }
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
        imgData.mask = maskBitmap;
        imgData.colorMode = colorMode;
        imgData.width = width;
        imgData.height = height;
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
                imgData.lookup = (SplashColorPtr)gmallocn(n, 4);
                for (i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getRGB(&pix, &rgb);
                    imgData.lookup[4 * i] = colToByte(rgb.r);
                    imgData.lookup[4 * i + 1] = colToByte(rgb.g);
                    imgData.lookup[4 * i + 2] = colToByte(rgb.b);
                    imgData.lookup[4 * i + 3] = 255;
                }
                break;
            case splashModeCMYK8:
                imgData.lookup = (SplashColorPtr)gmallocn(n, 4);
                for (i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getCMYK(&pix, &cmyk);
                    imgData.lookup[4 * i] = colToByte(cmyk.c);
                    imgData.lookup[4 * i + 1] = colToByte(cmyk.m);
                    imgData.lookup[4 * i + 2] = colToByte(cmyk.y);
                    imgData.lookup[4 * i + 3] = colToByte(cmyk.k);
                }
                break;
            case splashModeDeviceN8:
                imgData.lookup = (SplashColorPtr)gmallocn(n, SPOT_NCOMPS + 4);
                for (i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getDeviceN(&pix, &deviceN);
                    for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                        imgData.lookup[(SPOT_NCOMPS + 4) * i + cp] = colToByte(deviceN.c[cp]);
                }
                break;
            }
        }

        if (colorMode == splashModeMono1) {
            srcMode = splashModeMono8;
        } else {
            srcMode = colorMode;
        }
        splash->drawImage(&maskedImageSrc, nullptr, &imgData, srcMode, true, width, height, mat, interpolate);
        delete maskBitmap;
        gfree(imgData.lookup);
        delete imgData.imgStr;
        str->close();
    }
}

void SplashOutputDev::drawSoftMaskedImage(GfxState *state, Object * /* ref */, Stream *str, int width, int height, GfxImageColorMap *colorMap, bool interpolate, Stream *maskStr, int maskWidth, int maskHeight, GfxImageColorMap *maskColorMap,
                                          bool maskInterpolate)
{
    SplashCoord mat[6];
    SplashOutImageData imgData;
    SplashOutImageData imgMaskData;
    SplashColorMode srcMode;
    SplashBitmap *maskBitmap;
    Splash *maskSplash;
    SplashColor maskColor;
    GfxGray gray;
    GfxRGB rgb;
    GfxCMYK cmyk;
    GfxColor deviceN;
    unsigned char pix;

    colorMap->getColorSpace()->createMapping(bitmap->getSeparationList(), SPOT_NCOMPS);
    setOverprintMask(colorMap->getColorSpace(), state->getFillOverprint(), state->getOverprintMode(), nullptr);

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

    //----- set up the soft mask

    if (maskColorMap->getMatteColor() != nullptr) {
        int maskChars;
        if (checkedMultiply(maskWidth, maskHeight, &maskChars)) {
            return;
        }
        unsigned char *data = (unsigned char *)gmalloc(maskChars);
        maskStr->reset();
        const int readChars = maskStr->doGetChars(maskChars, data);
        if (unlikely(readChars < maskChars)) {
            memset(&data[readChars], 0, maskChars - readChars);
        }
        maskStr->close();
        maskStr = new AutoFreeMemStream((char *)data, 0, maskChars, maskStr->getDictObject()->copy());
    }
    imgMaskData.imgStr = new ImageStream(maskStr, maskWidth, maskColorMap->getNumPixelComps(), maskColorMap->getBits());
    imgMaskData.imgStr->reset();
    imgMaskData.colorMap = maskColorMap;
    imgMaskData.maskColors = nullptr;
    imgMaskData.colorMode = splashModeMono8;
    imgMaskData.width = maskWidth;
    imgMaskData.height = maskHeight;
    imgMaskData.y = 0;
    imgMaskData.maskStr = nullptr;
    imgMaskData.maskColorMap = nullptr;
    const unsigned imgMaskDataLookupSize = 1 << maskColorMap->getBits();
    imgMaskData.lookup = (SplashColorPtr)gmalloc(imgMaskDataLookupSize);
    for (unsigned i = 0; i < imgMaskDataLookupSize; ++i) {
        pix = (unsigned char)i;
        maskColorMap->getGray(&pix, &gray);
        imgMaskData.lookup[i] = colToByte(gray);
    }
    maskBitmap = new SplashBitmap(bitmap->getWidth(), bitmap->getHeight(), 1, splashModeMono8, false);
    maskSplash = new Splash(maskBitmap, vectorAntialias);
    maskColor[0] = 0;
    maskSplash->clear(maskColor);
    maskSplash->drawImage(&imageSrc, nullptr, &imgMaskData, splashModeMono8, false, maskWidth, maskHeight, mat, maskInterpolate);
    delete imgMaskData.imgStr;
    if (maskColorMap->getMatteColor() == nullptr) {
        maskStr->close();
    }
    gfree(imgMaskData.lookup);
    delete maskSplash;
    splash->setSoftMask(maskBitmap);

    //----- draw the source image

    imgData.imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
    imgData.imgStr->reset();
    imgData.colorMap = colorMap;
    imgData.maskColors = nullptr;
    imgData.colorMode = colorMode;
    imgData.width = width;
    imgData.height = height;
    imgData.maskStr = nullptr;
    imgData.maskColorMap = nullptr;
    if (maskColorMap->getMatteColor() != nullptr) {
        getMatteColor(colorMode, colorMap, maskColorMap->getMatteColor(), imgData.matteColor);
        imgData.maskColorMap = maskColorMap;
        imgData.maskStr = new ImageStream(maskStr, maskWidth, maskColorMap->getNumPixelComps(), maskColorMap->getBits());
        imgData.maskStr->reset();
    }
    imgData.y = 0;

    // special case for one-channel (monochrome/gray/separation) images:
    // build a lookup table here
    imgData.lookup = nullptr;
    if (colorMap->getNumPixelComps() == 1) {
        const unsigned n = 1 << colorMap->getBits();
        switch (colorMode) {
        case splashModeMono1:
        case splashModeMono8:
            imgData.lookup = (SplashColorPtr)gmalloc(n);
            for (unsigned i = 0; i < n; ++i) {
                pix = (unsigned char)i;
                colorMap->getGray(&pix, &gray);
                imgData.lookup[i] = colToByte(gray);
            }
            break;
        case splashModeRGB8:
        case splashModeBGR8:
            imgData.lookup = (SplashColorPtr)gmallocn_checkoverflow(n, 3);
            if (likely(imgData.lookup != nullptr)) {
                for (unsigned i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getRGB(&pix, &rgb);
                    imgData.lookup[3 * i] = colToByte(rgb.r);
                    imgData.lookup[3 * i + 1] = colToByte(rgb.g);
                    imgData.lookup[3 * i + 2] = colToByte(rgb.b);
                }
            }
            break;
        case splashModeXBGR8:
            imgData.lookup = (SplashColorPtr)gmallocn_checkoverflow(n, 4);
            if (likely(imgData.lookup != nullptr)) {
                for (unsigned i = 0; i < n; ++i) {
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
            imgData.lookup = (SplashColorPtr)gmallocn_checkoverflow(n, 4);
            if (likely(imgData.lookup != nullptr)) {
                for (unsigned i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getCMYK(&pix, &cmyk);
                    imgData.lookup[4 * i] = colToByte(cmyk.c);
                    imgData.lookup[4 * i + 1] = colToByte(cmyk.m);
                    imgData.lookup[4 * i + 2] = colToByte(cmyk.y);
                    imgData.lookup[4 * i + 3] = colToByte(cmyk.k);
                }
            }
            break;
        case splashModeDeviceN8:
            imgData.lookup = (SplashColorPtr)gmallocn_checkoverflow(n, SPOT_NCOMPS + 4);
            if (likely(imgData.lookup != nullptr)) {
                for (unsigned i = 0; i < n; ++i) {
                    pix = (unsigned char)i;
                    colorMap->getDeviceN(&pix, &deviceN);
                    for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                        imgData.lookup[(SPOT_NCOMPS + 4) * i + cp] = colToByte(deviceN.c[cp]);
                }
            }
            break;
        }
    }

    if (colorMode == splashModeMono1) {
        srcMode = splashModeMono8;
    } else {
        srcMode = colorMode;
    }
    splash->drawImage(&imageSrc, nullptr, &imgData, srcMode, false, width, height, mat, interpolate);
    splash->setSoftMask(nullptr);
    gfree(imgData.lookup);
    delete imgData.maskStr;
    delete imgData.imgStr;
    if (maskColorMap->getMatteColor() != nullptr) {
        maskStr->close();
        delete maskStr;
    }
    str->close();
}

bool SplashOutputDev::checkTransparencyGroup(GfxState *state, bool knockout)
{
    if (state->getFillOpacity() != 1 || state->getStrokeOpacity() != 1 || state->getAlphaIsShape() || state->getBlendMode() != gfxBlendNormal || splash->getSoftMask() != nullptr || knockout)
        return true;
    return transpGroupStack != nullptr && transpGroupStack->shape != nullptr;
}

void SplashOutputDev::beginTransparencyGroup(GfxState *state, const double *bbox, GfxColorSpace *blendingColorSpace, bool isolated, bool knockout, bool forSoftMask)
{
    SplashTransparencyGroup *transpGroup;
    SplashColor color;
    double xMin, yMin, xMax, yMax, x, y;
    int tx, ty, w, h;

    // transform the bbox
    state->transform(bbox[0], bbox[1], &x, &y);
    xMin = xMax = x;
    yMin = yMax = y;
    state->transform(bbox[0], bbox[3], &x, &y);
    if (x < xMin) {
        xMin = x;
    } else if (x > xMax) {
        xMax = x;
    }
    if (y < yMin) {
        yMin = y;
    } else if (y > yMax) {
        yMax = y;
    }
    state->transform(bbox[2], bbox[1], &x, &y);
    if (x < xMin) {
        xMin = x;
    } else if (x > xMax) {
        xMax = x;
    }
    if (y < yMin) {
        yMin = y;
    } else if (y > yMax) {
        yMax = y;
    }
    state->transform(bbox[2], bbox[3], &x, &y);
    if (x < xMin) {
        xMin = x;
    } else if (x > xMax) {
        xMax = x;
    }
    if (y < yMin) {
        yMin = y;
    } else if (y > yMax) {
        yMax = y;
    }
    tx = (int)floor(xMin);
    if (tx < 0) {
        tx = 0;
    } else if (tx >= bitmap->getWidth()) {
        tx = bitmap->getWidth() - 1;
    }
    ty = (int)floor(yMin);
    if (ty < 0) {
        ty = 0;
    } else if (ty >= bitmap->getHeight()) {
        ty = bitmap->getHeight() - 1;
    }
    w = (int)ceil(xMax) - tx + 1;
    if (tx + w > bitmap->getWidth()) {
        w = bitmap->getWidth() - tx;
    }
    if (w < 1) {
        w = 1;
    }
    h = (int)ceil(yMax) - ty + 1;
    if (ty + h > bitmap->getHeight()) {
        h = bitmap->getHeight() - ty;
    }
    if (h < 1) {
        h = 1;
    }

    // push a new stack entry
    transpGroup = new SplashTransparencyGroup();
    transpGroup->softmask = nullptr;
    transpGroup->tx = tx;
    transpGroup->ty = ty;
    transpGroup->blendingColorSpace = blendingColorSpace;
    transpGroup->isolated = isolated;
    transpGroup->shape = (knockout && !isolated) ? SplashBitmap::copy(bitmap) : nullptr;
    transpGroup->knockout = (knockout && isolated);
    transpGroup->knockoutOpacity = 1.0;
    transpGroup->next = transpGroupStack;
    transpGroupStack = transpGroup;

    // save state
    transpGroup->origBitmap = bitmap;
    transpGroup->origSplash = splash;
    transpGroup->fontAA = fontEngine->getAA();

    //~ this handles the blendingColorSpace arg for soft masks, but
    //~   not yet for transparency groups

    // switch to the blending color space
    if (forSoftMask && isolated && blendingColorSpace) {
        if (blendingColorSpace->getMode() == csDeviceGray || blendingColorSpace->getMode() == csCalGray || (blendingColorSpace->getMode() == csICCBased && blendingColorSpace->getNComps() == 1)) {
            colorMode = splashModeMono8;
        } else if (blendingColorSpace->getMode() == csDeviceRGB || blendingColorSpace->getMode() == csCalRGB || (blendingColorSpace->getMode() == csICCBased && blendingColorSpace->getNComps() == 3)) {
            //~ does this need to use BGR8?
            colorMode = splashModeRGB8;
        } else if (blendingColorSpace->getMode() == csDeviceCMYK || (blendingColorSpace->getMode() == csICCBased && blendingColorSpace->getNComps() == 4)) {
            colorMode = splashModeCMYK8;
        }
    }

    // create the temporary bitmap
    bitmap = new SplashBitmap(w, h, bitmapRowPad, colorMode, true, bitmapTopDown, bitmap->getSeparationList());
    if (!bitmap->getDataPtr()) {
        delete bitmap;
        w = h = 1;
        bitmap = new SplashBitmap(w, h, bitmapRowPad, colorMode, true, bitmapTopDown);
    }
    splash = new Splash(bitmap, vectorAntialias, transpGroup->origSplash->getScreen());
    if (transpGroup->next != nullptr && transpGroup->next->knockout) {
        fontEngine->setAA(false);
    }
    splash->setThinLineMode(transpGroup->origSplash->getThinLineMode());
    splash->setMinLineWidth(s_minLineWidth);
    //~ Acrobat apparently copies at least the fill and stroke colors, and
    //~ maybe other state(?) -- but not the clipping path (and not sure
    //~ what else)
    //~ [this is likely the same situation as in type3D1()]
    splash->setFillPattern(transpGroup->origSplash->getFillPattern()->copy());
    splash->setStrokePattern(transpGroup->origSplash->getStrokePattern()->copy());
    if (isolated) {
        splashClearColor(color);
        if (colorMode == splashModeXBGR8)
            color[3] = 255;
        splash->clear(color, 0);
    } else {
        SplashBitmap *shape = (knockout) ? transpGroup->shape : (transpGroup->next != nullptr && transpGroup->next->shape != nullptr) ? transpGroup->next->shape : transpGroup->origBitmap;
        int shapeTx = (knockout) ? tx : (transpGroup->next != nullptr && transpGroup->next->shape != nullptr) ? transpGroup->next->tx + tx : tx;
        int shapeTy = (knockout) ? ty : (transpGroup->next != nullptr && transpGroup->next->shape != nullptr) ? transpGroup->next->ty + ty : ty;
        splash->blitTransparent(transpGroup->origBitmap, tx, ty, 0, 0, w, h);
        splash->setInNonIsolatedGroup(shape, shapeTx, shapeTy);
    }
    transpGroup->tBitmap = bitmap;
    state->shiftCTMAndClip(-tx, -ty);
    updateCTM(state, 0, 0, 0, 0, 0, 0);
}

void SplashOutputDev::endTransparencyGroup(GfxState *state)
{
    // restore state
    delete splash;
    bitmap = transpGroupStack->origBitmap;
    colorMode = bitmap->getMode();
    splash = transpGroupStack->origSplash;
    state->shiftCTMAndClip(transpGroupStack->tx, transpGroupStack->ty);
    updateCTM(state, 0, 0, 0, 0, 0, 0);
}

void SplashOutputDev::paintTransparencyGroup(GfxState *state, const double *bbox)
{
    SplashBitmap *tBitmap;
    SplashTransparencyGroup *transpGroup;
    bool isolated;
    int tx, ty;

    tx = transpGroupStack->tx;
    ty = transpGroupStack->ty;
    tBitmap = transpGroupStack->tBitmap;
    isolated = transpGroupStack->isolated;

    // paint the transparency group onto the parent bitmap
    // - the clip path was set in the parent's state)
    if (tx < bitmap->getWidth() && ty < bitmap->getHeight()) {
        SplashCoord knockoutOpacity = (transpGroupStack->next != nullptr) ? transpGroupStack->next->knockoutOpacity : transpGroupStack->knockoutOpacity;
        splash->setOverprintMask(0xffffffff, false);
        splash->composite(tBitmap, 0, 0, tx, ty, tBitmap->getWidth(), tBitmap->getHeight(), false, !isolated, transpGroupStack->next != nullptr && transpGroupStack->next->knockout, knockoutOpacity);
        fontEngine->setAA(transpGroupStack->fontAA);
        if (transpGroupStack->next != nullptr && transpGroupStack->next->shape != nullptr) {
            transpGroupStack->next->knockout = true;
        }
    }

    // pop the stack
    transpGroup = transpGroupStack;
    transpGroupStack = transpGroup->next;
    if (transpGroupStack != nullptr && transpGroup->knockoutOpacity < transpGroupStack->knockoutOpacity) {
        transpGroupStack->knockoutOpacity = transpGroup->knockoutOpacity;
    }
    delete transpGroup->shape;
    delete transpGroup;

    delete tBitmap;
}

void SplashOutputDev::setSoftMask(GfxState *state, const double *bbox, bool alpha, Function *transferFunc, GfxColor *backdropColor)
{
    SplashBitmap *softMask, *tBitmap;
    Splash *tSplash;
    SplashTransparencyGroup *transpGroup;
    SplashColor color;
    SplashColorPtr p;
    GfxGray gray;
    GfxRGB rgb;
    GfxCMYK cmyk;
    GfxColor deviceN;
    double lum, lum2;
    int tx, ty, x, y;

    tx = transpGroupStack->tx;
    ty = transpGroupStack->ty;
    tBitmap = transpGroupStack->tBitmap;

    // composite with backdrop color
    if (!alpha && tBitmap->getMode() != splashModeMono1) {
        //~ need to correctly handle the case where no blending color
        //~ space is given
        if (transpGroupStack->blendingColorSpace) {
            tSplash = new Splash(tBitmap, vectorAntialias, transpGroupStack->origSplash->getScreen());
            switch (tBitmap->getMode()) {
            case splashModeMono1:
                // transparency is not supported in mono1 mode
                break;
            case splashModeMono8:
                transpGroupStack->blendingColorSpace->getGray(backdropColor, &gray);
                color[0] = colToByte(gray);
                tSplash->compositeBackground(color);
                break;
            case splashModeXBGR8:
                color[3] = 255;
                // fallthrough
            case splashModeRGB8:
            case splashModeBGR8:
                transpGroupStack->blendingColorSpace->getRGB(backdropColor, &rgb);
                color[0] = colToByte(rgb.r);
                color[1] = colToByte(rgb.g);
                color[2] = colToByte(rgb.b);
                tSplash->compositeBackground(color);
                break;
            case splashModeCMYK8:
                transpGroupStack->blendingColorSpace->getCMYK(backdropColor, &cmyk);
                color[0] = colToByte(cmyk.c);
                color[1] = colToByte(cmyk.m);
                color[2] = colToByte(cmyk.y);
                color[3] = colToByte(cmyk.k);
                tSplash->compositeBackground(color);
                break;
            case splashModeDeviceN8:
                transpGroupStack->blendingColorSpace->getDeviceN(backdropColor, &deviceN);
                for (int cp = 0; cp < SPOT_NCOMPS + 4; cp++)
                    color[cp] = colToByte(deviceN.c[cp]);
                tSplash->compositeBackground(color);
                break;
            }
            delete tSplash;
        }
    }

    softMask = new SplashBitmap(bitmap->getWidth(), bitmap->getHeight(), 1, splashModeMono8, false);
    unsigned char fill = 0;
    if (transpGroupStack->blendingColorSpace) {
        transpGroupStack->blendingColorSpace->getGray(backdropColor, &gray);
        fill = colToByte(gray);
    }
    memset(softMask->getDataPtr(), fill, softMask->getRowSize() * softMask->getHeight());
    p = softMask->getDataPtr() + ty * softMask->getRowSize() + tx;
    int xMax = tBitmap->getWidth();
    int yMax = tBitmap->getHeight();
    if (xMax > bitmap->getWidth() - tx)
        xMax = bitmap->getWidth() - tx;
    if (yMax > bitmap->getHeight() - ty)
        yMax = bitmap->getHeight() - ty;
    for (y = 0; y < yMax; ++y) {
        for (x = 0; x < xMax; ++x) {
            if (alpha) {
                if (transferFunc) {
                    lum = tBitmap->getAlpha(x, y) / 255.0;
                    transferFunc->transform(&lum, &lum2);
                    p[x] = (int)(lum2 * 255.0 + 0.5);
                } else
                    p[x] = tBitmap->getAlpha(x, y);
            } else {
                tBitmap->getPixel(x, y, color);
                // convert to luminosity
                switch (tBitmap->getMode()) {
                case splashModeMono1:
                case splashModeMono8:
                    lum = color[0] / 255.0;
                    break;
                case splashModeXBGR8:
                case splashModeRGB8:
                case splashModeBGR8:
                    lum = (0.3 / 255.0) * color[0] + (0.59 / 255.0) * color[1] + (0.11 / 255.0) * color[2];
                    break;
                case splashModeCMYK8:
                case splashModeDeviceN8:
                    lum = (1 - color[3] / 255.0) - (0.3 / 255.0) * color[0] - (0.59 / 255.0) * color[1] - (0.11 / 255.0) * color[2];
                    if (lum < 0) {
                        lum = 0;
                    }
                    break;
                }
                if (transferFunc) {
                    transferFunc->transform(&lum, &lum2);
                } else {
                    lum2 = lum;
                }
                p[x] = (int)(lum2 * 255.0 + 0.5);
            }
        }
        p += softMask->getRowSize();
    }
    splash->setSoftMask(softMask);

    // pop the stack
    transpGroup = transpGroupStack;
    transpGroupStack = transpGroup->next;
    delete transpGroup;

    delete tBitmap;
}

void SplashOutputDev::clearSoftMask(GfxState *state)
{
    splash->setSoftMask(nullptr);
}

bool SplashOutputDev::tilingPatternFill(GfxState *state, Gfx *gfxA, Catalog *catalog, GfxTilingPattern *tPat, const double *mat, int x0, int y0, int x1, int y1, double xStep, double yStep)
{
    PDFRectangle box;
    Splash *formerSplash = splash;
    SplashBitmap *formerBitmap = bitmap;
    double width, height;
    int surface_width, surface_height, result_width, result_height, i;
    int repeatX, repeatY;
    SplashCoord matc[6];
    Matrix m1;
    const double *ctm;
    double savedCTM[6];
    double kx, ky, sx, sy;
    bool retValue = false;
    const double *bbox = tPat->getBBox();
    const double *ptm = tPat->getMatrix();
    const int paintType = tPat->getPaintType();
    Dict *resDict = tPat->getResDict();

    width = bbox[2] - bbox[0];
    height = bbox[3] - bbox[1];

    if (xStep != width || yStep != height)
        return false;

    // calculate offsets
    ctm = state->getCTM();
    for (i = 0; i < 6; ++i) {
        savedCTM[i] = ctm[i];
    }
    state->concatCTM(mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
    state->concatCTM(1, 0, 0, 1, bbox[0], bbox[1]);
    ctm = state->getCTM();
    for (i = 0; i < 6; ++i) {
        if (!std::isfinite(ctm[i])) {
            state->setCTM(savedCTM[0], savedCTM[1], savedCTM[2], savedCTM[3], savedCTM[4], savedCTM[5]);
            return false;
        }
    }
    matc[4] = x0 * xStep * ctm[0] + y0 * yStep * ctm[2] + ctm[4];
    matc[5] = x0 * xStep * ctm[1] + y0 * yStep * ctm[3] + ctm[5];
    if (splashAbs(ctm[1]) > splashAbs(ctm[0])) {
        kx = -ctm[1];
        ky = ctm[2] - (ctm[0] * ctm[3]) / ctm[1];
    } else {
        kx = ctm[0];
        ky = ctm[3] - (ctm[1] * ctm[2]) / ctm[0];
    }
    result_width = (int)ceil(fabs(kx * width * (x1 - x0)));
    result_height = (int)ceil(fabs(ky * height * (y1 - y0)));
    kx = state->getHDPI() / 72.0;
    ky = state->getVDPI() / 72.0;
    m1.m[0] = (ptm[0] == 0) ? fabs(ptm[2]) * kx : fabs(ptm[0]) * kx;
    m1.m[1] = 0;
    m1.m[2] = 0;
    m1.m[3] = (ptm[3] == 0) ? fabs(ptm[1]) * ky : fabs(ptm[3]) * ky;
    m1.m[4] = 0;
    m1.m[5] = 0;
    m1.transform(width, height, &kx, &ky);
    surface_width = (int)ceil(fabs(kx));
    surface_height = (int)ceil(fabs(ky));

    sx = (double)result_width / (surface_width * (x1 - x0));
    sy = (double)result_height / (surface_height * (y1 - y0));
    m1.m[0] *= sx;
    m1.m[3] *= sy;
    m1.transform(width, height, &kx, &ky);

    if (fabs(kx) < 1 && fabs(ky) < 1) {
        kx = std::min<double>(kx, ky);
        ky = 2 / kx;
        m1.m[0] *= ky;
        m1.m[3] *= ky;
        m1.transform(width, height, &kx, &ky);
        surface_width = (int)ceil(fabs(kx));
        surface_height = (int)ceil(fabs(ky));
        repeatX = x1 - x0;
        repeatY = y1 - y0;
    } else {
        if ((unsigned long)surface_width * surface_height > 0x800000L) {
            state->setCTM(savedCTM[0], savedCTM[1], savedCTM[2], savedCTM[3], savedCTM[4], savedCTM[5]);
            return false;
        }
        while (fabs(kx) > 16384 || fabs(ky) > 16384) {
            // limit pattern bitmap size
            m1.m[0] /= 2;
            m1.m[3] /= 2;
            m1.transform(width, height, &kx, &ky);
        }
        surface_width = (int)ceil(fabs(kx));
        surface_height = (int)ceil(fabs(ky));
        // adjust repeat values to completely fill region
        if (unlikely(surface_width == 0 || surface_height == 0)) {
            state->setCTM(savedCTM[0], savedCTM[1], savedCTM[2], savedCTM[3], savedCTM[4], savedCTM[5]);
            return false;
        }
        repeatX = result_width / surface_width;
        repeatY = result_height / surface_height;
        if (surface_width * repeatX < result_width)
            repeatX++;
        if (surface_height * repeatY < result_height)
            repeatY++;
        if (x1 - x0 > repeatX)
            repeatX = x1 - x0;
        if (y1 - y0 > repeatY)
            repeatY = y1 - y0;
    }
    // restore CTM and calculate rotate and scale with rounded matrix
    state->setCTM(savedCTM[0], savedCTM[1], savedCTM[2], savedCTM[3], savedCTM[4], savedCTM[5]);
    state->concatCTM(mat[0], mat[1], mat[2], mat[3], mat[4], mat[5]);
    state->concatCTM(width * repeatX, 0, 0, height * repeatY, bbox[0], bbox[1]);
    ctm = state->getCTM();
    matc[0] = ctm[0];
    matc[1] = ctm[1];
    matc[2] = ctm[2];
    matc[3] = ctm[3];

    if (surface_width == 0 || surface_height == 0 || repeatX * repeatY <= 4) {
        state->setCTM(savedCTM[0], savedCTM[1], savedCTM[2], savedCTM[3], savedCTM[4], savedCTM[5]);
        return false;
    }
    m1.transform(bbox[0], bbox[1], &kx, &ky);
    m1.m[4] = -kx;
    m1.m[5] = -ky;

    box.x1 = bbox[0];
    box.y1 = bbox[1];
    box.x2 = bbox[2];
    box.y2 = bbox[3];
    std::unique_ptr<Gfx> gfx = std::make_unique<Gfx>(doc, this, resDict, &box, nullptr, nullptr, nullptr, gfxA);
    // set pattern transformation matrix
    gfx->getState()->setCTM(m1.m[0], m1.m[1], m1.m[2], m1.m[3], m1.m[4], m1.m[5]);
    if (splashAbs(matc[1]) > splashAbs(matc[0])) {
        kx = -matc[1];
        ky = matc[2] - (matc[0] * matc[3]) / matc[1];
    } else {
        kx = matc[0];
        ky = matc[3] - (matc[1] * matc[2]) / matc[0];
    }
    result_width = surface_width * repeatX;
    result_height = surface_height * repeatY;
    kx = result_width / (fabs(kx) + 1);
    ky = result_height / (fabs(ky) + 1);
    state->concatCTM(kx, 0, 0, ky, 0, 0);
    ctm = state->getCTM();
    matc[0] = ctm[0];
    matc[1] = ctm[1];
    matc[2] = ctm[2];
    matc[3] = ctm[3];

    const bool doFastBlit = matc[0] > 0 && matc[1] == 0 && matc[2] == 0 && matc[3] > 0;
    bitmap = new SplashBitmap(surface_width, surface_height, 1, (paintType == 1 || doFastBlit) ? colorMode : splashModeMono8, true);
    if (bitmap->getDataPtr() == nullptr) {
        SplashBitmap *tBitmap = bitmap;
        bitmap = formerBitmap;
        delete tBitmap;
        state->setCTM(savedCTM[0], savedCTM[1], savedCTM[2], savedCTM[3], savedCTM[4], savedCTM[5]);
        return false;
    }
    splash = new Splash(bitmap, true);
    updateCTM(gfx->getState(), m1.m[0], m1.m[1], m1.m[2], m1.m[3], m1.m[4], m1.m[5]);

    if (paintType == 2) {
        SplashColor clearColor;
        clearColor[0] = (colorMode == splashModeCMYK8 || colorMode == splashModeDeviceN8) ? 0x00 : 0xFF;
        splash->clear(clearColor, 0);
    } else {
        splash->clear(paperColor, 0);
    }
    splash->setThinLineMode(formerSplash->getThinLineMode());
    splash->setMinLineWidth(s_minLineWidth);
    if (doFastBlit) {
        // drawImage would colorize the greyscale pattern in tilingBitmapSrc buffer accessor while tiling.
        // blitImage can't, it has no buffer accessor. We instead colorize the pattern prototype in advance.
        splash->setFillPattern(formerSplash->getFillPattern()->copy());
        splash->setStrokePattern(formerSplash->getStrokePattern()->copy());
    }
    gfx->display(tPat->getContentStream());
    delete splash;
    splash = formerSplash;

    TilingSplashOutBitmap imgData;
    imgData.bitmap = bitmap;
    imgData.paintType = paintType;
    imgData.pattern = splash->getFillPattern();
    imgData.colorMode = colorMode;
    imgData.y = 0;
    imgData.repeatX = repeatX;
    imgData.repeatY = repeatY;
    SplashBitmap *tBitmap = bitmap;
    bitmap = formerBitmap;
    if (doFastBlit) {
        // draw the tiles
        for (int y = 0; y < imgData.repeatY; ++y) {
            for (int x = 0; x < imgData.repeatX; ++x) {
                x0 = splashFloor(matc[4]) + x * tBitmap->getWidth();
                y0 = splashFloor(matc[5]) + y * tBitmap->getHeight();
                splash->blitImage(tBitmap, true, x0, y0);
            }
        }
        retValue = true;
    } else {
        retValue = splash->drawImage(&tilingBitmapSrc, nullptr, &imgData, colorMode, true, result_width, result_height, matc, false, true) == splashOk;
    }
    delete tBitmap;
    return retValue;
}

bool SplashOutputDev::gouraudTriangleShadedFill(GfxState *state, GfxGouraudTriangleShading *shading)
{
    GfxColorSpaceMode shadingMode = shading->getColorSpace()->getMode();
    bool bDirectColorTranslation = false; // triggers an optimization.
    switch (colorMode) {
    case splashModeRGB8:
        bDirectColorTranslation = (shadingMode == csDeviceRGB);
        break;
    case splashModeCMYK8:
    case splashModeDeviceN8:
        bDirectColorTranslation = (shadingMode == csDeviceCMYK);
        break;
    default:
        break;
    }
    // restore vector antialias because we support it here
    SplashGouraudPattern splashShading(bDirectColorTranslation, state, shading);
    const bool vaa = getVectorAntialias();
    setVectorAntialias(true);
    const bool retVal = splash->gouraudTriangleShadedFill(&splashShading);
    setVectorAntialias(vaa);
    return retVal;
}

bool SplashOutputDev::univariateShadedFill(GfxState *state, SplashUnivariatePattern *pattern, double tMin, double tMax)
{
    double xMin, yMin, xMax, yMax;
    bool vaa = getVectorAntialias();
    // restore vector antialias because we support it here
    setVectorAntialias(true);

    bool retVal = false;
    // get the clip region bbox
    if (pattern->getShading()->getHasBBox()) {
        pattern->getShading()->getBBox(&xMin, &yMin, &xMax, &yMax);
    } else {
        state->getClipBBox(&xMin, &yMin, &xMax, &yMax);

        xMin = floor(xMin);
        yMin = floor(yMin);
        xMax = ceil(xMax);
        yMax = ceil(yMax);

        {
            Matrix ctm, ictm;
            double x[4], y[4];
            int i;

            state->getCTM(&ctm);
            ctm.invertTo(&ictm);

            ictm.transform(xMin, yMin, &x[0], &y[0]);
            ictm.transform(xMax, yMin, &x[1], &y[1]);
            ictm.transform(xMin, yMax, &x[2], &y[2]);
            ictm.transform(xMax, yMax, &x[3], &y[3]);

            xMin = xMax = x[0];
            yMin = yMax = y[0];
            for (i = 1; i < 4; i++) {
                xMin = std::min<double>(xMin, x[i]);
                yMin = std::min<double>(yMin, y[i]);
                xMax = std::max<double>(xMax, x[i]);
                yMax = std::max<double>(yMax, y[i]);
            }
        }
    }

    // fill the region
    state->moveTo(xMin, yMin);
    state->lineTo(xMax, yMin);
    state->lineTo(xMax, yMax);
    state->lineTo(xMin, yMax);
    state->closePath();
    SplashPath path = convertPath(state, state->getPath(), true);

    pattern->getShading()->getColorSpace()->createMapping(bitmap->getSeparationList(), SPOT_NCOMPS);
    setOverprintMask(pattern->getShading()->getColorSpace(), state->getFillOverprint(), state->getOverprintMode(), nullptr);
    // If state->getStrokePattern() is set, then the current clipping region
    // is a stroke path.
    retVal = (splash->shadedFill(&path, pattern->getShading()->getHasBBox(), pattern, (state->getStrokePattern() != nullptr)) == splashOk);
    state->clearPath();
    setVectorAntialias(vaa);

    return retVal;
}

bool SplashOutputDev::functionShadedFill(GfxState *state, GfxFunctionShading *shading)
{
    SplashFunctionPattern *pattern = new SplashFunctionPattern(colorMode, state, shading);
    double xMin, yMin, xMax, yMax;
    bool vaa = getVectorAntialias();
    // restore vector antialias because we support it here
    setVectorAntialias(true);

    bool retVal = false;
    // get the clip region bbox
    if (pattern->getShading()->getHasBBox()) {
        pattern->getShading()->getBBox(&xMin, &yMin, &xMax, &yMax);
    } else {
        state->getClipBBox(&xMin, &yMin, &xMax, &yMax);

        xMin = floor(xMin);
        yMin = floor(yMin);
        xMax = ceil(xMax);
        yMax = ceil(yMax);

        {
            Matrix ctm, ictm;
            double x[4], y[4];
            int i;

            state->getCTM(&ctm);
            ctm.invertTo(&ictm);

            ictm.transform(xMin, yMin, &x[0], &y[0]);
            ictm.transform(xMax, yMin, &x[1], &y[1]);
            ictm.transform(xMin, yMax, &x[2], &y[2]);
            ictm.transform(xMax, yMax, &x[3], &y[3]);

            xMin = xMax = x[0];
            yMin = yMax = y[0];
            for (i = 1; i < 4; i++) {
                xMin = std::min<double>(xMin, x[i]);
                yMin = std::min<double>(yMin, y[i]);
                xMax = std::max<double>(xMax, x[i]);
                yMax = std::max<double>(yMax, y[i]);
            }
        }
    }

    // fill the region
    state->moveTo(xMin, yMin);
    state->lineTo(xMax, yMin);
    state->lineTo(xMax, yMax);
    state->lineTo(xMin, yMax);
    state->closePath();
    SplashPath path = convertPath(state, state->getPath(), true);

    pattern->getShading()->getColorSpace()->createMapping(bitmap->getSeparationList(), SPOT_NCOMPS);
    setOverprintMask(pattern->getShading()->getColorSpace(), state->getFillOverprint(), state->getOverprintMode(), nullptr);
    // If state->getStrokePattern() is set, then the current clipping region
    // is a stroke path.
    retVal = (splash->shadedFill(&path, pattern->getShading()->getHasBBox(), pattern, (state->getStrokePattern() != nullptr)) == splashOk);
    state->clearPath();
    setVectorAntialias(vaa);

    delete pattern;

    return retVal;
}

bool SplashOutputDev::axialShadedFill(GfxState *state, GfxAxialShading *shading, double tMin, double tMax)
{
    SplashAxialPattern *pattern = new SplashAxialPattern(colorMode, state, shading);
    bool retVal = univariateShadedFill(state, pattern, tMin, tMax);

    delete pattern;

    return retVal;
}

bool SplashOutputDev::radialShadedFill(GfxState *state, GfxRadialShading *shading, double tMin, double tMax)
{
    SplashRadialPattern *pattern = new SplashRadialPattern(colorMode, state, shading);
    bool retVal = univariateShadedFill(state, pattern, tMin, tMax);

    delete pattern;

    return retVal;
}
