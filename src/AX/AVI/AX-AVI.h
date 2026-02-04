//
//  AX-AVI.h
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 26/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#ifdef AX_MEDIA_WITH_AVI

#pragma once

#include <vector>
#include <stack>
#include <AX/AX-MediaContainer.h>
#include "cinder/Stream.h"

namespace AX::Media::AVI
{
	enum class Tag : u32
	{
		kUnknown = AX_FOURCC ( '?', '?', '?', '?' ),
		kRIFF = AX_FOURCC ( 'R', 'I', 'F', 'F' )
	};

	inline std::string TagToString ( Tag tag )
	{
		return AX::FourCCToString ( static_cast<u32> ( tag ) );
	}

	class AVI;
	using ValueVariant = std::variant<u64, s64, std::string, double, std::vector<u8>>;
	using ElementRef = std::shared_ptr<class Element>;
	using ElementList = std::vector<ElementRef>;
	class Element : public Castable, public IInspectable
	{
	public:
		Element ( Tag tag )
			: _tag ( tag )
		{ }

		virtual bool				Parse ( AVI & context, const ci::IStreamRef & stream );

		const Tag &					Identifier ( ) const { return _tag; };
		virtual std::string			ToString ( ) const { return TagToString ( _tag ); }

		bool						HasInspectableChildren ( ) const override { return false; }
		std::string					InspectableValue ( ) const override { return TagToString ( _tag ); }
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

		usz					_size{ 0 };
		Tag					_tag{};
		ValueVariant		_value{};
	};

	class RiffElement : public Element
	{
	public:
		using Element::Element;
		virtual bool Parse ( AVI & context, const ci::IStreamRef & stream ) override
		{

		}
	};

	using ElementFactoryFn = std::function<ElementRef(Tag)>;
	using Format = AX::Media::Format;
	using AVIRef = std::shared_ptr<class AVI>;
	class AVI : public AX::Media::Container, public Element
	{
	public:

		static AVIRef       Create ( const ci::fs::path & path, const Format & format = Format ( ) );
		static AVIRef       Create ( const ci::DataSourceRef & source, const Format & format = Format ( ) );

		virtual bool		Parse ( AVI & context, const ci::IStreamRef & stream ) override;

		ElementRef          ReadNextElement ( const ci::IStreamRef& stream );
		void                Dump ( std::ostream & stream, bool verbose = false ) const override;

		static bool			CanProcessSource ( const ci::DataSourceRef & );

		void                Push ( Element * element );
		void                Pop ( );
		Element *			Top ( ) const;
		u32                 StackDepth ( ) const override { return static_cast<u32>( _stack.size ( ) ); }

		virtual operator AX::IInspectable * ( ) override { return (Element *)this; }

	protected:
		AVI					( const ci::DataSourceRef & source, const Format & format );
		using				FactoryMap = std::unordered_map<Tag, ElementFactoryFn>;

		bool                Load ( );
		bool                StartsWithRIFF ( const ci::IStreamRef & stream );
		void                RegisterElementFactory ( Tag type, ElementFactoryFn fn ) { _factories[type] = fn; }

		using ElementStack = std::stack<Element *>;
		ElementStack		_stack;
		FactoryMap			_factories;
		ci::BufferRef		_buffer;
	};

	using AVIMovieRef = std::shared_ptr<class AVIMovie>;
	class AVIMovie : public Movie
	{
	public:
		AVIMovie ( const ContainerRef & container );
	};

	using AVITrackRef = std::shared_ptr<class AVITrack>;
	class AVITrack : public Track
	{
	public:
		AVITrack	( );
		bool		ReadSample ( u32 index, Sample & sample ) const override;

	protected:

		ci::IStreamRef	_stream;
	};
}

#endif