//
//  AX-MP4.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 13/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#include "AX-MP4.h"
#include "cinder/CinderAssert.h"
#include "cinder/DataSource.h"
#include "cinder/app/App.h"
#include <sstream>

using namespace ci;

namespace AX
{
    struct StreamAutoAdvancer
    {
        StreamAutoAdvancer ( const IStreamRef & stream, off_t & delta )
            : _stream ( stream )
            , _before ( stream->tell ( ) )
            , _delta ( delta )
        { }

        ~StreamAutoAdvancer ( )
        {
            auto after = _stream->tell ( );
            _delta = ( after - _before );
        }

    protected:
        off_t &     _delta;
        off_t       _before{ 0 };
        IStreamRef  _stream;
    };
    
    #define AX_INDENT_SIZE 2
    #define AX_INDENT(x) ("+" + std::string ( x * AX_INDENT_SIZE, '-' ) + " ").c_str()
    #define AX_CURRENT_INDENT AX_INDENT ( context.StackDepth() )
    #define AX_CURRENT_INDENT_PLUS(x) AX_INDENT ( ( context.StackDepth() + x ) )

    std::string Atom::ToString ( ) const
    {
        std::stringstream ss;
        ss << AtomTypeToString ( Type ( ) ) << " (" << _info.Size << ")";

        return ss.str ( );
    }

    AtomRef BaseContainerAtom::FindFirstChild ( AtomType type, bool recursive ) const
    {
        for ( auto& child : _children )
        {
            if ( child->Type ( ) == type ) return child;
        }

        if ( recursive )
        {
            for ( auto& child : _children )
            {
                if ( child->IsContainer ( ) )
                {
                    if ( auto result = child->TryCast<IContainerAtom> ( )->FindFirstChild ( type, recursive ) )
                    {
                        return result;
                    }
                }
            }
        }

        return nullptr;
    }

    AtomList BaseContainerAtom::FindChildren ( AtomType type, bool recursive ) const
    {
        std::vector<AtomRef> children;
        for ( auto& child : _children )
        {
            if ( child->Type ( ) == type )
            {
                children.push_back ( child );
            }
        }

        if ( recursive )
        {
            for ( auto& child : _children )
            {
                if ( child->IsContainer ( ) )
                {
                    if ( auto container = child->TryCast<IContainerAtom> ( ) )
                    {
                        auto result = container->FindChildren ( type, recursive );
                        children.insert ( children.end ( ), result.begin ( ), result.end ( ) );
                    }
                }
            }
        }
        return children;
    }

    void ContainerAtom::Parse ( MP4 & context, const IStreamRef & stream, usz expectedLength )
    {
        auto streamStart = stream->tell ( );
        auto streamSize = stream->size ( );
        
        SetInfo ( { context.StackDepth ( ), expectedLength, streamStart } );
        context.Push ( this );
        
        bool done = false;
        while ( !done )
        {  
            u32 length32{};
            stream->readBig<u32> ( &length32 );
            assert ( length32 != 0 );

            AtomType type{ AtomType::kUNKN };
            stream->readBig<u32> ( (u32 *)&type );

            u64 streamPos = stream->tell ( );
            u64 actualLength{ 0 };

            if ( length32 == 1 )
            {
                u64 length{ 0 };
                stream->readBig<u64> ( &length );
                actualLength = length - 16;
            }
            else
            {
                actualLength = length32 - 8;
            }

            if ( auto atom = context.CreateAtom ( type ) )
            {
                try
                {
                    atom->SetInfo ( { context.StackDepth ( ), actualLength, stream->tell() } );
                    atom->Parse ( context, stream, actualLength );

                    AddChild ( atom );
                } catch ( const std::exception& e )
                {
                    std::printf ( "Error parsing atom '%s': %s\n", AtomTypeToString ( type ).c_str ( ), e.what ( ) );
                    done = true;
                    break;
                }
            }

            auto after = stream->tell ( );
            if ( ( after - streamStart ) >= expectedLength )
            {
                done = true;
                break;
            }

            if ( streamPos + actualLength >= streamSize )
            {
                stream->seekAbsolute ( streamSize );
                done = true;
                break;
            }
        }
        
        context.Pop ( );
    }

    void FullAtom::Parse ( MP4 & context, const IStreamRef & stream, usz expectedLength )
    {
        u32 header{ };
        stream->readBig<u32> ( &header );
        
        _version = ( header >> 24 ) & 0x000000FF;
        _flags   = ( header       ) & 0x00FFFFFF;

        if ( context.Settings ( ).TracksProperties ( ) )
        {
            WriteProperty ( "version", _version );
            WriteProperty ( "flags", _flags );
        }

        stream->seekRelative ( static_cast<off_t> ( expectedLength - sizeof ( u32 ) ) );
    }

