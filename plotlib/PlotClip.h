//========================================================================
//
// PlotClip.h
//
// Path clipping for line segments.
// I had hoped to reuse some of SplashClip to do path clip computation, but it
// doesn't do general 2D geometry clips, only raster span clips.
//
// This code currently doesn't support interesting clipping paths, just
// rectangles, but the interface is intended to allow that later. That
// will enable not only correct rendering of strokes inside clipping regions,
// but also neat tricks like removing hidden lines from behind filled regions
// and rendering crosshatch-based fills.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================
#ifndef _PLOTCLIP_H
#define _PLOTCLIP_H

#include "Plot.h"

class PlotClip
{
public:
    PlotClip() { }
    PlotClip(const PlotClip &) = delete;
    PlotClip &operator=(const PlotClip &) = delete;
    virtual ~PlotClip();

    enum Disposition
    {
        Clipped,
        Deleted
    };

    // Mutate seg in place to be clipped.
    // Returns Clipped if the seg is usable after the call (either because it
    // was clipped, or entirely inside the clip region).
    // Returns Deleted if seg was entirely outside the clip region, and
    // hence should be discarded by the caller.
    virtual Disposition clipSegment(PlotSeg *seg) = 0;
};

// A clipping region that admits all geometry.
class NoClip : public PlotClip
{
public:
    Disposition clipSegment(PlotSeg *seg) override { return Clipped; }
    ~NoClip() override;
};

// A rectangular clipping region that admits its interior.
class RectClip : public PlotClip
{
public:
    RectClip(double xMin, double yMin, double xMax, double yMax);
    ~RectClip() override;

    Disposition clipSegment(PlotSeg *seg) override;

protected:
    // Keeps the right half-plane
    Disposition clipHalfPlane(PlotSeg *victim, const PlotSeg &plane);

    PlotSeg left, right, top, bottom;
};

#endif // _PLOTCLIP_H
