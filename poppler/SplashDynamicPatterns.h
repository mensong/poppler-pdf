#ifndef SPLASHDYNAMICPATTERNS_H
#define SPLASHDYNAMICPATTERNS_H

#include "splash/SplashTypes.h"
#include "splash/SplashPattern.h"
#include "GfxState.h"

//------------------------------------------------------------------------
// Splash dynamic pattern
//------------------------------------------------------------------------

class SplashFunctionPattern : public SplashPattern
{
public:
    SplashFunctionPattern(SplashColorMode colorMode, GfxState *state, GfxFunctionShading *shading);

    SplashPattern *copy() override { return new SplashFunctionPattern(colorMode, state, (GfxFunctionShading *)shading); }

    ~SplashFunctionPattern() override;

    bool testPosition(int x, int y) override { return true; }

    bool isStatic() override { return false; }

    bool getColor(int x, int y, SplashColorPtr c) override;

    virtual GfxFunctionShading *getShading() { return shading; }

    bool isCMYK() override { return gfxMode == csDeviceCMYK; }

protected:
    Matrix ictm;
    double xMin, yMin, xMax, yMax;
    GfxFunctionShading *shading;
    GfxState *state;
    SplashColorMode colorMode;
    GfxColorSpaceMode gfxMode;
};

class SplashUnivariatePattern : public SplashPattern
{
public:
    SplashUnivariatePattern(SplashColorMode colorMode, GfxState *state, GfxUnivariateShading *shading);

    ~SplashUnivariatePattern() override;

    bool getColor(int x, int y, SplashColorPtr c) override;

    bool testPosition(int x, int y) override;

    bool isStatic() override { return false; }

    virtual bool getParameter(double xs, double ys, double *t) = 0;

    virtual GfxUnivariateShading *getShading() { return shading; }

    bool isCMYK() override { return gfxMode == csDeviceCMYK; }

protected:
    Matrix ictm;
    double t0, t1, dt;
    GfxUnivariateShading *shading;
    GfxState *state;
    SplashColorMode colorMode;
    GfxColorSpaceMode gfxMode;
};

class SplashAxialPattern : public SplashUnivariatePattern
{
public:
    SplashAxialPattern(SplashColorMode colorMode, GfxState *state, GfxAxialShading *shading);

    SplashPattern *copy() override { return new SplashAxialPattern(colorMode, state, (GfxAxialShading *)shading); }

    ~SplashAxialPattern() override;

    bool getParameter(double xc, double yc, double *t) override;

private:
    double x0, y0, x1, y1;
    double dx, dy, mul;
};

// see GfxState.h, GfxGouraudTriangleShading
class SplashGouraudPattern : public SplashGouraudColor
{
public:
    SplashGouraudPattern(bool bDirectColorTranslation, GfxState *state, GfxGouraudTriangleShading *shading);

    SplashPattern *copy() override { return new SplashGouraudPattern(bDirectColorTranslation, state, shading); }

    ~SplashGouraudPattern() override;

    bool getColor(int x, int y, SplashColorPtr c) override { return false; }

    bool testPosition(int x, int y) override { return false; }

    bool isStatic() override { return false; }

    bool isCMYK() override { return gfxMode == csDeviceCMYK; }

    bool isParameterized() override { return shading->isParameterized(); }
    int getNTriangles() override { return shading->getNTriangles(); }
    void getParametrizedTriangle(int i, double *x0, double *y0, double *color0, double *x1, double *y1, double *color1, double *x2, double *y2, double *color2) override
    {
        shading->getTriangle(i, x0, y0, color0, x1, y1, color1, x2, y2, color2);
    }

    void getNonParametrizedTriangle(int i, SplashColorMode mode, double *x0, double *y0, SplashColorPtr color0, double *x1, double *y1, SplashColorPtr color1, double *x2, double *y2, SplashColorPtr color2) override;

    void getParameterizedColor(double colorinterp, SplashColorMode mode, SplashColorPtr dest) override;

private:
    GfxGouraudTriangleShading *shading;
    GfxState *state;
    bool bDirectColorTranslation;
    GfxColorSpaceMode gfxMode;
};

// see GfxState.h, GfxRadialShading
class SplashRadialPattern : public SplashUnivariatePattern
{
public:
    SplashRadialPattern(SplashColorMode colorMode, GfxState *state, GfxRadialShading *shading);

    SplashPattern *copy() override { return new SplashRadialPattern(colorMode, state, (GfxRadialShading *)shading); }

    ~SplashRadialPattern() override;

    bool getParameter(double xs, double ys, double *t) override;

private:
    double x0, y0, r0, dx, dy, dr;
    double a, inva;
};

#endif // SPLASHDYNAMICPATTERNS_H
