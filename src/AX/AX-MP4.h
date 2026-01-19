//
//  AX-MP4.h
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 13/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#pragma once

#include "cinder/Buffer.h"
#include "cinder/Stream.h"
#include "cinder/Cinder.h"
#include "cinder/CinderGlm.h"
#include "cinder/gl/Texture.h"
#include <asio/asio.hpp>
#include <thread>
#include <functional>
#include <unordered_map>
#include <stack>
#include <variant>

namespace AX
{
    #define AX_FOURCC(c1,c2,c3,c4) (((static_cast<uint32_t>(c1))<<24) | ((static_cast<uint32_t>(c2))<<16) | ((static_cast<uint32_t>(c3))<< 8) | ((static_cast<uint32_t>(c4)) ))

    using s8 = std::int8_t;
    using u8 = std::uint8_t;

    using s16 = std::int16_t;
    using u16 = std::uint16_t;

    using s32 = std::int32_t;
    using u32 = std::uint32_t;

    using s64 = std::int64_t;
    using u64 = std::uint64_t;

    using usz = std::size_t;

    // @FIXME(andrew): Can probably just use 'xxxx' literals for these
    enum class AtomType : u32
    {
        // @note(andrew): These are internal to us.
        kUNKN = AX_FOURCC ( '?', '?', '?', '?' ),
        kFILE = AX_FOURCC ( 'f', 'i', 'l', 'e' ),

