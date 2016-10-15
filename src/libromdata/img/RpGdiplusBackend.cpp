/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * RpGdiplusBackend.hpp: rp_image_backend using GDI+.                      *
 *                                                                         *
 * Copyright (c) 2016 by David Korth.                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

// NOTE: This class is located in libromdata, not Win32,
// since RpPng_gdiplus.cpp uses the backend directly.

#include "RpGdiplusBackend.hpp"

// C includes.
#include <stdlib.h>

// C includes. (C++ namespace)
#include <cassert>

// C++ includes.
#include <memory>
using std::unique_ptr;

#include "img/rp_image.hpp"
#include "img/GdiplusHelper.hpp"

namespace LibRomData {

/**
 * Create an RpGdiplusBackend.
 *
 * This will create an internal Gdiplus::Bitmap
 * with the specified parameters.
 *
 * @param width Image width.
 * @param height Image height.
 * @param format Image format.
 */
RpGdiplusBackend::RpGdiplusBackend(int width, int height, rp_image::Format format)
	: super(width, height, format)
	, m_gdipToken(0)
	, m_pGdipBmp(nullptr)
	, m_isLocked(false)
	, m_gdipFmt(0)
	, m_pGdipPalette(nullptr)
{
	// Initialize GDI+.
	m_gdipToken = GdiplusHelper::InitGDIPlus();
	assert(m_gdipToken != 0);
	if (m_gdipToken == 0)
		return;

	// Initialize the Gdiplus bitmap.
	switch (format) {
		case rp_image::FORMAT_CI8:
			m_gdipFmt = PixelFormat8bppIndexed;
			break;
		case rp_image::FORMAT_ARGB32:
			m_gdipFmt = PixelFormat32bppARGB;
			break;
		default:
			assert(!"Unsupported rp_image::Format.");
			this->width = 0;
			this->height = 0;
			this->stride = 0;
			this->format = rp_image::FORMAT_NONE;
			return;
	}
	m_pGdipBmp = new Gdiplus::Bitmap(width, height, m_gdipFmt);

	// Do the initial lock.
	if (doInitialLock() != 0)
		return;

	if (this->format == rp_image::FORMAT_CI8) {
		// Initialize the palette.
		// Note that Gdiplus::Image doesn't support directly
		// modifying the palette, so we have to copy our
		// palette data every time the underlying image
		// is requested.
		size_t gdipPalette_sz = sizeof(Gdiplus::ColorPalette) + (sizeof(Gdiplus::ARGB)*255);
		m_pGdipPalette = (Gdiplus::ColorPalette*)calloc(1, gdipPalette_sz);
		m_pGdipPalette->Flags = 0;
		m_pGdipPalette->Count = 256;

		// Set this->palette to the first palette entry.
		this->palette = reinterpret_cast<uint32_t*>(&m_pGdipPalette->Entries[0]);
		// 256 colors allocated in the palette.
		this->palette_len = 256;
	}
}

/**
 * Create an RpGdiplusBackend using the specified Gdiplus::Bitmap.
 *
 * NOTE: This RpGdiplusBackend will take ownership of the Gdiplus::Bitmap.
 *
 * @param pGdipBmp Gdiplus::Bitmap.
 */
RpGdiplusBackend::RpGdiplusBackend(Gdiplus::Bitmap *pGdipBmp)
	: super(0, 0, rp_image::FORMAT_NONE)
	, m_gdipToken(0)
	, m_pGdipBmp(pGdipBmp)
	, m_isLocked(false)
	, m_gdipFmt(0)
	, m_pGdipPalette(nullptr)
{
	assert(pGdipBmp != nullptr);
	if (!pGdipBmp)
		return;

	// Initialize GDI+.
	m_gdipToken = GdiplusHelper::InitGDIPlus();
	assert(m_gdipToken != 0);
	if (m_gdipToken == 0) {
		delete m_pGdipBmp;
		m_pGdipBmp = nullptr;
		return;
	}

	// Check the pixel format.
	m_gdipFmt = pGdipBmp->GetPixelFormat();
	switch (m_gdipFmt) {
		case PixelFormat8bppIndexed:
			this->format = rp_image::FORMAT_CI8;
			break;

		case PixelFormat24bppRGB:
		case PixelFormat32bppRGB:
			// TODO: Is conversion needed?
			this->format = rp_image::FORMAT_ARGB32;
			m_gdipFmt = PixelFormat32bppRGB;
			break;

		case PixelFormat32bppARGB:
			this->format = rp_image::FORMAT_ARGB32;
			break;

		default:
			// Unsupported format.
			assert(!"Unsupported Gdiplus::PixelFormat.");
			delete m_pGdipBmp;
			m_pGdipBmp = nullptr;
			return;
	}

	// Set the width and height.
	this->width = pGdipBmp->GetWidth();
	this->height = pGdipBmp->GetHeight();

	// If the image has a palette, load it.
	if (this->format == rp_image::FORMAT_CI8) {
		// 256-color palette.
		size_t gdipPalette_sz = sizeof(Gdiplus::ColorPalette) + (sizeof(Gdiplus::ARGB)*255);
		m_pGdipPalette = (Gdiplus::ColorPalette*)malloc(gdipPalette_sz);

		// Actual GDI+ palette size.
		int palette_size = pGdipBmp->GetPaletteSize();
		assert(palette_size > 0);

		Gdiplus::Status status = pGdipBmp->GetPalette(m_pGdipPalette, palette_size);
		if (status != Gdiplus::Status::Ok) {
			// Failed to retrieve the palette.
			free(m_pGdipPalette);
			m_pGdipPalette = nullptr;
			delete m_pGdipBmp;
			m_pGdipBmp = nullptr;
			m_gdipFmt = 0;
			this->width = 0;
			this->height = 0;
			this->stride = 0;
			this->format = rp_image::FORMAT_NONE;
			return;
		}

		if (m_pGdipPalette->Count < 256) {
			// Extend the palette to 256 colors.
			// Additional colors will be set to 0.
			int diff = 256 - m_pGdipPalette->Count;
			memset(&m_pGdipPalette->Entries[m_pGdipPalette->Count], 0, diff*sizeof(Gdiplus::ARGB));
			m_pGdipPalette->Count = 256;
		}

		// Set this->palette to the first palette entry.
		this->palette = reinterpret_cast<uint32_t*>(&m_pGdipPalette->Entries[0]);
		// 256 colors allocated in the palette.
		this->palette_len = 256;
	}

	// Do the initial lock.
	doInitialLock();
}

RpGdiplusBackend::~RpGdiplusBackend()
{
	if (m_pGdipBmp) {
		// TODO: Is an Unlock required here?
		m_pGdipBmp->UnlockBits(&m_gdipBmpData);
		delete m_pGdipBmp;
	}

	free(this->m_pGdipPalette);
	GdiplusHelper::ShutdownGDIPlus(m_gdipToken);
}

/**
 * Initial GDI+ bitmap lock and palette initialization.
 * @return 0 on success; non-zero on error.
 */
int RpGdiplusBackend::doInitialLock(void)
{
	// Lock the bitmap.
	// It will only be (temporarily) unlocked when converting to HBITMAP.
	// FIXME: rp_image needs to support "stride", since GDI+ stride is
	// not necessarily the same as width*sizeof(pixel).
	Gdiplus::Status status = lock();
	if (status != Gdiplus::Status::Ok) {
		// Error locking the GDI+ bitmap.
		delete m_pGdipBmp;
		m_pGdipBmp = nullptr;
		m_gdipFmt = 0;
		this->width = 0;
		this->height = 0;
		this->stride = 0;
		this->format = rp_image::FORMAT_NONE;
		return -1;
	}

	// Set the image stride.
	// On Windows, it might not be the same as width*pixelsize.
	// TODO: If Stride is negative, the image is upside-down.
	this->stride = abs(m_gdipBmpData.Stride);
	return 0;
}

/**
 * Creator function for rp_image::setBackendCreatorFn().
 */
rp_image_backend *RpGdiplusBackend::creator_fn(int width, int height, rp_image::Format format)
{
	return new RpGdiplusBackend(width, height, format);
}

void *RpGdiplusBackend::data(void)
{
	if (!m_isLocked) {
		// Lock the image.
		const_cast<RpGdiplusBackend*>(this)->lock();
	}

	// Return the data from the locked GDI+ bitmap.
	return m_gdipBmpData.Scan0;
}

const void *RpGdiplusBackend::data(void) const
{
	if (!m_isLocked) {
		// Lock the image.
		const_cast<RpGdiplusBackend*>(this)->lock();
	}

	// Return the data from the locked GDI+ bitmap.
	return m_gdipBmpData.Scan0;
}

size_t RpGdiplusBackend::data_len(void) const
{
	return this->stride * this->height;
}

/**
 * Lock the GDI+ bitmap.
 *
 * WARNING: This *may* invalidate pointers
 * previously returned by data().
 *
 * @return Gdiplus::Status
 */
Gdiplus::Status RpGdiplusBackend::lock(void)
{
	// TODO: Recursive locks?
	if (m_isLocked)
		return Gdiplus::Status::Ok;

	const Gdiplus::Rect bmpRect(0, 0, this->width, this->height);
	Gdiplus::Status status = m_pGdipBmp->LockBits(&bmpRect,
		Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite,
		m_gdipFmt, &m_gdipBmpData);
	if (status == Gdiplus::Status::Ok) {
		m_isLocked = true;
	}
	return status;
}

/**
 * Unlock the GDI+ bitmap.
 *
 * WARNING: This *may* invalidate pointers
 * previously returned by data().
 *
 * @return Gdiplus::Status
 */
Gdiplus::Status RpGdiplusBackend::unlock(void)
{
	// TODO: Recursive locks?
	if (!m_isLocked)
		return Gdiplus::Status::Ok;

	Gdiplus::Status status = m_pGdipBmp->UnlockBits(&m_gdipBmpData);
	if (status == Gdiplus::Status::Ok) {
		m_isLocked = false;
	}
	return status;
}

/**
 * Duplicate the GDI+ bitmap.
 *
 * This function is intended to be used when drawing
 * GDI+ bitmaps directly to a window. As such, it will
 * automatically convert images to 32-bit ARGB in order
 * to avoid CI8 alpha transparency artifacting.
 *
 * WARNING: This *may* invalidate pointers
 * previously returned by data().
 *
 * @return Duplicated GDI+ bitmap.
 */
Gdiplus::Bitmap *RpGdiplusBackend::dup_ARGB32(void) const
{
	Gdiplus::Bitmap *pBmp;
	Gdiplus::Status status;

	status = const_cast<RpGdiplusBackend*>(this)->unlock();
	if (status != Gdiplus::Status::Ok) {
		return nullptr;
	}

	if (this->format == rp_image::FORMAT_CI8) {
		// Copy the local palette to the GDI+ image.
		m_pGdipBmp->SetPalette(m_pGdipPalette);
	}

	pBmp = m_pGdipBmp->Clone(0, 0, this->width, this->height, PixelFormat32bppARGB);
	status = const_cast<RpGdiplusBackend*>(this)->lock();
	if (status != Gdiplus::Status::Ok) {
		delete pBmp;
		return nullptr;
	}

	return pBmp;
}

/**
 * Convert the GDI+ image to HBITMAP.
 * Caller must delete the HBITMAP.
 *
 * WARNING: This *may* invalidate pointers
 * previously returned by data().
 *
 * @param bgColor Background color for images with alpha transparency. (ARGB32 format)
 * @return HBITMAP, or nullptr on error.
 */
HBITMAP RpGdiplusBackend::toHBITMAP(Gdiplus::ARGB bgColor)
{
	// TODO: Check for errors?

	// Temporarily unlock the GDI+ bitmap.
	unlock();

	unique_ptr<Gdiplus::Bitmap> pTmpBmp;
	if (this->format == rp_image::FORMAT_CI8) {
		// Copy the local palette to the GDI+ image.
		m_pGdipBmp->SetPalette(m_pGdipPalette);
		// TODO: Optimize has_translucent_palette_entries().
		if (this->tr_idx < 0 || this->has_translucent_palette_entries()) {
			// Need to convert to ARGB32 first.
			// Otherwise, the translucent entries won't show up correctly.
			// Example: SSBM GCN save icon has color fringing on Windows 7.
			// (...but not Windows XP)
			pTmpBmp.reset(this->dup_ARGB32());
			if (!pTmpBmp) {
				// Error converting to ARGB32.
				return nullptr;
			}
		}
	}

	// TODO: Specify a background color?
	HBITMAP hBitmap;
	Gdiplus::Status status;
	if (pTmpBmp) {
		// Use the temporary ARGB32 bitmap.
		status = pTmpBmp->GetHBITMAP(Gdiplus::Color(bgColor), &hBitmap);
	} else {
		// Use the regular bitmap.
		status = m_pGdipBmp->GetHBITMAP(Gdiplus::Color(bgColor), &hBitmap);
	}

	if (status != Gdiplus::Status::Ok) {
		// Error converting to HBITMAP.
		hBitmap = nullptr;
	}

	// Re-lock the bitmap.
	lock();
	return hBitmap;
}

/**
 * Convert the GDI+ image to HBITMAP.
 * Caller must delete the HBITMAP.
 *
 * This version preserves the alpha channel.
 *
 * WARNING: This *may* invalidate pointers
 * previously returned by data().
 *
 * @param forceARGB32	[in,opt] Force CI8 to ARGB32 conversion.
 * @return HBITMAP, or nullptr on error.
 */
HBITMAP RpGdiplusBackend::toHBITMAP_alpha(bool forceARGB32)
{
	// Convert to HBITMAP.
	HBITMAP hBitmap = nullptr;
	switch (this->format) {
		case rp_image::FORMAT_ARGB32:
			hBitmap = convBmpData_ARGB32(&m_gdipBmpData);
			break;

		case rp_image::FORMAT_CI8:
			// Color conversion may be needed if the image
			// has alpha transparency.
			if (forceARGB32 || this->tr_idx < 0 || this->has_translucent_palette_entries()) {
				// Translucent palette entries.
				// Color conversion is required.
				// NOTE: toHBITMAP_alpha_int() copies the CI8 palette,
				// so we don't need to do that here.
				static const SIZE size = {0, 0};
				hBitmap = toHBITMAP_alpha_int(size, false, forceARGB32);
			} else {
				// No translucent palette entries.
				m_pGdipBmp->SetPalette(m_pGdipPalette);
				hBitmap = convBmpData_CI8(&m_gdipBmpData);
			}
			break;

		default:
			assert(!"Unsupported rp_image::Format.");
			break;
	}

	return hBitmap;
}

/**
 * Convert the GDI+ image to HBITMAP.
 * Caller must delete the HBITMAP.
 *
 * This version preserves the alpha channel.
 *
 * WARNING: This *may* invalidate pointers
 * previously returned by data().
 *
 * @param size		[in] Resize the image to this size.
 * @param nearest	[in] If true, use nearest-neighbor scaling.
 * @param forceARGB32	[in,opt] Force CI8 to ARGB32 conversion.
 * @return HBITMAP, or nullptr on error.
 */
HBITMAP RpGdiplusBackend::toHBITMAP_alpha(const SIZE &size, bool nearest, bool forceARGB32)
{
	if (size.cx <= 0 || size.cy <= 0 ||
	    (size.cx == this->width && size.cy == this->height))
	{
		// No resize is required.
		return toHBITMAP_alpha(forceARGB32);
	}

	return toHBITMAP_alpha_int(size, nearest, forceARGB32);
}

/**
 * Convert the GDI+ image to HBITMAP.
 * Caller must delete the HBITMAP.
 *
 * This is an internal function used by both variants
 * of toHBITMAP_alpha().
 *
 * WARNING: This *may* invalidate pointers
 * previously returned by data().
 *
 * @param size		[in] Resize the image to this size.
 * @param nearest	[in] If true, use nearest-neighbor scaling.
 * @param forceARGB32	[in,opt] Force CI8 to ARGB32 conversion.
 * @return HBITMAP, or nullptr on error.
 */
HBITMAP RpGdiplusBackend::toHBITMAP_alpha_int(SIZE size, bool nearest, bool forceARGB32)
{
	// Convert the image to ARGB32 (if necessary) and resize it.
	if (size.cx <= 0 || size.cy <= 0) {
		// No resizing; just color conversion.
		size.cx = this->width;
		size.cy = this->height;
	}

	Gdiplus::Status status = Gdiplus::Status::GenericError;

	unique_ptr<Gdiplus::Bitmap> pTmpBmp;
	if (this->format == rp_image::FORMAT_CI8) {
		// Copy the local palette to the GDI+ image.
		m_pGdipBmp->SetPalette(m_pGdipPalette);
		// TODO: Optimize has_translucent_palette_entries().
		if (forceARGB32 || this->tr_idx < 0 || this->has_translucent_palette_entries()) {
			// Need to convert to ARGB32 first.
			// Otherwise, the translucent entries won't show up correctly.
			pTmpBmp.reset(this->dup_ARGB32());
			if (!pTmpBmp) {
				// Error converting to ARGB32.
				return nullptr;
			}
		}
	}

	// If the source is 32-bit ARGB and isn't being resized,
	// we don't need a temporary image.
	if (size.cx == this->width && size.cy == this->height) {
		if (pTmpBmp) {
			if (pTmpBmp->GetPixelFormat() == PixelFormat32bppARGB) {
				// Use pTmpBmp directly.
				const Gdiplus::Rect bmpTmpRect(0, 0, size.cx, size.cy);
				Gdiplus::BitmapData bmpTmpData;
				status = pTmpBmp->LockBits(&bmpTmpRect, Gdiplus::ImageLockModeRead,
					PixelFormat32bppARGB, &bmpTmpData);
				if (status != Gdiplus::Status::Ok) {
					// Error re-locking the resized GDI+ bitmap.
					return nullptr;
				}

				HBITMAP hBitmap = convBmpData_ARGB32(&bmpTmpData);
				pTmpBmp->UnlockBits(&bmpTmpData);
				return hBitmap;
			}
		} else {
			if (m_pGdipBmp->GetPixelFormat() == PixelFormat32bppARGB) {
				// Use m_pGdipBmp directly.
				return convBmpData_ARGB32(&m_gdipBmpData);
			}
		}
	}

	// Create a new bitmap.

	if (!pTmpBmp) {
		// Temporarily unlock the GDI+ bitmap.
		status = unlock();
		if (status != Gdiplus::Status::Ok) {
			// Error unlocking the GDI+ bitmap.
			return nullptr;
		}
	}

	// NOTE: We're using ARGB32 because GDI+ doesn't
	// handle resizing on CI8 properly.
	unique_ptr<Gdiplus::Bitmap> pResizeBmp(
		new Gdiplus::Bitmap(size.cx, size.cy, PixelFormat32bppARGB));
	Gdiplus::Graphics graphics(pResizeBmp.get());
	if (nearest) {
		// Set nearest-neighbor interpolation.
		// TODO: What's the default?
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
	}
	if (pTmpBmp) {
		// Use the temporary ARGB32 bitmap.
		graphics.DrawImage(pTmpBmp.get(), 0, 0, size.cx, size.cy);
	} else {
		// Use the regular bitmap.
		graphics.DrawImage(m_pGdipBmp, 0, 0, size.cx, size.cy);
	}

	if (!pTmpBmp) {
		// Re-lock the bitmap.
		status = lock();
		if (status != Gdiplus::Status::Ok) {
			// Error re-locking the GDI+ bitmap.
			return nullptr;
		}
	}

	// Lock the resized bitmap.
	const Gdiplus::Rect bmpResizeRect(0, 0, size.cx, size.cy);
	Gdiplus::BitmapData bmpResizeData;
	status = pResizeBmp->LockBits(&bmpResizeRect, Gdiplus::ImageLockModeRead,
		PixelFormat32bppARGB, &bmpResizeData);
	if (status != Gdiplus::Status::Ok) {
		// Error re-locking the resized GDI+ bitmap.
		return nullptr;
	}

	// Convert to HBITMAP.
	HBITMAP hBitmap = convBmpData_ARGB32(&bmpResizeData);

	// We're done here.
	pResizeBmp->UnlockBits(&bmpResizeData);
	return hBitmap;
}

/**
 * Convert a locked ARGB32 GDI+ bitmap to an HBITMAP.
 * Alpha transparency is preserved.
 * @param pBmpData Gdiplus::BitmapData.
 * @return HBITMAP.
 */
HBITMAP RpGdiplusBackend::convBmpData_ARGB32(const Gdiplus::BitmapData *pBmpData)
{
	// Create a bitmap.
	BITMAPINFO bmi;
	BITMAPINFOHEADER *const bmiHeader = &bmi.bmiHeader;

	// Initialize the BITMAPINFOHEADER.
	// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/dd183376%28v=vs.85%29.aspx
	bmiHeader->biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader->biWidth = pBmpData->Width;
	bmiHeader->biHeight = -(int)pBmpData->Height;	// Top-down
	bmiHeader->biPlanes = 1;
	bmiHeader->biBitCount = 32;
	bmiHeader->biCompression = BI_RGB;	// TODO: BI_BITFIELDS?
	bmiHeader->biSizeImage = 0;	// TODO?
	bmiHeader->biXPelsPerMeter = 0;	// TODO
	bmiHeader->biYPelsPerMeter = 0;	// TODO
	bmiHeader->biClrUsed = 0;
	bmiHeader->biClrImportant = 0;

	// Create the bitmap.
	uint8_t *pvBits;
	HBITMAP hBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS,
		reinterpret_cast<void**>(&pvBits), nullptr, 0);
	if (!hBitmap) {
		// Could not create the bitmap.
		return nullptr;
	}

