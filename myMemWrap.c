#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <time.h>

#include <dlfcn.h>
#include <sys/mman.h>

#include <pthread.h>

#if 0
#define	DEBUGOUT	(1)
#endif

// Internal meory allocation parameters to prevent the segmentation fault
#define ZALLOC_MAX	(1024)

static size_t	zalloc_cnt = 0;
static void *	zalloc_list[ZALLOC_MAX];

// Memory allocation hooks
static int		alloc_init_pending = 0;

static void *	(*real_malloc)( size_t )			= NULL;
static void *	(*real_realloc)( void*, size_t )	= NULL;
static void *	(*real_calloc)( size_t, size_t )	= NULL;
static void		(*real_free)( void*)				= NULL;

// Statistics - by size
#define BYSIZE_MAX	(12)

enum {
	ENTRY_4B = 0,
	ENTRY_8B,
	ENTRY_16B,
	ENTRY_32B,
	ENTRY_64B,
	ENTRY_128B,
	ENTRY_256B,
	ENTRY_512B,
	ENTRY_1024B,
	ENTRY_2048B,
	ENTRY_4096B,
	ENTRY_MAXB
};

static long		total_allocation;

static size_t	total_allocated_size;
static size_t	current_allocated_size;
static size_t	count_by_size[BYSIZE_MAX];

// Last memory allocation time per second
#define	TINTVL	(1)

static time_t	last_time = (time_t)NULL;

/*
 * Load original allocation routines at first use
 */
static void
alloc_init( void )
{
	int		ix;

	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	// Lock data structures to be thread-safe
	pthread_mutex_lock( &mutex );

	alloc_init_pending = 1;

	// Initialize functional hooks
	real_malloc  = dlsym(RTLD_NEXT, "malloc");
	real_realloc = dlsym(RTLD_NEXT, "realloc");
	real_calloc  = dlsym(RTLD_NEXT, "calloc");
	real_free    = dlsym(RTLD_NEXT, "free");
	
	// Initialize present time
	last_time = time( NULL );

	// Initailize statistic records - by size
	for ( ix = 0 ; ix < BYSIZE_MAX ; ix++ ) count_by_size[ix] = (size_t)0;

	// Unlock data structures
	pthread_mutex_unlock( &mutex );

	if ( !real_malloc || !real_realloc || !real_calloc || !real_free ) {
		fputs( "alloc.so: Unable to hook allocation!\n", stderr );
		fputs( dlerror(), stderr );

		exit( 1 );
	}
#ifdef	DEBUGOUT
	else {
		fputs( "alloc.so: Successfully hooked\n", stderr );
	}
#endif	/* DEBUGOUT */

	alloc_init_pending = 0;
}	/* alloc_init */

/*
 * Update statistics
 */
