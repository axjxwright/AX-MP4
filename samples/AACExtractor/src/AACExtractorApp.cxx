//
//  AACExtractor.cxx
//  AX-MP4
//
//  Created by Andrew Wright (@axjxwright) on 13/01/26.
//  (c) 2026 AX Interactive (axinteractive.com.au)
//

#include <AX/AX-MediaContainer.h>

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "cinder/CinderImGui.h"
#include "cinder/audio/audio.h"

namespace ui = ImGui;

using namespace ci;
using namespace ci::app;

#ifdef CINDER_MSW
extern "C"
{
    __declspec( dllexport ) unsigned int NvOptimusEnablement = 0x1;
}
#endif

class AACDecoderApp : public App
{
public:
    void                            setup    ( ) override;
    void                            update   ( ) override;
    void                            draw     ( ) override;
    void                            cleanup  ( ) override;
    void                            fileDrop ( FileDropEvent event ) override;

protected:

    void                            ExtractAudio    ( const AX::Media::TrackRef& track );
    bool                            LoadMedia     ( const DataSourceRef& source );

    AX::Media::ContainerRef			_container;
    AX::Media::MovieRef				_movie;
    AX::Media::TrackRef				_track{ nullptr };
    
    audio::FilePlayerNodeRef        _player;
    audio::MonitorNodeRef           _fft;
};

void AACDecoderApp::setup ( )
{
    gl::enableVerticalSync ( false );
    setFrameRate ( 100000 );

    ui::Initialize ( );
    
    LoadMedia ( loadAsset ( "sample-3.m4a" ) );
}

bool AACDecoderApp::LoadMedia ( const DataSourceRef& source )
{
    if ( _player )
    {
        _player->disconnectAll ( );
        _player = nullptr;
    }

    if ( _fft )
    {
        _fft->disconnectAll ( );
        _fft = nullptr;
    }

    audio::master ( )->disconnectAllNodes ( );

    try
    {
        _container = AX::Media::Container::Create ( source, AX::Media::Format{}.TrackProperties(true).PreloadIntoMemory(true) );
        if ( _container )
        {
            if ( _container->IsValid ( ) )
            {
                _movie = AX::Media::Movie::Create ( _container );
                if ( auto track = _movie->GetTrack ( AX::Media::TrackType::kAudio, 0 ) )
                {
					_track = track;
                    ExtractAudio ( _track );
                }
                //_container->Dump ( std::cout, true );
                
                return true;
            } else
            {
				std::printf ( "Error loading MP4: %s\n", AX::Media::ErrorCodeToString ( _container->Error ( ) ) );
            }
        }
    } catch ( const std::exception& e )
    {
        std::printf ( "Error loading MP4: %s\n", e.what ( ) );
    }

    return false;
}

void AACDecoderApp::update ( )
{
    
}

void AACDecoderApp::fileDrop ( FileDropEvent event )
{
    LoadMedia ( loadFile ( event.getFile ( 0 ) ) );
}

struct ADTSHeader
{
    uint16_t SyncWord{ 0xFFF };
    uint8_t  ID{ 0 };                   
    uint8_t  Layer{ 0 };                
    uint8_t  ProtectionAbsent{ 1 };    
    uint8_t  Profile{ 1 };              
    uint8_t  SamplingFrequencyIndex;
    uint8_t  PrivateBit{ 0 };          
    uint8_t  ChannelConfiguration;
    uint8_t  Originality{ 0 };          
    uint8_t  Home{ 0 };                 
    uint8_t  CopyrightIDBit{ 0 };     
    uint8_t  CopyrightIDStart{ 0 };   
    uint16_t AACFrameLength{ 0 };
    uint16_t ADTSBufferFullness{ 0x7FF };
    uint8_t  NumAACFramesInRawBlock{ 0 };
};

std::vector<uint8_t> GenerateADTSHeader ( AX::u32 sampleRate, AX::u32 channelCount, AX::u32 aacFrameLength ) 
{
    static std::vector<AX::u32> kSampleRates = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 };
    auto GetSampleRateIndex = [&]( AX::u32 sampleRate )
    {
        auto it = std::find ( kSampleRates.begin ( ), kSampleRates.end ( ), sampleRate );
        if ( it == kSampleRates.end ( ) ) return (AX::u8)4; // 44100
        return static_cast<AX::u8> ( it - kSampleRates.begin ( ) );
    };

    ADTSHeader header{};
    header.SamplingFrequencyIndex = GetSampleRateIndex ( sampleRate );
    header.ChannelConfiguration = static_cast<AX::u8> ( channelCount );
    header.AACFrameLength = static_cast<AX::u16>(aacFrameLength + 7);

    std::vector<uint8_t> result ( 7 );

    result[0] = 0xFF;

    result[1] = 0xF0;
    result[1] |= ( header.ID << 3 ) & 0x08;
    result[1] |= ( header.Layer << 1 ) & 0x06;
    result[1] |= header.ProtectionAbsent & 0x01;

    result[2] = ( header.Profile << 6 ) & 0xC0;
    result[2] |= ( header.SamplingFrequencyIndex << 2 ) & 0x3C;
    result[2] |= ( header.PrivateBit << 1 ) & 0x02;
    result[2] |= ( header.ChannelConfiguration >> 2 ) & 0x01;

    result[3] = ( header.ChannelConfiguration << 6 ) & 0xC0;
    result[3] |= ( header.Originality << 5 ) & 0x20;
    result[3] |= ( header.Home << 4 ) & 0x10;
    result[3] |= ( header.CopyrightIDBit << 3 ) & 0x08;
    result[3] |= ( header.CopyrightIDStart << 2 ) & 0x04;
    result[3] |= ( header.AACFrameLength >> 11 ) & 0x03;

    result[4] = ( header.AACFrameLength >> 3 ) & 0xFF;

    result[5] = ( header.AACFrameLength << 5 ) & 0xE0;
    result[5] |= ( header.ADTSBufferFullness >> 6 ) & 0x1F;

    result[6] = ( header.ADTSBufferFullness << 2 ) & 0xFC;
    result[6] |= header.NumAACFramesInRawBlock & 0x03;

    return result;
}

