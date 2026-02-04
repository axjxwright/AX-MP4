#pragma once

//
//  Decoders.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 13/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#include <AX/AX-TrackDecoder.h>

class MJPEGDecoder : public AX::Media::ITrackDecoder
{
public:
	using ITrackDecoder::ITrackDecoder;

	bool                Decode ( const AX::Media::Sample& sample ) override;
	ci::gl::TextureRef  CreateTexture ( uint32_t index, const ci::gl::Texture::Format& fmt = ci::gl::Texture::Format() ) const override;
};

class HAPDecoder : public AX::Media::ITrackDecoder
{
public:
	using ITrackDecoder::ITrackDecoder;

	bool                Decode ( const AX::Media::Sample& sample ) override;
	ci::gl::TextureRef  CreateTexture ( uint32_t index, const ci::gl::Texture::Format& fmt = ci::gl::Texture::Format ( ) ) const override;
};