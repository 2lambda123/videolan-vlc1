/*****************************************************************************
 * events.hpp : matroska demuxer
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

#ifndef VLC_MKV_DEMUX_EVENTS_HPP
#define VLC_MKV_DEMUX_EVENTS_HPP

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_actions.h>
#include <vlc_mouse.h>

#include "dvd_types.hpp"

#include <list>

struct vlc_spu_highlight_t;

namespace mkv {

struct demux_sys_t;

class event_thread_t
{
public:
    event_thread_t(demux_t *);
    virtual ~event_thread_t();

    void SetPci(const pci_t *data);
    void ResetPci();
    int SendEventNav( int );
    void SetHighlight( vlc_spu_highlight_t & spu_hl );

    bool AddES( es_out_id_t* es, int category );
    void DelES( es_out_id_t* es );

private:
    struct ESInfo {
        ESInfo( es_out_id_t* es, int category, event_thread_t& owner )
            : es( es )
            , category( category )
            , owner( owner )
        {
            vlc_mouse_Init( &mouse_state );
        }

        bool operator==( es_out_id_t* es ) const {
            return this->es == es;
        }

        es_out_id_t* es;
        int category;
        event_thread_t& owner;
        vlc_mouse_t mouse_state;
    };

    struct EventInfo {
        enum {
            ESMouseEvent,
            ActionEvent,
        } type;

        EventInfo( ESInfo* info, vlc_mouse_t state_old, vlc_mouse_t state_new )
            : type( ESMouseEvent )
        {
            mouse.es_info = info;
            mouse.state_old = state_old;
            mouse.state_new = state_new;
        }

        EventInfo( int query )
            : type( ActionEvent )
        {
            nav.query = query;
        }

        union {
            struct {
                ESInfo* es_info;
                vlc_mouse_t state_old;
                vlc_mouse_t state_new;
            } mouse;

            struct {
                int query;
            } nav;
        };
    };

    void EventThread();
    static void *EventThread(void *);

    static void EventMouse( vlc_mouse_t const* state, void* userdata );

    void HandleKeyEvent( EventInfo const& );
    void HandleMouseEvent( EventInfo const& );

    void ProcessNavAction( uint16_t button, const pci_t & pci );

    demux_t      *p_demux;

    bool         is_running;
    vlc_thread_t thread;

    vlc_mutex_t  lock;
    vlc_cond_t   wait;
    bool         b_abort;

    typedef std::list<ESInfo> es_list_t;
    es_list_t es_list;

    typedef std::list<EventInfo> pending_events_t;
    pending_events_t pending_events;

private:
    void HandleKeyEvent( NavivationKey key );
};
} // namespace

#endif