        kUDTA = AX_FOURCC ( 'u', 'd', 't', 'a' ),
        kURL_ = AX_FOURCC ( 'u', 'r', 'l', ' ' ),
        kTRAK = AX_FOURCC ( 't', 'r', 'a', 'k' ),
        kTRAF = AX_FOURCC ( 't', 'r', 'a', 'f' ),
        kTKHD = AX_FOURCC ( 't', 'k', 'h', 'd' ),
        kTFHD = AX_FOURCC ( 't', 'f', 'h', 'd' ),
        kTRUN = AX_FOURCC ( 't', 'r', 'u', 'n' ),
        kSTTS = AX_FOURCC ( 's', 't', 't', 's' ),
        kSTSZ = AX_FOURCC ( 's', 't', 's', 'z' ),
        kSTZ2 = AX_FOURCC ( 's', 't', 'z', '2' ),
        kSTSS = AX_FOURCC ( 's', 't', 's', 's' ),
        kSTSD = AX_FOURCC ( 's', 't', 's', 'd' ),
        kSTSC = AX_FOURCC ( 's', 't', 's', 'c' ),
        kSTCO = AX_FOURCC ( 's', 't', 'c', 'o' ),
        kCO64 = AX_FOURCC ( 'c', 'o', '6', '4' ),
        kSTBL = AX_FOURCC ( 's', 't', 'b', 'l' ),
        kSINF = AX_FOURCC ( 's', 'i', 'n', 'f' ),
        kSCHM = AX_FOURCC ( 's', 'c', 'h', 'm' ),
        kSCHI = AX_FOURCC ( 's', 'c', 'h', 'i' ),
        kMVHD = AX_FOURCC ( 'm', 'v', 'h', 'd' ),
        kMEHD = AX_FOURCC ( 'm', 'e', 'h', 'd' ),
        kMP4S = AX_FOURCC ( 'm', 'p', '4', 's' ),
        kMP4A = AX_FOURCC ( 'm', 'p', '4', 'a' ),
        kMP4V = AX_FOURCC ( 'm', 'p', '4', 'v' ),
        kAVC1 = AX_FOURCC ( 'a', 'v', 'c', '1' ),
        kAVC2 = AX_FOURCC ( 'a', 'v', 'c', '2' ),
        kAVC3 = AX_FOURCC ( 'a', 'v', 'c', '3' ),
        kAVC4 = AX_FOURCC ( 'a', 'v', 'c', '4' ),
        kDVAV = AX_FOURCC ( 'd', 'v', 'a', 'v' ),
        kDVA1 = AX_FOURCC ( 'd', 'v', 'a', '1' ),
        kHEV1 = AX_FOURCC ( 'h', 'e', 'v', '1' ),
        kHVC1 = AX_FOURCC ( 'h', 'v', 'c', '1' ),
        kDVHE = AX_FOURCC ( 'd', 'v', 'h', 'e' ),
        kDVH1 = AX_FOURCC ( 'd', 'v', 'h', '1' ),
        kVP08 = AX_FOURCC ( 'v', 'p', '0', '8' ),
        kVP09 = AX_FOURCC ( 'v', 'p', '0', '9' ),
        kVP10 = AX_FOURCC ( 'v', 'p', '1', '0' ),
        kAV01 = AX_FOURCC ( 'a', 'v', '0', '1' ),
        kALAC = AX_FOURCC ( 'a', 'l', 'a', 'c' ),
        kENCA = AX_FOURCC ( 'e', 'n', 'c', 'a' ),
        kENCV = AX_FOURCC ( 'e', 'n', 'c', 'v' ),
        kMOOV = AX_FOURCC ( 'm', 'o', 'o', 'v' ),
        kMOOF = AX_FOURCC ( 'm', 'o', 'o', 'f' ),
        kMVEX = AX_FOURCC ( 'm', 'v', 'e', 'x' ),
        kTREX = AX_FOURCC ( 't', 'r', 'e', 'x' ),
        kMINF = AX_FOURCC ( 'm', 'i', 'n', 'f' ),
        kMETA = AX_FOURCC ( 'm', 'e', 't', 'a' ),
        kMDHD = AX_FOURCC ( 'm', 'd', 'h', 'd' ),
        kMFHD = AX_FOURCC ( 'm', 'f', 'h', 'd' ),
        kILST = AX_FOURCC ( 'i', 'l', 's', 't' ),
        kHDLR = AX_FOURCC ( 'h', 'd', 'l', 'r' ),
        kFTYP = AX_FOURCC ( 'f', 't', 'y', 'p' ),
        kIODS = AX_FOURCC ( 'i', 'o', 'd', 's' ),
        kESDS = AX_FOURCC ( 'e', 's', 'd', 's' ),
        kEDTS = AX_FOURCC ( 'e', 'd', 't', 's' ),
        kDRMS = AX_FOURCC ( 'd', 'r', 'm', 's' ),
        kDRMI = AX_FOURCC ( 'd', 'r', 'm', 'i' ),
        kDREF = AX_FOURCC ( 'd', 'r', 'e', 'f' ),
        kDINF = AX_FOURCC ( 'd', 'i', 'n', 'f' ),
        kCTTS = AX_FOURCC ( 'c', 't', 't', 's' ),
        kMDIA = AX_FOURCC ( 'm', 'd', 'i', 'a' ),
        kELST = AX_FOURCC ( 'e', 'l', 's', 't' ),
        kVMHD = AX_FOURCC ( 'v', 'm', 'h', 'd' ),
        kSMHD = AX_FOURCC ( 's', 'm', 'h', 'd' ),
        kNMHD = AX_FOURCC ( 'n', 'm', 'h', 'd' ),
        kSTHD = AX_FOURCC ( 's', 't', 'h', 'd' ),
        kHMHD = AX_FOURCC ( 'h', 'm', 'h', 'd' ),
        kFRMA = AX_FOURCC ( 'f', 'r', 'm', 'a' ),
        kMDAT = AX_FOURCC ( 'm', 'd', 'a', 't' ),
        kFREE = AX_FOURCC ( 'f', 'r', 'e', 'e' ),
        kTIMS = AX_FOURCC ( 't', 'i', 'm', 's' ),
        kRTP_ = AX_FOURCC ( 'r', 't', 'p', ' ' ),
        kHNTI = AX_FOURCC ( 'h', 'n', 't', 'i' ),
        kSDP_ = AX_FOURCC ( 's', 'd', 'p', ' ' ),
        kIKMS = AX_FOURCC ( 'i', 'K', 'M', 'S' ),
        kISFM = AX_FOURCC ( 'i', 'S', 'F', 'M' ),
        kISLT = AX_FOURCC ( 'i', 'S', 'L', 'T' ),
        kTREF = AX_FOURCC ( 't', 'r', 'e', 'f' ),
        kHINT = AX_FOURCC ( 'h', 'i', 'n', 't' ),
        kCDSC = AX_FOURCC ( 'c', 'd', 's', 'c' ),
        kMPOD = AX_FOURCC ( 'm', 'p', 'o', 'd' ),
        kIPIR = AX_FOURCC ( 'i', 'p', 'i', 'r' ),
        kCHAP = AX_FOURCC ( 'c', 'h', 'a', 'p' ),
        kALIS = AX_FOURCC ( 'a', 'l', 'i', 's' ),
        kSYNC = AX_FOURCC ( 's', 'y', 'n', 'c' ),
        kDPND = AX_FOURCC ( 'd', 'p', 'n', 'd' ),
        kODRM = AX_FOURCC ( 'o', 'd', 'r', 'm' ),
        kODKM = AX_FOURCC ( 'o', 'd', 'k', 'm' ),
        kOHDR = AX_FOURCC ( 'o', 'h', 'd', 'r' ),
        kODDA = AX_FOURCC ( 'o', 'd', 'd', 'a' ),
        kODHE = AX_FOURCC ( 'o', 'd', 'h', 'e' ),
        kODAF = AX_FOURCC ( 'o', 'd', 'a', 'f' ),
        kGRPI = AX_FOURCC ( 'g', 'r', 'p', 'i' ),
        kIPRO = AX_FOURCC ( 'i', 'p', 'r', 'o' ),
        kMDRI = AX_FOURCC ( 'm', 'd', 'r', 'i' ),
        kAVCC = AX_FOURCC ( 'a', 'v', 'c', 'C' ),
        kHVCC = AX_FOURCC ( 'h', 'v', 'c', 'C' ),
        kDVCC = AX_FOURCC ( 'd', 'v', 'c', 'C' ),
        kVPCC = AX_FOURCC ( 'v', 'p', 'c', 'C' ),
        kDVVC = AX_FOURCC ( 'd', 'v', 'v', 'C' ),
        kHVCE = AX_FOURCC ( 'h', 'v', 'c', 'E' ),
        kAVCE = AX_FOURCC ( 'a', 'v', 'c', 'E' ),
        kAV1C = AX_FOURCC ( 'a', 'v', '1', 'C' ),
        kWAVE = AX_FOURCC ( 'w', 'a', 'v', 'e' ),
        kWIDE = AX_FOURCC ( 'w', 'i', 'd', 'e' ),
        kUUID = AX_FOURCC ( 'u', 'u', 'i', 'd' ),
        k8ID_ = AX_FOURCC ( '8', 'i', 'd', ' ' ),
        k8BDL = AX_FOURCC ( '8', 'b', 'd', 'l' ),
        kAC_3 = AX_FOURCC ( 'a', 'c', '-', '3' ),
        kEC_3 = AX_FOURCC ( 'e', 'c', '-', '3' ),
        kAC_4 = AX_FOURCC ( 'a', 'c', '-', '4' ),
        kDTSC = AX_FOURCC ( 'd', 't', 's', 'c' ),
        kDTSH = AX_FOURCC ( 'd', 't', 's', 'h' ),
        kDTSL = AX_FOURCC ( 'd', 't', 's', 'l' ),
        kDTSE = AX_FOURCC ( 'd', 't', 's', 'e' ),
        kFLAC = AX_FOURCC ( 'f', 'L', 'a', 'C' ),
        kOPUS = AX_FOURCC ( 'O', 'p', 'u', 's' ),
        kMFRA = AX_FOURCC ( 'm', 'f', 'r', 'a' ),
        kTFRA = AX_FOURCC ( 't', 'f', 'r', 'a' ),
        kMFRO = AX_FOURCC ( 'm', 'f', 'r', 'o' ),
        kTFDT = AX_FOURCC ( 't', 'f', 'd', 't' ),
        kTENC = AX_FOURCC ( 't', 'e', 'n', 'c' ),
        kSENC = AX_FOURCC ( 's', 'e', 'n', 'c' ),
        kSAIO = AX_FOURCC ( 's', 'a', 'i', 'o' ),
        kSAIZ = AX_FOURCC ( 's', 'a', 'i', 'z' ),
        kPDIN = AX_FOURCC ( 'p', 'd', 'i', 'n' ),
        kBLOC = AX_FOURCC ( 'b', 'l', 'o', 'c' ),
        kAINF = AX_FOURCC ( 'a', 'i', 'n', 'f' ),
        kPSSH = AX_FOURCC ( 'p', 's', 's', 'h' ),
        kMARL = AX_FOURCC ( 'm', 'a', 'r', 'l' ),
        kMKID = AX_FOURCC ( 'm', 'k', 'i', 'd' ),
        kPRFT = AX_FOURCC ( 'p', 'r', 'f', 't' ),
        kSTPP = AX_FOURCC ( 's', 't', 'p', 'p' ),
        kDAC3 = AX_FOURCC ( 'd', 'a', 'c', '3' ),
        kDEC3 = AX_FOURCC ( 'd', 'e', 'c', '3' ),
        kDAC4 = AX_FOURCC ( 'd', 'a', 'c', '4' ),
        kSIDX = AX_FOURCC ( 's', 'i', 'd', 'x' ),
        kSSIX = AX_FOURCC ( 's', 's', 'i', 'x' ),
        kSBGP = AX_FOURCC ( 's', 'b', 'g', 'p' ),
        kSGPD = AX_FOURCC ( 's', 'g', 'p', 'd' ),

