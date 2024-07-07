/*****************************************************************************
 * chapter_command.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "chapter_command.hpp"
#include "demux.hpp"
#include <algorithm>

namespace mkv {

constexpr binary MATROSKA_DVD_LEVEL_SS   = 0x30;
constexpr binary MATROSKA_DVD_LEVEL_LU   = 0x2A;
constexpr binary MATROSKA_DVD_LEVEL_TT   = 0x28;
constexpr binary MATROSKA_DVD_LEVEL_PGC  = 0x20;
constexpr binary MATROSKA_DVD_LEVEL_PG   = 0x18;
constexpr binary MATROSKA_DVD_LEVEL_PTT  = 0x10;
constexpr binary MATROSKA_DVD_LEVEL_CN   = 0x08;

void chapter_codec_cmds_c::AddCommand( const KaxChapterProcessCommand & command )
{
    uint32_t codec_time = uint32_t(-1);
    for( size_t i = 0; i < command.ListSize(); i++ )
    {
        if( MKV_CHECKED_PTR_DECL_CONST( p_cpt, KaxChapterProcessTime, command[i] ) )
        {
            codec_time = static_cast<uint32_t>( *p_cpt );
            break;
        }
    }

    for( size_t i = 0; i < command.ListSize(); i++ )
    {
        if( MKV_CHECKED_PTR_DECL_CONST( p_cpd, KaxChapterProcessData, command[i] ) )
        {
            std::vector<KaxChapterProcessData*> *containers[] = {
                &during_cmds, /* codec_time = 0 */
                &enter_cmds,  /* codec_time = 1 */
                &leave_cmds   /* codec_time = 2 */
            };

            if( codec_time < 3 )
                containers[codec_time]->push_back( new KaxChapterProcessData( *p_cpd ) );
        }
    }
}

int16_t dvd_chapter_codec_c::GetTitleNumber()
{
    if ( p_private_data->GetSize() >= 3)
    {
        const binary* p_data = p_private_data->GetBuffer();
        if ( p_data[0] == MATROSKA_DVD_LEVEL_SS )
        {
            return int16_t( (p_data[2] << 8) + p_data[3] );
        }
    }
    return -1;
}

bool dvd_chapter_codec_c::Enter()
{
    return EnterLeaveHelper( "Matroska DVD enter command", &enter_cmds );
}

bool dvd_chapter_codec_c::Leave()
{
    return EnterLeaveHelper( "Matroska DVD leave command", &leave_cmds );
}

bool dvd_chapter_codec_c::EnterLeaveHelper( char const * str_diag, std::vector<KaxChapterProcessData*> * p_container )
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator it = p_container->begin ();
    while( it != p_container->end() )
    {
        if( (*it)->GetSize() )
        {
            binary *p_data = (*it)->GetBuffer();
            size_t i_size  = std::min<size_t>( *p_data++, ( (*it)->GetSize() - 1 ) >> 3 ); // avoid reading too much
            for( ; i_size > 0; i_size -=1, p_data += 8 )
            {
                vlc_debug( l, "%s", str_diag);
                f_result |= intepretor.Interpret( p_data );
            }
        }
        ++it;
    }
    return f_result;
}


std::string dvd_chapter_codec_c::GetCodecName( bool f_for_title ) const
{
    std::string result;
    if ( p_private_data->GetSize() >= 3)
    {
        const binary* p_data = p_private_data->GetBuffer();
/*        if ( p_data[0] == MATROSKA_DVD_LEVEL_TT )
        {
            uint16_t i_title = (p_data[1] << 8) + p_data[2];
            char psz_str[11];
            sprintf( psz_str, " %d  ---", i_title );
            result = "---  DVD Title";
            result += psz_str;
        }
        else */ if ( p_data[0] == MATROSKA_DVD_LEVEL_LU )
        {
            char psz_str[11];
            snprintf( psz_str, ARRAY_SIZE(psz_str), " (%c%c)  ---", p_data[1], p_data[2] );
            result = "---  DVD Menu";
            result += psz_str;
        }
        else if ( p_data[0] == MATROSKA_DVD_LEVEL_SS && f_for_title )
        {
            if ( p_data[1] == 0x00 )
                result = "First Played";
            else if ( p_data[1] == 0xC0 )
                result = "Video Manager";
            else if ( p_data[1] == 0x80 )
            {
                uint16_t i_title = (p_data[2] << 8) + p_data[3];
                char psz_str[20];
                snprintf( psz_str, ARRAY_SIZE(psz_str), " %d -----", i_title );
                result = "----- Title";
                result += psz_str;
            }
        }
    }

    return result;
}

