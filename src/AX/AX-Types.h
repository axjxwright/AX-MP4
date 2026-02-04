//
//  AX-Types.h
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 24/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#pragma once

#include "cinder/Cinder.h"
#include "cinder/DataSource.h"

#include <memory>
#include <cstdint>
#include <unordered_map>

namespace AX
{
	using s8 = std::int8_t;
	using u8 = std::uint8_t;

	using s16 = std::int16_t;
	using u16 = std::uint16_t;

	using s32 = std::int32_t;
	using u32 = std::uint32_t;

	using s64 = std::int64_t;
	using u64 = std::uint64_t;

	using usz = std::size_t;
	using ivec2 = glm::ivec2;

	#define AX_FOURCC(c1,c2,c3,c4) (((static_cast<uint32_t>(c1))<<24) | ((static_cast<uint32_t>(c2))<<16) | ((static_cast<uint32_t>(c3))<< 8) | ((static_cast<uint32_t>(c4)) ))

	inline std::string FourCCToString ( u32 fcc, bool trim = true )
	{
		char chars[4] =
		{
			static_cast<char>( ( fcc >> 24 ) & 0xFF ),
			static_cast<char>( ( fcc >> 16 ) & 0xFF ),
			static_cast<char>( ( fcc >> 8 ) & 0xFF ),
			static_cast<char>( ( fcc >> 0 ) & 0xFF )
		};

		auto result = std::string ( chars, 4 );
		if ( trim ) while ( result.back ( ) == ' ' ) result.pop_back ( );
		return result;
	}

	class IInspectable
	{
	public:
		using PropertyMap					= std::unordered_map<std::string, std::string>;

		virtual bool						HasInspectableChildren ( ) const = 0;
		virtual std::vector<IInspectable *> InspectableChildren ( ) const = 0;
		virtual std::string					InspectableValue ( ) const = 0;
		const PropertyMap &					Properties ( ) const { return _properties; }

	protected:
		template <typename T>
		void WriteProperty ( const std::string & name, const T & value )
		{
			_properties[name] = std::to_string ( value );
		}

		template <>
		void WriteProperty ( const std::string & name, const std::string & value )
		{
			_properties[name] = value;
		}

		PropertyMap _properties{};
	};

	using CastableRef = std::shared_ptr<class Castable>;
	class Castable : public std::enable_shared_from_this<Castable>
	{
	public:

		virtual ~Castable ( ) { };

		template <typename T>
		std::shared_ptr<const T> As ( ) const { return std::static_pointer_cast<T> ( shared_from_this ( ) ); }

		template <typename T>
		std::shared_ptr<T> As ( ) { return std::static_pointer_cast<T> ( shared_from_this ( ) ); }

		template <typename T>
		std::shared_ptr<const T> TryCast ( ) const { return std::dynamic_pointer_cast<T> ( shared_from_this ( ) ); }

		template <typename T>
		std::shared_ptr<T> TryCast ( ) { return std::dynamic_pointer_cast<T> ( shared_from_this ( ) ); }

		template <typename T>
		bool Is ( ) const { return TryCast<T> ( ) != nullptr; }
	};

}