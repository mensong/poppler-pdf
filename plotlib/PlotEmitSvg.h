//========================================================================
//
// PlotEmitSvg.h
//
// Emit Plot geometry as SVG.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#ifndef PLOTEMITSVG_H
#define PLOTEMITSVG_H

#include "Plot.h"

class PlotEmitSvg
{
public:
    static void emitSvg(ofstream *ost, const Plot &plot, bool drawPenUpPaths = false);

private:
    static GfxRGB penMoveColor;

    static std::string rgbHexString(GfxRGB rgb);
    static void emitPathSvg(ofstream *proof, const PlotPath &path);
};

#endif // PLOTEMITSVG_H
