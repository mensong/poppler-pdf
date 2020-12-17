#include <cmath>
#include "SplashDynamicPatterns.h"

static inline void convertGfxColor(SplashColorPtr dest, const SplashColorMode colorMode, const GfxColorSpace *colorSpace, const GfxColor *src)
{
    SplashColor color;
    GfxGray gray;
    GfxRGB rgb;
    GfxCMYK cmyk;
    GfxColor deviceN;

    // make gcc happy
    color[0] = color[1] = color[2] = 0;
    color[3] = 0;
    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8:
        colorSpace->getGray(src, &gray);
        color[0] = colToByte(gray);
        break;
    case splashModeXBGR8:
        color[3] = 255;
        // fallthrough
    case splashModeBGR8:
    case splashModeRGB8:
        colorSpace->getRGB(src, &rgb);
        color[0] = colToByte(rgb.r);
        color[1] = colToByte(rgb.g);
        color[2] = colToByte(rgb.b);
        break;
    case splashModeCMYK8:
        colorSpace->getCMYK(src, &cmyk);
        color[0] = colToByte(cmyk.c);
        color[1] = colToByte(cmyk.m);
        color[2] = colToByte(cmyk.y);
        color[3] = colToByte(cmyk.k);
        break;
    case splashModeDeviceN8:
        colorSpace->getDeviceN(src, &deviceN);
        for (int i = 0; i < SPOT_NCOMPS + 4; i++)
            color[i] = colToByte(deviceN.c[i]);
        break;
    }
    splashColorCopy(dest, color);
}

// Copy a color according to the color mode.
// Use convertGfxShortColor() below when the destination is a bitmap
// to avoid overwriting cells.
// Calling this in SplashGouraudPattern::getParameterizedColor() fixes bug 90570.
// Use convertGfxColor() above when the destination is an array of SPOT_NCOMPS+4 bytes,
// to ensure that everything is initialized.

static inline void convertGfxShortColor(SplashColorPtr dest, const SplashColorMode colorMode, const GfxColorSpace *colorSpace, const GfxColor *src)
{
    switch (colorMode) {
    case splashModeMono1:
    case splashModeMono8: {
        GfxGray gray;
        colorSpace->getGray(src, &gray);
        dest[0] = colToByte(gray);
    } break;
    case splashModeXBGR8:
        dest[3] = 255;
        // fallthrough
    case splashModeBGR8:
    case splashModeRGB8: {
        GfxRGB rgb;
        colorSpace->getRGB(src, &rgb);
        dest[0] = colToByte(rgb.r);
        dest[1] = colToByte(rgb.g);
        dest[2] = colToByte(rgb.b);
    } break;
    case splashModeCMYK8: {
        GfxCMYK cmyk;
        colorSpace->getCMYK(src, &cmyk);
        dest[0] = colToByte(cmyk.c);
        dest[1] = colToByte(cmyk.m);
        dest[2] = colToByte(cmyk.y);
        dest[3] = colToByte(cmyk.k);
    } break;
    case splashModeDeviceN8: {
        GfxColor deviceN;
        colorSpace->getDeviceN(src, &deviceN);
        for (int i = 0; i < SPOT_NCOMPS + 4; i++)
            dest[i] = colToByte(deviceN.c[i]);
    } break;
    }
}

//------------------------------------------------------------------------
// SplashFunctionPattern
//------------------------------------------------------------------------

SplashFunctionPattern::SplashFunctionPattern(SplashColorMode colorModeA, GfxState *stateA, GfxFunctionShading *shadingA)
{
    Matrix ctm;
    SplashColor defaultColor;
    GfxColor srcColor;
    const double *matrix = shadingA->getMatrix();

    shading = shadingA;
    state = stateA;
    colorMode = colorModeA;

    state->getCTM(&ctm);

    double a1 = ctm.m[0];
    double b1 = ctm.m[1];
    double c1 = ctm.m[2];
    double d1 = ctm.m[3];

    ctm.m[0] = matrix[0] * a1 + matrix[1] * c1;
    ctm.m[1] = matrix[0] * b1 + matrix[1] * d1;
    ctm.m[2] = matrix[2] * a1 + matrix[3] * c1;
    ctm.m[3] = matrix[2] * b1 + matrix[3] * d1;
    ctm.m[4] = matrix[4] * a1 + matrix[5] * c1 + ctm.m[4];
    ctm.m[5] = matrix[4] * b1 + matrix[5] * d1 + ctm.m[5];
    ctm.invertTo(&ictm);

    gfxMode = shadingA->getColorSpace()->getMode();
    shadingA->getColorSpace()->getDefaultColor(&srcColor);
    shadingA->getDomain(&xMin, &yMin, &xMax, &yMax);
    convertGfxColor(defaultColor, colorModeA, shadingA->getColorSpace(), &srcColor);
}

