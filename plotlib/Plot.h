//========================================================================
//
// Plot.h
//
// Primitives for manipulating pen-plotter vector geometry.  A PlotPath is
// simpler than a SplashPath, in that it only contains straight line segments.
// A PlotPath also includes characteristic metadata, such as pen color, that
// can be used to coalesce or reorder segments into more efficient plot paths.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#ifndef PLOT_H
#define PLOT_H

#include <iostream>

#include "splash/SplashTypes.h" // SplashCoord (double!)
#include "splash/SplashPath.h"
#include "poppler/GfxState.h" // GfxRGB -- that's a lot of machinery for a color :v)

using namespace std;

struct PlotPt
{
    SplashCoord x, y;

    PlotPt minus(PlotPt pt) const;
    PlotPt plus(PlotPt pt) const;
    double dot(PlotPt pt) const;
    PlotPt scale(double f) const;
    double len() const;
    PlotPt unit() const;
    PlotPt left() const;

    bool closeEnough(PlotPt pt) const;

private:
    static constexpr double CLOSE_THRESH = 1e-6;
};
std::ostream &operator<<(std::ostream &os, PlotPt m);

struct PlotBBox
{
    SplashCoord left, right, bottom, top;
    SplashCoord width() { return right - left; }
    SplashCoord height() { return top - bottom; }

    static PlotBBox fromSplashPath(SplashPath *path);
};
std::ostream &operator<<(std::ostream &os, PlotBBox const &m);

class BBoxBuilder
{
public:
    bool valid = false;
    PlotBBox bbox = { 0, 0, 0, 0 };
    void add(PlotPt pt);
};

// A single line segment. Used in the clipping code.
struct PlotSeg
{
    PlotPt a, b;
};
std::ostream &operator<<(std::ostream &os, PlotSeg const &m);

// A characteristic feature of a stroke, which can be used to group
// strokes together (paint all the same strokes with the same pen)
// or keep them separate (draw strokes in original PDF render order).
// Someday it may include pen thickness.
class Characteristic
{
public:
    GfxRGB color;
    int order;
    Characteristic() : color(GfxRGB { 0, 0, 0 }), order(0) { }

    Characteristic(GfxRGB color_, int order_) : color(color_), order(order_) { }
    bool operator==(Characteristic) const;
};

// A PlotPath is a single pen-down sequence (hence it all shares one
// pen Characteristic).
class PlotPath
{
public:
    PlotPath() : characteristic(), debug(false) { }
    PlotPath(GfxRGB color_) : characteristic(color_, 0), debug(false) { }
    void add(PlotPt seg) { pts.push_back(seg); }
    PlotPt first() const { return pts.front(); }
    PlotPt last() const { return pts.back(); }

    // Returns a new PlotPath with zero-length segments removed.
    PlotPath clean() const;

    Characteristic characteristic;
    bool debug;
    std::vector<PlotPt> pts;
};

// Value-type metadata for a Plot, so it's easy to copy plots.
struct Metadata
{
    PlotBBox page;
};

// A one-page plot: page metadata plus a sequence of PlotPaths.
class Plot
{
public:
    void add(const PlotPath &path) { paths.push_back(path); }
    PlotBBox pageAsBBox() const { return metadata.page; }

    // return the bbox of the actual plotted points.
    PlotBBox plotBBox() const;

    // Coalesce adjacent paths in original order to eliminate pen-up/pen-downs.
    // Returns a new plot.
    Plot coalescePaths() const;

    std::vector<PlotPath> paths;
    Metadata metadata;
};

struct PageColor
{
    SplashColor color;

    PageColor();
};

// Poppler has GfxState Matrix and SplashMatrix.
// GfxState Matrix is missing matrix multiplication.
// SplashMatrix is missing almost everything (see in-line matrix multiply
// in ::SplashFunctionPattern ctor).
// So I added a *third* PlotMatrix (https://xkcd.com/927/) that has everything.
// Code reviewer: another possibility is to beef up GfxMatrix, but to do that
// right would suggest making it a real class, not just a double*. I'm open to
// it, but it may be quite a bit more invasive to the rest of Poppler.
class PlotMatrix
{
public:
    PlotMatrix(SplashCoord _xx, SplashCoord _xy, SplashCoord _yx, SplashCoord _yy, SplashCoord _x0, SplashCoord _y0);
    PlotMatrix(const Matrix &gfxMatrix); // from GfxState Matrix
    PlotMatrix(const double *coord); // from GfxState getTextMat
    PlotMatrix(SplashCoord *coord); // from Splash matrix
    PlotMatrix(); // Identity

    // Updates the matrix to appy a translation
    void translate(SplashCoord tx, SplashCoord ty);

    // Updates the matrix to appy a scale
    void scale(SplashCoord sx, SplashCoord sy);

    double determinant() const { return coef[0] * coef[3] - coef[1] * coef[2]; }

    // Returns the inverse of this.
    // If this is singular and success is not nullptr, success is set to
    // false and the return value is meaningless.
    PlotMatrix inverse(bool *out_success = nullptr) const;

    // Returns a new matrix that is the matrix product this * q
    PlotMatrix multiply(const PlotMatrix &q) const;

    PlotPt transform(PlotPt pt) const;
    PlotPath transform(const PlotPath &path) const;
    Plot transform(const Plot &plot) const;
    PlotBBox transform(const PlotBBox &bbox) const;

    SplashCoord *asSplashMatrix() const;

private:
    SplashCoord coef[6];
};

#endif // PLOT_H
