//========================================================================
//
// Plot.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#include <cmath>
#include "Plot.h"

//----------------------------------------------------------------------
// PlotPt
//----------------------------------------------------------------------

PlotPt PlotPt::minus(PlotPt pt) const
{
    return PlotPt { x - pt.x, y - pt.y };
}

PlotPt PlotPt::plus(PlotPt pt) const
{
    return PlotPt { x + pt.x, y + pt.y };
}

double PlotPt::dot(PlotPt pt) const
{
    return x * pt.x + y * pt.y;
}

PlotPt PlotPt::scale(double f) const
{
    return PlotPt { f * x, f * y };
}

double PlotPt::len() const
{
    return sqrt(x * x + y * y);
}

PlotPt PlotPt::unit() const
{
    return scale(1.0 / len());
}

PlotPt PlotPt::left() const
{
    return PlotPt { -y, x };
}

bool PlotPt::closeEnough(PlotPt pt) const
{
    return abs(x - pt.x) < CLOSE_THRESH && abs(y - pt.y) < CLOSE_THRESH;
}

std::ostream &operator<<(std::ostream &os, PlotPt m)
{
    return os << "(" << m.x << "," << m.y << ")";
}

//----------------------------------------------------------------------
// PlotBBox
//----------------------------------------------------------------------

PlotBBox PlotBBox::fromSplashPath(SplashPath *path)
{
    BBoxBuilder bb;
    for (int i = 0; i < path->getLength(); i++) {
        double x, y;
        unsigned char _flags;
        path->getPoint(i, &x, &y, &_flags);
        bb.add(PlotPt { x, y });
    }
    return bb.bbox;
}

std::ostream &operator<<(std::ostream &os, PlotBBox const &m)
{
    return os << "x(" << m.left << "," << m.right << ")"
              << " y(" << m.bottom << "," << m.top << ")";
}

void BBoxBuilder::add(PlotPt pt)
{
    if (!valid) {
        bbox.left = pt.x;
        bbox.right = pt.x;
        bbox.bottom = pt.y;
        bbox.top = pt.y;
        valid = true;
    } else {
        bbox.left = min(bbox.left, pt.x);
        bbox.right = max(bbox.right, pt.x);
        bbox.bottom = min(bbox.bottom, pt.y);
        bbox.top = max(bbox.top, pt.y);
    }
}

//----------------------------------------------------------------------
// PlotSeg
//----------------------------------------------------------------------

bool PlotSeg::closeEnough(const PlotSeg &other) const
{
    return (a.closeEnough(other.a) && b.closeEnough(other.b)) || (b.closeEnough(other.a) && a.closeEnough(other.b));
}

std::ostream &operator<<(std::ostream &os, PlotSeg const &m)
{
    return os << "[" << m.a << "--" << m.b << "]";
}

//----------------------------------------------------------------------
// GfxRGB==
//----------------------------------------------------------------------

