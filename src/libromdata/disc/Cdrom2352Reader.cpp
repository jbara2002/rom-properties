/***************************************************************************
 * ROM Properties Page shell extension. (libromdata)                       *
 * Cdrom2352Reader.hpp: CD-ROM reader for 2352-byte sector images.         *
 *                                                                         *
 * Copyright (c) 2016-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

/**
 * References:
 * - https://github.com/qeedquan/ecm/blob/master/format.txt
 * - https://github.com/Karlson2k/libcdio-k2k/blob/master/include/cdio/sector.h
 */

#include "stdafx.h"
#include "Cdrom2352Reader.hpp"
#include "librpbase/disc/SparseDiscReader_p.hpp"
#include "../cdrom_structs.h"

// librpbase, librpfile
using namespace LibRpBase;
using LibRpFile::IRpFile;

namespace LibRomData {

class Cdrom2352ReaderPrivate : public SparseDiscReaderPrivate {
	public:
		explicit Cdrom2352ReaderPrivate(Cdrom2352Reader *q, unsigned int physBlockSize = 2352);

	private:
		typedef SparseDiscReaderPrivate super;
		RP_DISABLE_COPY(Cdrom2352ReaderPrivate)

	public:
		// CD-ROM sync magic.
		static const uint8_t CDROM_2352_MAGIC[12];

		// Physical block size.
		// Supported block sizes: 2352 (raw), 2448 (raw+subchan)
		unsigned int physBlockSize;

		// Number of 2352-byte blocks.
		unsigned int blockCount;
};

/** Cdrom2352ReaderPrivate **/

// CD-ROM sync magic magic.
const uint8_t Cdrom2352ReaderPrivate::CDROM_2352_MAGIC[12] =
	{0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00};

Cdrom2352ReaderPrivate::Cdrom2352ReaderPrivate(Cdrom2352Reader *q, unsigned int physBlockSize)
	: super(q)
	, physBlockSize(physBlockSize)
	, blockCount(0)
{ }

/** Cdrom2352Reader **/

Cdrom2352Reader::Cdrom2352Reader(IRpFile *file)
	: super(new Cdrom2352ReaderPrivate(this, 2352), file)
{
	init();
}

Cdrom2352Reader::Cdrom2352Reader(IRpFile *file, unsigned int physBlockSize)
	: super(new Cdrom2352ReaderPrivate(this, physBlockSize), file)
{
	init();
}

/**
 * Common initialization function.
 */
void Cdrom2352Reader::init(void)
{
	if (!m_file) {
		// File could not be ref()'d.
		return;
	}

	RP_D(Cdrom2352Reader);

	// Check the disc size.
	// Should be a multiple of the physical block size.
	const off64_t fileSize = m_file->size();
	if (fileSize <= 0 || fileSize % d->physBlockSize != 0) {
		// Invalid disc size.
		UNREF_AND_NULL_NOCHK(m_file);
		m_lastError = EIO;
		return;
	}

	// Disc parameters.
	// NOTE: A 32-bit block count allows for ~8 TiB with 2048-byte sectors.
	d->blockCount = static_cast<unsigned int>(fileSize / 2352LL);
	d->block_size = 2048U;
	d->disc_size = fileSize / (off64_t)d->physBlockSize * 2048LL;

	// Reset the disc position.
	d->pos = 0;
}

/**
 * Is a disc image supported by this class?
 * @param pHeader Disc image header.
 * @param szHeader Size of header.
 * @return Class-specific disc format ID (>= 0) if supported; -1 if not.
 */
int Cdrom2352Reader::isDiscSupported_static(const uint8_t *pHeader, size_t szHeader)
{
	if (szHeader < 2352) {
		// Not enough data to check.
		return -1;
	}

	// Check the CD-ROM sync magic.
	if (!memcmp(pHeader, Cdrom2352ReaderPrivate::CDROM_2352_MAGIC,
	     sizeof(Cdrom2352ReaderPrivate::CDROM_2352_MAGIC)))
	{
		// Valid CD-ROM sync magic.
		return 0;
	}

	// Not supported.
	return -1;
}

/**
 * Is a disc image supported by this object?
 * @param pHeader Disc image header.
 * @param szHeader Size of header.
 * @return Class-specific system ID (>= 0) if supported; -1 if not.
 */
int Cdrom2352Reader::isDiscSupported(const uint8_t *pHeader, size_t szHeader) const
{
	return isDiscSupported_static(pHeader, szHeader);
}

/** SparseDiscReader functions. **/

/**
 * Get the physical address of the specified logical block index.
 *
 * @param blockIdx	[in] Block index.
 * @return Physical address. (0 == empty block; -1 == invalid block index)
 */
off64_t Cdrom2352Reader::getPhysBlockAddr(uint32_t blockIdx) const
{
	// NOTE: This function should NOT be used.
	// Use the readBlock() function instead.
	RP_UNUSED(blockIdx);
	assert(!"Cdrom2352Reader::getPhysBlockAddr() should not be used!");
	return -1;
}

/**
 * Read the specified block.
 *
 * This can read either a full block or a partial block.
 * For a full block, set pos = 0 and size = block_size.
 *
 * @param blockIdx	[in] Block index.
 * @param pos		[in] Starting position. (Must be >= 0 and <= the block size!)
 * @param ptr		[out] Output data buffer.
 * @param size		[in] Amount of data to read, in bytes. (Must be <= the block size!)
 * @return Number of bytes read, or -1 if the block index is invalid.
 */
int Cdrom2352Reader::readBlock(uint32_t blockIdx, int pos, void *ptr, size_t size)
{
	// Read 'size' bytes of block 'blockIdx', starting at 'pos'.
	// NOTE: This can only be called by SparseDiscReader,
	// so the main assertions are already checked there.
	RP_D(Cdrom2352Reader);
	assert(pos >= 0 && pos < (int)d->block_size);
	assert(size <= d->block_size);
	// TODO: Make sure overflow doesn't occur.
	assert(static_cast<off64_t>(pos + size) <= static_cast<off64_t>(d->block_size));
	if (pos < 0 || static_cast<off64_t>(pos + size) > static_cast<off64_t>(d->block_size)) {
		// pos+size is out of range.
		return -1;
	}

	if (unlikely(size == 0)) {
		// Nothing to read.
		return 0;
	}

	// Get the physical address first.
	const off64_t physBlockAddr = static_cast<off64_t>(blockIdx) * d->physBlockSize;

	// Read from the block.
	// NOTE: We need to read the entire 2352-byte block in order to
	// determine the data offset, since Mode 1 and Mode 2 XA have different
	// sector layouts.
	// NOTE 2: No changes neeed for 2448-byte mode, since subchannels are
	// stored *after* the 2352-byte sector data.
	CDROM_2352_Sector_t sector;
	size_t sz_read = m_file->seekAndRead(physBlockAddr, &sector, sizeof(sector));
	m_lastError = m_file->lastError();
	if (sz_read != sizeof(sector)) {
		// Read error.
		return -1;
	}

	// NOTE: Sector user data area position depends on the sector mode.
	const uint8_t *const data = cdromSectorDataPtr(&sector);
	memcpy(ptr, &data[pos], size);
	return static_cast<int>(size);
}

}