void
counter_update( size_t size )
{
	time_t		t;
	int			tobe;
	char 		*tstring;

	static pthread_mutex_t	cmutex = PTHREAD_MUTEX_INITIALIZER;
	static char				header1[1024], header2[1024], header3[1024];

	// Lock data structures to be thread-safe
	pthread_mutex_lock( &cmutex );

	tobe = 1;

	total_allocation++;

	total_allocated_size	+= size;
	current_allocated_size	= size;
	
	// Free memeory function
	if ( size < 0 ) {
		tobe	= -1;
		size	= -1 * size;
	}

	if ( size >= 4096 )			count_by_size[ENTRY_MAXB]	+= tobe;
	else if ( size >= 2048 )	count_by_size[ENTRY_4096B]	+= tobe;
	else if ( size >= 1024 )	count_by_size[ENTRY_2048B]	+= tobe;
	else if ( size >= 512 )		count_by_size[ENTRY_1024B]	+= tobe;
	else if ( size >= 256 )		count_by_size[ENTRY_512B]	+= tobe;
	else if ( size >= 128 )		count_by_size[ENTRY_256B]	+= tobe;
	else if ( size >= 64 )		count_by_size[ENTRY_128B]	+= tobe;
	else if ( size >= 32 )		count_by_size[ENTRY_64B]	+= tobe;
	else if ( size >= 16 )		count_by_size[ENTRY_32B]	+= tobe;
	else if ( size >= 8 )		count_by_size[ENTRY_16B]	+= tobe;
	else if ( size >= 4 )		count_by_size[ENTRY_8B]		+= tobe;
	else if ( size >= 0 )		count_by_size[ENTRY_4B]		+= tobe;

	t = time( NULL );

	if ( t >= (last_time + TINTVL) ) {
		int	ix;

		// tstring = ctime( &t );
		// sprintf( header1, ">>>>>>>>>>>>> %s >>>>>>>>>>>>>\n", tsring );
		sprintf( header1, "\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n" );
		sprintf( header2, "%ld\tOverall allocations since start\n", total_allocation );
		sprintf( header3, "%ld\tCurrent total allocated size in byte\n\n", total_allocated_size );

		// Display statistics
		fputs( header1, stderr );
		system( "date -u > /dev/stderr" );

		fputs( "Overall stats:\n", stderr );
		fputs( header2, stderr );
		fputs( header3, stderr );

		sprintf( header2, "Current allocations by size:\n" );
		fputs( header2, stderr );

		for ( ix = 0 ; ix < BYSIZE_MAX ; ix++ ) {
			switch ( ix ) {
				case ENTRY_4B :
					sprintf( header3, "0 - 4 bytes:\t\t\t%ld\n", count_by_size[ENTRY_4B] );
					fputs( header3, stderr );
					break;
				case ENTRY_8B :
					sprintf( header3, "4 - 8 bytes:\t\t\t%ld\n", count_by_size[ENTRY_8B] );
					fputs( header3, stderr );
					break;
				case ENTRY_16B :
					sprintf( header3, "8 - 16 bytes:\t\t\t%ld\n", count_by_size[ENTRY_16B] );
					fputs( header3, stderr );
					break;
				case ENTRY_32B :
					sprintf( header3, "16 - 32 bytes:\t\t\t%ld\n", count_by_size[ENTRY_32B] );
					fputs( header3, stderr );
					break;
				case ENTRY_64B :
					sprintf( header3, "32 - 64 bytes:\t\t\t%ld\n", count_by_size[ENTRY_64B] );
					fputs( header3, stderr );
					break;
				case ENTRY_128B :
					sprintf( header3, "64 - 128 bytes:\t\t\t%ld\n", count_by_size[ENTRY_128B] );
					fputs( header3, stderr );
					break;
				case ENTRY_256B :
					sprintf( header3, "128 - 256 bytes:\t\t%ld\n", count_by_size[ENTRY_256B] );
					fputs( header3, stderr );
					break;
				case ENTRY_512B :
					sprintf( header3, "256 - 512 bytes:\t\t%ld\n", count_by_size[ENTRY_512B] );
					fputs( header3, stderr );
					break;
				case ENTRY_1024B :
					sprintf( header3, "512 - 1024 bytes:\t\t%ld\n", count_by_size[ENTRY_1024B] );
					fputs( header3, stderr );
					break;
				case ENTRY_2048B :
					sprintf( header3, "1024 - 2048 bytes:\t\t%ld\n", count_by_size[ENTRY_2048B] );
					fputs( header3, stderr );
					break;
				case ENTRY_4096B :
					sprintf( header3, "2048 - 4096 bytes:\t\t%ld\n", count_by_size[ENTRY_4096B] );
					fputs( header3, stderr );
					break;
				default : 
					sprintf( header3, "4096 + bytes:\t\t\t%ld\n", count_by_size[ENTRY_MAXB] );
					fputs( header3, stderr );
					break;
			}	/* switch */
		}
		
		fputs( header1, stderr );
	}

	last_time = t;

	// Unlock data structures
	pthread_mutex_unlock( &cmutex );
}	/* counter_update */

/* The "dlsym" function needs dynamic memory before we can resolve the real memory allocator routines.
 * To support this, we offer simple mmap-based allocation during alloc_init_pending. 
 */
