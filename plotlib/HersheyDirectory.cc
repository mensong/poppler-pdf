//========================================================================
//
// HersheyDirectory.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2020 Jon Howell <jonh@jonh.net>
//
//========================================================================

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include "HersheyDirectory.h"

#if ENUMERATE
#    include <glob.h>
#endif // ENUMERATE

using namespace std;

HersheyDirectory::~HersheyDirectory()
{
    for (auto it = hershey_req_to_hershey_font.begin(); it != hershey_req_to_hershey_font.end(); /*erase advances it*/) {
        delete it->second;
        it = hershey_req_to_hershey_font.erase(it);
    }
}

HersheyFont *HersheyDirectory::selectFont(GfxFont *gfx_font)
{
    const GooString *font_name = gfx_font->getEmbeddedFontName();
    return selectFont(font_name ? font_name->c_str() : "Times");
}

HersheyFont *HersheyDirectory::selectFont(const string &pdf_font_name)
{
    if (pdf_name_to_hershey_req.count(pdf_font_name) == 0) {
        pdf_name_to_hershey_req[pdf_font_name] = fontSelectionPolicy(pdf_font_name);
    }
    return openHersheyFont(pdf_name_to_hershey_req[pdf_font_name]);
}

HersheyFont *HersheyDirectory::openHersheyFont(const HersheyFontRequest &req)
{
    if (hershey_req_to_hershey_font.count(req) == 0) {
        string font_path = pathForFontFilename(req.hershey_font_name);
        HersheyFont *font = HersheyFont::load(font_path.c_str(), req.oblique);
        if (!font) {
            return nullptr;
        }
        hershey_req_to_hershey_font[req] = font;
    }
    return hershey_req_to_hershey_font[req];
}

#if ENUMERATE
vector<string> HersheyDirectory::enumerateHersheyFontNames()
{
    vector<string> names;
    glob_t globb;
    string glob_base = getHersheyFontPath();
    if (glob_base.back() != '/') {
        glob_base += "/";
    }
    string extn = ".jhf";
    string glob_pattern = glob_base + "*" + extn;
    int rc = glob(glob_pattern.c_str(), 0, nullptr, &globb);
    assert(rc == 0);
    for (size_t fonti = 0; fonti < globb.gl_pathc; fonti++) {
        string font_path = globb.gl_pathv[fonti];
        assert(font_path.rfind(glob_base, 0) == 0); // font_path starts with glob_base
        string font_filename = font_path.substr(glob_base.length(), string::npos);
        assert(font_filename.substr(font_filename.length() - extn.length(), string::npos) == extn); // ends with .jhf
        string font_name = font_filename.substr(0, font_filename.length() - extn.length());
        names.push_back(font_name);
    }
    globfree(&globb);
    return names;
}
#endif // ENUMERATE

HersheyFontRequest HersheyDirectory::fontSelectionPolicy(const string &pdf_font_name)
{
    // Case-insensitive compare is surprisingly difficult in C++,
    // because it's trying to keep us from committing locale crimes.
    string lower = pdf_font_name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

    auto npos = string::npos;

    bool sans = lower.find("arial") != npos || lower.find("sans") != npos || lower.find("helvetica") != npos || lower.find("calibri") != npos || lower.find("carlito") != npos || lower.find("cordia") != npos
            || lower.find("centurygothic") != npos // CenturyGothic is excruciatingly sans. Very Futura.
            ;
    bool bold = lower.find("bold") != npos;
    bool italic = lower.find("italic") != npos || lower.find("oblique") != npos;
    bool gothic = lower.find("gothic") != npos;
    bool script = lower.find("script") != npos || lower.find("cursive") != npos;

    HersheyFontRequest req;
    if (sans) {
        if (bold) {
            req = HersheyFontRequest { "futuram", italic };
        } else {
            req = HersheyFontRequest { "futural", italic };
        }
    } else if (gothic) {
        if (italic) {
            req = HersheyFontRequest { "gothicita", false };
        } else if (bold) {
            req = HersheyFontRequest { "gothicger", false };
        } else {
            req = HersheyFontRequest { "gothiceng", false };
        }
    } else if (script) {
        req = HersheyFontRequest { "cursive", false };
    } else {
        // serif
        if (italic) {
            if (bold) {
                req = HersheyFontRequest { "timesib", false };
            } else {
                req = HersheyFontRequest { "timesi", false };
            }
        } else {
            if (bold) {
                req = HersheyFontRequest { "timesrb", false };
            } else {
                req = HersheyFontRequest { "timesr", false };
            }
        }
    }
    if (verboseFonts) {
        cout << " Policy for " << pdf_font_name << ": " << req.hershey_font_name << (req.oblique ? "-oblique" : "") << endl;
    }
    return req;
}

string HersheyDirectory::getHersheyFontPath()
{
    const char *path = getenv(HERSHEY_FONT_PATH_ENV);
    if (path == nullptr) {
        path = "/usr/share/hershey-fonts/";
    }
    return string(path);
}

string HersheyDirectory::pathForFontFilename(const string &hershey_font_name)
{
    stringstream path;
    path << getHersheyFontPath() << hershey_font_name << ".jhf";
    return path.str();
}
