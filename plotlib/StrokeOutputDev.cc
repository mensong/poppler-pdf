//========================================================================
//
// StrokeOutputDev.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#include <locale>
#include <codecvt>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <cassert>

#include "plotlib/StrokeOutputDev.h"
#include "splash/SplashFont.h"
#include "splash/SplashPath.h"
#include "splash/SplashXPath.h"
#include "splash/Splash.h"
#include "PlotClip.h"
#include "PlotEmitHpgl.h"
#include "PlotEmitSvg.h"
#include "GfxFont.h"

using namespace std;

void StrokeOutputDev::startPage(int pageNum, GfxState *state, XRef *xrefA)
{
    SplashOutputBase::startPage(pageNum, state, xrefA); // TODO factor into pieces?
    plot.metadata.page = PlotBBox { 0, state->getPageWidth(), 0, state->getPageHeight() };
    coalescedPlot = nullptr;
}

PageColor::PageColor()
{
    splashClearColor(color);
    color[0] = 255;
    color[1] = 255;
    color[2] = 255;
}

PageColor StrokeOutputDev::whiteColor;

StrokeOutputDev::StrokeOutputDev(bool verboseFonts)
    : SplashOutputBase(splashModeRGB8,
                       /*bitmapRowPadA*/ 4,
                       /*reverseVideoA*/ false, whiteColor.color,
                       /*bitmapTopDownA*/ true, splashThinLineDefault),
      hershey_directory(verboseFonts),
      hershey_unicode_substitutions(&hershey_directory),
      clock(0)
{
    // debugHersheyDisplay();   // do this before we start synthesizing glyphs
    plot = Plot();
    wstring s = hershey_unicode_substitutions.getKnownSubstitutionsForDebug();
    wofstream wos("/tmp/substitutions.wtxt");
    const std::locale utf8_locale = std::locale(std::locale(), new std::codecvt_utf8<wchar_t>());
    wos.imbue(utf8_locale);
    wos << "Known unicode substitutions: " << s << endl;
    wos.close();
}

StrokeOutputDev::~StrokeOutputDev()
{
    if (alarm > 0) {
        cout << "Clock ended at " << clock << endl;
    }
}

void StrokeOutputDev::coalesce()
{
    if (coalescedPlot == nullptr) {
        coalescedPlot = new Plot(plot.coalescePaths());
    }
}

void StrokeOutputDev::writePlotHpgl(const char *filename)
{
    coalesce();
    ofstream ost;
    ost.open(filename);
    PlotEmitHpgl::emitHpgl(&ost, *coalescedPlot);
}

void StrokeOutputDev::writePlotSvg(const char *filename)
{
    coalesce();
    ofstream ost;
    ost.open(filename);
    PlotEmitSvg::emitSvg(&ost, *coalescedPlot);
}

void StrokeOutputDev::hersheyCharAt(const HersheyFont &hfont, double x, double y, double size, char c, GfxRGB rgb)
{
    // ugly hack since getChar isn't really consty
    SplashPath *glyph = ((HersheyFont &)hfont).getChar(c);
    if (glyph == nullptr) {
        return;
    }

    PlotMatrix placeMat;
    placeMat.translate(x, y);
    placeMat.scale(size / 32.0, -size / 32.0);
    SplashPath *placed = glyph->transform(placeMat.asSplashMatrix());
    innerStroke(rgb, placed, nullptr);
}

GfxRGB StrokeOutputDev::color8(int r, int g, int b)
{
    GfxRGB color;
    color.r = byteToCol(r);
    color.g = byteToCol(g);
    color.b = byteToCol(b);
    return color;
}