void *
zalloc_internal(size_t size)
{
	static pthread_mutex_t zmutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef	DEBUGOUT
	fputs( "alloc.so: zalloc_internal called", stderr );
#endif	/* DEBUGOUT */

	if ( zalloc_cnt >= ZALLOC_MAX-1 ) {
		fputs( "alloc.so: Out of internal memory\n", stderr );

		return( NULL );
	}

	/* Anonymous mapping ensures that pages are zero'd */
	void *ptr = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0 );
	if ( MAP_FAILED == ptr ) {
		perror( "alloc.so: zalloc_internal mmap failed" );
		return( NULL );
	}

	// Lock data structures to be thread-safe
	pthread_mutex_lock( &zmutex );

	zalloc_list[zalloc_cnt++] = ptr; /* keep track for later calls to free */

	// Unlock data structures
	pthread_mutex_unlock( &zmutex );

	counter_update( size );

	return( ptr );
}	/* zalloc_internal */

/*
 * free() function
 */
void
free( void* ptr )
{
	size_t	size = 0;

	if ( alloc_init_pending ) {
#ifdef	DEBUGOUT
		fputs( "alloc.so: free internal\n", stderr );
#endif	/* DEBUGOUT */

		/* Ignore 'free' during initialization and ignore potential mem leaks 
		 * On the tested system, this did not happen
		 */
		return;
	}

	if( !real_malloc )	alloc_init();

	for ( size_t i = 0 ; i < zalloc_cnt ; i++ ) {
		if ( zalloc_list[i] == ptr ) {
			/* If dlsym cleans up its dynamic memory allocated with zalloc_internal,
			 * we intercept and ignore it, as well as the resulting mem leaks.
			 * On the tested system, this did not happen
			 */
			size = sizeof( *ptr );
			counter_update( -1 * size );

			return;
		}
	}

	size = sizeof( *ptr );
	real_free( ptr );

	counter_update( -1 * size );
}	/* free */

/*
 * malloc() function
 */
void *
malloc( size_t size )
{
	if ( alloc_init_pending ) {
#ifdef	DEBUGOUT
		fputs( "alloc.so: malloc internal\n", stderr );
#endif	/* DEBUGOUT */

		return( zalloc_internal(size) );
	}

	if ( !real_malloc )	alloc_init();

	// Call glibc malloc() function
	void* result = real_malloc( size );
	counter_update( size );

	return( result );
}	/* malloc */

/*
 * realloc() function
 */
void *
realloc( void* ptr, size_t size )
{
	void*	result;
	size_t	origSize;

	if ( alloc_init_pending ) {
#ifdef	DEBUGOUT
		fputs( "alloc.so: realloc internal\n", stderr );
#endif	/* DEBUGOUT */

		if ( ptr ) {
			fputs( "alloc.so: realloc resizing not supported\n", stderr );
			exit( 1 );
 		}

		return( zalloc_internal(size) );
	}

	if ( !real_malloc )	alloc_init();

	if ( (ptr !=  NULL) && (size == 0) ) {
		free( ptr );

		return( NULL );
	}
	else if ( ptr == NULL )	result = malloc( size );
	else {
		// Call glibc realloc() function
		result = NULL;
		origSize = sizeof( *ptr );
		if ( (result = real_realloc(ptr, size)) != NULL ) {
			counter_update( -1 * origSize );
			counter_update( size );
		}
	}

	return( result );
}	/* realloc */

/*
 * calloc() function
 */
void *
calloc( size_t nmemb, size_t size )
{
	if ( alloc_init_pending ) {
#ifdef	DEBUGOUT
		fputs( "alloc.so: calloc internal\n", stderr );
#endif	/* DEBUGOUT */

		/* Be aware of integer overflow in nmemb*size.
		 * Can only be triggered by dlsym */
		return( zalloc_internal(nmemb * size) );
	}

	if ( !real_malloc )	alloc_init();

	// Call glibc calloc() function
	void *result = real_calloc( nmemb, size );
	counter_update( nmemb * size );

	return( result );
}	/* calloc */
