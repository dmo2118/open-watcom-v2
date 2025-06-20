/****************************************************************
Copyright (C) Lucent Technologies 1997
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name Lucent Technologies or any of
its entities not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
****************************************************************/

/* lasciate ogne speranza, voi ch'intrate. */

#define DEBUG

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "awk.h"
#include "ytab.h"

/*
 * HAT matches ^ in regular expr
 * NCHARS is 2**n
 */
#define HAT (NCHARS+2)

#define MAXLIN 22

#define type(v)     (v)->nobj   /* badly overloaded here */
#define info(v)     (v)->ntype  /* badly overloaded here */
#define left(v)     (v)->narg[0]
#define right(v)    (v)->narg[1]
#define parent(v)   (v)->nnext

#define LEAF    case CCL: case NCCL: case CHAR: case DOT: case FINAL: case ALL:
#define ELEAF   case EMPTYRE:       /* empty string in regexp */
#define UNARY   case STAR: case PLUS: case QUEST:

/* encoding in tree Nodes:
    leaf (CCL, NCCL, CHAR, DOT, FINAL, ALL, EMPTYRE):
        left is index, right contains value or pointer to value
    unary (STAR, PLUS, QUEST): left is child, right is null
    binary (CAT, OR): left and right are children
    parent contains pointer to parent
*/


const char          *patbeg;
int                 patlen;

static int          maxsetvec = 0;
static bool         *setvec;
static int          *tmpset;

static int          rtok;       /* next token in current re */
static int          rlxval;

static const char   *rlxstr;
static const char   *prestr;    /* current position in current re */
static const char   *lastre;    /* origin of last re */

static int          setcnt;
static int          poscnt;

#define NFA         20  /* cache this many dynamic fa's */
static fa           *fatab[NFA];
static int          nfatab  = 0;    /* entries in fatab */

fa *makedfa( const char *s, bool anchor )   /* returns dfa for reg expr s */
{
    int i, use, nuse;
    fa *pfa;
    static int now = 1;

    if( setvec == NULL ) {  /* first time through any RE */
        maxsetvec = MAXLIN;
        setvec = (bool *)malloc( maxsetvec * sizeof( bool ) );
        tmpset = (int *)malloc( maxsetvec * sizeof( int ) );
        if( setvec == NULL || tmpset == NULL ) {
            overflo( "out of space initializing makedfa" );
            /* never return */
        }
    }

    if( compile_time )   /* a constant for sure */
        return( mkdfa( s, anchor ) );
    for( i = 0; i < nfatab; i++ ) {  /* is it there already? */
        if( fatab[i]->anchor == anchor && strcmp( fatab[i]->restr, s ) == 0 ) {
            fatab[i]->use = now++;
            return( fatab[i] );
        }
    }
    pfa = mkdfa( s, anchor );
    if( nfatab < NFA ) { /* room for another */
        fatab[nfatab] = pfa;
        fatab[nfatab]->use = now++;
        nfatab++;
        return pfa;
    }
    use = fatab[0]->use;    /* replace least-recently used */
    nuse = 0;
    for( i = 1; i < nfatab; i++ ) {
        if( fatab[i]->use < use ) {
            use = fatab[i]->use;
            nuse = i;
        }
    }
    freefa( fatab[nuse] );
    fatab[nuse] = pfa;
    pfa->use = now++;
    return pfa;
}

