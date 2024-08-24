/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2024 The Open Watcom Contributors. All Rights Reserved.
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  DWARF debug ranges processing (.debug_aranges section).
*
****************************************************************************/


#include "drpriv.h"
#include "drutils.h"
#include "drgettab.h"


#define DEBUG_ARANGES_VERSION   2

#include "pushpck1.h"
typedef struct arange_header {
    uint_32     len;
    uint_16     version;
    uint_32     dbg_pos;
    uint_8      addr_size;
    uint_8      seg_size;
} _WCUNALIGNED arange_header;
#include "poppck.h"


void DRENTRY DRWalkARange( DRARNGWLK callback, void *data )
/*********************************************************/
{
    dr_arange_data      arange;
    arange_header       header;
    drmem_hdl           base;
    drmem_hdl           pos;
    drmem_hdl           finish;
    uint_32             tuple_size;
    pointer_uint        aligned_addr;
    pointer_uint        addr;
    bool                wat_producer;
    bool                zero_padding = true;

    base = DR_CurrNode->sections[DR_DEBUG_INFO].base;
    pos = DR_CurrNode->sections[DR_DEBUG_ARANGES].base;
    finish = pos + DR_CurrNode->sections[DR_DEBUG_ARANGES].size;
    while( pos < finish ) {
        DR_VMRead( pos, &header, sizeof( header ) );
        pos += sizeof( header );
        if( DR_CurrNode->byte_swap ) {
            SWAP_32( header.len );
            SWAP_16( header.version );
            SWAP_32( header.dbg_pos );
        }
        if( header.version != DEBUG_ARANGES_VERSION )
            DR_EXCEPT( DREXCEP_BAD_DBG_VERSION );
        arange.dbg = base + header.dbg_pos;
        arange.addr_size = header.addr_size;
        arange.seg_size = header.seg_size;
        arange.is_start = true;   /* start of bunch */
        tuple_size = ( header.addr_size + header.seg_size + header.addr_size );
        /* Open Watcom prior to 1.4 didn't align tuples properly; unfortunately
         * it may be hard to tell if debug info was generated by Watcom!
         */
        /* NB: The writers of the DWARF 2/3 standard clearly didn't think very hard
         * about segmented architectures. If tuple size is 10 (32-bit offset, 16-bit
         * segment, 32-bit size) we assume the data was generated by our tools and
         * no alignment padding was used.
         */
        wat_producer = ( DR_CurrNode->wat_producer_ver > VER_NONE ) || ( tuple_size == 10 );
        addr = (pointer_uint)pos;
        aligned_addr = (addr + tuple_size - 1) & ~(tuple_size - 1);
        if( aligned_addr != addr ) {
            /* try reading the padding; if it's nonzero, assume it's not there */
            if( DR_ReadInt( pos, header.addr_size ) != 0 )
                zero_padding = false;
            if( header.seg_size && DR_ReadInt( pos + header.addr_size, header.seg_size ) != 0 )
                zero_padding = false;
            if( !wat_producer && zero_padding ) {
                pos = (drmem_hdl)aligned_addr;
            }
        }
        for( ;; ) {
            arange.addr = DR_ReadInt( pos, header.addr_size );
            pos += header.addr_size;
            if( header.seg_size != 0 ) {
                arange.seg = (uint_16)DR_ReadInt( pos, header.seg_size );
                pos += header.seg_size;
            } else { /* flat */
                arange.seg = 0;
            }
            arange.len = DR_ReadInt( pos, header.addr_size );
            pos += header.addr_size;
            if( arange.addr == 0 && arange.seg == 0 && arange.len == 0 )
                break;
            if( !callback( data, &arange ) )
                break;
            arange.is_start = false;
        }
    }
}
