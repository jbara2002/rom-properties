/***************************************************************************
 * ROM Properties Page shell extension. (librpbase)                        *
 * RpImageLoader.cpp: Image loader class.                                  *
 *                                                                         *
 * Copyright (c) 2016-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "stdafx.h"
#include "config.librpbase.h"

#include "RpImageLoader.hpp"
#include "librpfile/IRpFile.hpp"

// librpfile, librptexture
using LibRpFile::IRpFile;
using LibRpTexture::rp_image;

// Image loaders.
#include "RpPng.hpp"
#ifdef HAVE_JPEG
# include "RpJpeg.hpp"
#endif /* HAVE_JPEG */

namespace LibRpBase {

class RpImageLoaderPrivate
{
	private:
		// RpImageLoaderPrivate is a static class.
		RpImageLoaderPrivate();
		~RpImageLoaderPrivate();
		RP_DISABLE_COPY(RpImageLoaderPrivate)

	public:
		// Magic numbers.
		static const uint8_t png_magic[8];
#ifdef HAVE_JPEG
		static const uint8_t jpeg_magic_1[4];
		static const uint8_t jpeg_magic_2[4];
#endif /* HAVE_JPEG */
};

/** RpImageLoaderPrivate **/

// Magic numbers.
const uint8_t RpImageLoaderPrivate::png_magic[8] =
	{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
#ifdef HAVE_JPEG
const uint8_t RpImageLoaderPrivate::jpeg_magic_1[4] =
	{0xFF, 0xD8, 0xFF, 0xE0};
const uint8_t RpImageLoaderPrivate::jpeg_magic_2[4] =
	{'J','F','I','F'};
#endif /* HAVE_JPEG */

/** RpImageLoader **/

/**
 * Load an image from an IRpFile.
 * @param file IRpFile to load from.
 * @return rp_image*, or nullptr on error.
 */
rp_image *RpImageLoader::load(IRpFile *file)
{
	file->rewind();

	// Check the file header to see what kind of image this is.
	uint8_t buf[256];
	size_t sz = file->read(buf, sizeof(buf));
	if (sz >= sizeof(RpImageLoaderPrivate::png_magic)) {
		// Check for PNG.
		if (!memcmp(buf, RpImageLoaderPrivate::png_magic,
		     sizeof(RpImageLoaderPrivate::png_magic)))
		{
			// Found a PNG image.
			return RpPng::load(file);
		}
#ifdef HAVE_JPEG
		else if (!memcmp(buf, RpImageLoaderPrivate::jpeg_magic_1,
			  sizeof(RpImageLoaderPrivate::jpeg_magic_1)) &&
			 !memcmp(&buf[6], RpImageLoaderPrivate::jpeg_magic_2,
			  sizeof(RpImageLoaderPrivate::jpeg_magic_2)))
		{
			// Found a JPEG image.
			return RpJpeg::load(file);
		}
#endif /* HAVE_JPEG */
	}

	// Unsupported image format.
	return nullptr;
}

}
