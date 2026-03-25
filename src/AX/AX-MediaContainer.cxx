//
//  AX-MediaContainer.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 24/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#include "AX-MediaContainer.h"
#include "cinder/app/App.h"
#include "cinder/Utilities.h"
#include "asio/asio.hpp"

#include <AX/MP4/AX-MP4.h>
#include <AX/MKV/AX-MKV.h>
#include <AX/AVI/AX-AVI.h>

using namespace ci;

namespace AX::Media
{
	const char * ErrorCodeToString ( ErrorCode code )
	{
		static std::unordered_map<ErrorCode, const char *> kTable =
		{
			{ ErrorCode::Success, "Success" },
			{ ErrorCode::InvalidHeader, "Invalid Header" },
			{ ErrorCode::Unknown, "Unknown" },
		};

		auto it = kTable.find ( code );
		if ( it != kTable.end ( ) )
		{
			return it->second;
		} else
		{
			static const char * kNull = "Unknown Error";
			return kNull;
		}
	}

	Container::Container ( ContainerType type, const DataSourceRef & source, const Format & format )
		: _source ( source )
		, _format ( format )
		, _type ( type )
	{ }

	ContainerRef Container::Create ( const fs::path & path, const Format & format )
	{
		if ( !fs::exists ( path ) ) return nullptr;
		return Container::Create ( loadFile ( path ), format );
	}

	ContainerRef Container::Create ( const DataSourceRef & source, const Format & format )
	{
		if ( MP4::MP4::CanProcessSource ( source ) )
		{
			return MP4::MP4::Create ( source, format );
		}

		#if AX_MEDIA_WITH_MKV
		if ( MKV::MKV::CanProcessSource ( source ) )
		{
			return MKV::MKV::Create ( source, format );
		}
		#endif

		#if AX_MEDIA_WITH_AVI
		if ( AVI::AVI::CanProcessSource ( source ) )
		{
			return AVI::AVI::Create ( source, format );
		}
		#endif

		return nullptr;
	}

	//
	// Movie
	//

	MovieRef Movie::Create ( const ContainerRef & container )
	{
		switch ( container->Type ( ) )
		{
			#ifdef AX_MEDIA_WITH_MKV
			case ContainerType::MKV:
			{
				return MKV::MKVMovieRef ( new MKV::MKVMovie ( container ) );
			}
			#endif
			#ifdef AX_MEDIA_WITH_AVI
			case ContainerType::AVI:
			{
				return AVI::AVIMovieRef ( new AVI::AVIMovie ( container ) );
			}
			#endif
			case ContainerType::MP4 :
			default:
			{
				return MP4::MP4MovieRef ( new MP4::MP4Movie ( container ) );
			}
		}
	}

	Movie::Movie ( const ContainerRef & container )
		: _container ( container )
	{ }

	std::vector<TrackRef> Movie::FindTracks ( TrackType type ) const
	{
		std::vector<TrackRef> result;
		for ( auto & track : _tracks )
		{
			if ( track->Type ( ) == type ) result.push_back ( track );
		}
		return result;
	}

	TrackRef Movie::GetTrack ( TrackType type, u32 index ) const
	{
		auto tracks = FindTracks ( type );
		if ( index < tracks.size ( ) ) return tracks[index];
		return nullptr;
	}

	///
	/// Track
	/// 
	
	Track::AsyncContext::AsyncContext ( )
		: Work ( asio::make_work_guard ( Io ) )
	{
		Thread = std::thread ( [&]
		{
			setThreadName ( "AX::MP4::TrackDecoder" );
			Io.run ( );
		} );
	}

	Track::AsyncContext::~AsyncContext ( )
	{
		try
		{
			Io.stop ( );
		} catch ( const std::exception & e )
		{
			std::printf ( "Error stopping AsyncContext: %s\n", e.what ( ) );
		}

		if ( Thread.joinable ( ) ) Thread.join ( );
	}

	ITrackDecoderRef Track::CreateDecoder ( u32 handler ) const
	{
		auto it = _decoders.find ( handler );
		if ( it != _decoders.end ( ) )
		{
			return it->second ( );
		}
		return nullptr;
	}

	ITrackDecoderRef Track::DecodeSample ( u32 index ) const
	{
		AX::Media::Sample sample{};
		if ( ReadSample ( index, sample ) )
		{
			if ( auto decoder = CreateDecoder ( sample.Handler ( ) ) )
			{
				auto success = decoder->Decode ( sample );
				decoder->_succeeded = success;
				return decoder;
			}
		}

		return nullptr;
	}

	void Track::DecodeSampleAsync ( u32 index, AsyncDecodeCallback callback ) const
	{
		// @TODO(andrew): Atomic semaphore that caps the amount of frames
		// that can be queued at once, semaphore++ when queued, semaphore-- when
		// firing the callback

		if ( !_async ) _async = std::make_unique<AsyncContext> ( );

		asio::post ( _async->Io, [&, index, callback]
		{
			if ( !_async.get ( ) ) return;

			Sample sample{};
			bool success = false;
			ITrackDecoderRef decoder = nullptr;
			if ( ReadSample ( index, sample ) )
			{
				decoder = DecodeSample ( index );
				success = decoder != nullptr;
			}

			app::App::get ( )->dispatchAsync ( [s = success, i = index, cb = callback, dec = decoder]
			{
				cb ( i, s, dec );
			} );
		} );
	}

	void Track::ReadSampleAsync ( u32 index, AsyncReadCallback callback ) const
	{
		if ( !_async ) _async = std::make_unique<AsyncContext> ( );
		asio::post ( _async->Io, [&, index, callback]
		{
			Sample sample{};
			bool success = ReadSample ( index, sample );
			app::App::get ( )->dispatchAsync ( [s = success, i = index, cb = callback, sam = std::move ( sample )]
			{
				cb ( i, s, std::move ( sam ) );
			} );
		} );
	}

	Track::~Track ( )
	{
		_async = nullptr;
		_decoders.clear ( );
	}
}