// see http://www.dvd-replica.com/DVD/vmcmdset.php for a description of DVD commands
bool dvd_command_interpretor_c::Interpret( const binary * p_command, size_t i_size )
{
    if ( i_size != 8 )
        return false;

    virtual_segment_c *p_vsegment = NULL;
    virtual_chapter_c *p_vchapter = NULL;
    bool f_result = false;
    uint16_t i_command = ( p_command[0] << 8 ) + p_command[1];

    // handle register tests if there are some
    if ( (i_command & 0xF0) != 0 )
    {
        bool b_test_positive = true;//(i_command & CMD_DVD_IF_NOT) == 0;
        bool b_test_value    = (i_command & CMD_DVD_TEST_VALUE) != 0;
        uint8_t i_test = i_command & 0x70;
        uint16_t i_value;

        // see http://dvd.sourceforge.net/dvdinfo/vmi.html
        uint8_t  i_cr1;
        uint16_t i_cr2;
        switch ( i_command >> 12 )
        {
        default:
            i_cr1 = p_command[3];
            i_cr2 = (p_command[4] << 8) + p_command[5];
            break;
        case 3:
        case 4:
        case 5:
            i_cr1 = p_command[6];
            i_cr2 = p_command[7];
            b_test_value = false;
            break;
        case 6:
        case 7:
            if ( ((p_command[1] >> 4) & 0x7) == 0)
            {
                i_cr1 = p_command[4];
                i_cr2 = (p_command[6] << 8) + p_command[7];
            }
            else
            {
                i_cr1 = p_command[5];
                i_cr2 = (p_command[6] << 8) + p_command[7];
            }
            break;
        }

        if ( b_test_value )
            i_value = i_cr2;
        else
            i_value = GetPRM( i_cr2 );

        switch ( i_test )
        {
        case CMD_DVD_IF_GPREG_EQUAL:
            // if equals
            vlc_debug( l, "IF %s EQUALS %s", GetRegTypeName( false, i_cr1 ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) == i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_NOT_EQUAL:
            // if not equals
            vlc_debug( l, "IF %s NOT EQUALS %s", GetRegTypeName( false, i_cr1 ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) != i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_INF:
            // if inferior
            vlc_debug( l, "IF %s < %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) < i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_INF_EQUAL:
            // if inferior or equal
            vlc_debug( l, "IF %s < %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) <= i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_AND:
            // if logical and
            vlc_debug( l, "IF %s & %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) & i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_SUP:
            // if superior
            vlc_debug( l, "IF %s >= %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) > i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_SUP_EQUAL:
            // if superior or equal
            vlc_debug( l, "IF %s >= %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) >= i_value ))
            {
                b_test_positive = false;
            }
            break;
        }

        if ( !b_test_positive )
            return false;
    }

    // strip the test command
    i_command &= 0xFF0F;

    switch ( i_command )
    {
    case CMD_DVD_NOP:
    case CMD_DVD_NOP2:
        {
            vlc_debug( l, "NOP" );
            break;
        }
    case CMD_DVD_BREAK:
        {
            vlc_debug( l, "Break" );
            // TODO
            break;
        }
    case CMD_DVD_JUMP_TT:
        {
            uint8_t i_title = p_command[5];
            vlc_debug( l, "JumpTT %d", i_title );

            // find in the ChapProcessPrivate matching this Title level
            p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                [i_title](const chapter_codec_cmds_c &data) {
                    return MatchTitleNumber(data, i_title);
                }, p_vsegment );
            if ( p_vsegment != NULL && p_vchapter != NULL )
            {
                /* enter via the First Cell */
                uint8_t i_cell = 1;
                p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                    [i_cell](const chapter_codec_cmds_c &data) {
                        return MatchCellNumber( data, i_cell );
                    });
                if ( p_vchapter != NULL )
                {
                    vm.JumpTo( *p_vsegment, *p_vchapter );
                    f_result = true;
                }
            }

            break;
        }
    case CMD_DVD_CALLSS_VTSM1:
        {
            vlc_debug( l, "CallSS" );
            binary p_type;
            switch( (p_command[6] & 0xC0) >> 6 ) {
                case 0:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x00:
                        vlc_debug( l, "CallSS PGC (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x02:
                        vlc_debug( l, "CallSS Title Entry (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x03:
                        vlc_debug( l, "CallSS Root Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x04:
                        vlc_debug( l, "CallSS Subpicture Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x05:
                        vlc_debug( l, "CallSS Audio Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x06:
                        vlc_debug( l, "CallSS Angle Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x07:
                        vlc_debug( l, "CallSS Chapter Menu (rsm_cell %x)", p_command[4]);
                        break;
                    default:
                        vlc_debug( l, "CallSS <unknown> (rsm_cell %x)", p_command[4]);
                        break;
                    }
                    p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                        [p_type](const chapter_codec_cmds_c &data) {
                            return MatchPgcType( data, p_type );
                        }, p_vsegment );
                    if ( p_vsegment != NULL && p_vchapter != NULL )
                    {
                        /* enter via the first Cell */
                        uint8_t i_cell = 1;
                        p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                            [i_cell](const chapter_codec_cmds_c &data) {
                                return MatchCellNumber( data, i_cell ); } );
                        if ( p_vchapter != NULL )
                        {
                            vm.JumpTo( *p_vsegment, *p_vchapter );
                            f_result = true;
                        }
                    }
                break;
                case 1:
                    vlc_debug( l, "CallSS VMGM (menu %d, rsm_cell %x)", p_command[5] & 0x0F, p_command[4]);
                break;
                case 2:
                    vlc_debug( l, "CallSS VTSM (menu %d, rsm_cell %x)", p_command[5] & 0x0F, p_command[4]);
                break;
                case 3:
                    vlc_debug( l, "CallSS VMGM (pgc %d, rsm_cell %x)", (p_command[2] << 8) + p_command[3], p_command[4]);
                break;
            }
            break;
        }
    case CMD_DVD_JUMP_SS:
        {
            vlc_debug( l, "JumpSS");
            binary p_type;
            switch( (p_command[5] & 0xC0) >> 6 ) {
                case 0:
                    vlc_debug( l, "JumpSS FP");
                break;
                case 1:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x02:
                        vlc_debug( l, "JumpSS VMGM Title Entry");
                        break;
                    case 0x03:
                        vlc_debug( l, "JumpSS VMGM Root Menu");
                        break;
                    case 0x04:
                        vlc_debug( l, "JumpSS VMGM Subpicture Menu");
                        break;
                    case 0x05:
                        vlc_debug( l, "JumpSS VMGM Audio Menu");
                        break;
                    case 0x06:
                        vlc_debug( l, "JumpSS VMGM Angle Menu");
                        break;
                    case 0x07:
                        vlc_debug( l, "JumpSS VMGM Chapter Menu");
                        break;
                    default:
                        vlc_debug( l, "JumpSS <unknown>");
                        break;
                    }
                    // find the VMG
                    p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                        [](const chapter_codec_cmds_c &data) {
                            return MatchIsVMG( data); }, p_vsegment );
                    if ( p_vsegment != NULL )
                    {
                        p_vchapter = p_vsegment->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                            [p_type](const chapter_codec_cmds_c &data) {
                                return MatchPgcType( data, p_type ); } );
                        if ( p_vchapter != NULL )
                        {
                            vm.JumpTo( *p_vsegment, *p_vchapter );
                            f_result = true;
                        }
                    }
                break;
                case 2:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x02:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Title Entry", p_command[4], p_command[3]);
                        break;
                    case 0x03:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Root Menu", p_command[4], p_command[3]);
                        break;
                    case 0x04:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Subpicture Menu", p_command[4], p_command[3]);
                        break;
                    case 0x05:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Audio Menu", p_command[4], p_command[3]);
                        break;
                    case 0x06:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Angle Menu", p_command[4], p_command[3]);
                        break;
                    case 0x07:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Chapter Menu", p_command[4], p_command[3]);
                        break;
                    default:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) <unknown>", p_command[4], p_command[3]);
                        break;
                    }

                    {
                    uint8_t i_vts = p_command[4];
                    p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                        [i_vts](const chapter_codec_cmds_c &data) {
                            return MatchVTSMNumber( data,  i_vts ); }, p_vsegment );

                    if ( p_vsegment != NULL && p_vchapter != NULL )
                    {
                        // find the title in the VTS
                        uint8_t i_title = p_command[3];
                        p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                            [i_title](const chapter_codec_cmds_c &data) {
                                return MatchTitleNumber( data, i_title ); } );
                        if ( p_vchapter != NULL )
                        {
                            // find the specified menu in the VTSM
                            p_vchapter = p_vsegment->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                                [p_type](const chapter_codec_cmds_c &data) {
                                    return MatchPgcType( data, p_type ); } );
                            if ( p_vchapter != NULL )
                            {
                                vm.JumpTo( *p_vsegment, *p_vchapter );
                                f_result = true;
                            }
                        }
                        else
                            vlc_debug( l, "Title (%d) does not exist in this VTS", i_title );
                    }
                    else
                        vlc_debug( l, "DVD Domain VTS (%d) not found", i_vts );
                    }
                break;
                case 3:
                    vlc_debug( l, "JumpSS VMGM (pgc %d)", (p_command[2] << 8) + p_command[3]);
                break;
            }
            break;
        }
    case CMD_DVD_JUMPVTS_PTT:
        {
            uint8_t i_title = p_command[5];
            uint8_t i_ptt = p_command[3];

            vlc_debug( l, "JumpVTS Title (%d) PTT (%d)", i_title, i_ptt);

            // find the current VTS content segment
            p_vchapter = vm.GetCurrentVSegment()->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                [](const chapter_codec_cmds_c &data) {
                    return MatchIsDomain( data); } );
            if ( p_vchapter != NULL )
            {
                int16_t i_curr_title = ( p_vchapter->p_chapter )? p_vchapter->p_chapter->GetTitleNumber() : 0;
                if ( i_curr_title > 0 )
                {
                    p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                        [i_curr_title](const chapter_codec_cmds_c &data) {
                            return MatchVTSNumber( data, i_curr_title ); }, p_vsegment );

                    if ( p_vsegment != NULL && p_vchapter != NULL )
                    {
                        // find the title in the VTS
                        p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                            [i_title](const chapter_codec_cmds_c &data) {
                                return MatchTitleNumber( data, i_title ); } );
                        if ( p_vchapter != NULL )
                        {
                            // find the chapter in the title
                            p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                                [i_ptt](const chapter_codec_cmds_c &data) {
                                    return MatchChapterNumber( data, i_ptt ); } );
                            if ( p_vchapter != NULL )
                            {
                                vm.JumpTo( *p_vsegment, *p_vchapter );
                                f_result = true;
                            }
                        }
                    else
                        vlc_debug( l, "Title (%d) does not exist in this VTS", i_title );
                    }
                    else
                        vlc_debug( l, "DVD Domain VTS (%d) not found", i_curr_title );
                }
                else
                    vlc_debug( l, "JumpVTS_PTT command found but not in a VTS(M)");
            }
            else
                vlc_debug( l, "JumpVTS_PTT command but the DVD domain wasn't found");
            break;
        }
    case CMD_DVD_SET_GPRMMD:
        {
            vlc_debug( l, "Set GPRMMD [%d]=%d", (p_command[4] << 8) + p_command[5], (p_command[2] << 8) + p_command[3]);

            if ( !SetGPRM( (p_command[4] << 8) + p_command[5], (p_command[2] << 8) + p_command[3] ) )
                vlc_debug( l, "Set GPRMMD failed" );
            break;
        }
    case CMD_DVD_LINKPGCN:
        {
            uint16_t i_pgcn = (p_command[6] << 8) + p_command[7];

            vlc_debug( l, "Link PGCN(%d)", i_pgcn );
            p_vchapter = vm.GetCurrentVSegment()->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                [i_pgcn](const chapter_codec_cmds_c &data) {
                    return MatchPgcNumber( data, i_pgcn ); } );
            if ( p_vchapter != NULL )
            {
                vm.JumpTo( *vm.GetCurrentVSegment(), *p_vchapter );
                f_result = true;
            }
            break;
        }
    case CMD_DVD_LINKCN:
        {
            uint8_t i_cn = p_command[7];

            p_vchapter = vm.GetCurrentVSegment()->CurrentChapter();

            vlc_debug( l, "LinkCN (cell %d)", i_cn );
            p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                [i_cn](const chapter_codec_cmds_c &data) {
                    return MatchCellNumber( data, i_cn ); } );
            if ( p_vchapter != NULL )
            {
                vm.JumpTo( *vm.GetCurrentVSegment(), *p_vchapter );
                f_result = true;
            }
            break;
        }
    case CMD_DVD_GOTO_LINE:
        {
            vlc_debug( l, "GotoLine (%d)", (p_command[6] << 8) + p_command[7] );
            // TODO
            break;
        }
    case CMD_DVD_SET_HL_BTNN1:
        {
            vlc_debug( l, "SetHL_BTN (%d)", p_command[4] );
            SetSPRM( 0x88, p_command[4] );
            break;
        }
    default:
        {
            vlc_debug( l, "unsupported command : %02X %02X %02X %02X %02X %02X %02X %02X"
                     ,p_command[0]
                     ,p_command[1]
                     ,p_command[2]
                     ,p_command[3]
                     ,p_command[4]
                     ,p_command[5]
                     ,p_command[6]
                     ,p_command[7]);
            break;
        }
    }

    return f_result;
}