        kTAPT = AX_FOURCC ( 't', 'a', 'p', 't' ),
        kCLEF = AX_FOURCC ( 'c', 'l', 'e', 'f' ),

        kMECO = AX_FOURCC ( 'm', 'e', 'c', 'o' ),
        kMERE = AX_FOURCC ( 'm', 'e', 'r', 'e' ),
        kIPRP = AX_FOURCC ( 'i', 'p', 'r', 'p' ),
        kFIIN = AX_FOURCC ( 'f', 'i', 'i', 'n' ),
        kPAEN = AX_FOURCC ( 'p', 'a', 'e', 'n' ),
        kSTRK = AX_FOURCC ( 's', 't', 'r', 'k' ),

        kPITM = AX_FOURCC ( 'p', 'i', 't', 'm' ),
        kIINF = AX_FOURCC ( 'i', 'i', 'n', 'f' ),
        kURN_ = AX_FOURCC ( 'u', 'r', 'n', ' ' ),
        kILOC = AX_FOURCC ( 'i', 'l', 'o', 'c' ),
        kIREF = AX_FOURCC ( 'i', 'r', 'e', 'f' ),
        kINFE = AX_FOURCC ( 'i', 'n', 'f', 'e' ),
        kIROT = AX_FOURCC ( 'i', 'r', 'o', 't' ),
        kDIMG = AX_FOURCC ( 'd', 'i', 'm', 'g' ),
        kTHMB = AX_FOURCC ( 't', 'h', 'm', 'b' ),
        kCOLR = AX_FOURCC ( 'c', 'o', 'l', 'r' ),
        kISPE = AX_FOURCC ( 'i', 's', 'p', 'e' ),
        kIPMA = AX_FOURCC ( 'i', 'p', 'm', 'a' ),
        kPIXI = AX_FOURCC ( 'p', 'i', 'x', 'i' ),
        kIPCO = AX_FOURCC ( 'i', 'p', 'c', 'o' ),
    };

    enum class HDLRType : u32
    {
        kMHLR = AX_FOURCC ( 'm', 'h', 'l', 'r' ),
        kDHLR = AX_FOURCC ( 'd', 'h', 'l', 'r' ),
    };

    enum class HDLRSubtype : u32
    {
        kSOUN = AX_FOURCC ( 's', 'o', 'u', 'n' ),
        kVIDE = AX_FOURCC ( 'v', 'i', 'd', 'e' ),
        kHINT = AX_FOURCC ( 'h', 'i', 'n', 't' ),
        kMDIR = AX_FOURCC ( 'm', 'd', 'i', 'r' ),
        kTEXT = AX_FOURCC ( 't', 'e', 'x', 't' ),
        kTX3G = AX_FOURCC ( 't', 'x', '3', 'g' ),
        kJPEG = AX_FOURCC ( 'j', 'p', 'e', 'g' ),
        kODSM = AX_FOURCC ( 'o', 'd', 's', 'm' ),
        kSDSM = AX_FOURCC ( 's', 'd', 's', 'm' ),
        kSUBT = AX_FOURCC ( 's', 'u', 'b', 't' ),
        kSBTL = AX_FOURCC ( 's', 'b', 't', 'l' ),
    };

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

