//========================================================================
//
// HersheyUnicodeSubstitutions.h
//
// Tables to replace beyond-ASCII7 Unicode characters with characters
// from other Herhsey fonts. This enables stroke rendering of Greek,
// math and other symbols.
//
// TODO: Hershey also provides Japanese and Cyrillic; this class does
// not yet provide substitution mappings.
//
// TODO: Greek has serif & sans-serif variants; it would be nice to
// provide the appropriate substitution based on the requested base font.
//
// TODO: All of these substitutions could be obliqued for use when the
// base font is italic/oblique.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#ifndef HERSHEYUNICODESUBSTITUTIONS_H
#define HERSHEYUNICODESUBSTITUTIONS_H

#include <unordered_map>

#include "HersheyFont.h"
#include "HersheyDirectory.h"
#include "splash/SplashPath.h"

// Mappings into a given Hershey font, font implied from context.
struct UnicodeHersheyMapping
{
    Unicode u;
    char h;
};

struct HersheyFontDescriptor
{
    HersheyFontRequest req;
    HersheyFont *loaded_font = nullptr;
};

struct GlyphPtr
{
    HersheyFontDescriptor *f;
    char h;
    GlyphPtr() : f(nullptr), h(0) { }
    GlyphPtr(HersheyFontDescriptor *_f, char _h) : f(_f), h(_h) { }
};

// Hershey fonts are 7-bit.
// This class makes a mapping from non-ASCII7 unicode to hershey "symbol"
// fonts, in which the low-valued characters get used for other symbols.
// I'm sure I've missed a few translations.
//
// TODO Add Unicode synonyms using Unicode NamesList.txt:
// https://unicode.org/Public/UNIDATA/NamesList.txt
// Note that poppler already does some amount of unicode-high-to-ASCII7
// mapping, which we exploit inside HersheyFont.cc.
//
// NB there are a few ligatures out at the end of symbolic, but they're
// times-italic. I decided to just stitch ligatures together from the
// requested font (in HersheyFont) instead, so they match whichever font
// the user requested.
//
// Of the available hershey fonts, here is how they are handled:
//
// Base ASCII7 fonts for user use, accessible via the font selection policy in
// HersheyDirectory:
//
// cursive.jhf
// futural.jhf
// futuram.jhf
// gothiceng.jhf
// gothicger.jhf
// gothicita.jhf
// timesib.jhf
// timesi.jhf
// timesrb.jhf
// timesr.jhf
//
// Fonts with interesting symbols in the "wrong place"; this class provides
// mappings from unicode values to glyphs from these files:
//
// greek.jhf
// markers.jhf
// mathlow.jhf
// symbolic.jhf
//
// Could add unicode translations for these files, but I haven't yet:
//
// timesg.jhf  // This is an alternate font for greek (I used the sans one); we'd need a cleverer lookup scheme to decide which variant we should fall back on.
// cyrillic.jhf
// japanese.jhf
// astrology.jhf
// meteorology.jhf
// music.jhf
// mathupp.jhf // I don't think there's anything here that's not in mathlow
//
// My code isn't smart enough to parse these yet.
//
// cyrilc_1.jhf
// greekc.jhf
// greeks.jhf
// scripts.jhf
//
// Some weird layout; not sure what they're good for:
//
// scriptc.jhf // parses, but laid out weirdly and the lowercase is cut off
// gothgbt.jhf
// gothgrt.jhf
// gothitt.jhf
// rowmand.jhf
// rowmans.jhf
// rowmant.jhf

// TODO: This framework supports only one "font" for fallbacks. Consider
// ways to select among them. There are serif and sans Hershey fonts for
// Greek. And HersheyFont.cc can -oblique fonts programmatically, which
// would allow italic-ish fallback characters when the surrounding text
// is italic.

class HersheyUnicodeSubstitutions
{
public:
    HersheyUnicodeSubstitutions(HersheyDirectory *hershey_directory);

    // Look for c in hershey_font; if absent, fall through to a substitute
    // character from one this giant cross-font lookup table.
    SplashPath *getChar(HersheyFont *hershey_font, Unicode c);

    std::wstring getKnownSubstitutionsForDebug();

private:
    std::unordered_map<Unicode, GlyphPtr> passthrough_table;
    HersheyDirectory *hershey_directory;

    static HersheyFontDescriptor greek_font;
    static UnicodeHersheyMapping greek_mapping[];
    static HersheyFontDescriptor futuram_font;
    static UnicodeHersheyMapping futuram_mapping[];
    static HersheyFontDescriptor symbolic_font;
    static UnicodeHersheyMapping symbolic_mapping[];
    static HersheyFontDescriptor mathlow_font;
    static UnicodeHersheyMapping mathlow_mapping[];
    void LoadMapping(HersheyFontDescriptor *descriptor, const UnicodeHersheyMapping *mapping);
};

#endif // HERSHEYUNICODESUBSTITUTIONS_H
