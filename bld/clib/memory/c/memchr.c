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
* Description:  Implementation of memchr() and wmemchr().
*
****************************************************************************/


#include "variety.h"
#include "widechar.h"
#include "libwchar.h"
#include "xstring.h"

/* locate the first occurrence of c in the initial n characters of the
   object pointed to by s.
   If the character c is not found, NULL is returned.
*/

_WCRTLINK VOID_WC_TYPE *__F_NAME(memchr,wmemchr)( const VOID_WC_TYPE *vs, INT_WC_TYPE c, size_t n )
{
#if defined(__INLINE_FUNCTIONS__) && !defined(__WIDECHAR__) && defined(_M_IX86)
    return( _inline_memchr( vs, c, n ) );
#else
    const UCHAR_TYPE *s;

    for( s = vs; n != 0; ++s, --n ) {
        if( *s == c ) {
            return( (VOID_WC_TYPE *)s );
        }
    }
    return( NULL );
#endif
}