    inline std::string AtomTypeToString ( AtomType type )
    {
        return FourCCToString ( static_cast<u32> ( type ) );
    }

    inline std::string HandlerTypeToString ( HDLRType type )
    {
        return FourCCToString ( static_cast<u32> ( type ) );
    }

    inline std::string HandlerSubTypeToString ( HDLRSubtype type )
    {
        return FourCCToString ( static_cast<u32> ( type ) );
    }

    class MP4;

    using CastableRef = std::shared_ptr<class Castable>;
    class Castable : public std::enable_shared_from_this<Castable>
    {
    public:

        virtual ~Castable ( ) {};

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

    using AtomRef   = std::shared_ptr<class Atom>;
    class Atom      : public Castable
    {
    public:
        // @FIXME(andrew): Bit gross
        friend class ContainerAtom;
        using PropertyMap   = std::unordered_map<std::string, std::string>;

        struct AtomInfo
        {
            u32     Depth{};
            usz     Size{};
            off_t   Offset{};
        };

        virtual ~Atom ( )   {};
        virtual AtomType    Type ( ) const = 0;
        
        virtual void        Parse       ( MP4 & context, const ci::IStreamRef & stream, usz expectedLength ) = 0;
        virtual bool        IsContainer ( ) const { return false; }
        virtual bool        IsFull      ( ) const { return false; }
        const AtomInfo&     Info        ( ) const { return _info; }
        void                SetInfo     ( const AtomInfo& info ) { _info = info; }
        const PropertyMap&  Properties  ( ) const { return _properties; }
        virtual std::string ToString    ( ) const;

        template <typename T>
        void WriteProperty ( const std::string& name, const T& value )
        {
            _properties[name] = std::to_string ( value );
        }

        template <>
        void WriteProperty ( const std::string& name, const std::string& value )
        {
            _properties[name] = value;
        }

    protected:

        
        AtomInfo    _info{ 0 };
        PropertyMap _properties{};
    };

    template<typename T>
    using AtomListT = std::vector<std::shared_ptr<T>>;
    using AtomList = AtomListT<Atom>;

    using IContainerAtomRef = std::shared_ptr<class IContainerAtom>;
    class IContainerAtom
    {
    public:
        virtual AtomRef         FindFirstChild ( AtomType type, bool recursive = true ) const = 0;
        virtual AtomList        FindChildren ( AtomType type, bool recursive = true ) const = 0;
        virtual void            AddChild ( const AtomRef& atom ) = 0;
        virtual const AtomList& GetChildren ( ) const = 0;

        template <typename T>
        std::shared_ptr<T> FindFirstChildOfType ( bool recursive = true ) const
        {
            for ( auto& child : GetChildren ( ) )
            {
                if ( auto type = child->TryCast<T> ( ) ) return type;
            }

            if ( recursive )
            {
                for ( auto& child : GetChildren ( ) )
                {
                    if ( child->IsContainer ( ) )
                    {
                        if ( auto result = child->TryCast<IContainerAtom> ( )->FindFirstChildOfType<T> ( recursive ) )
                        {
                            return result;
                        }
                    }
                }
            }

            return nullptr;
        }

        virtual ~IContainerAtom ( ) {};
    };

    using BaseContainerAtomRef = std::shared_ptr<class BaseContainerAtom>;
    class BaseContainerAtom : public IContainerAtom
    {
    public:
        void            AddChild ( const AtomRef& atom ) override { _children.push_back ( atom ); }
        const AtomList& GetChildren ( ) const override { return _children; }

        AtomRef         FindFirstChild ( AtomType type, bool recursive = true ) const override;
        AtomList        FindChildren ( AtomType type, bool recursive = true ) const override;

        template <typename T>
        std::shared_ptr<T> FindFirstChildAs ( AtomType type, bool recursive = true ) const
        {
            if ( auto child = FindFirstChild ( type, recursive ) )
            {
                return child->TryCast<T> ( );
            }

            return nullptr;
        }

        template <typename T>
        AtomListT<T> FindChildrenAs ( AtomType type, bool recursive = true )
        {
            auto children = FindChildren ( type, recursive );
            AtomListT<T> cast;
            cast.reserve ( children.size ( ) );
            for ( auto& child : children )
            {
                if ( auto is = child->TryCast<T> ( ) )
                {
                    cast.push_back ( is );
                }
            }

            return cast;
        }


    protected:
        AtomList    _children;
    };

    using ContainerAtomRef = std::shared_ptr<class ContainerAtom>;
    class ContainerAtom : public Atom, public BaseContainerAtom
    {
    public:
        ContainerAtom ( AtomType type = AtomType::kUNKN )
            : _type ( type )
        { }
        
        AtomType Type ( ) const override { return _type; }

        void Parse ( MP4 & context, const ci::IStreamRef & stream, usz expectedLength ) override;
        bool IsContainer ( ) const override { return true; }

    protected:
        AtomType                _type{ AtomType::kUNKN };
    };

