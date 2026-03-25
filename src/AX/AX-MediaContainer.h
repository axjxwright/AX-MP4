//
//  AX-MediaContainer.h
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 24/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#pragma once

#include <AX/AX-Types.h>
#include <ax/AX-TrackDecoder.h>

#include <cstdint>
#include <variant>
#include <ostream>
#include <vector>
#include <functional>
#include <thread>
#include <asio/asio.hpp>

namespace AX
{
	namespace Media
	{
		struct Format
		{
			Format & TrackProperties ( bool track ) { _trackProperties = track; return *this; }
			bool     TracksProperties ( ) const { return _trackProperties; };

			Format & PreloadIntoMemory ( bool preload ) { _preloadIntoMemory = preload; return *this; }
			bool     PreloadsIntoMemory ( ) const { return _preloadIntoMemory; };

		protected:
			bool    _trackProperties{ false };
			bool    _preloadIntoMemory{ false };
		};
		
		enum class TrackType
		{
			kUnknown,
			kAudio,
			kVideo,
			kHint,
			kJPEG,
			kSubtitles
		};


		enum class ErrorCode
		{
			Success,
			InvalidHeader, // Doesn't start with FTYP/EBML/RIFF depending on format
			Unknown
		};

		const char * ErrorCodeToString ( ErrorCode code );

		enum class ContainerType
		{
			MP4,
			#ifdef AX_MEDIA_WITH_MKV
			MKV,
			#endif
			#ifdef AX_MEDIA_WITH_AVI
			AVI,
			#endif
		};

		using ContainerRef = std::shared_ptr<class Container>;
		class Container : public AX::Castable
		{
		public:
			static ContainerRef Create ( const ci::fs::path & path, const Format & format );
			static ContainerRef Create ( const ci::DataSourceRef & source, const Format & format );

			virtual void		Dump ( std::ostream &, bool verbose = false ) const { (void)verbose; };
			
			ContainerType		Type ( ) const { return _type; }

			virtual u32			StackDepth ( ) const { return 0; }
			virtual bool        IsValid ( ) const { return _isValid; }
			virtual ErrorCode	Error ( ) const { return _error; }
			const	Format &	Settings ( ) const { return _format; }
			usz					Length ( ) const { return _length; }

			virtual operator AX::IInspectable * ( ) = 0;
		protected:

			Container			( ContainerType type, const ci::DataSourceRef & source, const Format & format );

			ErrorCode           _error{ ErrorCode::Unknown };
			bool                _isValid{ false };
			Format              _format;
			ci::DataSourceRef   _source;
			ContainerType		_type{};
			usz					_length{ 0 };

		};

		using TrackRef = std::shared_ptr<class Track>;
		class Track				: public Castable
		{
		public:
			Track				( TrackType type ) : _type ( type ) { }
			~Track				( );

			using               AsyncReadCallback = std::function<void ( u32, bool, const Sample && )>;
			using               AsyncDecodeCallback = std::function<void ( u32, bool, const ITrackDecoderRef & )>;

			virtual bool		ReadSample ( u32 index, Sample & sample ) const = 0;

			ITrackDecoderRef    DecodeSample ( u32 index ) const;
			void                DecodeSampleAsync ( u32 index, AsyncDecodeCallback callback ) const;
			void                ReadSampleAsync ( u32 index, AsyncReadCallback callback ) const;

			template <typename T>
			void                RegisterDecoder ( u32 handler )
			{
				_decoders[handler] = [=] { return std::make_shared<T> ( handler ); };
			}

			ITrackDecoderRef    CreateDecoder ( u32 handler ) const;
			bool                HasDecoder ( u32 handler ) const { return _decoders.count ( handler ); }
			
			TrackType			Type ( ) const { return _type; };

			const glm::ivec2 &  Size ( ) const { return _size; }
			const int           Width ( ) const { return _size.x; }
			const int           Height ( ) const { return _size.y; }
			u32					CodecId ( ) const { return _codecId; }

			u32					SampleCount ( ) const { return _sampleCount; }
			float				DurationSeconds ( ) const { return _durationSeconds; }

			u32					SampleRate ( ) const { return _sampleRate; }
			u32					ChannelCount ( ) const { return _channelCount; }


		protected:

			using AsioWork = asio::executor_work_guard<asio::io_context::executor_type>;
			class AsyncContext
			{
			public:
				std::thread         Thread;
				asio::io_context    Io;
				AsioWork            Work;

				AsyncContext ( );
				~AsyncContext ( );
			};
			using AsyncContextRef	= std::unique_ptr<class AsyncContext>;
			using DecoderFactory	= std::function<ITrackDecoderRef ( )>;
			using DecoderFactoryMap	= std::unordered_map<u32, DecoderFactory>;

			glm::ivec2              _size;
			mutable AsyncContextRef _async;
			DecoderFactoryMap       _decoders;
			TrackType				_type{ TrackType::kUnknown };

			u32						_sampleCount{ 0 };
			float					_durationSeconds{ 0 };

			u32						_sampleRate{ 0 };
			u32						_channelCount{ 0 };

			u32						_codecId{ 0 };
		};

		using MovieRef = std::shared_ptr<class Movie>;
		class Movie : public Castable
		{
		public:
			static MovieRef                 Create ( const ContainerRef & container );

			const std::vector<TrackRef>&	GetTracks  ( ) const { return _tracks; }
			std::vector<TrackRef>           FindTracks ( TrackType type ) const;
			TrackRef                        GetTrack   ( TrackType type, u32 index = 0 ) const;

		protected:

			Movie ( const ContainerRef & container );

			ContainerRef                    _container;
			std::vector<TrackRef>           _tracks;
		};
	}
}