#if ENUMERATE
// Debug function: render all the known Hershey fonts into a
// big SVG page. Used for building the HersheyUnicodeSubstitutions mappings.
void StrokeOutputDev::debugHersheyDisplay()
{
    HersheyFont *caption_font = hershey_directory.openHersheyFont({ "futural", false });

    // Neutralize the leftover matrix
    PlotMatrix ident;
    getSplash()->setMatrix(ident.asSplashMatrix());
    // Start a fresh plot
    plot = Plot();

    GfxRGB label_color = color8(20, 20, 20);
    GfxRGB sample_color = color8(0, 0, 180);

    double spacing = 48;
    double labelsz = 12;
    double samplesz = 24;

    auto font_names = hershey_directory.enumerateHersheyFontNames();
    for (size_t fonti = 0; fonti < font_names.size(); fonti++) {
        string font_name = font_names[fonti];
        cout << "Rendering " << font_name << endl;
        HersheyFont *row_font = hershey_directory.openHersheyFont({ font_name, false });
        if (!row_font->valid()) {
            cout << "Invalid!" << endl;
            continue;
        }
        // Paint the font name
        for (size_t j = 0; j < font_name.length(); j++) {
            int row = fonti;
            int col = j;
            hersheyCharAt(*caption_font, (col + 1.5) * 0.2 * spacing, (row + 1.8) * spacing, labelsz, font_name[j], label_color);
        }
        for (auto c : row_font->glyphSet()) {
            int offset = c - 32;
            int row = fonti;
            int col = offset;

            stringbuf buf;
            ostream os(&buf);
            os << hex << setfill('0') << setw(2) << c;
            string str = buf.str();
            hersheyCharAt(*caption_font, (col + 1.5) * spacing, (row + 1.5) * spacing, labelsz, str[0], label_color);
            hersheyCharAt(*caption_font, (col + 1.5) * spacing + 0.5 * labelsz, (row + 1.5) * spacing, labelsz, str[1], label_color);
            hersheyCharAt(*row_font, (col + 1.5) * spacing, (row + 1.5) * spacing - labelsz, samplesz, c, sample_color);
        }
    }
    GfxRGB rgb = color8(0, 255, 128);
    SplashPath dPath;
    dPath.moveTo(72, 72);
    dPath.lineTo(144, 72);
    dPath.lineTo(144, 144);
    dPath.lineTo(72, 144);
    dPath.close();
    innerStroke(rgb, &dPath, nullptr);
    getSplash()->stroke(&dPath);

    plot = plot.coalescePaths();
    // Use bounds of plot as page boundary.
    plot.metadata.page = plot.plotBBox();
    ofstream ost;
    ost.open("hershey-display.svg");
    PlotEmitSvg::emitSvg(&ost, plot);
}
#endif // ENUMERATE

bool StrokeOutputDev::tick()
{
    clock++;

    // debug = (1000 < clock && clock < 1200);
    // debug = (clock == 1129 || clock == 8 || clock == 19 || clock == 29);
    debug = false;
    getSplash()->setDebugMode(debug);

    return (alarm > 0 && clock > alarm);
}

void StrokeOutputDev::endtick()
{
    debug = false;
    getSplash()->setDebugMode(debug);
}

void StrokeOutputDev::stroke(GfxState *state)
{
    if (tick()) {
        return;
    }

    GfxRGB rgb;
    state->getStrokeRGB(&rgb);

    // Convert poppler path to Splash path
    SplashPath spath = convertPath(state, state->getPath(),
                                   /* dropEmptySubpaths*/ false);

    innerStroke(rgb, &spath, state, nullptr, true);

    // Paint the bitmap, too.
    SplashOutputBase::stroke(state);
    endtick();
}

PlotClip *StrokeOutputDev::clipFor(GfxState *state)
{
    if (state == nullptr) {
        return new NoClip();
    }
    double xMin, yMin, xMax, yMax;
    state->getClipBBox(&xMin, &yMin, &xMax, &yMax);
    return new RectClip(xMin, yMin, xMax, yMax);
}

void StrokeOutputDev::innerStroke(const GfxRGB rgb, SplashPath *path, GfxState *stateForClip, SplashCoord *matrix, bool dashEnable)
{

    if (matrix == nullptr) {
        matrix = getSplash()->getMatrix();
    }
    SplashCoord flatness = 1;

    // Flatten the curves in the path down to segments.
    SplashPath *pathIn;
    pathIn = getSplash()->flattenPath(path, matrix, flatness);

    // Render dashes down to individual strokes.
    if (dashEnable && getSplash()->getLineDashLength() > 0) {
        SplashPath *dashPath = getSplash()->makeDashedPath(pathIn);
        delete pathIn;
        pathIn = dashPath;
        if (pathIn->getLength() == 0) {
            delete pathIn;
            return;
        }
    }

    // Extract the resulting path segments out one-by-one,
    // clip each to the current clipping region,
    // and collect them in the pen-specific PlotPath data structure.
    //
    // We use SplashXPath to avoid creating yet more code that
    // interprets SplashPath flags.
    SplashXPath xPath(pathIn, matrix, flatness, false);

    PlotClip *clip = clipFor(stateForClip);

    // Extract the flattened path back into a SplashPath, which we'll
    // use as an internal representation of the output segments.
    int i;
    for (i = 0; i < xPath.getLength(); ++i) {
        SplashXPathSeg xseg = xPath.getSegment(i);
        PlotSeg seg { PlotPt { xseg.x0, xseg.y0 }, PlotPt { xseg.x1, xseg.y1 } };
        PlotClip::Disposition disp = clip->clipSegment(&seg);
        if (disp == PlotClip::Disposition::Clipped) {
            PlotPath ppath(rgb);
            ppath.debug = debug;
            ppath.add(seg.a);
            ppath.add(seg.b);
            plot.add(ppath);
        }
    }
    delete pathIn;
    delete clip;
}