    using FileAtomRef = std::shared_ptr<class FileAtom>;
    class FileAtom : public ContainerAtom
    {
    public:
        FileAtom ( ) : ContainerAtom ( AtomType::kFILE ) {};
    };

    using UnknownAtomRef = std::shared_ptr<class UnknownAtom>;
    class UnknownAtom : public Atom
    {
    public:
        UnknownAtom ( AtomType type = AtomType::kUNKN )
            : _type ( type )
        { }

        AtomType    Type ( ) const override { return _type; }
        void        Parse ( MP4 & context, const ci::IStreamRef & stream, usz expectedLength ) override;

    protected:
        AtomType    _type{ AtomType::kUNKN };  
    };
    
    using IFullAtomRef  = std::shared_ptr<class IFullAtom>;
    class IFullAtom
    {
    public:
        virtual u8      Version ( ) const = 0;
        virtual u32     Flags   ( ) const = 0;
        virtual ~IFullAtom ( ) { };
    };

    using BaseFullAtomRef = std::shared_ptr<class BaseFullAtom>;
    class BaseFullAtom : public IFullAtom
    {
    public:
        BaseFullAtom ( AtomType type = AtomType::kUNKN ) : _type ( type ) {};
        u8           Version ( ) const { return  _version; }
        u32          Flags ( ) const { return    _flags; }

    protected:
        u8           _version{ 0 };
        u32          _flags{ 0 };
        AtomType     _type{ AtomType::kUNKN };
    };

    using FullAtomRef = std::shared_ptr<class FullAtom>;
    class FullAtom : public Atom, public BaseFullAtom
    {
    public:
        FullAtom ( AtomType type = AtomType::kUNKN )
            : BaseFullAtom ( type )
        { }

        AtomType    Type ( ) const override { return _type; }
        void        Parse ( MP4& context, const ci::IStreamRef & stream, usz expectedLength ) override;
        bool        IsFull ( ) const override { return true; }
    };

    using FTYPAtomRef = std::shared_ptr<class FTYPAtom>;
    class FTYPAtom : public Atom
    {
    public:
        using BrandList = std::vector<std::string>;

        virtual AtomType Type ( ) const override { return AtomType::kFTYP; }
        void             Parse ( MP4 & context, const ci::IStreamRef & stream, usz expectedLength ) override;

        u32              MinorVersion ( ) const { return _minorVersion; }
        std::string      Brand ( ) const { return _brand; }
        const BrandList& CompatibleBrands ( ) const { return _compatibleBrands; }
    
    protected:
        u32              _minorVersion{ 0 };
        std::string      _brand;
        BrandList        _compatibleBrands;
    };

    using TKHDAtomRef = std::shared_ptr<class TKHDAtom>;
    class TKHDAtom : public FullAtom
    {
    public:
        AtomType    Type ( ) const override { return AtomType::kTKHD; }
        void        Parse ( MP4 & context, const ci::IStreamRef & stream, usz expectedLength ) override;

        u64         CreationTime    ( ) const { return _creationTime; }
        u64         ModificationTime( ) const { return _modificationTime; }
        u32         TrackID         ( ) const { return _trackID; };
        u64         Duration        ( ) const { return _duration; };
        u16         Layer           ( ) const { return _layer; };
        u16         AlternateGroup  ( ) const { return _alternateGroup; };
        u16         Volume          ( ) const { return _volume; };
        glm::mat3   Matrix          ( ) const { return _matrix; };
        u32         Width           ( ) const { return _width; }
        u32         Height          ( ) const { return _height; }
    
    protected:
        u64         _creationTime{ 0 };
        u64         _modificationTime{ 0 };
        u32         _trackID{ 0 };
        u64         _duration{ 0 };
        u16         _layer{ 0 };
        u16         _alternateGroup{ 0 };
        u16         _volume{ 0 };
        glm::mat3   _matrix{ };
        u32         _width{ 0 };
        u32         _height{ 0 }; 
    };

    using MDHDAtomRef = std::shared_ptr<class MDHDAtom>;
    class MVHDAtom : public FullAtom
    {
    public:
        MVHDAtom ( )        : FullAtom ( AtomType::kMVHD ) {};
        void                Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        u64                 CreationTime     ( ) const { return _creationTime; }
        u64                 ModificationTime ( ) const { return _modificationTime; }
        u32                 TimeScale        ( ) const { return _timeScale; }
        u64                 Duration         ( ) const { return _duration; }
        u32                 Rate             ( ) const { return _rate; }
        u16                 Volume           ( ) const { return _volume; }
        const glm::mat3&    Matrix           ( ) const { return _matrix; }
        u32                 NextTrackID      ( ) const { return _nextTrackID; }
        float               DurationSeconds  ( ) const { return _durationSeconds; }

    protected:
        u64                 _creationTime{ 0 };
        u64                 _modificationTime{ 0 };
        u32                 _timeScale{ 0 };
        u64                 _duration{ 0 };
        u32                 _rate{ 0 };
        u16                 _volume{ 0 };
        glm::mat3           _matrix{ };
        u32                 _nextTrackID{ 0 };
        float               _durationSeconds{ 0 };
    };

    using MDHDAtomRef = std::shared_ptr<class MDHDAtom>;
    class MDHDAtom : public FullAtom
    {
    public:
        AtomType    Type ( ) const override { return AtomType::kMDHD; }
        void        Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        u64         CreationTime ( ) const { return _creationTime; }
        u64         ModificationTime ( ) const { return _modificationTime; }
        u32         TimeScale ( ) const { return _timeScale; }
        u64         Duration ( ) const { return _duration; }
        float       DurationSeconds ( ) const { return _durationSeconds; }
        
