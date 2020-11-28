//========================================================================
//
// PlotClip.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#include <iostream>
#include "PlotClip.h"
using namespace std;

PlotClip::~PlotClip() { }

NoClip::~NoClip() { }

RectClip::~RectClip() { }

RectClip::RectClip(double xMin, double yMin, double xMax, double yMax)
{
    PlotPt ll = PlotPt { xMin, yMin };
    PlotPt ul = PlotPt { xMin, yMax };
    PlotPt ur = PlotPt { xMax, yMax };
    PlotPt lr = PlotPt { xMax, yMin };
    left = PlotSeg { ll, ul };
    top = PlotSeg { ul, ur };
    right = PlotSeg { ur, lr };
    bottom = PlotSeg { lr, ll };
}

PlotClip::Disposition RectClip::clipHalfPlane(PlotSeg *victim, const PlotSeg &plane)
{
    PlotPt norm = plane.b.minus(plane.a).unit().left();
    double r0 = norm.dot(victim->a.minus(plane.a));
    double r1 = norm.dot(victim->b.minus(plane.a));
    if (r0 <= 0 && r1 <= 0) {
        // Both are right of the clip plane
        return Disposition::Clipped;
    } else if (r0 < 0 && r1 > 0) {
        PlotPt c = victim->b.minus(victim->a).scale(-r0 / (r1 - r0)).plus(victim->a);
        victim->b = c;
        return Disposition::Clipped;
    } else if (r0 > 0 && r1 < 0) {
        PlotPt c = victim->a.minus(victim->b).scale(r1 / (r1 - r0)).plus(victim->b);
        victim->a = c;
        return Disposition::Clipped;
    } else {
        return Disposition::Deleted;
    }
}

PlotClip::Disposition RectClip::clipSegment(PlotSeg *seg)
{
    Disposition d;

    d = clipHalfPlane(seg, left);
    if (d == Disposition::Deleted) {
        return d;
    }
    d = clipHalfPlane(seg, top);
    if (d == Disposition::Deleted) {
        return d;
    }
    d = clipHalfPlane(seg, right);
    if (d == Disposition::Deleted) {
        return d;
    }
    d = clipHalfPlane(seg, bottom);
    return d;
}
