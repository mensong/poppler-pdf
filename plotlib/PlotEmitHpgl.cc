//========================================================================
//
// PlotEmitHpgl.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <cassert>

#include "PlotEmitHpgl.h"

using namespace std;

//----------------------------------------------------------------------
// Pen
//----------------------------------------------------------------------

bool Pen::operator==(Pen other) const
{
    return id == other.id && color.r == other.color.r && color.g == other.color.g && color.b == other.color.b;
}

std::ostream &operator<<(std::ostream &os, Pen m)
{
    return os << m.id;
}

//----------------------------------------------------------------------
// PenPalette
//----------------------------------------------------------------------

int PenPalette::colorDistance(GfxRGB a, GfxRGB b)
{
    return abs(a.r - b.r) + abs(a.g - b.g) + abs(a.b - b.b);
}

Pen PenPalette::toPen(GfxRGB color)
{
    Pen best = pens[0];
    int dist = colorDistance(color, best.color);
    for (size_t i = 1; i < pens.size(); i++) {
        Pen newPen = pens[i];
        int newDist = colorDistance(color, newPen.color);
        if (newDist < dist) {
            best = newPen;
            dist = newDist;
        }
    }
    return best;
}

PenPalette PenPalette::defaultPalette()
{
    PenPalette palette;
    palette.pens.push_back(Pen { 1, { byteToCol(0), byteToCol(0), byteToCol(0) } });
    palette.pens.push_back(Pen { 2, { byteToCol(255), byteToCol(0), byteToCol(0) } });
    palette.pens.push_back(Pen { 3, { byteToCol(0), byteToCol(255), byteToCol(0) } });
    palette.pens.push_back(Pen { 4, { byteToCol(0), byteToCol(0), byteToCol(255) } });
    return palette;
}

//----------------------------------------------------------------------
// PlotEmitHpgl
//----------------------------------------------------------------------

constexpr double PlotterDPI = 1000.0;
constexpr double SVGDPI = 96.0;

void PlotEmitHpgl::emitHpgl(ofstream *ost, const Plot &plot)
{
    Plot scaledPlot = rescaleForHpgl(plot);

    *ost << "IN;";
    *ost << "SP1;";
    PenPalette palette = PenPalette::defaultPalette();
    Pen lastPen = Pen::UndefinedPen();
    for (auto const &path : scaledPlot.paths) {
        Pen pen = palette.toPen(path.characteristic.color);
        if (!(pen == lastPen)) {
            *ost << "SP" << pen << ";" << endl;
        }
        emitPathHpgl(ost, path);
        lastPen = pen;
    }
}

Plot PlotEmitHpgl::rescaleForHpgl(const Plot &plot)
{
    double scale = PlotterDPI / SVGDPI;
    PlotMatrix matrix(scale, 0, 0, -scale, 0, scale * plot.pageAsBBox().height());
    Plot scaledPlot = matrix.transform(plot);
    PlotPath boundary = drawRect(scaledPlot.pageAsBBox());
    scaledPlot.add(boundary);
    return scaledPlot;
}

PlotPath PlotEmitHpgl::drawRect(const PlotBBox &bbox)
{
    PlotPath path;
    path.add(PlotPt { bbox.left, bbox.bottom });
    path.add(PlotPt { bbox.right, bbox.bottom });
    path.add(PlotPt { bbox.right, bbox.top });
    path.add(PlotPt { bbox.left, bbox.top });
    path.add(PlotPt { bbox.left, bbox.bottom });
    return path;
}

void PlotEmitHpgl::emitCoordHpgl(ofstream *ost, PlotPt pt)
{
    *ost << int(pt.x) << "," << int(pt.y);
}

void PlotEmitHpgl::emitPathHpgl(ofstream *ost, const PlotPath &path)
{
    *ost << "PU";
    emitCoordHpgl(ost, path.first());
    *ost << ";" << endl;
    *ost << "PD";
    for (size_t i = 1; i < path.pts.size(); i++) {
        emitCoordHpgl(ost, path.pts[i]);
        if (i < path.pts.size() - 1) {
            *ost << ",";
        }
    }
    *ost << ";" << endl;
}
