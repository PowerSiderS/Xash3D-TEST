// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2024-2026 Alibek Omarov

#include "fileimage.h"
#include "vgui_internal.h"

bool Load32BitTGA( FileImageStream *fp, FileImage *image )
{
	TGAFileHeader hdr;

	image->Term();

	fp->Read( &hdr, sizeof( hdr ));
	if( fp->ErrorStatus() ||
		( hdr.m_PixelDepth != 24 && hdr.m_PixelDepth != 32 ) ||
		( hdr.m_ImageType != 2 && hdr.m_ImageType != 10 ))
		return false;

	for( int i = 0; i < hdr.m_IDLength; i++ )
	{
		unsigned char data;
		fp->Read( &data, sizeof( data ));
	}

	if( fp->ErrorStatus())
		return false;

	image->m_Width = hdr.m_Width;
	image->m_Height = hdr.m_Height;
	image->m_pData = new unsigned char[image->m_Width * image->m_Height * 4];
	if( !image->m_pData )
		return false;

	const int bytesPerPixel = hdr.m_PixelDepth / 8;

	for( int y = image->m_Height - 1; y >= 0; y-- )
	{
		uint8_t *line = &image->m_pData[y * image->m_Width * 4];
		int curOut = 0;
		while( curOut < image->m_Width )
		{
			int runLength = 1;
			bool runPacket = false;

			if( hdr.m_ImageType == 10 )
			{
				uint8_t packetHeader;
				fp->Read( &packetHeader, sizeof( packetHeader ));

				if( fp->ErrorStatus())
					return false;

				runLength = (int)( packetHeader & 0x7f ) + 1;
				runPacket = ( packetHeader & 0x80 ) != 0;
			}

			if( curOut + runLength > image->m_Width )
				return false;

			uint8_t pixel[4] = { 0, 0, 0, 255 };
			for( int x = 0; x < runLength; x++ )
			{
				// RLE packets contain one pixel; raw packets contain one pixel
				// per iteration. TGA stores pixels as BGR(A), while freevgui
				// uploads RGBA textures.
				if( !runPacket || x == 0 )
				{
					fp->Read( pixel, bytesPerPixel );
					if( fp->ErrorStatus())
						return false;
				}

				line[0] = pixel[2];
				line[1] = pixel[1];
				line[2] = pixel[0];
				line[3] = pixel[3];
				line += 4;
			}

			curOut += runLength;
		}
	}

	return true;
}

static void WriteRun( unsigned char *pColor, FILE *fp, int runLength )
{
	uint8_t runCount = ( runLength - 1 ) | 0x80;

	fwrite( &runCount, 1, 1, fp );
	fwrite( pColor, 1, 4, fp );
}

void Save32BitTGA( FileHandle_t fp, FileImage *image )
{
	TGAFileHeader hdr = {};

	hdr.m_PixelDepth = 32;
	hdr.m_ImageType = 10;
	hdr.m_Width = image->m_Width;
	hdr.m_Height = image->m_Height;

	fwrite( &hdr, 1, sizeof( hdr ), (FILE *)fp );

	for( int y = image->m_Height - 1; y >= 0; y-- )
	{
		uint8_t *line = &image->m_pData[y * image->m_Width * 4];
		int runStart = 0, x;

		for( x = 0; x < image->m_Width; x++ )
		{
			if(( x - runStart ) >= 128 || *(( uint32_t* )&line[runStart * 4] ) != *(( uint32_t* )&line[x * 4] ))
			{
				WriteRun( &line[runStart * 4], (FILE *)fp, x - runStart );
				runStart = x;
			}
		}

		if( x - runStart > 0 )
		{
			WriteRun( &line[runStart * 4], (FILE *)fp, x - runStart );
		}
	}
}
