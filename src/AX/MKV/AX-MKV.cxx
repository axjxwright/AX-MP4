//
//  AX-MKV.cxx
//  AX-MKV
//
//  Created by Andrew Wright (@axjxwright) on 24/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#ifdef AX_MEDIA_WITH_MKV

#include "AX-MKV.h"
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <cassert>

using namespace ci;

namespace AX::Media::MKV
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
		
		static u64 Pack ( u64 n, uint8_t * b )
		{
			uint64_t v = 0;
			uint64_t k = ( (uint64_t)n - 1 ) * 8;

			for ( int i = 0; i < n; ++i )
			{
				v |= (uint64_t)b[i] << k;
				k -= 8;
			}

			return v;
		}

		static u64 Unpack ( const uint8_t * data, size_t & bytesRead )
		{
			u8 first = data[0];
			u32 length = 0;
			for ( u32 i = 0; i < 8; ++i ) 
			{
				if ( ( first >> ( 7 - i ) ) & 1 ) 
				{
					length = i + 1;
					break;
				}
			}

			u64 value = first & ( 0xFF >> length );

			for ( u32 i = 1; i < length; ++i ) 
			{
				value = ( value << 8 ) | data[i];
			}

			bytesRead = length;
			return value;
		}

		static std::string ReadFixedString ( const IStreamRef & stream, usz & length )
		{
			if ( length == 0 ) return "";

			std::string result;
			stream->readFixedString ( &result, length );
			length = 0;
			return result;
		}

		static u64 ReadUint ( const IStreamRef & stream, usz & length )
		{
			if ( length == 0 ) return 0;

			u64 value = 0;
			for ( int i = 0; i < length; i++ )
			{
				u8 byte{};
				stream->read ( &byte );
				value = ( value << 8 ) | byte;
			}

			length = 0;
			return value;
		}

		static s64 ReadInt ( const IStreamRef & stream, usz & length )
		{
			if ( length == 0 ) return 0;
			s64 value = 0;
			for ( int i = 0; i < length; i++ )
			{
				u8 byte{};
				stream->read ( &byte );
				value = ( value << 8 ) | byte;
			}

			length = 0;
			return value;
		}

		static double ReadFloat ( const IStreamRef & stream, usz & length )
		{
			assert ( length == 4 || length == 8 );

			bool isFloat = length == 4;

			if ( isFloat )
			{
				u32 bits{};
				for ( int i = 0; i < 4; i++ )
				{
					u8 byte{};
					stream->read ( &byte );
					bits = ( bits << 8 ) | byte;
				}
				length = 0;
				return static_cast<double> ( bit_cast<float, u32> ( bits ) );
			} else
			{
				u64 bits{};
				for ( int i = 0; i < 8; i++ )
				{
					u8 byte{};
					stream->read ( &byte );
					bits = ( bits << 8 ) | byte;
				}
				length = 0;
				return bit_cast<double, u64> ( bits );
			}
		}

		static std::pair<usz, u32> ReadElementIDAndWidth ( u32 block )
		{
			u8 * buffer = reinterpret_cast<u8 *>( &block );
			uint8_t b = buffer[0];

			if ( ( ( b & 0x80 ) >> 7 ) == 1 ) // ID Class A (8 bits)
			{
				return { u32 ( b ), 1 };
			}
			if ( ( ( b & 0x40 ) >> 6 ) == 1 ) // ID Class B (16 bits)
			{
				return { static_cast<u32>( Pack ( 2, buffer ) ), 2 };
			}
			if ( ( ( b & 0x20 ) >> 5 ) == 1 ) // ID Class C (24 bits)
			{
				return { static_cast<u32>( Pack ( 3, buffer ) ), 3 };
			}

			if ( ( ( b & 0x10 ) >> 4 ) == 1 ) // ID Class D (32 bits)
			{
				return { static_cast<u32>( Pack ( 4, buffer ) ), 4 };
			}

			return { 0, 0 };
		}

		static std::pair<u64, u64> ReadElementSize ( u64 block )
		{
			u8 * buffer = reinterpret_cast<u8 *>( &block );
			uint8_t b = buffer[0];

			uint8_t mask{};
			uint64_t length{};

			if ( b >= 0x80 )
			{
				length = 1;
				mask = 0x7f;
			} else if ( b >= 0x40 )
			{
				length = 2;
				mask = 0x3f;
			} else if ( b >= 0x20 )
			{
				length = 3;
				mask = 0x1f;
			} else if ( b >= 0x10 )
			{
				length = 4;
				mask = 0x0f;
			} else if ( b >= 0x08 )
			{
				length = 5;
				mask = 0x07;
			} else if ( b >= 0x04 )
			{
				length = 6;
				mask = 0x03;
			} else if ( b >= 0x02 )
			{
				length = 7;
				mask = 0x01;
			} else if ( b >= 0x01 )
			{
				length = 8;
				mask = 0x00;
			} else {
				return { 0, 0 };
			}

			buffer[0] = b & mask;
			return { Pack ( length, buffer ), length };
		}

	}

	MKVRef MKV::Create ( const ci::fs::path & path, const Format & format )
	{
		if ( !fs::exists ( path ) ) return nullptr;
		return MKV::Create ( loadFile ( path ), format );
	}
	
	MKVRef MKV::Create ( const ci::DataSourceRef & source, const Format & format )
	{
		if ( auto mkv = MKVRef ( new MKV ( source, format ) ) )
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

		if ( element->Identifier().Type == MatroskaDataType::kMaster )
		{
			auto * container = static_cast<const MasterElement *> ( element );
			for ( auto & child : container->GetChildren ( ) )
			{
				DumpElement ( child.get ( ), stream, verbose, indent + 1 );
			}
		}
	}

	void MKV::Dump ( std::ostream & stream, bool verbose ) const
	{
		DumpElement ( this, stream, verbose );
	}

	bool MKV::CanProcessSource ( const ci::DataSourceRef & source )
	{
		auto mkv = MKVRef ( new MKV ( source, AX::Media::Format{} ) );
		return mkv->StartsWithEBML ( source->createStream ( ) );
	}

	MKV::MKV ( const ci::DataSourceRef & source, const Format & format )
		: Container ( ContainerType::MKV, source, format )
		, MasterElement ( { MatroskaElementId::kUnknown, MatroskaDataType::kMaster, "Root" } )
		
	{
		RegisterElementFactory ( MatroskaElementId::kTrackEntry, [=]( const MatroskaIdentifier & id ) { return std::make_shared<TrackElement> ( id ); } );
	}

	bool MKV::Load ( )
	{
		try
		{
			IStreamRef stream = _source->createStream ( );
			if ( Settings ( ).PreloadsIntoMemory ( ) )
			{
				_buffer = _source->getBuffer ( );
				stream = IStreamMem::create ( _buffer->getData ( ), _buffer->getSize ( ) );
			}

			stream->seekAbsolute ( 0 );

			u64 streamSize = stream->size ( );
			_length = streamSize;

			if ( !StartsWithEBML ( stream ) )
			{
				_error = ErrorCode::InvalidHeader;
				return _isValid;
			}

			Parse ( *this, stream, streamSize );
			assert ( _stack.empty ( ) && "MKV Element stack underflow" );

			_isValid = true;
		}catch ( const std::exception & e )
		{
			std::printf ( "Error: %s\n", e.what ( ) );
			_error = ErrorCode::Unknown;
		}

		return _isValid;
	}

	ElementRef MKV::ReadNextElement ( const IStreamRef & stream )
	{
		usz position = stream->tell ( );
		u32 toRead = std::min ( 8u, (u32)(stream->size ( ) - position) );

		if ( toRead == 0 ) return nullptr;

		u64 data{};
		stream->readData ( &data, toRead );

		auto [id, bytesUsed] = ReadElementIDAndWidth ( static_cast<u32>(data) );
		stream->seekAbsolute ( static_cast<off_t>(position + bytesUsed) );

		toRead = std::min ( 8u, (u32)( stream->size ( ) - stream->tell() ) );
		if ( toRead == 0 ) return nullptr;

		position = stream->tell ( );
		stream->readData ( &data, toRead );

		auto [size, bytesUsed2] = ReadElementSize ( data );
		stream->seekAbsolute ( static_cast<off_t>(position + bytesUsed2) );

		auto & identifier = FindIdentifier ( static_cast<MatroskaElementId> ( id ) );

		if ( identifier.ID == MatroskaElementId::kUnknown )
		{
			std::printf ( "Missing: 0x%llx (%llu)\n", id, size );
			stream->seekRelative ( static_cast<off_t>(size) );
			return nullptr;
		}

		if ( _factories.count ( identifier.ID ) )
		{
			auto element = _factories[identifier.ID] ( identifier );
			element->Parse ( *this, stream, size );
			return element;
		} else
		{
			switch ( identifier.Type )
			{
				case MatroskaDataType::kMaster:
				{
					if ( auto element = std::make_shared<MasterElement> ( identifier ) )
					{
						element->Parse ( *this, stream, size );
						return element;
					}
					break;
				}
				default:
				{
					if ( identifier.ID == MatroskaElementId::kSimpleBlock || identifier.ID == MatroskaElementId::kBlock )
					{
						auto element = std::make_shared<SimpleBlockElement> ( identifier );
						element->Parse ( *this, stream, size );
						return element;
					} else
					{
						auto element = std::make_shared<UnhandledElement> ( identifier );
						element->Parse ( *this, stream, size );
						return element;
					}
				}
			}
		}
		return nullptr;
	}

	bool MKV::Parse ( MKV &, const ci::IStreamRef & stream, usz length )
	{
		auto success = MasterElement::Parse ( *this, stream, length );
		auto tracks = FindChildren ( MatroskaElementId::kTrackEntry );

		for ( auto & track : tracks )
		{
			track->As<TrackElement> ( )->CollectSamples ( *this );
		}

		return success;
	}

	bool MKV::StartsWithEBML ( const ci::IStreamRef & stream )
	{
		bool success = false;
		if ( auto element = ReadNextElement ( stream ) )
		{
			if ( element->Identifier ( ).ID == MatroskaElementId::kEBML )
			{
				success = true;
			}
		}

		stream->seekAbsolute ( 0 );
		return success;
	}

	void MKV::Push ( Element * element )
	{
		_stack.push ( element );
	}

	void MKV::Pop ( )
	{
		_stack.pop ( );
	}

	Element * MKV::Top ( ) const
	{
		if ( _stack.empty ( ) ) return nullptr;
		return _stack.top ( );
	}

	bool UnhandledElement::Parse ( MKV & context, const IStreamRef& stream, usz length )
	{
		std::string value{};

		switch ( _identifier.Type )
		{
			case MatroskaDataType::kString:
			{
				_value = ReadFixedString ( stream, length );
				value = "String{" + Value<std::string>() + "}";
				break;
			}

			case MatroskaDataType::kFloat:
			{
				_value = ReadFloat ( stream, length );
				value = "Float{" + std::to_string ( Value<double>() ) + "}";
				break;
			}

			case MatroskaDataType::kInteger:
			{
				_value = ReadInt ( stream, length );
				value = "SINT{" + std::to_string ( Value<s64>() ) + "}";
				break;
			}

			case MatroskaDataType::kUnsignedInt:
			{
				_value = ReadUint ( stream, length );
				value = "UINT{" + std::to_string ( Value<u64>() ) + "}";
				break;
			}

			case MatroskaDataType::kBinary:
			{
				std::stringstream ss;
				std::vector<u8> buffer;
				buffer.resize ( length );
				stream->readData ( buffer.data ( ), buffer.size ( ) );
				length = 0;
				ss << std::hex;

				for ( int i = 0; i < buffer.size ( ); i++ )
				{
					if ( i > 16 )
					{
						ss << "...";
						break;
					}
					ss << ( i > 0 ? " " : "" ) << (int)buffer[i];
				}

				_value = buffer;
				value = "Binary{" + ss.str ( ) + "}";
				break;
			}

			case MatroskaDataType::kUnicode:
			{
				_value = ReadFixedString ( stream, length );
				value = "UTF8{" + Value<std::string>() + "}";
				break;
			}

			default:
			{
				value = "TODO{}";
			}
		}

		if ( context.Settings ( ).TracksProperties ( ) )
		{
			WriteProperty ( "value", value );
		}

		if ( _identifier.Type != MatroskaDataType::kMaster )
		{
			stream->seekRelative ( static_cast<off_t>(length) );
		}

		_stringValue = value;
		return true;
	}

	std::string UnhandledElement::ToString ( ) const { return _identifier.Name + _stringValue; }

	bool MasterElement::Parse ( MKV & context, const IStreamRef & stream, usz length )
	{
		u64 pos = stream->tell( );
		u64 end = pos + length;
		context.Push ( this );

		while ( stream->tell() < end )
		{
			if ( auto element = context.ReadNextElement ( stream ) )
			{
				_children.push_back ( element );
			} else
			{
				break;
			}
			pos = stream->tell ( );
		}

		context.Pop ( );
		return true;
	}

	ElementRef MasterElement::FindFirstChild ( MatroskaElementId type, bool recursive ) const
	{
		for ( auto & child : GetChildren ( ) )
		{
			if ( child->Identifier().ID == type ) return child;
		}

		if ( recursive )
		{
			for ( auto & child : GetChildren ( ) )
			{
				if ( child->Identifier ( ).Type == MatroskaDataType::kMaster )
				{
					if ( auto result = child->TryCast<MasterElement> ( )->FindFirstChild ( type, recursive ) )
					{
						return result;
					}
				}
			}
		}

		return nullptr;
	}

	ElementList MasterElement::FindChildren ( MatroskaElementId type, bool recursive ) const
	{
		ElementList children;
		for ( auto & child : GetChildren ( ) )
		{
			if ( child->Identifier ( ).ID == type )
			{
				children.push_back ( child );
			}
		}

		if ( recursive )
		{
			for ( auto & child : GetChildren ( ) )
			{
				if ( child->Identifier ( ).Type == MatroskaDataType::kMaster )
				{
					if ( auto container = child->TryCast<MasterElement> ( ) )
					{
						auto result = container->FindChildren ( type, recursive );
						children.insert ( children.end ( ), result.begin ( ), result.end ( ) );
					}
				}
			}
		}
		return children;
	}

	bool TrackElement::Parse ( MKV & context, const ci::IStreamRef & stream, usz length )
	{
		_stream = stream;

		bool success = MasterElement::Parse ( context, stream, length );

		if ( auto element = FindFirstChild ( MatroskaElementId::kTrackType ) )
		{
			auto type = element->Value<u64> ( );
			switch ( type )
			{
				case 1 :
				{
					_trackType = TrackType::kVideo;
					break;
				}

				case 2:
				{
					_trackType = TrackType::kAudio;
					break;
				}

				case 17:
				{
					_trackType = TrackType::kSubtitles;
					break;
				}

				case 3: // Complex
				case 18: // Buttons
				case 32: // Control
				case 33: // Metadata
				{
					_trackType = TrackType::kUnknown;
					break;
				}
			}
		}

		if ( auto element = FindFirstChild ( MatroskaElementId::kTrackNumber ) )
		{
			_trackNumber = element->Value<u64> ( );
		}

		// @todo(andrew): Handle these properly
		// https://www.matroska.org/technical/codec_specs.html
		if ( auto element = FindFirstChild ( MatroskaElementId::kCodecID ) )
		{
			_codecId = element->Value<std::string> ( );

			if ( _trackType == TrackType::kVideo )
			{
				if ( _codecId == "V_MJPEG" )
				{
					_handler = 'jpeg';
				} else if ( _codecId == "V_QUICKTIME" )
				{
					if ( auto priv = FindFirstChild ( MatroskaElementId::kCodecPrivate ) )
					{
						// @note(andrew): This buffer is actually an STSD Atom from
						// the mp4 container format. Skip the fourcc + size bytes
						// to read the codec id.

						auto data = priv->Value<std::vector<u8>> ( );
						std::string_view view{ (char *)data.data ( ), data.size ( ) };
						auto index = view.find ( "Hap" );
						if ( index != std::string::npos && index < data.size ( ) - 4 )
						{
							std::string hap{ (char *)data.data ( ) + index, 4 };
							_handler = AX_FOURCC ( hap[0], hap[1], hap[2], hap[3] );
						}
					} else
					{
						// @todo(andrew): This is not a fair assumption to make
						// but will have to do for now.

						_handler = 'Hap1';
					}
				}
			} else if ( _trackType == TrackType::kAudio )
			{
				if ( _codecId == "A_EAC3" )
				{
					_handler = 'eac3';
				} else if ( _codecId.find ( "AAC" ) != std::string::npos )
				{
					_handler = 'mp4a';
				}
			} else if ( _trackType == TrackType::kSubtitles )
			{
				std::printf ( "Subs!\n" );
				if ( auto priv = FindFirstChild ( MatroskaElementId::kCodecPrivate ) )
				{
					// @note(andrew): This buffer is actually an STSD Atom from
					// the mp4 container format. Skip the fourcc + size bytes
					// to read the codec id.

					auto data = priv->Value<std::vector<u8>> ( );
					std::string_view view{ (char *)data.data ( ), data.size ( ) };
				}
			}
		}

		if ( auto element = FindFirstChild ( MatroskaElementId::kDefaultDuration ) )
		{
			_duration = static_cast<float>( element->Value<u64> ( ) / 1e9 );
		}

		if ( auto element = FindFirstChild ( MatroskaElementId::kPixelWidth ) )
		{
			_width = static_cast<u32>(element->Value<u64> ( ));
		}

		if ( auto element = FindFirstChild ( MatroskaElementId::kPixelHeight ) )
		{
			_height = static_cast<u32>( element->Value<u64> ( ) );
		}

		if ( context.Settings ( ).TracksProperties ( ) )
		{
			WriteProperty ( "width", _width );
			WriteProperty ( "height", _height );
			WriteProperty ( "duration", _duration );
			WriteProperty ( "track_number", _trackNumber );
			WriteProperty ( "track_type", (u32)_trackType );
			WriteProperty ( "handler", FourCCToString ( _handler ) );
			WriteProperty ( "codec_id", _codecId );
		}

		return success;
	}

	void TrackElement::CollectSamples ( MKV & container )
	{
		auto blocks = container.FindChildren ( MatroskaElementId::kSimpleBlock );
		for ( auto & b : blocks )
		{
			if ( auto block = b->TryCast<SimpleBlockElement> ( ) )
			{
				if ( block->TrackNumber ( ) == TrackNumber ( ) )
				{
					_blocks.push_back ( block );
				}
			}
		}

		blocks = container.FindChildren ( MatroskaElementId::kBlock );
		for ( auto & b : blocks )
		{
			if ( auto block = b->TryCast<SimpleBlockElement> ( ) )
			{
				if ( block->TrackNumber ( ) == TrackNumber ( ) )
				{
					_blocks.push_back ( block );
				}
			}
		}

		_duration *= static_cast<float>( blocks.size ( ) );
		if ( container.Settings ( ).TracksProperties ( ) )
		{
			WriteProperty ( "duration", _duration );
		}
	}

	bool TrackElement::FillSample ( u32 index, Sample & sample )
	{
		if ( index < _blocks.size ( ) )
		{
			auto & block = _blocks[index];
			if ( block->IsZeroCopy() )
			{
				sample = Sample ( _handler, block->ZeroCopyData(), static_cast<u32>(block->LengthInStream ( )), index, ivec2 ( _width, _height ) );
			} else
			{
				sample = Sample ( _handler, block->Data ( ), index, ivec2 ( _width, _height ) );
			}
			return true;
		}

		return false;
	}

	bool SimpleBlockElement::Parse ( MKV & context, const IStreamRef & stream, usz length )
	{
		// @perf(andrew): All these little reads are killing performance
		// Perhaps implement a higher level Element type and bulk read
		// them into a large block of memory and then write SimpleBlockElementViews
		// over the data. 
		u64 position = stream->tell ( );
		u64 data{};
		stream->read ( &data );
		
		usz size{};
		_trackNumber = static_cast<u8> ( Unpack ( (u8 *)&data, size ) );

		stream->seekAbsolute ( static_cast<off_t>(position + size) );

		stream->read ( &_timestamp );
		u8 flags{};
		stream->read ( &flags );
		_isKeyframe = flags & 0x80;

		auto headerSize = stream->tell ( ) - position;

		length -= headerSize;
		_blockLength = length;
		
		_positionInStream = stream->tell ( );
		_lengthInStream = length;

		// @note(andrew): Already in memory
		if ( auto mem = std::dynamic_pointer_cast<IStreamMem> ( stream ) )
		{
			_stream = IStreamMem::create ( (u8 *)mem->getData ( ) + mem->tell ( ), length );
			_offsetFromStartOfFile = 0;
			_isZeroCopy = true;
		} else
		{
			if ( context.Settings ( ).PreloadsIntoMemory ( ) )
			{
				auto cursor = stream->tell ( );
				_data.resize ( length );
				stream->readData ( _data.data ( ), length );
				stream->seekAbsolute ( cursor );

				_offsetFromStartOfFile = 0;
				_stream = IStreamMem::create ( _data.data ( ), length );
				_ownsMemory = true;
				_isZeroCopy = true;
			} else
			{
				_offsetFromStartOfFile = stream->tell();
				_stream = stream;
			}
		}

		stream->seekRelative ( static_cast<off_t>(length) );

		return true;
	}
	
	const u8 * SimpleBlockElement::ZeroCopyData ( ) const
	{
		if ( _ownsMemory )
		{
			return _data.data ( );
		}

		if ( auto mem = std::dynamic_pointer_cast<IStreamMem> ( _stream ) )
		{
			return (const u8 *)mem->getData ( ) + 0 - _offsetFromStartOfFile;
		}
		
		return nullptr;
	}
	
	std::vector<u8> SimpleBlockElement::Data ( ) const
	{
		if ( _isZeroCopy ) std::printf ( "MDAT is zero copy, use MDATAtom::ZeroCopyDataWithOffset instead\n" );

		std::vector<u8> data;
		data.resize ( _blockLength );

		_stream->seekAbsolute ( static_cast<off_t>( 0 - _offsetFromStartOfFile ) );
		_stream->readData ( data.data ( ), data.size() );
		
		return data;
	}

	MKVMovie::MKVMovie ( const ContainerRef & container )
		: Movie ( container )
	{
		auto * mkv = dynamic_cast<MKV *>( _container.get ( ) );
		if ( mkv )
		{
			for ( auto track : mkv->FindChildren ( MatroskaElementId::kTrackEntry ) )
			{
				_tracks.push_back ( std::make_shared<MKVTrack> ( track->As<TrackElement>() ) );
			}
		}
	}


	MKVTrack::MKVTrack ( const TrackElementRef& track )
		: Track ( track->Type() )
		, _track ( track )
	{
		_sampleCount = static_cast<u32>(_track->Blocks ( ).size ( ));
		_durationSeconds = _track->Duration ( );
		_size = ivec2 ( _track->Width ( ), _track->Height ( ) );

		if ( _type == TrackType::kAudio )
		{
			if ( auto element = track->FindFirstChild ( MatroskaElementId::kAudio ) )
			{
				auto audio = element->As<MasterElement> ( );
				if ( auto channels = audio->FindFirstChild ( MatroskaElementId::kChannels ) )
				{
					_channelCount = static_cast<u32> ( channels->Value<u64> ( ) );
				}
				
				if ( auto sampling = audio->FindFirstChild ( MatroskaElementId::kSamplingFrequency ) )
				{
					_sampleRate = static_cast<u32> ( sampling->Value<double> ( ) );
				}
			}
		}
	}

	bool MKVTrack::ReadSample ( u32 index, Sample & sample ) const
	{
		return _track->FillSample ( index, sample );
	}
}

#endif