void AACDecoderApp::ExtractAudio ( const AX::Media::TrackRef& track )
{
    std::vector<AX::u8> audio;
    for ( AX::u32 i = 0; i < track->SampleCount ( ); i++ )
    {
		AX::Media::Sample sample;
        if ( track->ReadSample ( i, sample ) )
        {
			// @FIXME(andrew): Occasionally the channel count reported
            // by the file is incorrect. Need to parse the ESDS descriptors
            // to get the actual correct channel counts.
            auto channelCount = track->ChannelCount ( );
            auto sampleRate = track->SampleRate();
            
			// @note(andrew): Prepend ADTS header to AAC samples
			if ( track->CodecId ( ) == 'mp4a' )
			{
				auto header = GenerateADTSHeader ( sampleRate, channelCount, sample.Length ( ) );
				std::copy ( header.begin ( ), header.end ( ), std::back_inserter ( audio ) );
			}
            std::copy ( sample.Data ( ), sample.Data ( ) + sample.Length ( ), std::back_inserter ( audio ) );
        }
    }

    auto stream = IStreamMem::create ( audio.data ( ), audio.size ( ) );
    auto file = audio::load ( DataSourceBuffer::create ( loadStreamBuffer ( stream ) ) );
    
    std::printf ( "duration: %.2f\n", file->getNumSeconds ( ) );

    auto ctx = audio::master ( );
    _player = ctx->makeNode<audio::FilePlayerNode> ( file );
    _fft = ctx->makeNode<audio::MonitorNode> ( audio::MonitorNode::Format ( ).windowSize ( 1024 ) );
    _player >> _fft >> ctx->getOutput ( );
   
    ctx->enable ( );
    _player->setLoopEnabled ( );
    _player->enable ( );
    _fft->enable ( );
}

static void Inspect ( AX::IInspectable* item )
{
    ui::ScopedId id{ item };
    if ( ui::TreeNode ( item->InspectableValue().c_str() ) )
    {
        for ( auto& [name, value] : item->Properties ( ) )
        {
            ui::BulletText ( "%s = %s", name.c_str ( ), value.c_str ( ) );
        }
        if ( item->HasInspectableChildren ( ) )
        {
            for ( auto& child : item->InspectableChildren ( ) )
            {
                Inspect ( child );
            }
        }
        ui::TreePop ( );
    }
}

void AACDecoderApp::draw ( )
{
    gl::clear ( Colorf::gray ( 0.02f ) );
    {
        static auto kRenderer = gl::getString ( GL_RENDERER );

        ui::ScopedWindow window{ "Settings" };
        ui::Text ( "AX Audio Extraction | FPS %.2f | %s", getAverageFps ( ), kRenderer.c_str() );

        if ( _player )
        {
            static double kPauseTime = _player->getReadPositionTime ( );
            double currentTime = _player->isEnabled ( ) ? _player->getReadPositionTime ( ) : kPauseTime;
            ui::Text ( "%.2f/%.2f", currentTime, _player->getNumSeconds() );
            if ( !_player->isEnabled() )
            {
                if ( ui::Button ( "Play" ) )
                {
                    _player->start ( );
                    _player->seekToTime ( kPauseTime );
                }
            } else
            {
                if ( ui::Button ( "Pause" ) )
                {
                    kPauseTime = _player->getReadPositionTime ( );
                    _player->stop ( );
                }
            }

            if ( _fft )
            {
                auto& buffer = _fft->getBuffer ( );
                ui::PlotLines ( "##", buffer.getChannel(0), static_cast<int>(buffer.getNumFrames()), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(ui::GetWindowWidth() - 2, 64 ) );
            }
        }

        if ( _container ) Inspect ( *_container );
    }
}

void AACDecoderApp::cleanup ( )
{
	audio::master()->disconnectAllNodes();

    _player = nullptr;
	_fft = nullptr;
}

void Init ( App::Settings* settings )
{
    settings->setWindowSize ( 1280, 720 );
#ifdef CINDER_MSW
    settings->setConsoleWindowEnabled ( true );
#endif
}

CINDER_APP ( AACDecoderApp, RendererGl ( RendererGl::Options ( ) ), Init );