bool operator==(GfxRGB a, GfxRGB b);
bool operator==(GfxRGB a, GfxRGB b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

//----------------------------------------------------------------------
// Characteristic
//----------------------------------------------------------------------

bool Characteristic::operator==(Characteristic other) const
{
    return color == other.color && order == other.order;
}

//----------------------------------------------------------------------
// PlotPath
//----------------------------------------------------------------------

PlotPath PlotPath::clean() const
{
    PlotPath outPath = PlotPath();
    outPath.characteristic = characteristic;
    outPath.add(pts[0]);
    for (size_t i = 1; i < pts.size(); i++) {
        PlotPt pt = pts[i];
        if (!outPath.last().closeEnough(pt)) {
            outPath.add(pt);
        }
    }
    return outPath;
}

//----------------------------------------------------------------------
// Plot
//----------------------------------------------------------------------

PlotBBox Plot::plotBBox() const
{
    BBoxBuilder bb;
    for (auto const &path : paths) {
        for (auto pt : path.pts) {
            bb.add(pt);
        }
    }
    return bb.bbox;
}

Plot Plot::coalescePaths() const
{
    Plot outPlot;
    outPlot.metadata = metadata;
    bool curPathValid = false;
    PlotPath curPath = PlotPath();
    for (auto const &path : paths) {
        if (curPathValid && path.characteristic == curPath.characteristic && curPath.last().closeEnough(path.first())) {
            // Append path to curPath and keep going
            curPath.pts.insert(curPath.pts.end(), path.pts.begin(), path.pts.end());
        } else {
            // Close out curPath
            if (curPathValid) {
                outPlot.add(curPath.clean());
            }
            curPath = path; // Copy path and its characteristic
            curPathValid = true;
        }
    }
    // Emit any leftover path.
    if (curPathValid) {
        outPlot.add(curPath.clean());
    }
    return outPlot;
}

//----------------------------------------------------------------------
// PlotMatrix
//----------------------------------------------------------------------

PlotMatrix::PlotMatrix(SplashCoord _xx, SplashCoord _xy, SplashCoord _yx, SplashCoord _yy, SplashCoord _x0, SplashCoord _y0) : coef { _xx, _xy, _yx, _yy, _x0, _y0 } { }

PlotMatrix::PlotMatrix(const Matrix &gfxMatrix) : coef { gfxMatrix.m[0], gfxMatrix.m[1], gfxMatrix.m[2], gfxMatrix.m[3], gfxMatrix.m[4], gfxMatrix.m[5] } { }

PlotMatrix::PlotMatrix(const double *coord) : coef { coord[0], coord[1], coord[2], coord[3], coord[4], coord[5] } { }

PlotMatrix::PlotMatrix(SplashCoord *coord) : coef { coord[0], coord[1], coord[2], coord[3], coord[4], coord[5] } { }

PlotMatrix::PlotMatrix() : coef { 1, 0, 0, 1, 0, 0 } { }

void PlotMatrix::translate(SplashCoord tx, SplashCoord ty)
{
    SplashCoord x0 = tx * coef[0] + ty * coef[2] + coef[4];
    SplashCoord y0 = tx * coef[1] + ty * coef[3] + coef[5];
    coef[4] = x0;
    coef[5] = y0;
}

void PlotMatrix::scale(SplashCoord sx, SplashCoord sy)
{
    coef[0] *= sx;
    coef[1] *= sx;
    coef[2] *= sy;
    coef[3] *= sy;
}

PlotMatrix PlotMatrix::inverse(bool *out_success) const
{
    PlotMatrix result;
    bool success = false;

    const double det_denominator = determinant();
    if (unlikely(det_denominator == 0)) {
        success = false;
    } else {
        const double det = 1 / det_denominator;
        result.coef[0] = coef[3] * det;
        result.coef[1] = -coef[1] * det;
        result.coef[2] = -coef[2] * det;
        result.coef[3] = coef[0] * det;
        result.coef[4] = (coef[2] * coef[5] - coef[3] * coef[4]) * det;
        result.coef[5] = (coef[1] * coef[4] - coef[0] * coef[5]) * det;
        success = true;
    }

    if (out_success != nullptr) {
        *out_success = success;
    }
    return result;
}

PlotMatrix PlotMatrix::multiply(const PlotMatrix &q) const
{
    PlotMatrix out;
    out.coef[0] = q.coef[0] * coef[0] + q.coef[1] * coef[2];
    out.coef[1] = q.coef[0] * coef[1] + q.coef[1] * coef[3];
    out.coef[2] = q.coef[2] * coef[0] + q.coef[3] * coef[2];
    out.coef[3] = q.coef[2] * coef[1] + q.coef[3] * coef[3];
    out.coef[4] = q.coef[4] * coef[0] + q.coef[5] * coef[2] + coef[4];
    out.coef[5] = q.coef[4] * coef[1] + q.coef[5] * coef[3] + coef[5];
    return out;
}

PlotPt PlotMatrix::transform(PlotPt pt) const
{
    PlotPt outPt;
    outPt.x = pt.x * coef[0] + pt.y * coef[2] + coef[4];
    outPt.y = pt.x * coef[1] + pt.y * coef[3] + coef[5];
    return outPt;
}

PlotPath PlotMatrix::transform(const PlotPath &path) const
{
    PlotPath outPath = PlotPath();
    outPath.characteristic = path.characteristic;
    for (auto pt : path.pts) {
        outPath.add(transform(pt));
    }
    return outPath;
}

PlotBBox PlotMatrix::transform(const PlotBBox &bbox) const
{
    PlotPt ll = transform(PlotPt { bbox.left, bbox.bottom });
    PlotPt ur = transform(PlotPt { bbox.right, bbox.top });
    return PlotBBox { min(ll.x, ur.x), max(ll.x, ur.x), min(ll.y, ur.y), max(ll.y, ur.y) };
}

Plot PlotMatrix::transform(const Plot &plot) const
{
    Plot outPlot;
    outPlot.metadata = plot.metadata;
    outPlot.metadata.page = transform(plot.metadata.page);
    for (auto const &path : plot.paths) {
        outPlot.add(transform(path));
    }
    return outPlot;
}

SplashCoord *PlotMatrix::asSplashMatrix() const
{
    return const_cast<SplashCoord *>(coef);
}
