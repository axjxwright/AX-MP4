#pragma once

//
//  Decoders.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 13/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#include <AX/AX-MP4.h>

class MJPEGDecoder : public AX::ITrackDecoder
{
public:
	using ITrackDecoder::ITrackDecoder;

	bool                Decode ( const AX::Sample& sample ) override;
	ci::gl::TextureRef  CreateTexture ( AX::u32 index, const ci::gl::Texture::Format& fmt = ci::gl::Texture::Format() ) const override;
};

class HAPDecoder : public AX::ITrackDecoder
{
public:
	using ITrackDecoder::ITrackDecoder;

	bool                Decode ( const AX::Sample& sample ) override;
	ci::gl::TextureRef  CreateTexture ( AX::u32 index, const ci::gl::Texture::Format& fmt = ci::gl::Texture::Format ( ) ) const override;
};