	// Copy the data from the GDI+ bitmap to the HBITMAP directly.
	// FIXME: Do we need to handle special cases for odd widths?
	const uint8_t *gdip_px = reinterpret_cast<const uint8_t*>(pBmpData->Scan0);
	const size_t active_px_sz = pBmpData->Width * 4;
	for (int y = (int)pBmpData->Height; y > 0; y--) {
		memcpy(pvBits, gdip_px, active_px_sz);
		pvBits += pBmpData->Stride;
		gdip_px += pBmpData->Stride;
	}

	// Bitmap is ready.
	return hBitmap;
}

/**
 * Convert a locked CI8 GDI+ bitmap to an HBITMAP.
 * Alpha transparency is preserved.
 * @param pBmpData Gdiplus::BitmapData.
 * @return HBITMAP.
 */
HBITMAP RpGdiplusBackend::convBmpData_CI8(const Gdiplus::BitmapData *pBmpData)
{
	// BITMAPINFO with 256-color palette.
	const size_t szBmi = sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD)*256);
	BITMAPINFO *bmi = (BITMAPINFO*)malloc(szBmi);
	BITMAPINFOHEADER *bmiHeader = &bmi->bmiHeader;

	// Initialize the BITMAPINFOHEADER.
	// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/dd183376%28v=vs.85%29.aspx
	bmiHeader->biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader->biWidth = pBmpData->Width;
	bmiHeader->biHeight = -(int)pBmpData->Height;	// Top-down
	bmiHeader->biPlanes = 1;
	bmiHeader->biBitCount = 8;
	bmiHeader->biCompression = BI_RGB;
	bmiHeader->biSizeImage = 0;	// TODO?
	bmiHeader->biXPelsPerMeter = 0;	// TODO
	bmiHeader->biYPelsPerMeter = 0;	// TODO
	// FIXME: Specify palette as a parameter?
	bmiHeader->biClrUsed = this->palette_len;
	bmiHeader->biClrImportant = this->palette_len;	// TODO

	// Copy the palette from the image.
	memcpy(bmi->bmiColors, this->palette, this->palette_len * sizeof(RGBQUAD));

	// Create the bitmap.
	uint8_t *pvBits;
	HBITMAP hBitmap = CreateDIBSection(nullptr, bmi, DIB_RGB_COLORS,
		reinterpret_cast<void**>(&pvBits), nullptr, 0);
	free(bmi);
	if (!hBitmap) {
		// Could not create the bitmap.
		return nullptr;
	}

	// Copy the data from the GDI+ bitmap to the HBITMAP directly.
	// FIXME: Do we need to handle special cases for odd widths?
	const uint8_t *gdip_px = reinterpret_cast<const uint8_t*>(pBmpData->Scan0);
	const size_t active_px_sz = pBmpData->Width;
	for (int y = (int)pBmpData->Height; y > 0; y--) {
		memcpy(pvBits, gdip_px, active_px_sz);
		pvBits += pBmpData->Stride;
		gdip_px += pBmpData->Stride;
	}

	// Bitmap is ready.
	return hBitmap;
}

}