fa *mkdfa( const char *s, bool anchor )
/* does the real work of making a dfa */
/* anchor = true for anchored matches, else false */
{
    Node *p, *p1;
    fa *f;

    p = reparse(s);
    p1 = op2(CAT, op2(STAR, op2(ALL, NIL, NIL), NIL), p);
        /* put ALL STAR in front of reg.  exp. */
    p1 = op2(CAT, p1, op2(FINAL, NIL, NIL));
        /* put FINAL after reg.  exp. */

    poscnt = 0;
    penter(p1); /* enter parent pointers and leaf indices */
    if( (f = (fa *)calloc( 1, sizeof( fa ) + poscnt * sizeof( rrow ) )) == NULL )
        overflo(     "out of space for fa" );
        /* never return */
    f->accept = poscnt - 1; /* penter has computed number of positions in re */
    cfoll( f, p1 );         /* set up follow sets */
    freetr(p1);
    if( (f->posns[0] = (int *)calloc( 1, *(f->re[0].lfollow) * sizeof( int ) )) == NULL )
        overflo( "out of space in makedfa" );
        /* never return */
    if( (f->posns[1] = (int *)calloc( 1, sizeof( int ) )) == NULL )
        overflo( "out of space in makedfa" );
        /* never return */
    *f->posns[1] = 0;
    f->initstat = makeinit( f, anchor );
    f->anchor = anchor;
    f->restr = tostring( s );
    return f;
}

int makeinit( fa *f, bool anchor )
{
    int i, k;

    f->curstat = 2;
    f->out[2] = false;
    f->reset = false;
    k = *(f->re[0].lfollow);
    xfree(f->posns[2]);
    if( (f->posns[2] = (int *)calloc( 1, ( k + 1 ) * sizeof( int ) )) == NULL )
        overflo( "out of space in makeinit" );
        /* never return */
    for( i = 0; i <= k; i++ ) {
        (f->posns[2])[i] = (f->re[0].lfollow)[i];
    }
    if( (f->posns[2])[1] == f->accept )
        f->out[2] = true;
    for( i = 0; i < NCHARS; i++ )
        f->gototab[2][i] = 0;
    f->curstat = cgoto( f, 2, HAT );
    if( anchor ) {
        *f->posns[2] = k - 1; /* leave out position 0 */
        for( i = 0; i < k; i++ ) {
            (f->posns[0])[i] = (f->posns[2])[i];
        }

        f->out[0] = f->out[2];
        if( f->curstat != 2 ) {
            --(*f->posns[f->curstat]);
        }
    }
    return( f->curstat );
}

void penter(Node *p)    /* set up parent pointers and leaf indices */
{
    switch( type( p ) ) {
    ELEAF
    LEAF
        info( p ) = poscnt;
        poscnt++;
        break;
    UNARY
        penter( left( p ) );
        parent( left( p ) ) = p;
        break;
    case CAT:
    case OR:
        penter( left( p ) );
        penter( right( p ) );
        parent( left( p ) ) = p;
        parent( right( p ) ) = p;
        break;
    default:    /* can't happen */
        FATAL( "can't happen: unknown type %d in penter", type( p ) );
        /* never return */
    }
}

void freetr(Node *p)    /* free parse tree */
{
    switch (type(p)) {
    ELEAF
    LEAF
        xfree(p);
        break;
    UNARY
        freetr(left(p));
        xfree(p);
        break;
    case CAT:
    case OR:
        freetr(left(p));
        freetr(right(p));
        xfree(p);
        break;
    default:    /* can't happen */
        FATAL( "can't happen: unknown type %d in freetr", type( p ) );
        /* never return */
    }
}

/* in the parsing of regular expressions, metacharacters like . have */
/* to be seen literally;  \056 is not a metacharacter. */

#define X2BIN(c)    ((isdigit( c )) ? (c - '0') : (c - 'a' + 10))

int hexstr( const char **pp )
/* find and eval hex string at pp, return new p */
/* only pick up one 8-bit byte (2 chars) */
{
    int n = 0;
    int c;

    c = tolower( *(uschar *)*pp );
    if( isxdigit( c ) ) {
        n = X2BIN( c );
        (*pp)++;
        c = tolower( *(uschar *)*pp );
        if( isxdigit( c ) ) {
            n = 16 * n + X2BIN( c );
            (*pp)++;
        }
    }
    return( n );
}

