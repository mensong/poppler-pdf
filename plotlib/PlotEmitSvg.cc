//========================================================================
//
// PlotEmitSvg.cc
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

#include "PlotEmitSvg.h"

void PlotEmitSvg::emitSvg(ofstream *ost, const Plot &plot, bool drawPenUpPaths)
{
    *ost << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" << endl;
    PlotBBox page = plot.pageAsBBox();
    *ost << "<svg "
         << "viewBox=\"" << page.left << " " << page.bottom << " " << page.width() << " " << page.height() << "\" "
         << "id=\"svg2\" version=\"1.1\" >" << endl;
    int pathi = 0;
    for (size_t i = 0; i < plot.paths.size(); i++) {
        PlotPath path = plot.paths[i];
        emitPathSvg(ost, path);
        if (drawPenUpPaths && i > 0) {
            PlotPath penMove(penMoveColor);
            penMove.add(plot.paths[i - 1].last());
            penMove.add(path.first());
            emitPathSvg(ost, penMove);
        }
        pathi += 1;
    }
    *ost << "</svg>" << endl;
    ost->close();
}

GfxRGB PlotEmitSvg::penMoveColor = GfxRGB { byteToCol(255), byteToCol(128), byteToCol(192) };

std::string PlotEmitSvg::rgbHexString(GfxRGB rgb)
{
    char buf[6];

    std::string str = "";
    snprintf(buf, sizeof(buf), "%02x", colToByte(rgb.r));
    str += buf;
    snprintf(buf, sizeof(buf), "%02x", colToByte(rgb.g));
    str += buf;
    snprintf(buf, sizeof(buf), "%02x", colToByte(rgb.b));
    str += buf;
    return str;
}

void PlotEmitSvg::emitPathSvg(ofstream *proof, const PlotPath &path)
{
    *proof << "<path style=\"fill:none;"
           << "stroke:#" << rgbHexString(path.characteristic.color) << ";"
           << "stroke-width:1px;\" ";
    *proof << "d=\"";
    for (unsigned i = 0; i < path.pts.size(); i++) {
        PlotPt seg = path.pts[i];
        if (i == 0) {
            *proof << "M ";
        } else {
        }
        *proof << seg.x << "," << seg.y << " ";
    }
    *proof << "\" />";
    if (path.debug) {
        *proof << " <!-- debug -->";
    }
    *proof << endl;
}
