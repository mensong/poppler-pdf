//========================================================================
//
// StrokeOutputDev.h
//
// A Poppler output device that renders the PDF using only simple straight
// line-segment strokes, suitable for drawing on a pen plotter (or other
// vector device). Fonts can be substituted with Hershey (stroke) fonts
// rather than drawn as outlines.
//
// Future work aspirations:
// - Render thick strokes with multiple pen strokes.
// - Render fills with crosshatching
// - Render *images* with modulated crosshatching!
// - Implement clipping paths (not just clipping rects)
// - Remove hidden strokes that should be obliterated by fills (esp white
//     fills) and perhaps even wide strokes that arrive later.
// - Further improvements to Hershey text translation quality, such as
//     warping characters to match replaced font character widths.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#ifndef STROKEOUTPUTDEV_H
#define STROKEOUTPUTDEV_H

#include "OutputDev.h"
#include "splash/SplashTypes.h"
#include "SplashOutputBase.h"
#include "Plot.h"
#include "HersheyFont.h"
#include "HersheyUnicodeSubstitutions.h"

class PlotClip;

class StrokeOutputDev : public SplashOutputBase
{
public:
    // Constructor.
    StrokeOutputDev(bool verboseFonts);

    // Destructor.
    ~StrokeOutputDev() override;

    void writePlotHpgl(const char *filename);
    void writePlotSvg(const char *filename);

    //----- get info about output device

    // Does this device use upside-down coordinates?
    // (Upside-down means (0,0) is the top left corner of the page.)
    bool upsideDown() override { return true; }

    // The real work happens here
    void startPage(int pageNum, GfxState *state, XRef *xref) override;
    void stroke(GfxState * /*state*/) override;
    void drawChar(GfxState *state, double x, double y, double dx, double dy, double originX, double originY, CharCode code, int nBytes, const Unicode *u, int uLen) override;

    // my stuff
    void innerStroke(const GfxRGB _rgb, SplashPath *path, GfxState *state, SplashCoord *matrix = nullptr, bool dashEnable = false);

    void setDebugAlarm(int _alarm) { alarm = _alarm; }

private:
    static PageColor whiteColor;
    void hersheyCharAt(const HersheyFont &hfont, double x, double y, double size, char c, GfxRGB rgb);
    void debugHersheyDisplay();

    HersheyDirectory hershey_directory;
    HersheyUnicodeSubstitutions hershey_unicode_substitutions;

    // Collect the output rendering here.
    Plot plot;

    static GfxRGB color8(int r, int g, int b);
    static PlotClip *clipFor(GfxState *state);

    void coalesce();
    Plot *coalescedPlot;

    ////////////////////////////////////////////////////////////
    // Debug stuff for identifying and studying specific PDF
    // objects (strokes, drawChars).
    ////////////////////////////////////////////////////////////

    // count each object this class renders. 'true' means alarm has fired; skip rendering
    bool tick();
    // finished rendering one counted object
    void endtick();

    // Counts how many objects have been rendered
    int clock;

    // Use the -a command-line flag to set this value, and bisect your way to identifying
    // which clock tick renders a particular feature that needs debugging.
    int alarm = 0; // when to stop rendering.

    // Tell subroutines to report debugging. Set this to true at specific clock values
    // in tick() to dive into rendering a specific object.
    bool debug = false;
};

#endif // STROKEOUTPUTDEV_H