bool dvd_command_interpretor_c::MatchIsDomain( const chapter_codec_cmds_c &data )
{
    return ( data.p_private_data != NULL && data.p_private_data->GetBuffer()[0] == MATROSKA_DVD_LEVEL_SS );
}

bool dvd_command_interpretor_c::MatchIsVMG( const chapter_codec_cmds_c &data )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 2 )
        return false;

    return ( data.p_private_data->GetBuffer()[0] == MATROSKA_DVD_LEVEL_SS && data.p_private_data->GetBuffer()[1] == 0xC0);
}

bool dvd_command_interpretor_c::MatchVTSNumber( const chapter_codec_cmds_c &data, uint16_t i_title )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_SS || data.p_private_data->GetBuffer()[1] != 0x80 )
        return false;

    uint16_t i_gtitle = (data.p_private_data->GetBuffer()[2] << 8 ) + data.p_private_data->GetBuffer()[3];

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchVTSMNumber( const chapter_codec_cmds_c &data, uint8_t i_title )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_SS || data.p_private_data->GetBuffer()[1] != 0x40 )
        return false;

    uint8_t i_gtitle = data.p_private_data->GetBuffer()[3];

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchTitleNumber( const chapter_codec_cmds_c &data, uint8_t i_title )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_TT )
        return false;

    uint16_t i_gtitle = (data.p_private_data->GetBuffer()[1] << 8 ) + data.p_private_data->GetBuffer()[2];

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchPgcType( const chapter_codec_cmds_c &data, uint8_t i_pgc )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 8 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PGC )
        return false;

    uint8_t i_pgc_type = data.p_private_data->GetBuffer()[3] & 0x0F;

    return (i_pgc_type == i_pgc);
}

