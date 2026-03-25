//
//  AX-MKV.h
//  AX-MKV
//
//  Created by Andrew Wright (@axjxwright) on 24/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#ifdef AX_MEDIA_WITH_MKV

#pragma once

#include <vector>
#include <stack>
#include <AX/AX-MediaContainer.h>
#include "cinder/Stream.h"
#include "AX-MKVSchema.h"

namespace AX::Media::MKV
{
	class MKV;
	using ValueVariant = std::variant<u64, s64, std::string, double, std::vector<u8>>;
	using ElementRef = std::shared_ptr<class Element>;
	using ElementList = std::vector<ElementRef>;
	class Element : public Castable, public IInspectable
	{
	public:
		Element ( const MatroskaIdentifier & identifier )
			: _identifier ( identifier )
		{ }

		virtual bool				Parse ( MKV & context, const ci::IStreamRef& stream, usz length ) = 0;

		const MatroskaIdentifier &	Identifier ( ) const { return _identifier; };
		virtual std::string			ToString  ( ) const { return _identifier.Name; }
		
		bool						HasInspectableChildren ( ) const override { return _identifier.Type == MatroskaDataType::kMaster; }
		std::string					InspectableValue ( ) const override { return _identifier.Name; }
		std::vector<IInspectable *> InspectableChildren ( ) const override { return {}; };

		template <typename T>
		T							Value ( T defaultValue = {} ) const
		{
			try
			{
				return std::get<T> ( _value );
			} catch ( const std::exception & e )
			{
				std::printf ( "Error reading value: %s\n", e.what ( ) );
				return defaultValue;
			}
		}
		
		virtual			~Element ( ) { };

	protected:

		MatroskaIdentifier	_identifier;
		ValueVariant		_value{};
	};

	class UnhandledElement : public Element
	{
	public:
		using Element::Element;
		virtual bool	Parse ( MKV & context, const ci::IStreamRef & stream, usz length ) override;
		std::string		ToString ( ) const override;

	protected:
		std::string		_stringValue;
	};
	
	class MasterElement : public Element
	{
	public:
		using Element::Element;
		virtual bool		Parse ( MKV & context, const ci::IStreamRef & stream, usz length ) override;

		virtual ElementRef  FindFirstChild ( MatroskaElementId type, bool recursive = true ) const;
		virtual ElementList	FindChildren ( MatroskaElementId type, bool recursive = true ) const;
		const ElementList&	GetChildren ( ) const { return _children; }

		std::vector<IInspectable *> InspectableChildren ( ) const override 
		{ 
			std::vector<IInspectable *> result;
			for ( auto & child : GetChildren ( ) )
			{
				result.push_back ( child.get ( ) );
			}
			return result;
		};

		template <typename T>
		std::shared_ptr<T> FindFirstChildOfType ( bool recursive = true ) const
		{
			for ( auto & child : GetChildren ( ) )
			{
				if ( auto type = child->TryCast<T> ( ) ) return type;
			}

			if ( recursive )
			{
				for ( auto & child : GetChildren ( ) )
				{
					if ( child->Identifier ( ).Type == MatroskaDataType::kMaster)
					{
						if ( auto result = child->TryCast<MasterElement> ( )->FindFirstChildOfType<T> ( recursive ) )
						{
							return result;
						}
					}
				}
			}

			return nullptr;
		}

	protected:
		std::vector<ElementRef> _children;
	};

	using ClusterElementRef = std::shared_ptr<class ClusterElement>;
	class ClusterElement : public MasterElement
	{
	public:
		using MasterElement::MasterElement;
		virtual bool	Parse ( MKV & context, const ci::IStreamRef & stream, usz length ) override;
		u64				Timestamp ( ) const { return _timestamp; };
	;
	protected:
		u64				_timestamp{ 0 };
	};

	using SimpleBlockElementRef = std::shared_ptr<class SimpleBlockElement>;
	using BlockList = std::vector<SimpleBlockElementRef>;
	class SimpleBlockElement : public Element
	{
	public:
		using Element::Element;
		virtual bool	Parse ( MKV & context, const ci::IStreamRef & stream, usz length ) override;

		u64				PositionInStream ( ) const { return _positionInStream; };
		u64				LengthInStream ( ) const { return _lengthInStream; };
		u8				TrackNumber ( ) const { return _trackNumber; }
		u16				Timestamp ( ) const { return _timestamp; }
		float			AbsoluteTimestampSeconds ( ) const { return _absoluteTimestampSeconds; }
		void			SetAbsoluteTimestampSeconds ( float timestamp ) { _absoluteTimestampSeconds = timestamp; }
		bool			IsKeyframe ( ) const { return _isKeyframe; }

