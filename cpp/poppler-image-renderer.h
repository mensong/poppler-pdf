/*
 * Copyright (C) 2019, Zsombor Hollay-Horvath <hollay.horvath@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef POPPLER_IMAGE_RENDERER_H
#define POPPLER_IMAGE_RENDERER_H

#include "poppler-global.h"
#include "poppler-image.h"

#include "goo/ImgWriter.h"
#include "OutputDev.h"

class GfxState;

namespace poppler
{

//------------------------------------------------------------------------
// ImageOutputDev
//------------------------------------------------------------------------

class POPPLER_CPP_EXPORT image_renderer: public OutputDev {
public:
  enum ImageFormat {
    imgRGB,
    imgRGB48,
    imgGray,
    imgMonochrome,
    imgCMYK
  };

  // Create an OutputDev which will write images to it's own buffer
  image_renderer(std::vector<image> &imagesA);

  // Destructor.
  virtual ~image_renderer();

  // Does this device use tilingPatternFill()?  If this returns false,
  // tiling pattern fills will be reduced to a series of other drawing
  // operations.
  bool useTilingPatternFill() override { return true; }

  // Does this device use beginType3Char/endType3Char?  Otherwise,
  // text in Type 3 fonts will be drawn with drawChar/drawString.
  bool interpretType3Chars() override { return false; }

  // Does this device need non-text content?
  bool needNonText() override { return true; }

  //---- get info about output device

  // Does this device use upside-down coordinates?
  // (Upside-down means (0,0) is the top left corner of the page.)
  bool upsideDown() override { return true; }

  // Does this device use drawChar() or drawString()?
  bool useDrawChar() override { return false; }

  void drawImage(GfxState *state, Object *ref, Stream *str,
         int width, int height, GfxImageColorMap *colorMap,
         bool interpolate, const int *maskColors, bool inlineImg) override;

private:
  long getInlineImageLength(Stream *str, int width, int height, GfxImageColorMap *colorMap);

  std::vector<image> *images; // contains all the images
};

}

#endif