bool dvd_command_interpretor_c::MatchPgcNumber( const chapter_codec_cmds_c &data, uint16_t i_pgc_n )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 8 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PGC )
        return false;

    uint16_t i_pgc_num = (data.p_private_data->GetBuffer()[1] << 8) + data.p_private_data->GetBuffer()[2];

    return (i_pgc_num == i_pgc_n);
}

bool dvd_command_interpretor_c::MatchChapterNumber( const chapter_codec_cmds_c &data, uint8_t i_ptt )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 2 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PTT )
        return false;

    uint8_t i_chapter = data.p_private_data->GetBuffer()[1];

    return (i_chapter == i_ptt);
}

bool dvd_command_interpretor_c::MatchCellNumber( const chapter_codec_cmds_c &data, uint8_t i_cell_n )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 5 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_CN )
        return false;

    uint8_t i_cell_num = data.p_private_data->GetBuffer()[3];

    return (i_cell_num == i_cell_n);
}

const std::string matroska_script_interpretor_c::CMD_MS_GOTO_AND_PLAY = "GotoAndPlay";

// see http://www.matroska.org/technical/specs/chapters/index.html#mscript
//  for a description of existing commands
bool matroska_script_interpretor_c::Interpret( const binary * p_command, size_t i_size )
{
    bool b_result = false;

    std::string sz_command( reinterpret_cast<const char*> (p_command), i_size );

    vlc_debug( l, "command : %s", sz_command.c_str() );

    if ( sz_command.compare( 0, CMD_MS_GOTO_AND_PLAY.size(), CMD_MS_GOTO_AND_PLAY ) == 0 )
    {
        size_t i,j;

        // find the (
        for ( i=CMD_MS_GOTO_AND_PLAY.size(); i<sz_command.size(); i++)
        {
            if ( sz_command[i] == '(' )
            {
                i++;
                break;
            }
        }
        // find the )
        for ( j=i; j<sz_command.size(); j++)
        {
            if ( sz_command[j] == ')' )
            {
                i--;
                break;
            }
        }

        std::string st = sz_command.substr( i+1, j-i-1 );
        chapter_uid i_chapter_uid = std::stoul( st );

        virtual_segment_c *p_vsegment;
        virtual_chapter_c *p_vchapter = vm.FindVChapter( i_chapter_uid, p_vsegment );

        if ( p_vchapter == NULL )
            vlc_debug( l, "Chapter %" PRId64 " not found", i_chapter_uid);
        else
        {
            if ( !p_vchapter->EnterAndLeave( vm.GetCurrentVSegment()->CurrentChapter(), false ) )
                vm.JumpTo( *p_vsegment, *p_vchapter );
            b_result = true;
        }
    }

    return b_result;
}

bool matroska_script_codec_c::Enter()
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator index = enter_cmds.begin();
    while ( index != enter_cmds.end() )
    {
        if ( (*index)->GetSize() )
        {
            vlc_debug( l, "Matroska Script enter command" );
            f_result |= interpreter.Interpret( (*index)->GetBuffer(), (*index)->GetSize() );
        }
        ++index;
    }
    return f_result;
}

bool matroska_script_codec_c::Leave()
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator index = leave_cmds.begin();
    while ( index != leave_cmds.end() )
    {
        if ( (*index)->GetSize() )
        {
            vlc_debug( l, "Matroska Script leave command" );
            f_result |= interpreter.Interpret( (*index)->GetBuffer(), (*index)->GetSize() );
        }
        ++index;
    }
    return f_result;
}

} // namespace