#define isoctdigit(c) ((c) >= '0' && (c) <= '7')    /* multiple use of arg */

static int octstr( int n, const char **pp )
/* find and eval oct string at pp, return new pp */
/* only pick up one 8-bit byte (2 chars) */
{
    int c;

    c = *(uschar *)*pp;
    if( isoctdigit( c ) ) {
        n = 8 * n + c - '0';
        (*pp)++;
        c = *(uschar *)*pp;
        if( isoctdigit( c ) ) {
            n = 8 * n + c - '0';
            (*pp)++;
        }
    }
    return( n );
}

int quoted( const char **pp )
/* pick up next thing after a \\ */
/* and increment *pp */
{
    int c;

    c = *(uschar *)(*pp)++;
    switch( c ) {
    case 't':
        c = '\t';
        break;
    case 'n':
        c = '\n';
        break;
    case 'f':
        c = '\f';
        break;
    case 'r':
        c = '\r';
        break;
    case 'b':
        c = '\b';
        break;
    case 'x':
        /* \xh \xhh */
        c = hexstr( pp );   /* this return a null if number is invalid */
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
        /* \o \oo \ooo */
        c = octstr( c - '0', pp );
        break;
    case '\\':
    default:
        break;
    }
    return( c );
}

char *cclenter( const char *argp )    /* add a character class */
{
    int         i, c, c2;
    const char  *p = argp;
    char        *bp;
    char        *buf;
    size_t      bufsz = 100;

    buf = (char *)malloc( bufsz );
    if( buf == NULL ) {
        FATAL( "out of space for character class [%.10s...] 1", p );
        /* never return */
    }
    bp = buf;
    for( i = 0; (c = *(uschar *)p++) != '\0'; ) {
        if( c == '\\' ) {
            c = quoted( &p );
        } else if( c == '-' && i > 0 && bp[-1] != '\0' ) {
            if( *p != '\0' ) {
                c = (uschar)bp[-1];
                c2 = *(uschar *)p++;
                if( c2 == '\\' )
                    c2 = quoted( &p );
                if( c > c2 ) {   /* empty; ignore */
                    bp--;
                    i--;
                    continue;
                }
                while( c < c2 ) {
                    if( !adjbuf( &buf, &bufsz, bp - buf + 2, 100, &bp, "cclenter1" ) )
                        FATAL( "out of space for character class [%.10s...] 2", p );
                        /* never return */
                    *bp++ = ++c;
                    i++;
                }
                continue;
            }
        }
        if( !adjbuf( &buf, &bufsz, bp - buf + 2, 100, &bp, "cclenter2" ) )
            FATAL( "out of space for character class [%.10s...] 3", p );
            /* never return */
        *bp++ = c;
        i++;
    }
    *bp = '\0';
    dprintf(( "cclenter: in = |%s|, out = |%s|\n", argp, buf ));
    return( buf );
}

void overflo( const char *s )
{
    FATAL( "regular expression too big: %.30s...", s );
    /* never return */
}

void cfoll(fa *f, Node *v)  /* enter follow set of each leaf of vertex v into lfollow[leaf] */
{
    int i;
    int *p;

    switch (type(v)) {
    ELEAF
    LEAF
        f->re[info(v)].ltype = type(v);
        f->re[info(v)].lval.np = right(v);
        while( f->accept >= maxsetvec ) {    /* guessing here! */
            maxsetvec *= 4;
            setvec = (bool *)realloc( setvec, maxsetvec * sizeof( bool ) );
            tmpset = (int *)realloc( tmpset, maxsetvec * sizeof( int ) );
            if( setvec == NULL || tmpset == NULL ) {
                overflo( "out of space in cfoll()" );
                /* never return */
            }
        }
        for( i = 0; i <= f->accept; i++ ) {
            setvec[i] = false;
        }
        setcnt = 0;
        follow( v );  /* computes setvec and setcnt */
        if( (p = (int *)calloc( 1, ( setcnt + 1 ) * sizeof( int ) )) == NULL )
            overflo( "out of space building follow set" );
            /* never return */
        f->re[info( v )].lfollow = p;
        *p = setcnt;
        for( i = f->accept; i >= 0; i-- ) {
            if( setvec[i] ) {
                *++p = i;
            }
        }
        break;
    UNARY
        cfoll( f, left( v ) );
        break;
    case CAT:
    case OR:
        cfoll( f, left( v ) );
        cfoll( f, right( v ) );
        break;
    default:    /* can't happen */
        FATAL( "can't happen: unknown type %d in cfoll", type( v ) );
        /* never return */
    }
}