SplashFunctionPattern::~SplashFunctionPattern() { }

bool SplashFunctionPattern::getColor(int x, int y, SplashColorPtr c)
{
    GfxColor gfxColor;
    double xc, yc;

    ictm.transform(x, y, &xc, &yc);
    if (xc < xMin || xc > xMax || yc < yMin || yc > yMax)
        return false;
    shading->getColor(xc, yc, &gfxColor);
    convertGfxColor(c, colorMode, shading->getColorSpace(), &gfxColor);
    return true;
}

//------------------------------------------------------------------------
// SplashUnivariatePattern
//------------------------------------------------------------------------

SplashUnivariatePattern::SplashUnivariatePattern(SplashColorMode colorModeA, GfxState *stateA, GfxUnivariateShading *shadingA)
{
    Matrix ctm;
    double xMin, yMin, xMax, yMax;

    shading = shadingA;
    state = stateA;
    colorMode = colorModeA;

    state->getCTM(&ctm);
    ctm.invertTo(&ictm);

    // get the function domain
    t0 = shading->getDomain0();
    t1 = shading->getDomain1();
    dt = t1 - t0;

    stateA->getUserClipBBox(&xMin, &yMin, &xMax, &yMax);
    shadingA->setupCache(&ctm, xMin, yMin, xMax, yMax);
    gfxMode = shadingA->getColorSpace()->getMode();
}

SplashUnivariatePattern::~SplashUnivariatePattern() { }

bool SplashUnivariatePattern::getColor(int x, int y, SplashColorPtr c)
{
    GfxColor gfxColor;
    double xc, yc, t;

    ictm.transform(x, y, &xc, &yc);
    if (!getParameter(xc, yc, &t))
        return false;

    const int filled = shading->getColor(t, &gfxColor);
    if (unlikely(filled < shading->getColorSpace()->getNComps())) {
        for (int i = filled; i < shading->getColorSpace()->getNComps(); ++i)
            gfxColor.c[i] = 0;
    }
    convertGfxColor(c, colorMode, shading->getColorSpace(), &gfxColor);
    return true;
}

bool SplashUnivariatePattern::testPosition(int x, int y)
{
    double xc, yc, t;

    ictm.transform(x, y, &xc, &yc);
    if (!getParameter(xc, yc, &t))
        return false;
    return (t0 < t1) ? (t > t0 && t < t1) : (t > t1 && t < t0);
}

//------------------------------------------------------------------------
// SplashRadialPattern
//------------------------------------------------------------------------
#define RADIAL_EPSILON (1. / 1024 / 1024)

SplashRadialPattern::SplashRadialPattern(SplashColorMode colorModeA, GfxState *stateA, GfxRadialShading *shadingA) : SplashUnivariatePattern(colorModeA, stateA, shadingA)
{
    SplashColor defaultColor;
    GfxColor srcColor;

    shadingA->getCoords(&x0, &y0, &r0, &dx, &dy, &dr);
    dx -= x0;
    dy -= y0;
    dr -= r0;
    a = dx * dx + dy * dy - dr * dr;
    if (fabs(a) > RADIAL_EPSILON)
        inva = 1.0 / a;
    shadingA->getColorSpace()->getDefaultColor(&srcColor);
    convertGfxColor(defaultColor, colorModeA, shadingA->getColorSpace(), &srcColor);
}

SplashRadialPattern::~SplashRadialPattern() { }