    void UnknownAtom::Parse ( MP4 &, const IStreamRef & stream, usz expectedLength )
    {
        if ( stream->tell ( ) + expectedLength < stream->size ( ) )
        {
            stream->seekRelative ( static_cast<off_t> ( expectedLength ) );
        }
    }
    
    void FTYPAtom::Parse ( MP4 & context, const IStreamRef & stream, usz expectedLength )
    {
        usz startLength{ expectedLength };
        
        u32 brand{ 0 };
        stream->readBig<u32> ( &brand );
        
        _brand = AX::FourCCToString ( brand );

        stream->readBig<u32> ( &_minorVersion );
        _compatibleBrands.push_back ( _brand );

        if ( expectedLength > 8 )
        {
            expectedLength -= 8;
            while ( expectedLength > 0 )
            {
                u32 type{};
                stream->readBig<u32> ( &type );
                expectedLength -= 4;

                if ( type != 0 )
                {
                    _compatibleBrands.push_back ( FourCCToString ( type ) );
                }
            }
        }

        if ( context.Settings ( ).TracksProperties ( ) )
        {
            std::stringstream ss;
            ss << "[";
            for ( auto& b : _compatibleBrands ) ss << b << ", ";
            auto str = ss.str ( );

            // note(andrew): Remove trailing ', ';
            if ( str.length ( ) > 2 )
            {
                str.pop_back ( );
                str.pop_back ( );
            }
            str += "]";

            WriteProperty ( "length", startLength );
            WriteProperty ( "brand", _brand );
            WriteProperty ( "minor_version", _minorVersion );
            WriteProperty ( "compatible_brands", str );
        }
    }

    // @TODO(andrew): This seems wrong to me
    static glm::mat3 ReadMatrix ( const IStreamRef& stream )
    {
        glm::mat3 result{ };
        auto* element = glm::value_ptr ( result );

        for ( int i = 0; i < 9; i++ )
        {
            u32 n{ 0 };
            stream->readBig<u32> ( &n );
            *element++ = static_cast<float> ( n ) / 65536.0f;
        }

        return result;
    }

