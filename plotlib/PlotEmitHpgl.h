//========================================================================
//
// PlotEmitHpgl.h
//
// Emit Plot geometry as HPGL.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#ifndef PLOTEMITHPGL_H
#define PLOTEMITHPGL_H

#include <vector>
#include "Plot.h"

class Pen
{
public:
    int id;
    GfxRGB color;
    static Pen UndefinedPen() { return Pen { -1, GfxRGB { 0, 0, 0 } }; }
    bool operator==(Pen other) const;
};
std::ostream &operator<<(std::ostream &os, Pen m);

class PenPalette
{
public:
    std::vector<Pen> pens;
    Pen toPen(GfxRGB color);

    // Construct a default four-pen palette (K, R, G, B)
    // TODO specify palette with an HPGL-output-mode format config file.
    static PenPalette defaultPalette();

private:
    static int colorDistance(GfxRGB a, GfxRGB b);
};

class PlotEmitHpgl
{
public:
    static void emitHpgl(ofstream *ost, const Plot &plot);

private:
    static Plot rescaleForHpgl(const Plot &plot);
    static PlotPath drawRect(const PlotBBox &bbox);
    static void emitCoordHpgl(ofstream *ost, PlotPt pt);
    static void emitPathHpgl(ofstream *ost, const PlotPath &path);
};

#endif // PLOTEMITHPGL_H