bool SplashRadialPattern::getParameter(double xs, double ys, double *t)
{
    double b, c, s0, s1;

    // We want to solve this system of equations:
    //
    // 1. (x - xc(s))^2 + (y -yc(s))^2 = rc(s)^2
    // 2. xc(s) = x0 + s * (x1 - xo)
    // 3. yc(s) = y0 + s * (y1 - yo)
    // 4. rc(s) = r0 + s * (r1 - ro)
    //
    // To simplify the system a little, we translate
    // our coordinates to have the origin in (x0,y0)

    xs -= x0;
    ys -= y0;

    // Then we have to solve the equation:
    //   A*s^2 - 2*B*s + C = 0
    // where
    //   A = dx^2  + dy^2  - dr^2
    //   B = xs*dx + ys*dy + r0*dr
    //   C = xs^2  + ys^2  - r0^2

    b = xs * dx + ys * dy + r0 * dr;
    c = xs * xs + ys * ys - r0 * r0;

    if (fabs(a) <= RADIAL_EPSILON) {
        // A is 0, thus the equation simplifies to:
        //   -2*B*s + C = 0
        // If B is 0, we can either have no solution or an indeterminate
        // equation, thus we behave as if we had an invalid solution
        if (fabs(b) <= RADIAL_EPSILON)
            return false;

        s0 = s1 = 0.5 * c / b;
    } else {
        double d;

        d = b * b - a * c;
        if (d < 0)
            return false;

        d = sqrt(d);
        s0 = b + d;
        s1 = b - d;

        // If A < 0, one of the two solutions will have negative radius,
        // thus it will be ignored. Otherwise we know that s1 <= s0
        // (because d >=0 implies b - d <= b + d), so if both are valid it
        // will be the true solution.
        s0 *= inva;
        s1 *= inva;
    }

    if (r0 + s0 * dr >= 0) {
        if (0 <= s0 && s0 <= 1) {
            *t = t0 + dt * s0;
            return true;
        } else if (s0 < 0 && shading->getExtend0()) {
            *t = t0;
            return true;
        } else if (s0 > 1 && shading->getExtend1()) {
            *t = t1;
            return true;
        }
    }

    if (r0 + s1 * dr >= 0) {
        if (0 <= s1 && s1 <= 1) {
            *t = t0 + dt * s1;
            return true;
        } else if (s1 < 0 && shading->getExtend0()) {
            *t = t0;
            return true;
        } else if (s1 > 1 && shading->getExtend1()) {
            *t = t1;
            return true;
        }
    }

    return false;
}

#undef RADIAL_EPSILON

//------------------------------------------------------------------------
// SplashAxialPattern
//------------------------------------------------------------------------

SplashAxialPattern::SplashAxialPattern(SplashColorMode colorModeA, GfxState *stateA, GfxAxialShading *shadingA) : SplashUnivariatePattern(colorModeA, stateA, shadingA)
{
    SplashColor defaultColor;
    GfxColor srcColor;

    shadingA->getCoords(&x0, &y0, &x1, &y1);
    dx = x1 - x0;
    dy = y1 - y0;
    const double mul_denominator = (dx * dx + dy * dy);
    if (unlikely(mul_denominator == 0)) {
        mul = 0;
    } else {
        mul = 1 / mul_denominator;
    }
    shadingA->getColorSpace()->getDefaultColor(&srcColor);
    convertGfxColor(defaultColor, colorModeA, shadingA->getColorSpace(), &srcColor);
}

SplashAxialPattern::~SplashAxialPattern() { }

bool SplashAxialPattern::getParameter(double xc, double yc, double *t)
{
    double s;

    xc -= x0;
    yc -= y0;

    s = (xc * dx + yc * dy) * mul;
    if (0 <= s && s <= 1) {
        *t = t0 + dt * s;
    } else if (s < 0 && shading->getExtend0()) {
        *t = t0;
    } else if (s > 1 && shading->getExtend1()) {
        *t = t1;
    } else {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------
// SplashGouraudPattern
//------------------------------------------------------------------------
SplashGouraudPattern::SplashGouraudPattern(bool bDirectColorTranslationA, GfxState *stateA, GfxGouraudTriangleShading *shadingA)
{
    state = stateA;
    shading = shadingA;
    bDirectColorTranslation = bDirectColorTranslationA;
    gfxMode = shadingA->getColorSpace()->getMode();
}

SplashGouraudPattern::~SplashGouraudPattern() { }

void SplashGouraudPattern::getNonParametrizedTriangle(int i, SplashColorMode mode, double *x0, double *y0, SplashColorPtr color0, double *x1, double *y1, SplashColorPtr color1, double *x2, double *y2, SplashColorPtr color2)
{
    GfxColor c0, c1, c2;
    shading->getTriangle(i, x0, y0, &c0, x1, y1, &c1, x2, y2, &c2);

    const GfxColorSpace *srcColorSpace = shading->getColorSpace();
    convertGfxColor(color0, mode, srcColorSpace, &c0);
    convertGfxColor(color1, mode, srcColorSpace, &c1);
    convertGfxColor(color2, mode, srcColorSpace, &c2);
}

void SplashGouraudPattern::getParameterizedColor(double colorinterp, SplashColorMode mode, SplashColorPtr dest)
{
    GfxColor src;
    shading->getParameterizedColor(colorinterp, &src);

    if (bDirectColorTranslation) {
        const int colorComps = splashColorModeNComps[mode];
        for (int m = 0; m < colorComps; ++m)
            dest[m] = colToByte(src.c[m]);
    } else {
        GfxColorSpace *srcColorSpace = shading->getColorSpace();
        convertGfxShortColor(dest, mode, srcColorSpace, &src);
    }
}
