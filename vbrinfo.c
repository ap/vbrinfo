/* Copyright (c) 2005 Thomas Jansen <mithi@mithi.net>
 * Copyright (c) 2005 Aristotle Pagaltzis <pagaltzis@gmx.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <mad.h>

#define BUFFERSIZE ( 1024 * 1024 )
#define MAX_BITRATE 320

int opt_verbose = 0;
int opt_summary = 0;
int opt_histogram = 0;
int opt_precision = 3;

typedef struct {
	int fd;
	void * buf;
	int bufsize;
	int frames;
	int min_br;
	int max_br;
	int frames_for_br[ MAX_BITRATE + 1 ];
	double total_bits;
} decode_info;

void
usage( void )
{
	fprintf( stderr,
		"usage: vbrinfo [-v] [-g] [-s] [-p prec] [-h] files\n"
		"       -v  be verbose: print the bitrate of each frame\n"
		"       -g  print histogram\n"
		"       -s  print summary [default if nothing else specified]\n"
		"       -p  precision for average bitrate in summary,\n"
		"           takes number of fractional digits, implies -s\n"
		"       -h  this help\n"
	);
	exit( 1 );
}

void
process_arg( int argc, char **argv )
{
	int i;

	do {
		char * end;
		i = getopt( argc, argv, "sgvp:h" );
		switch ( i ) {
			case 's':
				opt_summary = 1;
				break;
			case 'g':
				opt_histogram = 1;
				break;
			case 'v':
				opt_verbose = 1;
				break;
			case 'p':
				opt_summary = 1;
				opt_precision = strtol( optarg, &end, 10 );
				if( errno || opt_precision < 0 )
					warnx( "precision must be a non-negative integer" ), usage();
				break;
			case '?':
			case 'h':
				usage();
				break;
		}
	} while ( i != -1 );

	if( optind >= argc ) usage();

	if ( ! ( opt_verbose || opt_histogram ) ) opt_summary = 1;
}

enum mad_flow
read_callback( void * data, struct mad_stream * stream )
{
	decode_info * info = ( decode_info * ) data;
	int bytes_incompl = stream->bufend - stream->next_frame;
	int len;

	if ( bytes_incompl )
		memmove( info->buf, stream->next_frame, bytes_incompl );

	len = read( info->fd, ( unsigned char * ) info->buf + bytes_incompl, info->bufsize - bytes_incompl );
	mad_stream_buffer( stream, info->buf, len );

	return len > 0 ? MAD_FLOW_CONTINUE : MAD_FLOW_STOP;
}

enum mad_flow
header_callback( void * data, struct mad_header const * header )
{
	decode_info * info = ( decode_info * ) data;
	int br = header->bitrate / 1000;

	/* being extremly verbose - and annoying */
	if ( opt_verbose ) printf( "  Frame %i: %i\n", info->frames, br );

	if ( info->min_br > br || ! info->min_br ) info->min_br = br;
	if ( info->max_br < br || ! info->max_br ) info->max_br = br;

	if( br > 0 && br <= MAX_BITRATE )
		++info->frames_for_br[ br ];
	else
		++info->frames_for_br[ 0 ];

	info->total_bits += br;
	info->frames++;

	return MAD_FLOW_CONTINUE;
}

int
main( int argc, char **argv )
{
	void * the_buffer = NULL;

	process_arg( argc, argv );

	the_buffer = malloc( BUFFERSIZE );
	if( ! the_buffer )
		errx( 255, "Memory allocation failed" );

	for ( ; optind < argc ; ++optind ) {
		struct mad_decoder decoder;
		decode_info info;

		memset( &info, 0, sizeof( decode_info ) );

		info.fd = open( argv[ optind ], O_RDONLY );
		info.buf = the_buffer;
		info.bufsize = BUFFERSIZE;

		if( -1 == info.fd ) {
			warn( "Could not open %s", argv[ optind ] );
			continue;
		}

		printf( "VBR information for %s:\n", argv[ optind ] );

		mad_decoder_init( &decoder, ( void * ) &info, read_callback, header_callback, NULL, NULL, NULL, NULL );
		mad_decoder_run( &decoder, MAD_DECODER_MODE_SYNC );
		mad_decoder_finish( &decoder );

		close( info.fd );

		if ( opt_summary ) {
			char * avg;

			if( asprintf( &avg, "%.*f", opt_precision, info.total_bits / info.frames ) == -1 )
				errx( 255, "Memory allocation failed" );

			if( rindex( avg, '.' ) ) {
				int i = strlen( avg ) - 1;
				while( avg[ i ] == '0' )
					avg[ i-- ] = '\0';
				if( avg[ i ] == '.' )
					avg[ i-- ] = '\0';
			}

			printf(
				"  Minimum bitrate: %i\n"
				"  Maximum bitrate: %i\n"
				"  Average bitrate: %s\n",
				info.min_br, info.max_br, avg
			);
		}

		if ( opt_histogram ) {
			printf( "  Histogram:\n" );
			int i;
			for( i = 1 ; i <= MAX_BITRATE ; ++i )
				if( info.frames_for_br[ i ] )
					printf( "    %i\t%6i\n", i, info.frames_for_br[ i ] );
			if( info.frames_for_br[ 0 ] )
				printf( "    ???\t%i\n", info.frames_for_br[ 0 ] );
		}

		if ( optind < ( argc - 1 ) ) printf( "\n" );
	}

	free( the_buffer );

	return 0;
}
