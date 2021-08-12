//========================================================================
//
// plotlib-clip.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2021 Jon Howell <jonh@jonh.net>
//
//========================================================================

/*
 * testing program for the RectClip class.
 */

#include <cassert>
#include "plotlib/PlotClip.h"

void test_rect_clip();

void test_rect_clip()
{
    RectClip rectClip(-1.0, 0.5, 7.35, 2.1);

    PlotSeg ref_seg, seg;
    PlotClip::Disposition d;

    // A segment entirely inside the rect.
    ref_seg = PlotSeg { { 0, 1 }, { 3, 1 } };
    seg = ref_seg;
    d = rectClip.clipSegment(&seg);
    assert(d == PlotClip::Disposition::Clipped);
    assert(seg.closeEnough(ref_seg)); // No change.

    // A segment entirely outside the rect.
    ref_seg = PlotSeg { { -6, 0 }, { 0, 6 } };
    seg = ref_seg;
    d = rectClip.clipSegment(&seg);
    assert(d == PlotClip::Disposition::Deleted);

    // A horizontal segment, clipped at both ends.
    ref_seg = PlotSeg { { -2, 1 }, { 9, 1 } };
    seg = ref_seg;
    d = rectClip.clipSegment(&seg);
    assert(d == PlotClip::Disposition::Clipped);
    assert(seg.closeEnough({ { -1.0, 1 }, { 7.35, 1 } }));

    // A sloping segment half inside, half outside
    ref_seg = PlotSeg { { 0, 1 }, { 3, 4 } };
    seg = ref_seg;
    d = rectClip.clipSegment(&seg);
    assert(d == PlotClip::Disposition::Clipped);
    assert(seg.closeEnough(PlotSeg { { 0, 1 }, { 1.1, 2.1 } }));
}

int main(int argc, char *argv[])
{
    test_rect_clip();
    return 0;
}