int first( Node *p )
/* collects initially active leaves of p into setvec */
/* returns 0 if p matches empty string */
{
    int b, lp;

    switch( type( p ) ) {
    ELEAF
    LEAF
        lp = info( p );     /* look for high-water mark of subscripts */
        while( setcnt >= maxsetvec || lp >= maxsetvec ) {    /* guessing here! */
            maxsetvec *= 4;
            setvec = (bool *)realloc( setvec, maxsetvec * sizeof( bool ) );
            tmpset = (int *)realloc( tmpset, maxsetvec * sizeof( int ) );
            if( setvec == NULL || tmpset == NULL ) {
                overflo( "out of space in first()" );
                /* never return */
            }
        }
        if( type( p ) == EMPTYRE ) {
            setvec[lp] = false;
            return( 0 );
        }
        if( !setvec[lp] ) {
            setvec[lp] = true;
            setcnt++;
        }
        if( type( p ) == CCL && *(char *)right( p ) == '\0' )
            return( 0 );    /* empty CCL */
        break;
    case PLUS:
        if( first( left( p ) ) == 0 )
            return( 0 );
        break;
    case STAR:
    case QUEST:
        first( left( p ) );
        return( 0 );
    case CAT:
        if( first( left( p ) ) == 0 && first( right( p ) ) == 0 )
            return( 0 );
        break;
    case OR:
        b = first( right( p ) );
        if( first( left( p ) ) == 0 || b == 0 )
            return( 0 );
        break;
    default:
        FATAL( "can't happen: unknown type %d in first", type( p ) );   /* can't happen */
        /* never return */
    }
    return( 1 );
}

void follow( Node *v )
/* collects leaves that can follow v into setvec */
{
    Node *p;

    if( type( v ) == FINAL )
        return;
    p = parent( v );
    switch( type( p ) ) {
    case STAR:
    case PLUS:
        first( v );
        follow( p );
        return;
    case OR:
    case QUEST:
        follow( p );
        return;
    case CAT:
        if( v == left( p ) ) { /* v is left child of p */
            if( first( right( p ) ) == 0 ) {
                follow( p );
            }
        } else {    /* v is right child */
            follow( p );
        }
        return;
    }
}

bool member( int c, const char *s )   /* is c in s? */
{
    while( *s != '\0' ) {
        if( c == *(uschar *)s++ ) {
            return( true );
        }
    }
    return( false );
}

bool match( fa *f, const char *p )      /* shortest match ? */
{
    int s, ns;

    s = ( f->reset ) ? makeinit( f, false ) : f->initstat;
    if( f->out[s] )
        return( true );
    do {
        /* assert(*p < NCHARS); */
        if( (ns = f->gototab[s][*(uschar *)p]) != 0 ) {
            s = ns;
        } else {
            s = cgoto( f, s, *(uschar *)p );
        }
        if( f->out[s] ) {
            return( true );
        }
    } while( *p++ != '\0' );
    return( false );
}