SplashCoord transformedPathWidth(const Matrix &mat, SplashPath *path, char debugC);
SplashCoord transformedPathWidth(const Matrix &mat, SplashPath *path, char debugC)
{
    Matrix inv;
    mat.invertTo(&inv);

    SplashCoord minX = 0, maxX = 0;
    for (int i = 0; i < path->getLength(); i++) {
        double dblx, dbly;
        unsigned char flags;
        path->getPoint(i, &dblx, &dbly, &flags);
        double tx, ty;
        mat.transform(dblx, dbly, &tx, &ty);
        if (i == 0) {
            minX = tx;
            maxX = tx;
        }
        minX = min(minX, tx);
        maxX = max(maxX, tx);
    }
    double width = maxX - minX;
    return width;
}

void StrokeOutputDev::drawChar(GfxState *state, double x, double y, double dx, double dy, double originX, double originY, CharCode code, int nBytes, const Unicode *u, int uLen)
{
    if (tick()) {
        return;
    }

    if (needFontUpdate) {
        doUpdateFont(state);
    }

    SplashPath *path = nullptr;
    GfxRGB rgb;
    state->getFillRGB(&rgb);

    // Unicode u is the "real" character in Unicode; CharCode code is the
    // offset to the glyph in the currently-loaded face. So we have to
    // ask font for code to get width info, but we'll use u to grovel around
    // looking for a compatible Hershey char alternative.
    //
    // TODO Note that some PDFs will lie about u, giving "f" for "fi" ligatures
    // and "h" for \langle -- I'm looking at you, LaTeX! It would be nice
    // to figure out how to detect this nonsense (by the TeX-specific name of
    // the symbol font?) and map it back to a unicode symbol.
    //
    if ((path = font->getGlyphPath(code)) && path->getLength() > 0) {
        // Draw the letter in the right place.
        PlotMatrix offMat;
        offMat.translate(x, y);

        SplashPath *glyph = nullptr;
        HersheyFont *preferred_font = hershey_directory.selectFont(state->getFont());
        if (preferred_font) {
            glyph = hershey_unicode_substitutions.getChar(preferred_font, *u);
        }
        if (glyph != nullptr) {
            // Pull the character into an axis-aligned reference frame.
            // I never did figure out how to eliminate whatever translation
            // was applied to the char path, but for this calculation we can live
            // without it.
            PlotMatrix textMat(state->getTextMat());
            PlotMatrix textInvMat = textMat.inverse();
            PlotMatrix rInvMat = textInvMat.multiply(offMat);
            SplashPath *basePath = path->transform(rInvMat.asSplashMatrix());

            // Compute its bounding box in axis-aligned space.
            PlotBBox bb = PlotBBox::fromSplashPath(basePath);
            delete basePath;

            // Pull given x,y glyph origin placement into baseline space,
            // so we can use the y when going forward again.
            PlotPt bo = textInvMat.transform(PlotPt { x, y });

            // And transform the baseline edges and center back into
            // state->getCTM() space.
            PlotPt bc = { (bb.left + bb.right) * 0.5, bo.y };
            PlotPt pc = textMat.transform(bc);
            PlotPt pz = textMat.transform(PlotPt { 0, 0 });

            // Scale the hershey glyph to be in state space
            // This scaling factor is a magic number; how big is the hershey
            // space?
            // TODO maybe measure the available glyphs in the PDF font and
            // find a nice median scale factor to fit the hershey glyphs into
            // the same hole?
            double factor = 32.;

            double scale_factor = state->getFontSize() / factor;
            PlotMatrix roffMat;
            roffMat.translate(pc.x - pz.x, pc.y - pz.y);
            PlotMatrix offTextScaleMat = roffMat.multiply(textMat);
            offTextScaleMat.scale(scale_factor, scale_factor);

            SplashPath *localGlyph = glyph->transform(offTextScaleMat.asSplashMatrix());
            innerStroke(rgb, localGlyph, state);
            delete localGlyph;
        }
        if (glyph == nullptr || debug) {
            // No hershey translation available; render the outline font stroke
            // GfxRGB orgb = { byteToCol(0), byteToCol(255), byteToCol(128) };
            SplashPath *offPath = path->transform(PlotMatrix(offMat).asSplashMatrix());
            innerStroke(rgb, offPath, state, nullptr);
        }
    } else {
        // I'm not sure how we can get here with an empty path from the
        // Splash world; what happened to the bitmap in this case?
    }
    delete path;

    // Paint the bitmap, too.
    SplashOutputBase::drawChar(state, x, y, dx, dy, originX, originY, code, nBytes, u, uLen);

    endtick();
}
