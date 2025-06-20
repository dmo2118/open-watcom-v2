/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2025 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  Implementation of memcmp() and wmemcmp().
*
****************************************************************************/


#include "variety.h"
#include "widechar.h"
#include "libwchar.h"
#include "xstring.h"


_WCRTLINK int __F_NAME(memcmp,wmemcmp)( const VOID_WC_TYPE *vs1, const VOID_WC_TYPE *vs2, size_t len )
{
#if defined(__INLINE_FUNCTIONS__) && !defined(__WIDECHAR__) && defined(_M_IX86)
    return( _inline_memcmp( vs1, vs2, len ) );
#else
    const CHAR_TYPE *s1;
    const CHAR_TYPE *s2;

    for( s1 = vs1, s2 = vs2; len != 0; ++s1, ++s2, --len )  {
        if( *s1 != *s2 ) {
            return( *s1 - *s2 );
        }
    }
    return( 0 );    /* both operands are equal */
#endif
}