    protected:
        u64         _creationTime{ 0 };
        u64         _modificationTime{ 0 };
        u32         _timeScale{ 0 };
        u64         _duration{ 0 };
        float       _durationSeconds{ 0 };
    };

    using MDATAtomRef = std::shared_ptr<class MDATAtom>;
    class MDATAtom : public Atom
    {
    public:
        AtomType            Type ( ) const override { return AtomType::kMDAT; }
        void                Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        std::vector<u8>     DataWithOffset ( off_t offset, size_t size ) const;
        const u8*           ZeroCopyDataWithOffset ( off_t offset ) const;
        usz                 DataSize ( ) const { return _length; }
        bool                OwnsMemory ( ) const { return _ownsMemory; }
        bool                IsZeroCopy ( ) const { return _isZeroCopy; }

    protected:
        
        std::vector<u8>     _data;
        ci::IStreamRef      _stream;
        bool                _ownsMemory{ false };
        bool                _isZeroCopy{ false };
        u32                 _offsetFromStartOfFile{ 0 };
        usz                 _length{ 0 };
    };

    using STSDAtomRef = std::shared_ptr<class STSDAtom>;
    class STSDAtom : public ContainerAtom
    {
    public:
        AtomType Type ( ) const override { return AtomType::kSTSD; }
        void     Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        bool GetSampleDescription ( u32 index, u32& description )
        {
            if ( index == 0 || index > _descriptions.size ( )  ) return false;
            description = _descriptions[index-1];
            return true;
        }
    protected:
        std::vector<u32>    _descriptions; // @TODO(andrew): These have more data inside
    };

    using STSZAtomRef = std::shared_ptr<class STSZAtom>;
    class STSZAtom : public FullAtom
    {
    public:
        AtomType Type ( ) const override { return AtomType::kSTSZ; }
        void     Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        u32  GetSampleCount ( ) const { return _sampleCount; }
        bool GetSampleSize  ( u32 index, u32& sample ) const
        {
            if ( index > _sampleSizes.size ( ) || index == 0 )
            {
                sample = 0;
                return false;
            }
            sample = _sampleSizes[index - 1];
            return true;
        }

    protected:
        u32                 _sampleCount{ 0 };
        std::vector<u32>    _sampleSizes;
    };

    using STTSAtomRef = std::shared_ptr<class STTSAtom>;
    class STTSAtom : public FullAtom
    {
    public:

        struct Entry
        {
            u32  SampleCount{ 0 };
            u32  SampleDelta{ 0 };
        };

        AtomType Type  ( ) const override { return AtomType::kSTTS; }
        void     Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        u32      GetEntryCount ( ) const { return _entryCount; }
        bool     GetEntry      ( u32 index, Entry& entry ) const
        {
            if ( index > _entries.size ( ) || index == 0 )
            {
                return false;
            }
            entry = _entries[index - 1];
            return true;
        }

    protected:
        u32                 _entryCount{ 0 };
        std::vector<Entry>  _entries;
    };

    using HDLRAtomRef = std::shared_ptr<class HDLRAtom>;
    class HDLRAtom : public FullAtom
    {
    public:
        AtomType    Type ( ) const override { return AtomType::kHDLR; }
        void        Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        HDLRType    Handler     ( ) const { return _handlerType; }
        HDLRSubtype Subtype     ( ) const { return _handlerSubType; }
        const std::string& Name ( ) const { return _handlerName; }

    protected:
        HDLRType    _handlerType{ 0 };
        HDLRSubtype _handlerSubType{ 0 };
        std::string _handlerName{ "Unknown" };
    };

    using STCOAtomRef = std::shared_ptr<class STCOAtom>;
    class STCOAtom : public FullAtom
    {
    public:
        AtomType Type ( ) const override { return AtomType::kSTCO; }
        void     Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;
        
        u32  GetChunkCount ( ) const { return _chunkCount; }
        bool GetChunkOffset ( u32 index, u32& offset ) const
        {
            if ( index == 0 || index > _chunkOffsets.size() ) return false;
            offset = _chunkOffsets[index-1];
            return true;
        }

    protected:
        u32                     _chunkCount{ 0 };
        std::vector<u32>        _chunkOffsets;
    };

    using STSCAtomRef = std::shared_ptr<class STSCAtom>;
    class STSCAtom : public FullAtom
    {
    public:
        struct Chunk
        {
            u32 FirstChunk{ 0 };
            u32 FirstSample{ 0 };
            u32 ChunkCount{ 0 };
            u32 SamplesPerChunk{ 0 };
            u32 SampleDescIndex{ 0 };
        };

        AtomType    Type ( ) const override { return AtomType::kSTSC; }
        void        Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        bool        GetChunkForSample ( u32 index, u32& chunk, u32& skip, u32& desc ) const;
        
    protected:
        std::vector<Chunk>  _chunks;
    };

    using SampleEntryAtomRef = std::shared_ptr<class SampleEntryAtom>;
    class SampleEntryAtom : public ContainerAtom
    {
    public:
        SampleEntryAtom ( AtomType type ) : ContainerAtom ( type ) {};
        void            Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

    protected:
        u16             _dataReferenceIndex{ 0 };
        u8              _reserved[6]{};
    };