		bool			IsZeroCopy ( ) const { return _isZeroCopy; }
		const u8 *		ZeroCopyData ( ) const;
		std::vector<u8>	Data ( ) const;

	protected:
		u64				_positionInStream{ 0 };
		usz				_lengthInStream;
		u8				_trackNumber{ 0 };
		u16				_timestamp{ 0 };
		float			_absoluteTimestampSeconds{ 0.0f };
		bool			_isKeyframe{ false };	
		u64				_offsetFromStartOfFile{ 0 };
		std::vector<u8>	_data;
		ci::IStreamRef	_stream;
		bool			_isZeroCopy{ false };
		bool			_ownsMemory{ false };
		u64				_blockLength{ 0 };
	};

	class BlockElement : public SimpleBlockElement
	{
	public:
		using SimpleBlockElement::SimpleBlockElement;
	};

	using TrackElementRef	= std::shared_ptr<class TrackElement>;
	class TrackElement		: public MasterElement
	{
	public:
		using MasterElement::MasterElement;
		virtual bool		Parse ( MKV & context, const ci::IStreamRef & stream, usz length ) override;

		u64					TrackNumber ( ) const { return _trackNumber; }
		std::string			CodecId ( ) const { return _codecId; }
		TrackType			Type ( ) const { return _trackType; }
		u32					Handler ( ) const { return _handler; }
		float				Duration ( ) const { return _duration; }
		u32					Width ( ) const { return _width; }
		u32					Height ( ) const { return _height; }
		
		const BlockList &	Blocks ( ) const { return _blocks; }

		void				CollectSamples ( MKV & container );
		bool				FillSample ( u32 index, Sample & sample );

	protected:
		u64					_trackNumber;
		std::string			_codecId;
		u32					_handler{ 0 };
		TrackType			_trackType;
		float				_duration{ 0 };
		u32					_width{ 0 };
		u32					_height{ 0 };
		BlockList			_blocks;
		ci::IStreamRef		_stream;
	};

	using SkipElementRef = std::shared_ptr<class SkipElement>;
	class SkipElement : public Element
	{
	public:
		using Element::Element;
		virtual bool Parse ( MKV & context, const ci::IStreamRef & stream, usz length ) override
		{
			stream->seekRelative ( length );
			return true;
		}
	};

	using ElementFactoryFn = std::function<ElementRef(MatroskaIdentifier)>;
	using Format = AX::Media::Format;
	using MKVRef = std::shared_ptr<class MKV>;
	class MKV : public AX::Media::Container, public MasterElement
	{
	public:

		static MKVRef       Create ( const ci::fs::path & path, const Format & format = Format ( ) );
		static MKVRef       Create ( const ci::DataSourceRef & source, const Format & format = Format ( ) );

		virtual bool		Parse ( MKV & context, const ci::IStreamRef & stream, usz length ) override;
		u64					TimestampScale ( ) const { return _timestampScale; }

		ElementRef          ReadNextElement ( const ci::IStreamRef& stream );
		void                Dump ( std::ostream & stream, bool verbose = false ) const override;

		static bool			CanProcessSource ( const ci::DataSourceRef & );

		void                Push ( Element * element );
		void                Pop ( );
		Element *			Top ( ) const;
		u32                 StackDepth ( ) const override { return static_cast<u32>( _stack.size ( ) ); }

		virtual operator AX::IInspectable * ( ) override { return (MasterElement *)this; }

	protected:
		MKV					( const ci::DataSourceRef & source, const Format & format );
		using				FactoryMap = std::unordered_map<MatroskaElementId, ElementFactoryFn>;

		bool                Load ( );
		bool                StartsWithEBML ( const ci::IStreamRef & stream );
		void                RegisterElementFactory ( MatroskaElementId type, ElementFactoryFn fn ) { _factories[type] = fn; }

		using ElementStack = std::stack<Element *>;
		ElementStack		_stack;
		FactoryMap			_factories;
		ci::BufferRef		_buffer;
		u64					_timestampScale{ 1 };
	};

	using MKVMovieRef = std::shared_ptr<class MKVMovie>;
	class MKVMovie : public Movie
	{
	public:
		MKVMovie ( const ContainerRef & container );
	};

	using MKVTrackRef = std::shared_ptr<class MKVTrack>;
	class MKVTrack : public Track
	{
	public:
		MKVTrack	( const TrackElementRef& track );
		bool		ReadSample ( u32 index, Sample & sample ) const override;

	protected:

		TrackElementRef	_track;
		ci::IStreamRef	_stream;
	};
}

#endif