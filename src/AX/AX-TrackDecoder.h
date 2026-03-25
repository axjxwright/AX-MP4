//
//  AX-TrackDecoder.h
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 24/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#pragma once

#include <AX/AX-Types.h>

#include "cinder/gl/gl.h"
#include "cinder/Surface.h"

#include <cstdint>
#include <variant>
#include <ostream>
#include <vector>

namespace AX
{
	namespace Media
	{
		class Sample
		{
		public:
			Sample ( ) { };
			Sample ( u32 handler, const u8 * data, u32 length, u32 index, float time, const ivec2 & size, u8 depth )
				: _handler ( handler )
				, _data ( data )
				, _length ( length )
				, _ownsData ( false )
				, _index ( index )
				, _time ( time )
				, _size ( size )
				, _depth ( depth )
			{ }
			Sample ( u32 handler, const std::vector<u8> & data, u32 index, float time, const ivec2 & size, u8 depth )
				: _handler ( handler )
				, _data ( data )
				, _length ( static_cast<u32>( data.size ( ) ) )
				, _ownsData ( true )
				, _index ( index )
				, _time ( time )
				, _size ( size )
				, _depth ( depth )
			{ }

			u32				Handler ( ) const { return _handler; }
			const u8 *		Data ( ) const { return _ownsData ? std::get<std::vector<u8>> ( _data ).data ( ) : std::get<const u8 *> ( _data ); }
			u32				Length ( ) const { return _length; }
			u32				Index ( ) const { return _index; }

			const ivec2 &	Dimensions ( ) const { return _size; }
			u32				Width ( ) const { return _size.x; }
			u32				Height ( ) const { return _size.y; }
			float			Timestamp ( ) const { return _time; }
			u8				BitDepth ( ) const { return _depth; }

		protected:

			using DataUnion = std::variant<std::vector<u8>, const u8 *>;

			int             _handler{ 0 };
			u32             _length{ 0 };
			bool            _ownsData{ false };
			u32             _index{ 0 };
			float			_time{ 0.0f };
			ivec2			_size;
			u8				_depth{ 0 };
			DataUnion       _data;
		};

		using ITrackDecoderRef = std::shared_ptr<class ITrackDecoder>;
		class ITrackDecoder : public Castable
		{
		public:
			using RawBufferRef = std::shared_ptr<std::vector<u8>>;
			struct DecodedFrame
			{
				u32                 Index{ 0 };
				u32                 GPUFormat{ GL_RGBA };
				bool                IsCompressed{ false };
				u32                 Width{ 0 };
				u32                 Height{ 0 };
				float               DecodeTime{ 0.0f };
				float				Timestamp{ 0.0f };
				u32                 Channels{ 0 };

				u64                 PixelBufferSize{ 0 };
				const u8 *			PixelBuffer{ nullptr };

				void                SetPixelData ( const ci::Surface8uRef & surface );
				void                SetPixelData ( const ci::Channel8uRef & channel );
				void                SetPixelData ( const RawBufferRef & raw );

			protected:

				std::variant<ci::Surface8uRef, ci::Channel8uRef, RawBufferRef> _pixelData;
			};

			ITrackDecoder ( u32 handler ) : _handler ( handler ) { }
			virtual ~ITrackDecoder ( ) { };

			u32                         Handler ( ) const { return _handler; }
			virtual u32                 FrameCount ( ) const { return static_cast<u32>( _decodedFrames.size ( ) ); }
			virtual const DecodedFrame & FrameAt ( u32 index ) const { return _decodedFrames.at ( index ); }

			virtual bool                DecodeSucceeded ( ) const { return _succeeded; }
			virtual bool                Decode ( const Sample & sample ) = 0;
			virtual ci::gl::TextureRef  CreateTexture ( u32 index, const ci::gl::Texture::Format & fmt = ci::gl::Texture::Format ( ) ) const = 0;

			// @fixme(andrew): Better way to handle this
			bool                        _succeeded{ false };
		protected:
			std::vector<DecodedFrame>   _decodedFrames;
			u32                         _handler{ 0 };
		};
	}
}