bool pmatch( fa *f, const char *p )         /* longest match, for sub */
{
    int s, ns;
    const char *q;
    int i, k;

    /* s = ( f->reset ) ? makeinit( f, true ) : f->initstat; */
    if( f->reset ) {
        f->initstat = s = makeinit( f, true );
    } else {
        s = f->initstat;
    }
    patbeg = p;
    patlen = NOPAT;
    do {
        q = p;
        do {
            if( f->out[s] )         /* final state */
                patlen = q - p;
            /* assert(*q < NCHARS); */
            if( (ns = f->gototab[s][*(uschar *)q]) != 0 ) {
                s = ns;
            } else {
                s = cgoto(f, s, *(uschar *)q);
            }
            if( s == 1 ) {          /* no transition */
                if( patlen != NOPAT ) {
                    patbeg = p;
                    return( true );
                } else {
                    goto nextin;    /* no match */
                }
            }
        } while( *q++ != '\0' );
        if( f->out[s] )
            patlen = q - p - 1;     /* don't count $ */
        if( patlen != NOPAT ) {
            patbeg = p;
            return( true );
        }
nextin:
        s = 2;
        if( f->reset ) {
            for( i = 2; i <= f->curstat; i++ ) {
                xfree( f->posns[i] );
            }
            k = *f->posns[0];
            if( (f->posns[2] = (int *)calloc( 1, ( k + 1 ) * sizeof( int ) )) == NULL )
                overflo( "out of space in pmatch" );
                /* never return */
            for( i = 0; i <= k; i++ ) {
                (f->posns[2])[i] = (f->posns[0])[i];
            }
            f->initstat = f->curstat = 2;
            f->out[2] = f->out[0];
            for( i = 0; i < NCHARS; i++ ) {
                f->gototab[2][i] = 0;
            }
        }
    } while( *p++ != '\0' );
    return( false );
}

bool nematch( fa *f, const char *p )    /* non-empty match, for sub */
{
    int s, ns;
    const char *q;
    int i, k;

    /* s = f->reset ? makeinit( f, true ) : f->initstat; */
    if( f->reset ) {
        f->initstat = s = makeinit( f, true );
    } else {
        s = f->initstat;
    }
    patlen = NOPAT;
    while( *p != '\0' ) {
        q = p;
        do {
            if( f->out[s] )      /* final state */
                patlen = q - p;
            /* assert(*q < NCHARS); */
            if( (ns = f->gototab[s][*(uschar *)q]) != 0 ) {
                s = ns;
            } else {
                s = cgoto(f, s, *(uschar *)q);
            }
            if (s == 1) {   /* no transition */
                if( patlen != NOPAT && patlen > 0 ) {
                    patbeg = p;
                    return( true );
                } else {
                    goto nnextin;   /* no nonempty match */
                }
            }
        } while( *q++ != '\0' );
        if( f->out[s] )
            patlen = q - p - 1; /* don't count $ */
        if( patlen != NOPAT && patlen > 0 ) {
            patbeg = p;
            return( true );
        }
nnextin:
        s = 2;
        if( f->reset ) {
            for( i = 2; i <= f->curstat; i++ ) {
                xfree( f->posns[i] );
            }
            k = *f->posns[0];
            if( (f->posns[2] = (int *)calloc( 1, ( k + 1 ) * sizeof( int ) )) == NULL )
                overflo( "out of state space" );
                /* never return */
            for( i = 0; i <= k; i++ ) {
                (f->posns[2])[i] = (f->posns[0])[i];
            }
            f->initstat = f->curstat = 2;
            f->out[2] = f->out[0];
            for( i = 0; i < NCHARS; i++ ) {
                f->gototab[2][i] = 0;
            }
        }
        p++;
    }
    return( false );
}

Node *reparse( const char *p )
/* parses regular expression pointed to by p */
/* uses relex() to scan regular expression */
{
    Node *np;

    dprintf(( "reparse <%s>\n", p ));
    lastre = prestr = p; /* prestr points to string to be parsed */
    rtok = relex();
    /* GNU compatibility: an empty regexp matches anything */
    if( rtok == '\0' ) {
        /* FATAL( "empty regular expression" ); previous */
        return( op2( EMPTYRE, NIL, NIL ) );
    }
    np = regexp();
    if( rtok != '\0' )
        FATAL( "syntax error in regular expression %s at %s", lastre, prestr );
        /* never return */
    return( np );
}

