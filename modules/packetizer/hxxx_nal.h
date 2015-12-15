/*****************************************************************************
 * hxxx_nal.h: Common helpers for AVC/HEVC NALU
 *****************************************************************************
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
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
#ifndef HXXX_NAL_H
#define HXXX_NAL_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

static const uint8_t  annexb_startcode4[] = { 0x00, 0x00, 0x00, 0x01 };
#define annexb_startcode3 (&annexb_startcode4[1])

static inline bool hxxx_strip_AnnexB_startcode( const uint8_t **pp_data, size_t *pi_data )
{
    const uint8_t *p_data = *pp_data;
    if(*pi_data < 4 || p_data[0])
        return false;

    /* Skip 4 bytes startcode */
    if( !memcmp(&p_data[1], &annexb_startcode4[1], 3) )
    {
        *pp_data += 4;
        *pi_data -= 4;
    }
    /* Skip 3 bytes startcode */
    else if( !memcmp(&p_data[1], &annexb_startcode4[2], 2) )
    {
        *pp_data += 3;
        *pi_data -= 3;
    }
    else
        return false;
    return true;
}

/* Discards emulation prevention three bytes */
static inline uint8_t * hxxx_ep3b_to_rbsp(const uint8_t *p_src, size_t i_src, size_t *pi_ret)
{
    uint8_t *p_dst;
    if(!p_src || !(p_dst = malloc(i_src)))
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < i_src; i++) {
        if (i < i_src - 3 &&
            p_src[i] == 0 && p_src[i+1] == 0 && p_src[i+2] == 3) {
            p_dst[j++] = 0;
            p_dst[j++] = 0;
            i += 2;
            continue;
        }
        p_dst[j++] = p_src[i];
    }
    *pi_ret = j;
    return p_dst;
}

#endif // HXXX_NAL_H