    void TKHDAtom::Parse ( MP4 & context, const IStreamRef & stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom::Parse ( context, stream, 4 );

            auto ReadByVersion = [&]( u64& out )
            {
                if ( Version ( ) == 1 )
                {
                    stream->readBig<u64> ( &out );
                } else
                {
                    u32 inner{ 0 };
                    stream->readBig<u32> ( &inner );
                    out = inner;
                }
            };

            ReadByVersion ( _creationTime );
            ReadByVersion ( _modificationTime );
            stream->readBig<u32> ( &_trackID );

            // Skip reserved1
            stream->seekRelative ( sizeof ( u32 ) );

            ReadByVersion ( _duration );

            // Skip reserved2
            stream->seekRelative ( 2 * sizeof ( u32 ) );

            stream->readBig<u16> ( &_layer );
            stream->readBig<u16> ( &_alternateGroup );
            stream->readBig<u16> ( &_volume );

            // Skip reserved3
            stream->seekRelative ( sizeof ( u16 ) );

            _matrix = ReadMatrix ( stream );

            stream->readBig<u32> ( &_width );
            stream->readBig<u32> ( &_height );
            
            _width /= 65536;
            _height /= 65536;

            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
                WriteProperty ( "creation_time", _creationTime );
                WriteProperty ( "modification_time", _modificationTime );
                WriteProperty ( "duration", _duration );
                WriteProperty ( "layer", _layer );
                WriteProperty ( "alternate_group", _alternateGroup );
                WriteProperty ( "volume", _volume );
                WriteProperty ( "width", _width );
                WriteProperty ( "height", _height );
            }
        }

        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    void MVHDAtom::Parse ( MP4& context, const IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom::Parse ( context, stream, 4 );

            auto ReadByVersion = [&]( u64& out )
            {
                if ( Version ( ) == 1 )
                {
                    stream->readBig<u64> ( &out );
                } else
                {
                    u32 inner{ 0 };
                    stream->readBig<u32> ( &inner );
                    out = inner;
                }
            };
            
            ReadByVersion ( _creationTime );
            ReadByVersion ( _modificationTime );
            stream->readBig<u32> ( &_timeScale );
            ReadByVersion ( _duration );

            if ( _timeScale != 0 )
            {
                _durationSeconds = static_cast<float>( _duration ) / static_cast<float>( _timeScale );
            } else
            {
                // @todo(andrew): What to do here? Are we already in bad shape in this case?
                _durationSeconds = static_cast<float> ( _duration );
            }

            stream->readBig<u32> ( &_rate );
            stream->readBig<u16> ( &_volume );

            struct Reserved { u16 A; u32 B[2]; };
            stream->seekRelative ( sizeof ( Reserved ) );

            // @TODO(andrew): Is this completely wrong?
            _matrix = ReadMatrix ( stream );
            
            // @note(andrew): Skip predefined
            stream->seekRelative ( 6 * sizeof ( u32 ) );
            stream->readBig<u32> ( &_nextTrackID );

            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
                WriteProperty ( "creation_time", _creationTime );
                WriteProperty ( "modification_time", _modificationTime );
                WriteProperty ( "time_scale", _timeScale );
                WriteProperty ( "duration", _duration );
                WriteProperty ( "duration_seconds", _durationSeconds );
                WriteProperty ( "rate", _rate );
                WriteProperty ( "volume", _volume );
                WriteProperty ( "next_track_id", _nextTrackID );
            }
        }

        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }
    
    void MDHDAtom::Parse ( MP4& context, const IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom::Parse ( context, stream, 4 );
            
            auto ReadByVersion = [&]( u64& out )
            {
                if ( Version() == 1 )
                {
                    stream->readBig<u64> ( &out );
                } else
                {
                    u32 inner{ 0 };
                    stream->readBig<u32> ( &inner );
                    out = inner;
                }
            };

            ReadByVersion ( _creationTime );
            ReadByVersion ( _modificationTime );
            stream->readBig<u32> ( &_timeScale );
            ReadByVersion ( _duration );

            u16 language{};
            stream->readBig ( &language );

            if ( _timeScale != 0 )
            {
                _durationSeconds = static_cast<float>( _duration ) / static_cast<float>( _timeScale );
            } else
            {
                // @todo(andrew): What to do here? Are we already in bad shape in this case?
                _durationSeconds = static_cast<float> ( _duration );
            }

            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
                WriteProperty ( "creation_time", _creationTime );
                WriteProperty ( "modification_time", _modificationTime );
                WriteProperty ( "time_scale", _timeScale );
                WriteProperty ( "duration", _duration );
                WriteProperty ( "duration_seconds", _durationSeconds );
            }
        }

        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    void MDATAtom::Parse ( MP4& context, const IStreamRef& stream, usz expectedLength )
    {
        _length = expectedLength;
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            
            if ( auto mem = std::dynamic_pointer_cast<IStreamMem> ( stream ) )
            {
                _stream = IStreamMem::create ( (u8*)mem->getData ( ) + mem->tell ( ), expectedLength );
                _offsetFromStartOfFile = stream->tell ( );
                _isZeroCopy = true;
            } else
            {
                if ( context.Settings ( ).PreloadsIntoMemory ( ) )
                {
                    auto cursor = stream->tell ( );
                    _data.resize ( expectedLength );
                    stream->readData ( _data.data ( ), expectedLength );
                    stream->seekAbsolute ( cursor );
                    
                    _offsetFromStartOfFile = stream->tell ( );
                    _stream = IStreamMem::create ( _data.data ( ), expectedLength );
                    _ownsMemory = true;
                    _isZeroCopy = true;
                } else
                {
                    _offsetFromStartOfFile = 0;
                    _stream = stream;
                }
            }
        }

        if ( context.Settings ( ).TracksProperties ( ) )
        {
            WriteProperty ( "length", expectedLength );
        }

        // @FIXME(andrew): This occurs when MDATs are towards the end,
        // have only seen it when parsing M4A/AAC, not sure if correct.
        if ( stream->tell ( ) + expectedLength > stream->size ( ) )
        {
            stream->seekAbsolute ( stream->size ( ) );
        } else
        {
            stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
        }
    }

    std::vector<u8> MDATAtom::DataWithOffset ( off_t offset, size_t size ) const
    {
        if ( _isZeroCopy ) std::printf ( "MDAT is zero copy, use MDATAtom::ZeroCopyDataWithOffset instead\n" );

        std::vector<u8> data; 
        data.resize ( size );
        
        try
        {
            _stream->seekAbsolute ( offset - _offsetFromStartOfFile );
            _stream->readData ( data.data ( ), size );
        } catch ( const std::exception& e )
        {
            std::printf ( "Error reading from MDAT: %s\n", e.what ( ) );
            size -= offset;

            _stream->seekAbsolute ( offset - _offsetFromStartOfFile );
            _stream->readData ( data.data ( ), size );
        }
        return data;
    }

    const u8* MDATAtom::ZeroCopyDataWithOffset ( off_t offset ) const
    {
        assert ( _isZeroCopy );
        if ( auto mem = std::dynamic_pointer_cast<IStreamMem> ( _stream ) )
        {
            return (const u8*)mem->getData ( ) + offset - _offsetFromStartOfFile;
        }
        return nullptr;
    }

    void STSDAtom::Parse ( MP4& context, const IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom header{};
            header.Parse ( context, stream, 4 );

            u32 entryCount;
            stream->readBig<u32> ( &entryCount );
            
            u32 nextLength{ 0 };
            stream->readBig<u32> ( &nextLength );
            stream->seekRelative ( -static_cast<off_t>(sizeof ( u32 )) );
            
            for ( u32 i = 0; i < entryCount; i++ )
            {
                u32 length{};
                stream->readBig<u32> ( &length );

                u32 entry{ 0 };
                stream->readBig<u32> ( &entry );

                _descriptions.push_back ( entry );
            }

            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
                WriteProperty ( "num_descriptions", _descriptions.size ( ) );
            }
        }

        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    void HDLRAtom::Parse ( MP4& context, const IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom::Parse ( context, stream, 4 );

            stream->readBig<u32> ( (u32 *) & _handlerType );
            stream->readBig<u32> ( (u32 *) & _handlerSubType );

            u32 reserved[3] = {};
            for ( int i = 0; i < 3; i++ )
            {
                stream->readBig<u32> ( &reserved[i] );
            }

            _handlerName.clear ( );

            bool isPascalString = _handlerType == HDLRType::kMHLR || reserved[0] == AX_FOURCC ( 'a', 'p', 'p', 'l' );
            
            // @note(andrew): So far these have always been pascal strings for mhlr and dhlr
            isPascalString = true;
            if ( isPascalString )
            {
                u8 length{};
                stream->read<u8> ( &length );
                if ( length > 0 )
                {
                    stream->readFixedString ( &_handlerName, length );
                }
            } else
            {
                u8 byte{ 0 };
                while ( true )
                {
                    stream->read<u8> ( &byte );
                    if ( byte == 0 ) break;
                    _handlerName.append ( 1, byte );
                }
            }

            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
                WriteProperty ( "handler", HandlerTypeToString ( _handlerType ) );
                WriteProperty ( "subtype", HandlerSubTypeToString ( _handlerSubType ) );
                WriteProperty ( "name", Name ( ) );
            }
        }
        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    void STSZAtom::Parse ( MP4& context, const IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom::Parse ( context, stream, 4 );

            u32 sampleSize{ 0 };
            stream->readBig<u32> ( &sampleSize );

            stream->readBig<u32> ( &_sampleCount );

            _sampleSizes.resize ( _sampleCount );

            if ( sampleSize == 0 )
            {
                for ( u32 i = 0; i < _sampleCount; i++ )
                {
                    stream->readBig<u32> ( &_sampleSizes[i] );
                }
            } else
            {
                for ( u32 i = 0; i < _sampleCount; i++ )
                {
                    _sampleSizes[i] = sampleSize;
                }
            }

            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
                WriteProperty ( "num_samples", _sampleSizes.size() );
            }
        }

        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    void STTSAtom::Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom::Parse ( context, stream, 4 );

            stream->readBig<u32> ( &_entryCount );
            _entries.resize ( _entryCount );

            for ( u32 i = 0; i < _entryCount; i++ )
            {
                stream->readBig<u32> ( &_entries[i].SampleCount );
                stream->readBig<u32> ( &_entries[i].SampleDelta );
            }
            
            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
                WriteProperty ( "num_entries", _entries.size ( ) );
            }
        }

        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    void STCOAtom::Parse ( MP4& context, const IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom::Parse ( context, stream, 4 );

            stream->readBig<u32> ( &_chunkCount );

            _chunkOffsets.resize ( _chunkCount );
            
            for ( u32 i = 0; i < _chunkCount; i++ )
            {
                u32 offset{ 0 };
                stream->readBig<u32> ( &offset );
                
                _chunkOffsets[i] = offset;
            }

            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
                WriteProperty ( "num_chunk_offsets", _chunkOffsets.size ( ) );
            }
        }

        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    void STSCAtom::Parse ( MP4& context, const IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom::Parse ( context, stream, 4 );

            u32 entryCount{ 0 };
            stream->readBig<u32> ( &entryCount );

            u32 firstSample = 1;

            _chunks.resize ( entryCount );
            for ( u32 i = 0; i < entryCount; i++ )
            {
                auto& chunk = _chunks[i];
                stream->readBig<u32> ( &chunk.FirstChunk );
                stream->readBig<u32> ( &chunk.SamplesPerChunk );
                stream->readBig<u32> ( &chunk.SampleDescIndex );

                if ( i > 0 )
                {
                    auto& prev = _chunks[i - 1];
                    prev.ChunkCount = chunk.FirstChunk - prev.FirstChunk;
                    firstSample += prev.ChunkCount * prev.SamplesPerChunk;
                }

                chunk.ChunkCount = 0;
                chunk.FirstSample = firstSample;
            }

            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
                WriteProperty ( "num_chunks", _chunks.size() );
            }
        }
        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    bool STSCAtom::GetChunkForSample ( u32 sample, u32& chunk, u32& skip, u32& desc ) const
    {
        // @TODO(andrew): cache last searched chunk for common case
        u32 group = 0;
        while ( group < _chunks.size ( ) ) 
        {
            u32 sampleCount = _chunks[group].ChunkCount * _chunks[group].SamplesPerChunk;
            if ( sampleCount == 0 ) 
            {
                if ( _chunks[group].FirstSample > sample ) return false;
            } else 
            {
                if ( _chunks[group].FirstSample + sampleCount <= sample ) 
                {
                    group++;
                    continue;
                }
            }

            if ( _chunks[group].SamplesPerChunk == 0 ) return false;
            
            u32 chunkOffset = ( ( sample - _chunks[group].FirstSample ) / _chunks[group].SamplesPerChunk );
            chunk = _chunks[group].FirstChunk + chunkOffset;
            skip = sample - ( _chunks[group].FirstSample + _chunks[group].SamplesPerChunk * chunkOffset );
            desc = _chunks[group].SampleDescIndex;

            return true;
        }

        chunk = 0;
        skip = 0;
        desc = 0;
        
        return false;
    }

    void ExtensionAtom::Parse ( MP4&, const ci::IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            stream->seekRelative ( 6 );
            stream->readBig<u16> ( &_dataReferenceIndex );
        }
        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    void MP4AExtensionAtom::Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            ExtensionAtom::Parse ( context, stream, 8 );

            off_t delta2{ 0 };
            {
                StreamAutoAdvancer adv2{ stream, delta2 };
                u32 reserved2[2];
                u16 reserved3;
                u16 reserved4;
                u32 reserved5;
                u16 timeScale;
                u16 reserved6;
                
                stream->readData ( &reserved2, 2 * sizeof ( u32 ) );
                stream->readBig ( &reserved3 );
                stream->readBig ( &reserved4 );
                stream->readBig ( &reserved5 );
                stream->readBig ( &timeScale );
                stream->readBig ( &reserved6 );

                // @FIXME(andrew): Need to work out when this is required
                // Something to do with QT specific data
                stream->seekRelative ( 4 * sizeof ( u32 ) );
            }

            ContainerAtom::Parse ( context, stream, expectedLength - delta2 - 8 );
        }
        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    void ESDSAtom::Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength )
    {
        off_t delta{ 0 };
        {
            StreamAutoAdvancer adv{ stream, delta };
            FullAtom::Parse ( context, stream, 4 );

            u8 objectType{ 0 };
            stream->readBig<u8> ( &objectType );

            // @TODO(andrew): Read the Elementary Stream Descriptor table

            if ( context.Settings ( ).TracksProperties ( ) )
            {
                WriteProperty ( "length", expectedLength );
            }
        }
        stream->seekRelative ( static_cast<off_t> ( expectedLength - delta ) );
    }

    const char * MP4ErrorCodeToString ( MP4ErrorCode code )
    {
        static std::unordered_map<MP4ErrorCode, const char *> kTable =
        {
            { MP4ErrorCode::Success, "Success" },
            { MP4ErrorCode::InvalidHeader, "Invalid Header" },
            { MP4ErrorCode::Unknown, "Unknown" },
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

    MP4Ref MP4::Create ( const fs::path& path, const Format& format )
    {
        if ( !fs::exists ( path ) ) return nullptr;
        return MP4::Create ( loadFile ( path ), format );
    }

    MP4Ref MP4::Create ( const DataSourceRef& source, const Format& format )
    {
        if ( auto mp4 = MP4Ref ( new MP4 ( source, format ) ) )
        {
            mp4->Load ( );
            return mp4;
        }

        return nullptr;
    }

    MP4::MP4 ( const DataSourceRef& source, const Format& format )
        : _format ( format )
        , _source ( source )
    {
        auto containers =
        {
            AtomType::kMOOV, AtomType::kTRAK, AtomType::kEDTS, AtomType::kMDIA,
            AtomType::kMINF, AtomType::kSTBL, AtomType::kMVEX, AtomType::kMOOF,
            AtomType::kTRAF, AtomType::kMFRA, AtomType::kMECO, AtomType::kMERE,
            AtomType::kDINF, AtomType::kIPRO, AtomType::kSINF, AtomType::kIPRP,
            AtomType::kFIIN, AtomType::kPAEN, AtomType::kSTRK, AtomType::kTAPT,
            AtomType::kSCHI
        };

        for ( auto& container : containers )
        {
            RegisterAtomFactory ( container, [=] { return std::make_shared<ContainerAtom> ( container ); } );
        }

        RegisterAtomFactory ( AtomType::kFTYP, [=] { return std::make_shared<FTYPAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kMDAT, [=] { return std::make_shared<MDATAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kMVHD, [=] { return std::make_shared<MVHDAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kTKHD, [=] { return std::make_shared<TKHDAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kMETA, [=] { return std::make_shared<FullAtom> ( AtomType::kMETA ); } );
        RegisterAtomFactory ( AtomType::kHDLR, [=] { return std::make_shared<HDLRAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kMDHD, [=] { return std::make_shared<MDHDAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kESDS, [=] { return std::make_shared<ESDSAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kPITM, [=] { return std::make_shared<FullAtom> ( AtomType::kPITM ); } );
        RegisterAtomFactory ( AtomType::kIINF, [=] { return std::make_shared<FullAtom> ( AtomType::kIINF ); } );
        RegisterAtomFactory ( AtomType::kDREF, [=] { return std::make_shared<FullAtom> ( AtomType::kDREF ); } );
        RegisterAtomFactory ( AtomType::kURL_, [=] { return std::make_shared<FullAtom> ( AtomType::kURL_ ); } );
        RegisterAtomFactory ( AtomType::kURN_, [=] { return std::make_shared<FullAtom> ( AtomType::kURN_ ); } );
        RegisterAtomFactory ( AtomType::kILOC, [=] { return std::make_shared<FullAtom> ( AtomType::kILOC ); } );
        RegisterAtomFactory ( AtomType::kIREF, [=] { return std::make_shared<FullAtom> ( AtomType::kIREF ); } );
        RegisterAtomFactory ( AtomType::kINFE, [=] { return std::make_shared<FullAtom> ( AtomType::kINFE ); } );
        RegisterAtomFactory ( AtomType::kIROT, [=] { return std::make_shared<UnknownAtom> ( AtomType::kIROT ); } );
        RegisterAtomFactory ( AtomType::kHVCC, [=] { return std::make_shared<UnknownAtom> ( AtomType::kHVCC ); } );
        RegisterAtomFactory ( AtomType::kAVCC, [=] { return std::make_shared<UnknownAtom> ( AtomType::kAVCC ); } );
        RegisterAtomFactory ( AtomType::kDIMG, [=] { return std::make_shared<UnknownAtom> ( AtomType::kDIMG ); } );
        RegisterAtomFactory ( AtomType::kTHMB, [=] { return std::make_shared<UnknownAtom> ( AtomType::kTHMB ); } );
        RegisterAtomFactory ( AtomType::kCDSC, [=] { return std::make_shared<UnknownAtom> ( AtomType::kCDSC ); } );
        RegisterAtomFactory ( AtomType::kCOLR, [=] { return std::make_shared<UnknownAtom> ( AtomType::kCOLR ); } );
        RegisterAtomFactory ( AtomType::kISPE, [=] { return std::make_shared<FullAtom> ( AtomType::kISPE ); } );
        RegisterAtomFactory ( AtomType::kIPMA, [=] { return std::make_shared<FullAtom> ( AtomType::kIPMA ); } );
        RegisterAtomFactory ( AtomType::kPIXI, [=] { return std::make_shared<FullAtom> ( AtomType::kPIXI ); } );
        RegisterAtomFactory ( AtomType::kIPCO, [=] { return std::make_shared<UnknownAtom> ( AtomType::kIPCO ); } );
        RegisterAtomFactory ( AtomType::kSTSD, [=] { return std::make_shared<STSDAtom> (); } );
        RegisterAtomFactory ( AtomType::kSTSZ, [=] { return std::make_shared<STSZAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kSTCO, [=] { return std::make_shared<STCOAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kSTSC, [=] { return std::make_shared<STSCAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kSTSS, [=] { return std::make_shared<FullAtom> ( AtomType::kSTSS ); } );
        RegisterAtomFactory ( AtomType::kSTTS, [=] { return std::make_shared<STTSAtom> ( ); } );
        RegisterAtomFactory ( AtomType::kFRMA, [=] { return std::make_shared<UnknownAtom> ( AtomType::kFRMA ); } );
        RegisterAtomFactory ( AtomType::kSCHM, [=] { return std::make_shared<FullAtom> ( AtomType::kSCHM ); } );
        RegisterAtomFactory ( AtomType::kHVC1, [=] { return std::make_shared<FullAtom> ( AtomType::kHVC1 ); } );
        RegisterAtomFactory ( AtomType::kAVC1, [=] { return std::make_shared<FullAtom> ( AtomType::kAVC1 ); } );
        //RegisterAtomFactory ( AtomType::kMP4A, [=] { return std::make_shared<FullAtom> ( AtomType::kMP4A ); } );
        //RegisterAtomFactory ( AtomType::kMP4A, [=] { return std::make_shared<MP4AExtensionAtom> ( AtomType::kMP4A ); } );
        
    }

    bool MP4::Load ( )
    {
        try
        {
            IStreamRef stream = _source->createStream ( );
            u64 streamSize = stream->size ( );

            if ( !StartsWithFTYP ( stream ) )
            {
                _error = MP4ErrorCode::InvalidHeader;
                return _isValid;
            }

            Parse ( *this, stream, streamSize );
            assert ( _stack.empty ( ) && "MP4 ContainerAtom stack underflow" );

            _isValid = true;
        } catch ( const std::exception& )
        {
            _error = MP4ErrorCode::Unknown;
        }

        return _isValid;
    }

    bool MP4::StartsWithFTYP ( const IStreamRef& stream ) const
    {
        const u32 kHeaderSize = 2 * sizeof ( u32 );
     
        if ( !stream ) return false;
        if ( stream->size() < kHeaderSize ) return false;

        auto cursor = stream->tell ( );
        
        try
        {
            u32 length{};
            stream->readBig<u32> ( &length );
            if ( length < 4 * sizeof ( u32 ) ) return false;

            AtomType type{ AtomType::kUNKN };
            stream->readBig<u32> ( (u32*)&type );
            stream->seekRelative ( cursor - stream->tell() );

            return type == AtomType::kFTYP;
        } catch ( const std::exception& )
        {
            stream->seekRelative ( cursor - stream->tell ( ) );
            return false;
        }
    }

    AtomRef MP4::CreateAtom ( AtomType type )
    {
        if ( _factories.count ( type ) ) return _factories.at ( type )( );
        return std::make_shared<UnknownAtom> ( type );
    }

    void MP4::RegisterAtomFactory ( AtomType type, AtomFactoryFn fn )
    {
        _factories[type] = fn;
    }

    static void DumpAtom ( const Atom* atom, std::ostream& stream, bool verbose, int indent = 0 )
    {
        stream << AX_INDENT ( indent ) << atom->ToString ( );
        if ( verbose )
        {
            u32 ctr{ 0 };
            auto size = atom->Properties ( ).size ( );
            if ( size > 0 )
            {
                stream << " [";
                for ( auto& [name, value] : atom->Properties ( ) )
                {
                    stream << name << "=" << value;
                    if ( ctr++ < size - 1 ) stream << ", ";
                }
                stream << "]";
            }
        }
        stream << "\n";

        if ( atom->IsContainer ( ) )
        {
            auto* container = static_cast<const ContainerAtom*> ( atom );
            for ( auto& child : container->GetChildren ( ) )
            {
                DumpAtom ( child.get ( ), stream, verbose, indent + 1 );
            }
        }
    }

    void MP4::Dump ( std::ostream& stream, bool verbose ) const
    {
        DumpAtom ( this, stream, verbose );
    }

    void MP4::Push ( ContainerAtom * atom )
    {
        _stack.push ( atom );
    }

    void MP4::Pop ( )
    {
        assert ( !_stack.empty ( ) && "MP4 ContainerAtom stack overflow" );
        _stack.pop ( );
    }

    ContainerAtom * MP4::Top ( ) const
    {
        if ( _stack.empty ( ) ) return nullptr;
        return _stack.top ( );
    }
    
    ///
    /// Convenience Playback API
    /// 

    MovieRef Movie::Create ( const MP4Ref& mp4 )
    {
        return MovieRef ( new Movie ( mp4 ) );
    }

    Movie::Movie ( const MP4Ref& mp4 )
        : _mp4 ( mp4 )
    {
        if ( _mp4 )
        {
            auto mdat = _mp4->FindFirstChildAs<MDATAtom> ( AtomType::kMDAT, true );
            for ( auto& trak : _mp4->FindChildrenAs<ContainerAtom> ( AtomType::kTRAK, true ) )
            {
                assert ( trak->Type ( ) == AtomType::kTRAK );
                _tracks.push_back ( std::make_shared<Track> ( mdat, trak ) );
            }
        }
    }

    std::vector<TrackRef> Movie::FindTracks ( TrackType type ) const
    {
        std::vector<TrackRef> result;
        for ( auto& track : _tracks )
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

    Track::AsyncContext::AsyncContext ( )
        : Work ( asio::make_work_guard ( Io ) )
    {
        Thread = std::thread ( [&] { Io.run ( ); } );
    }
    
    Track::AsyncContext::~AsyncContext ( )
    {
        try
        {
            Io.stop ( );
        } catch ( const std::exception& e )
        {
            std::printf ( "Error stopping AsyncContext: %s\n", e.what ( ) );
        }

        if ( Thread.joinable ( ) ) Thread.join ( );
    }

    Track::Track ( const MDATAtomRef& mdat, const ContainerAtomRef& trak )
        : _trak ( trak )
        , _mdat ( mdat )
    {
        if ( auto tkhd = trak->FindFirstChildAs<TKHDAtom> ( AtomType::kTKHD ) )
        {
            _size.x = tkhd->Width ( );
            _size.y = tkhd->Height ( );
        }

        _stsz = trak->FindFirstChildAs<STSZAtom> ( AtomType::kSTSZ );
        _stco = trak->FindFirstChildAs<STCOAtom> ( AtomType::kSTCO );
        _stsc = trak->FindFirstChildAs<STSCAtom> ( AtomType::kSTSC );
        _stsd = trak->FindFirstChildAs<STSDAtom> ( AtomType::kSTSD );
        _hdlr = trak->FindFirstChildAs<HDLRAtom> ( AtomType::kHDLR );
        _mdhd = trak->FindFirstChildAs<MDHDAtom> ( AtomType::kMDHD );
    }

    // @note(andrew): Not exhaustive
    TrackType Track::Type ( ) const
    {
        if ( _hdlr.expired() ) return TrackType::kUnknown;
        
        switch ( _hdlr.lock()->Subtype ( ) )
        {
            case HDLRSubtype::kSOUN:
            {
                return TrackType::kAudio;
            }

            case HDLRSubtype::kVIDE:
            {
                return TrackType::kVideo;
            }

            case HDLRSubtype::kHINT:
            {
                return TrackType::kHint;
            }
            case HDLRSubtype::kJPEG:
            {
                return TrackType::kJPEG;
            }
            
            case HDLRSubtype::kTX3G: 
            case HDLRSubtype::kSUBT:
            case HDLRSubtype::kSBTL:
            {
                return TrackType::kSubtitles;
            }

            default:
            {
                return TrackType::kUnknown;
            }
        }
    }

    void ITrackDecoder::DecodedFrame::SetPixelData ( const ci::Surface8uRef& surface )
    {
        _pixelData = surface;
        PixelBufferSize = surface->getRowBytes ( ) * surface->getHeight ( );
        PixelBuffer = surface->getData ( );
    }

    void ITrackDecoder::DecodedFrame::SetPixelData ( const ci::Channel8uRef& channel )
    {
        _pixelData = channel;
        PixelBufferSize = channel->getRowBytes ( ) * channel->getHeight ( );
        PixelBuffer = channel->getData ( );
    }

    void ITrackDecoder::DecodedFrame::SetPixelData ( const RawBufferRef& raw )
    {
        _pixelData = raw;
        PixelBufferSize = raw->size ( );
        PixelBuffer = raw->data ( );
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
        AX::Sample sample{};
        if ( ReadSample ( index, sample ) )
        {
            if ( auto decoder = CreateDecoder ( sample.Handler() ) )
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
        if ( !_async ) _async = std::make_unique<AsyncContext> ( );

        asio::post(_async->Io, [&, index, callback]
        {
            Sample sample{};
            bool success = false;
            ITrackDecoderRef decoder = nullptr;
            if ( ReadSample ( index, sample ) )
            {
                decoder = DecodeSample ( index );
                success = decoder != nullptr;
            }

            app::App::get ( )->dispatchAsync ( [s = success, i = index, cb = callback, dec=decoder]
            {
                cb ( i, s, dec );
            } );
        } );
    }

    void Track::ReadSampleAsync ( u32 index, AsyncReadCallback callback ) const
    {
        if ( !_async ) _async = std::make_unique<AsyncContext> ( );
		asio::post(_async->Io, [&, index, callback]
        {
            Sample sample{};
            bool success = ReadSample ( index, sample );
            app::App::get ( )->dispatchAsync ( [s = success, i = index, cb = callback, sam = std::move ( sample )]
            {
                cb ( i, s, std::move(sam) );
            } );
        } );
    }

    bool Track::ReadSample ( u32 index, Sample& sample ) const
    {
        if ( _mdat.expired() ) return false;
        if ( _stsz.expired() ) return false;
        if ( _stco.expired() ) return false;
        if ( _stsc.expired() ) return false;

        index++; // @note(andrew): 1 based in mp4 apparently

        auto stsc = _stsc.lock ( );
       
        u32 chunk{ 0 }, skip{ 0 }, desc{ 0 };
        stsc->GetChunkForSample ( index, chunk, skip, desc );

        if ( skip > index ) return false;

        u64 offset{ 0 };
        u32 offset32{ 0 };

        auto stco = _stco.lock ( );
        if ( !stco->GetChunkOffset ( chunk, offset32 ) ) return false;
        offset = offset32;
        
        auto stsz = _stsz.lock ( );
        for ( u32 i = index - skip; i < index; i++ ) 
        {
            u32 size = 0;
            if ( !stsz->GetSampleSize ( i, size ) ) return false;
            offset += size;
        }

        u32 sampleSize = 0;
        if ( !stsz->GetSampleSize ( index, sampleSize ) ) return false;

        auto stsd = _stsd.lock ( );
        u32 handler = AX_FOURCC ( '?', '?', '?', '?' );
        if ( stsd ) stsd->GetSampleDescription ( desc, handler );
       
        if ( auto mdat = _mdat.lock ( ) )
        {
            if ( mdat->IsZeroCopy ( ) )
            {
                sample = Sample ( handler, mdat->ZeroCopyDataWithOffset ( static_cast<off_t>( offset ) ), sampleSize, index, Size() );
            } else
            {
                sample = Sample ( handler, mdat->DataWithOffset ( static_cast<off_t>( offset ), sampleSize ), index, Size() );
            }
            return true;
        }
        
        return false;
    }

    u32 Track::SampleCount ( ) const
    {
        if ( auto stsz = _stsz.lock ( ) )
        {
            return stsz->GetSampleCount ( );
        }
        return 0;
    }

    float Track::DurationSeconds ( ) const
    {
        if ( auto mdhd = _mdhd.lock ( ) )
        {
            return mdhd->DurationSeconds ( );
        }

        return 0.0f;
    }
}