Node *regexp(void)  /* top-level parse of reg expr */
{
    return (alt(concat(primary())));
}

Node *primary(void)
{
    Node *np;

    switch( rtok ) {
    case CHAR:
        np = op2( CHAR, NIL, itonp( rlxval ) );
        rtok = relex();
        np = unary( np );
        break;
    case ALL:
        rtok = relex();
        np = unary( op2( ALL, NIL, NIL ) );
        break;
    case EMPTYRE:
        rtok = relex();
        np = unary( op2( ALL, NIL, NIL ) );
        break;
    case DOT:
        rtok = relex();
        np = unary( op2( DOT, NIL, NIL ) );
        break;
    case CCL:
        np = op2( CCL, NIL, (Node *)cclenter( rlxstr ) );
        rtok = relex();
        np = unary( np );
        break;
    case NCCL:
        np = op2( NCCL, NIL, (Node *)cclenter( rlxstr ) );
        rtok = relex();
        np = unary( np );
        break;
    case '^':
        rtok = relex();
        np = unary( op2( CHAR, NIL, itonp( HAT ) ) );
        break;
    case '$':
        rtok = relex();
        np = unary( op2( CHAR, NIL, NIL ) );
        break;
    case '(':
        rtok = relex();
        if( rtok == ')' ) {  /* special pleading for () */
            rtok = relex();
            np = unary( op2( CCL, NIL, (Node *)tostring( "" ) ) );
            break;
        }
        np = regexp();
        if( rtok == ')' ) {
            rtok = relex();
            np = unary( np );
            break;
        } else {
            FATAL( "syntax error in regular expression %s at %s", lastre, prestr );
            /* never return */
        }
    default:
        FATAL( "illegal primary in regular expression %s at %s", lastre, prestr );
        /* never return */
    }
    return( np );
}

Node *concat( Node *np )
{
    switch( rtok ) {
    case CHAR:
    case DOT:
    case ALL:
    case EMPTYRE:
    case CCL:
    case NCCL:
    case '$':
    case '(':
        return( concat( op2( CAT, np, primary() ) ) );
    }
    return( np );
}

Node *alt( Node *np )
{
    if( rtok == OR ) {
        rtok = relex();
        np = alt( op2( OR, np, concat( primary() ) ) );
    }
    return( np );
}

Node *unary(Node *np)
{
    switch( rtok ) {
    case STAR:
        rtok = relex();
        return( unary( op2( STAR, np, NIL ) ) );
    case PLUS:
        rtok = relex();
        return( unary( op2( PLUS, np, NIL ) ) );
    case QUEST:
        rtok = relex();
        return( unary( op2( QUEST, np, NIL ) ) );
    default:
        return( np );
    }
}

/*
 * Character class definitions conformant to the POSIX locale as
 * defined in IEEE P1003.1 draft 7 of June 2001, assuming the source
 * and operating character sets are both ASCII (ISO646) or supersets
 * thereof.
 *
 * Note that to avoid overflowing the temporary buffer used in
 * relex(), the expanded character class (prior to range expansion)
 * must be less than twice the size of their full name.
 */

/* Because isblank doesn't show up in any of the header files on any
 * system i use, it's defined here.  if some other locale has a richer
 * definition of "blank", define HAS_ISBLANK and provide your own
 * version.
 * the parentheses here are an attempt to find a path through the maze
 * of macro definition and/or function and/or version provided.  thanks
 * to nelson beebe for the suggestion; let's see if it works everywhere.
 */

static int (xisblank)( int c )
{
    return( c == ' ' || c == '\t' );
}

