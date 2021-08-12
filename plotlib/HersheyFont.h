//========================================================================
//
// HersheyFont.h
//
// Represents a Hershey (pen stroke) font, used by StrokeOutputDev to
// make text render nicely for pen plotters.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#ifndef HERSHEYFONT_H
#define HERSHEYFONT_H

#include <unordered_map>
#include <vector>
#include <string>
#include "splash/SplashPath.h"
#include "UTF.h"

using namespace std;

struct Glyph
{
    SplashPath *path;
    double width;
    double xShift; // used during construction
};

class HersheyFont
{
public:
    static HersheyFont *load(const string &font_file_name, bool oblique = false);
    HersheyFont(const HersheyFont &) = delete;
    ~HersheyFont();

    SplashPath *getChar(Unicode c);

    // Returns true if the hershey font has this character natively,
    // without an ASCII7 downgrade translation. Used by UnicodeMappingTables
    // to avoid falling back to an ASCII7 character when a more appropriate
    // character is available in another Hershey font.
    bool haveNativeChar(Unicode uc);

    std::vector<Unicode> glyphSet();
    bool valid() { return _valid; }

private:
    HersheyFont(const string &font_file_name, bool oblique = false);

    bool _valid = false;
    void makeChar(Unicode c);
    void makeLigature(Unicode c, Unicode *translation);
    std::unordered_map<Unicode, Glyph> glyphs;
    double yShift;
};

#endif // HERSHEYFONT_H