    using VideoSampleEntryAtomRef = std::shared_ptr<class VideoSampleEntryAtom>;
    class VideoSampleEntryAtom : public SampleEntryAtom
    {
    public:
        using SampleEntryAtom::SampleEntryAtom;

        void        Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        u16         Width           ( ) const { return _width; }
        u16         Height          ( ) const { return _height; }
        u32         HorzResolution  ( ) const { return _horzResolution; }
        u32         VertResolution  ( ) const { return _vertResolution; }
        std::string Compressor      ( ) const { return _compressor; }
        u16         Depth           ( ) const { return _depth; }

    protected:
        u16         _width{ 0 };
        u16         _height{ 0 };
        u32         _horzResolution{ 0 };
        u32         _vertResolution{ 0 };
        std::string _compressor{};
        u16         _depth{ 0 };
    };

    using AudioSampleEntryAtomRef = std::shared_ptr<class AudioSampleEntryAtom>;
    class AudioSampleEntryAtom : public SampleEntryAtom
    {
    public:
        using       SampleEntryAtom::SampleEntryAtom;
        void        Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

        u16         ChannelCount  ( ) const { return _channelCount; }
        u16         BitsPerSample ( ) const { return _bitsPerSample; }
        u32         SampleRate    ( ) const { return _sampleRate; }

    protected:
        u16         _channelCount{ 0 };
        u16         _bitsPerSample{ 0 };
        u32         _sampleRate{ 0 };
    };

    using MP4ASampleEntryAtomRef = std::shared_ptr<class MP4ASampleEntryAtom>;
    class MP4ASampleEntryAtom : public AudioSampleEntryAtom
    {
    public:
        using       AudioSampleEntryAtom::AudioSampleEntryAtom;
        void        Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;
    };

    using ESDSAtomRef = std::shared_ptr<class ESDSAtom>;
    class ESDSAtom : public FullAtom
    {
    public:
        
        AtomType    Type ( ) const override { return AtomType::kESDS; }
        void        Parse ( MP4& context, const ci::IStreamRef& stream, usz expectedLength ) override;

    protected:
        
    };

    using AtomFactoryFn = std::function<AtomRef()>;

    enum class MP4ErrorCode
    {
        Success,
        InvalidHeader, // Doesn't start with FTYP atom
        Unknown
    };

    const char * MP4ErrorCodeToString ( MP4ErrorCode code );
    
    using MP4Ref = std::shared_ptr<class MP4>;
    class MP4 : public FileAtom
    {
    public:
        struct Format
        {
            Format& TrackProperties  ( bool track ) { _trackProperties = track; return *this; }
            bool    TracksProperties ( ) const { return _trackProperties; };

            Format& PreloadIntoMemory ( bool preload ) { _preloadIntoMemory = preload; return *this; }
            bool    PreloadsIntoMemory ( ) const { return _preloadIntoMemory; };

        protected:
            bool    _trackProperties{ false };
            bool    _preloadIntoMemory{ false };
        };

        friend class ContainerAtom;
        
        static MP4Ref               Create ( const ci::fs::path& path, const Format& format = Format() );
        static MP4Ref               Create ( const ci::DataSourceRef& source, const Format& format = Format() );

        AtomRef                     CreateAtom  ( AtomType type );
        void                        Dump        ( std::ostream& stream, bool verbose = false ) const;
        
        // @note(andrew): -1 to Ignore self
        u32                         StackDepth  ( ) const { return static_cast<u32>(_stack.size ( )) - 1; } 
        bool                        IsValid     ( ) const { return _isValid; }
        MP4ErrorCode                Error       ( ) const { return _error; }
        const Format&               Settings    ( ) const { return _format; }
        
    protected:
        MP4                         ( const ci::DataSourceRef& source, const Format& format );
        using FactoryMap            = std::unordered_map<AtomType, AtomFactoryFn>;
        
        bool                        Load           ( );
        bool                        StartsWithFTYP ( const ci::IStreamRef& stream ) const;

        void                        RegisterAtomFactory ( AtomType type, AtomFactoryFn fn );
        void                        Push ( ContainerAtom* atom );
        void                        Pop ( );

        ContainerAtom*              Top ( ) const;
        
        bool                        _isValid{ false };
        std::stack<ContainerAtom *> _stack;
        MP4ErrorCode                _error{ MP4ErrorCode::Unknown };
        FactoryMap                  _factories;
        Format                      _format;
        ci::DataSourceRef           _source;
    };

    class Sample
    {
    public:
        Sample ( ) {};
        Sample ( u32 handler, const u8* data, u32 length, u32 index, const ci::ivec2& size )
            : _handler ( handler )
            , _data ( data )
            , _length ( length )
            , _ownsData ( false )
            , _index ( index )
            , _size ( size )
        {}
        Sample ( u32 handler, const std::vector<u8>& data, u32 index, const ci::ivec2& size )
            : _handler ( handler )
            , _data ( data )
            , _length ( static_cast<u32>(data.size ( )) )
            , _ownsData ( true )
            , _index ( index )
            , _size ( size )
        {}

        u32       Handler ( ) const { return _handler; }
        const u8* Data ( ) const { return _ownsData ? std::get<std::vector<u8>> ( _data ).data ( ) : std::get<const u8 *> ( _data ); }
        u32       Length ( ) const { return _length; }
        u32       Index ( ) const { return _index; }

        const ci::ivec2& Dimensions ( ) const { return _size; }
        u32          Width ( ) const { return _size.x; }
        u32          Height ( ) const { return _size.y; }

    protected:

        using DataUnion = std::variant<std::vector<u8>, const u8*>;

        int             _handler{ 0 };
        u32             _length{ 0 };
        bool            _ownsData{ false };
        u32             _index{ 0 };
        ci::ivec2       _size;
        DataUnion       _data;
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

    ///
    /// Convenience Playback API
    /// 
    
    using ITrackDecoderRef = std::shared_ptr<class ITrackDecoder>;
    class ITrackDecoder : public Castable
    {
    public:
        friend class Track;
        using RawBufferRef      = std::shared_ptr<std::vector<u8>>;
        struct DecodedFrame
        {
            u32                 Index{ 0 };
            u32                 GPUFormat{ GL_RGBA };
            bool                IsCompressed{ false };
            u32                 Width{ 0 };
            u32                 Height{ 0 };
            float               DecodeTime{ 0.0f };
            u32                 Channels{ 0 };
            
            u64                 PixelBufferSize{ 0 };
            const u8*           PixelBuffer{ nullptr };
            
            void                SetPixelData ( const ci::Surface8uRef& surface );
            void                SetPixelData ( const ci::Channel8uRef& channel );
            void                SetPixelData ( const RawBufferRef& raw );

        protected:

            std::variant<ci::Surface8uRef, ci::Channel8uRef, RawBufferRef> _pixelData;
        };

        ITrackDecoder               ( u32 handler ) : _handler ( handler ) {}
        virtual ~ITrackDecoder      ( ) {};

        u32                         Handler ( ) const { return _handler; }
        virtual u32                 FrameCount ( ) const { return static_cast<u32>(_decodedFrames.size ( )); }
        virtual const DecodedFrame& FrameAt ( u32 index ) const { return _decodedFrames.at ( index ); }

        virtual bool                DecodeSucceeded ( ) const { return _succeeded; }
        virtual bool                Decode ( const Sample& sample ) = 0;
        virtual ci::gl::TextureRef  CreateTexture ( u32 index, const ci::gl::Texture::Format& fmt = ci::gl::Texture::Format ( ) ) const = 0;

    protected:
        std::vector<DecodedFrame>   _decodedFrames;
        u32                         _handler{ 0 };
        bool                        _succeeded{ false };
    };
   
    using TrackRef          = std::shared_ptr<class Track>;
    class Track             : public Castable
    {
    public:
        using               AsyncReadCallback = std::function<void ( u32, bool, const Sample&& )>;
        using               AsyncDecodeCallback = std::function<void ( u32, bool, const ITrackDecoderRef& )>;
        
        Track               ( const MDATAtomRef& mdat, const ContainerAtomRef& trak );
        
        TrackType           Type ( ) const;

        ITrackDecoderRef    DecodeSample ( u32 index ) const;
        void                DecodeSampleAsync ( u32 index, AsyncDecodeCallback callback ) const;
        
        bool                ReadSample ( u32 index, Sample& sample ) const;
        void                ReadSampleAsync ( u32 index, AsyncReadCallback callback ) const;
        
        template <typename T>
        void                RegisterDecoder ( u32 handler )
        {
            _decoders[handler] = [=] { return std::make_shared<T> ( handler ); };
        }

        ITrackDecoderRef    CreateDecoder ( u32 handler ) const;
        bool                HasDecoder ( u32 handler ) const { return _decoders.count ( handler ); }

        u32                 SampleCount ( ) const;
        float               DurationSeconds ( ) const;

        const glm::ivec2&   Size   ( ) const { return _size; }
        const int           Width  ( ) const { return _size.x; }
        const int           Height ( ) const { return _size.y; }

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
        using AsyncContextRef = std::unique_ptr<class AsyncContext>;
        
        template <typename T> 
        using Weak = std::weak_ptr<T>;

        Weak<ContainerAtom>     _trak{ };
        Weak<STSZAtom>          _stsz{ };
        Weak<STCOAtom>          _stco{ };
        Weak<STSCAtom>          _stsc{ };
        Weak<STSDAtom>          _stsd{ };
        Weak<MDATAtom>          _mdat{ };
        Weak<HDLRAtom>          _hdlr{ };
        Weak<MDHDAtom>          _mdhd{ };

        glm::ivec2              _size;
        mutable AsyncContextRef _async;

        using DecoderFactory    = std::function<ITrackDecoderRef()>;
        using DecoderFactoryMap = std::unordered_map<u32, DecoderFactory>;

        DecoderFactoryMap       _decoders;
    };

    using AudioTrackRef = std::shared_ptr<class AudioTrack>;
    class AudioTrack : public Track
    {
    public:
        AudioTrack  ( const MDATAtomRef& mdat, const ContainerAtomRef& trak );

        u32         SampleRate ( ) const { return _sampleRate; }
        u32         ChannelCount ( ) const { return _channelCount; }

    protected:

        u32         _sampleRate{ 0 };
        u32         _channelCount{ 0 };
    };

    using MovieRef = std::shared_ptr<class Movie>;
    class Movie : public Castable
    {
    public:
        static MovieRef                 Create     ( const MP4Ref& mp4 );
        
        const std::vector<TrackRef>&    GetTracks  ( ) const { return _tracks; }
        std::vector<TrackRef>           FindTracks ( TrackType type ) const;
        TrackRef                        GetTrack   ( TrackType type, u32 index = 0 ) const;

    protected:

        Movie ( const MP4Ref& mp4 );

        MP4Ref                          _mp4;
        std::vector<TrackRef>           _tracks;
    };
}