static struct charclass {
    const char *cc_name;
    int cc_namelen;
    int (*cc_func)(int);
} charclasses[] = {
    { "alnum",  5,  isalnum },
    { "alpha",  5,  isalpha },
    { "blank",  5,  xisblank },
    { "cntrl",  5,  iscntrl },
    { "digit",  5,  isdigit },
    { "graph",  5,  isgraph },
    { "lower",  5,  islower },
    { "print",  5,  isprint },
    { "punct",  5,  ispunct },
    { "space",  5,  isspace },
    { "upper",  5,  isupper },
    { "xdigit", 6,  isxdigit },
    { NULL,     0,  NULL },
};


int relex( void )   /* lexical analyzer for reparse */
{
    int             c;
    size_t          n;
    bool            cflag;
    static char     *buf = NULL;
    static size_t   bufsz = 100;
    char            *bp;
    struct charclass *cc;
    int             i;

    c = *(uschar *)prestr++;
    switch( c ) {
    case '|': return( OR );
    case '*': return( STAR );
    case '+': return( PLUS );
    case '?': return( QUEST );
    case '.': return( DOT );
    case '\0': prestr--; return( '\0' );
    case '^':
    case '$':
    case '(':
    case ')':
        return( c );
    case '\\':
        rlxval = quoted( &prestr );
        return( CHAR );
    default:
        rlxval = c;
        return( CHAR );
    case '[':
        if( buf == NULL && (buf = (char *)malloc( bufsz )) == NULL )
            FATAL( "out of space in reg expr %.10s..", lastre );
            /* never return */
        bp = buf;
        if( *prestr == '^' ) {
            cflag = true;
            prestr++;
        } else {
            cflag = false;
        }
        n = 2 * strlen( prestr ) + 1;
        if( !adjbuf( &buf, &bufsz, n, n, &bp, "relex1" ) ) {
            FATAL( "out of space for reg expr %.10s...", lastre );
            /* never return */
        }
        for( ;; ) {
            c = *(uschar *)prestr++;
            if( c == '\\' ) {
                *bp++ = '\\';
                c = *(uschar *)prestr++;
                if( c == '\0' )
                    FATAL( "nonterminated character class %.20s...", lastre );
                    /* never return */
                *bp++ = c;
            /* } else if (c == '\n') { */
            /*  FATAL( "newline in character class %.20s...", lastre ); */
            } else if( c == '[' && *prestr == ':' ) {
                /* POSIX char class names, Dag-Erling Smorgrav, des@ofug.org */
                for( cc = charclasses; cc->cc_name != NULL; cc++ ) {
                    if( strncmp( prestr + 1, cc->cc_name, cc->cc_namelen ) == 0 ) {
                        break;
                    }
                }
                if( cc->cc_name != NULL && prestr[1 + cc->cc_namelen] == ':'
                  && prestr[2 + cc->cc_namelen] == ']' ) {
                    prestr += cc->cc_namelen + 3;
                    for( i = 0; i < NCHARS; i++ ) {
                        if( !adjbuf( &buf, &bufsz, bp - buf + 1, 100, &bp, "relex2" ) )
                            FATAL( "out of space for reg expr %.10s...", lastre );
                            /* never return */
                        if( cc->cc_func( i ) ) {
                            *bp++ = i;
                            n++;
                        }
                    }
                } else {
                    *bp++ = c;
                }
            } else if( c == '\0' ) {
                FATAL( "nonterminated character class %.20s", lastre );
                /* never return */
            } else if( bp == buf ) { /* 1st char is special */
                *bp++ = c;
            } else if( c == ']' ) {
                *bp++ = '\0';
                rlxstr = tostring( buf );
                if( !cflag ) {
                    return( CCL );
                } else {
                    return( NCCL );
                }
            } else {
                *bp++ = c;
            }
        }
    }
}

