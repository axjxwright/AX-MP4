//
//  AX-AVI.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 26/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#ifdef AX_MEDIA_WITH_AVI

#include "AX-AVI.h"
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cassert>

using namespace ci;

namespace AX::Media::AVI
{
	namespace
	{
		#define AX_INDENT_SIZE 2
		#define AX_INDENT(x) ("+" + std::string ( x * AX_INDENT_SIZE, '-' ) + " ").c_str()
		#define AX_CURRENT_INDENT AX_INDENT ( context.StackDepth() )
		#define AX_CURRENT_INDENT_PLUS(x) AX_INDENT ( ( context.StackDepth() + x ) )

		// @fixme(andrew): Proper std::bit_cast polyfill for < c++20
		template <typename To, typename From>
		typename std::enable_if<sizeof ( To ) == sizeof ( From ) && std::is_trivially_copyable<From>::value && std::is_trivially_copyable<To>::value, To>::type
		bit_cast ( const From & src ) noexcept
		{
			static_assert( std::is_trivially_constructible<To>::value, "Destination type must be trivially constructible in this polyfill implementation" );

			To dst{};
			std::memcpy ( &dst, &src, sizeof ( To ) );
			return dst;
		}
	}

	bool Element::Parse ( AVI & context, const ci::IStreamRef & stream )
	{
		u32 fileSize{ 0 };
		stream->read ( &fileSize );

		u32 fileType{ 0 };
		stream->readBig ( &fileType );

		std::printf ( "%s %lu %s\n", TagToString ( _tag ).c_str ( ), fileSize, AX::FourCCToString ( fileType ).c_str ( ) );

		stream->seekRelative ( fileSize );

		return true;
	}

	AVIRef AVI::Create ( const ci::fs::path & path, const Format & format )
	{
		if ( !fs::exists ( path ) ) return nullptr;
		return AVI::Create ( loadFile ( path ), format );
	}
	
	AVIRef AVI::Create ( const ci::DataSourceRef & source, const Format & format )
	{
		if ( auto mkv = AVIRef ( new AVI ( source, format ) ) )
		{
			mkv->Load ( );
			return mkv;
		}
		return nullptr;
	}

	static void DumpElement ( const Element * element, std::ostream & stream, bool verbose, int indent = 0 )
	{
		stream << AX_INDENT ( indent ) << element->ToString ( );
		if ( verbose )
		{
			u32 ctr{ 0 };
			auto size = element->Properties ( ).size ( );
			if ( size > 0 )
			{
				stream << " [";
				for ( auto & [name, value] : element->Properties ( ) )
				{
					stream << name << "=" << value;
					if ( ctr++ < size - 1 ) stream << ", ";
				}
				stream << "]";
			}
		}
		stream << "\n";

		/*if ( element->Identifier ( ).Type == MatroskaDataType::kMaster )
		{
			auto * container = static_cast<const MasterElement *> ( element );
			for ( auto & child : container->GetChildren ( ) )
			{
				DumpElement ( child.get ( ), stream, verbose, indent + 1 );
			}
		}*/
	}

	void AVI::Dump ( std::ostream & stream, bool verbose ) const
	{
		DumpElement ( this, stream, verbose );
	}

	bool AVI::CanProcessSource ( const ci::DataSourceRef & source )
	{
		auto mkv = AVIRef ( new AVI ( source, AX::Media::Format{} ) );
		return mkv->StartsWithRIFF ( source->createStream ( ) );
	}

	AVI::AVI ( const ci::DataSourceRef & source, const Format & format )
		: Container ( ContainerType::AVI, source, format )
		, Element ( Tag::kUnknown )
		
	{
		
	}

	bool AVI::Load ( )
	{
		try
		{
			IStreamRef stream = _source->createStream ( );
			if ( Settings ( ).PreloadsIntoMemory ( ) )
			{
				_buffer = _source->getBuffer ( );
				stream = IStreamMem::create ( _buffer->getData ( ), _buffer->getSize ( ) );
			}

			u64 streamSize = stream->size ( );
			_length = streamSize;

			if ( !StartsWithRIFF ( stream ) )
			{
				_error = ErrorCode::InvalidHeader;
				return _isValid;
			}

			Parse ( *this, stream );
			assert ( _stack.empty ( ) && "AVI Element stack underflow" );

			_isValid = true;
		}catch ( const std::exception & e )
		{
			std::printf ( "Error: %s\n", e.what ( ) );
			_error = ErrorCode::Unknown;
		}

		return _isValid;
	}

	ElementRef AVI::ReadNextElement ( const IStreamRef & stream )
	{
		Tag tag{};
		stream->readBig<u32> ( (u32 *)&tag );

		return std::make_shared<Element> ( tag );
	}

	bool AVI::Parse ( AVI & context, const ci::IStreamRef & stream )
	{
		stream->seekAbsolute ( 0 );
		while ( stream->tell ( ) < stream->size ( ) )
		{
			if ( auto element = ReadNextElement ( stream ) )
			{
				element->Parse ( context, stream );
			}
		}
		return true;
	}

	bool AVI::StartsWithRIFF ( const ci::IStreamRef & stream )
	{
		bool success = false;
		if ( auto element = ReadNextElement ( stream ) )
		{
			if ( element->Identifier ( ) == Tag::kRIFF )
			{
				return success = true;
			}
		}
		
		stream->seekAbsolute ( 0 );
		return success;
	}

	void AVI::Push ( Element * element )
	{
		_stack.push ( element );
	}

	void AVI::Pop ( )
	{
		_stack.pop ( );
	}

	Element * AVI::Top ( ) const
	{
		if ( _stack.empty ( ) ) return nullptr;
		return _stack.top ( );
	}

	AVIMovie::AVIMovie ( const ContainerRef & container )
		: Movie ( container )
	{
		auto * avi = dynamic_cast<AVI *>( _container.get ( ) );
		if ( avi )
		{
			
		}
	}

	AVITrack::AVITrack ( )
		: Track ( TrackType::kUnknown )
	{
		
	}

	bool AVITrack::ReadSample ( u32 index, Sample & sample ) const
	{
		return false;
	}
}

#endif