//========================================================================
//
// HersheyDirectory.h
//
// Heuristically maps font names to one of the few Hershey fonts that might be
// a reasonable substitute.
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#ifndef HERSHEYDIRECTORY_H
#define HERSHEYDIRECTORY_H

#include <unordered_map>
#include <string>
#include "GfxFont.h"
#include "HersheyFont.h"

// Used for development, for displaying all hershey fonts.
// Default disabled, as it adds a dependency on glob.h
#define ENUMERATE 0

using namespace std;

struct HersheyFontRequest
{
    // Short name, like "timesrb", omitting ".jhf" extn and path.
    string hershey_font_name;
    // true means programatically oblique the font at load time.
    // Times has pretty italics, but sans means obliquing Futura.
    bool oblique;

    bool operator==(const HersheyFontRequest &other) const { return hershey_font_name == other.hershey_font_name && oblique == other.oblique; }
};

namespace std {
template<>
struct hash<HersheyFontRequest>
{
    std::size_t operator()(const HersheyFontRequest &req) const
    {
        // Compute individual hash values for first,
        // second and third and combine them using XOR
        // and bit shifting:

        return (hash<string>()(req.hershey_font_name)) ^ (hash<bool>()(req.oblique));
    }
};
}

class HersheyDirectory
{
public:
    HersheyDirectory(bool _verboseFonts) : verboseFonts(_verboseFonts) { }
    HersheyDirectory(const HersheyDirectory &) = delete;
    ~HersheyDirectory();
    HersheyFont *selectFont(GfxFont *gfx_font);
    HersheyFont *selectFont(const string &pdf_font_name);
    HersheyFont *openHersheyFont(const HersheyFontRequest &req);

#if ENUMERATE
    // used for debugging/development.
    vector<string> enumerateHersheyFontNames();
#endif // ENUMERATE

    static constexpr const char *HERSHEY_FONT_PATH_ENV = "HERSHEY_FONT_PATH";
    static string getHersheyFontPath();

private:
    bool verboseFonts;

    HersheyFontRequest fontSelectionPolicy(const string &pdf_font_name);
    static string pathForFontFilename(const string &hershey_font_name);

    unordered_map<string, HersheyFontRequest> pdf_name_to_hershey_req;
    unordered_map<HersheyFontRequest, HersheyFont *> hershey_req_to_hershey_font;
};

#endif // HERSHEYDIRECTORY_H