int cgoto( fa *f, int s, int c )
{
    int i, j, k;
    int *p, *q;

    assert(c == HAT || c < NCHARS);
    while( f->accept >= maxsetvec ) {    /* guessing here! */
        maxsetvec *= 4;
        setvec = (bool *)realloc( setvec, maxsetvec * sizeof( bool ) );
        tmpset = (int *)realloc( tmpset, maxsetvec * sizeof( int ) );
        if( setvec == NULL || tmpset == NULL ) {
            overflo( "out of space in cgoto()" );
            /* never return */
        }
    }
    for( i = 0; i <= f->accept; i++ ) {
        setvec[i] = false;
    }
    setcnt = 0;
    /* compute positions of gototab[s,c] into setvec */
    p = f->posns[s];
    for( i = 1; i <= *p; i++ ) {
        if( (k = f->re[p[i]].ltype) != FINAL ) {
            if( (k == CHAR && c == ptoi( f->re[p[i]].lval.np ))
             || (k == DOT && c != 0 && c != HAT)
             || (k == ALL && c != 0)
             || (k == EMPTYRE && c != 0)
             || (k == CCL && member( c, (char *)f->re[p[i]].lval.up ))
             || (k == NCCL && !member( c, (char *)f->re[p[i]].lval.up ) && c != 0 && c != HAT) ) {
                q = f->re[p[i]].lfollow;
                for( j = 1; j <= *q; j++ ) {
                    if( q[j] >= maxsetvec ) {
                        maxsetvec *= 4;
                        setvec = (bool *)realloc( setvec, maxsetvec * sizeof( bool ) );
                        tmpset = (int *)realloc( tmpset, maxsetvec * sizeof( int ) );
                        if( setvec == NULL || tmpset == NULL ) {
                            overflo( "cgoto overflow" );
                            /* never return */
                        }
                    }
                    if( !setvec[q[j]] ) {
                        setcnt++;
                        setvec[q[j]] = true;
                    }
                }
            }
        }
    }
    /* determine if setvec is a previous state */
    tmpset[0] = setcnt;
    j = 1;
    for( i = f->accept; i >= 0; i-- ) {
        if( setvec[i] ) {
            tmpset[j++] = i;
        }
    }
    /* tmpset == previous state? */
    for( i = 1; i <= f->curstat; i++ ) {
        p = f->posns[i];
        if ((k = tmpset[0]) != p[0])
            goto different;
        for( j = 1; j <= k; j++ ) {
            if( tmpset[j] != p[j] ) {
                goto different;
            }
        }
        /* setvec is state i */
        f->gototab[s][c] = i;
        return i;
different:;
    }

    /* add tmpset to current set of states */
    if (f->curstat >= NSTATES-1) {
        f->curstat = 2;
        f->reset = true;
        for( i = 2; i < NSTATES; i++ ) {
            xfree(f->posns[i]);
        }
    } else {
        ++(f->curstat);
    }
    for( i = 0; i < NCHARS; i++ ) {
        f->gototab[f->curstat][i] = 0;
    }
    xfree( f->posns[f->curstat] );
    if( (p = (int *)calloc( 1, ( setcnt + 1 ) * sizeof( int ) )) == NULL )
        overflo( "out of space in cgoto" );
        /* never return */

    f->posns[f->curstat] = p;
    f->gototab[s][c] = f->curstat;
    for( i = 0; i <= setcnt; i++ ) {
        p[i] = tmpset[i];
    }
    f->out[f->curstat] = setvec[f->accept];
    return( f->curstat );
}


void freefa( fa *f )  /* free a finite automaton */
{
    int i;

    if( f == NULL )
        return;
    for( i = 0; i <= f->curstat; i++ ) {
        xfree( f->posns[i] );
    }
    for( i = 0; i <= f->accept; i++ ) {
        xfree( f->re[i].lfollow );
        if( f->re[i].ltype == CCL || f->re[i].ltype == NCCL ) {
            xfree( f->re[i].lval.np );
        }
    }
    xfree( f->restr );
    xfree( f );
}
