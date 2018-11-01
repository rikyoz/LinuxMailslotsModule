#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "../src/mailslot.h"
#include "../src/mailslot_driver.h"

#define DEVICE_FILE "/dev/test_mailslot"
#define NEW_MAX_MSG_SIZE (DEFAULT_MAX_MSG_SIZE / 2)

/* terminal colors */
#define GREEN_STR( str ) "\x1B[92m"str"\x1B[0m"
#define RED_STR( str )   "\x1B[91m"str"\x1B[0m"

#define REQUIRE( expr, error_str ) \
do { \
    if ( !( expr ) ) { \
        printf( RED_STR( "[ERROR] (%s)\n" ), error_str ); \
        return; \
    } \
} while ( 0 )

int is_nonblocking( int fd );

void set_nonblocking( int fd, int non_blocking );

int fill_device( int fd, char* buffer, size_t size );

void cleanup_device( int fd );

void test_mailslot( int fd ) {
    int cres; /* results of calls */
    int pid;
    char buffer[ 4096 ];

    {/* ioctl test */
        printf("Testing ioctl...             ");

        cres = ioctl( fd, MAILSLOT_SET_NONBLOCKING, 1 );
        REQUIRE( cres == 0, "failed to set non-blocking IO!" );
        REQUIRE( is_nonblocking(fd), "ioctl succeeded but IO is still blocking!" );

        cres = ioctl( fd, MAILSLOT_SET_NONBLOCKING, 0 );
        REQUIRE( cres == 0, "failed to set blocking IO!" );
        REQUIRE( !is_nonblocking(fd), "ioctl succeeded but IO is still blocking!" );

        cres = ioctl( fd, MAILSLOT_SET_MAX_MSG_SIZE, NEW_MAX_MSG_SIZE );
        REQUIRE( cres == 0, "failed to set max data unit size!" );

        cres = ioctl( fd, MAILSLOT_SET_MAX_MSG_SIZE, LIMIT_MAX_MSG_SIZE );
        REQUIRE( cres == 0, "failed to set max data unit size to upper limit!" );

        cres = ioctl( fd, MAILSLOT_SET_MAX_MSG_SIZE, -10 );
        REQUIRE( cres == -1, "succeeded in setting a negative max data unit size (-10)!" );

        cres = ioctl( fd, MAILSLOT_SET_MAX_MSG_SIZE, 0 );
        REQUIRE( cres == -1, "succeeded in setting an invalid max data unit size (0)!" );

        cres = ioctl( fd, MAILSLOT_SET_MAX_MSG_SIZE, LIMIT_MAX_MSG_SIZE + 1 );
        REQUIRE( cres == -1, "succeeded in setting an invalid max data unit size (upper limit)!" );

        cres = ioctl( fd, MAILSLOT_SET_MAX_MSG_SIZE, DEFAULT_MAX_MSG_SIZE );
        REQUIRE( cres == 0, "failed to reset max data unit size to default value!" );

        cres = ioctl( fd, 42 );
        REQUIRE( cres == -1, "succeeded in sending a non valid ioctl command!" );

        printf( GREEN_STR( "[OK]\n" ) );
    }

    {/* basic read/write test */
        printf("Testing basic read/write...  "); /* expecting empty slot! */

        cres = write( fd, "ciao mondo!", 12 );
        REQUIRE( cres == 12, "failed in writing a message!" );

        cres = read( fd, buffer, 4096 );
        REQUIRE( cres == 12, "failed in reading a message!" );

        printf( GREEN_STR( "[OK]\n" ) );
    }

    {/* queue order test */
        printf("Testing queue order...       "); /* expecting empty slot! */

        cres = write( fd, "abc", 4 );
        REQUIRE( cres == 4, "failed in writing a message!" );

        cres = write( fd, "123", 4 );
        REQUIRE( cres == 4, "failed in writing a message!" );

        cres = write( fd, "xyz", 4 );
        REQUIRE( cres == 4, "failed in writing a message!" );

        cres = read( fd, buffer, 4096 );
        REQUIRE( cres == 4, "failed in reading a message!" );
        REQUIRE( strncmp( buffer, "abc", 3 ) == 0, "retrieved wrong message" );

        cres = read( fd, buffer, 4096 );
        REQUIRE( cres == 4, "failed in reading a message!" );
        REQUIRE( strncmp( buffer, "123", 3 ) == 0, "retrieved wrong message" );

        cres = read( fd, buffer, 4096 );
        REQUIRE( cres == 4, "failed in reading a message!" );
        REQUIRE( strncmp( buffer, "xyz", 3 ) == 0, "retrieved wrong message" );

        printf( GREEN_STR( "[OK]\n" ) );
    }

    {/* write test */
        printf("Testing write...             "); /* expecting empty slot! */

        cres = write( fd, "ciao mondo!", 24 );
        REQUIRE( cres == 24, "failed in writing a msg (double of real size)!" );

        cres = write( fd, NULL, 24 );
        REQUIRE( cres == -1, "succeeded in writing a NULL msg!" );

        /* BVA (size must be in range [1, DEFAULT_MAX_MSG_SIZE]) */
        cres = write( fd, "ciao mondo!", 1 );
        REQUIRE( cres == 1, "failed in writing a msg with size 1!" );

        cres = write( fd, "ciao mondo!", DEFAULT_MAX_MSG_SIZE );
        REQUIRE( cres == DEFAULT_MAX_MSG_SIZE, "failed in writing a msg with size DEFAULT_MAX_MSG_SIZE!" );

        cres = write( fd, "ciao mondo!", 0 );
        REQUIRE( cres == 0, "succeeded in writing a msg with size 0!" );

        cres = write( fd, "ciao mondo!", DEFAULT_MAX_MSG_SIZE + 1 );
        REQUIRE( cres == -1, "succeeded in writing a msg with size greater than (default) max!" );

        /* BVA (with new size range [1, DEFAULT_MAX_MSG_SIZE/2]) */
        cres = ioctl( fd, MAILSLOT_SET_MAX_MSG_SIZE, NEW_MAX_MSG_SIZE );
        REQUIRE( cres == 0, "failed to set max data unit size!" );

        cres = write( fd, "ciao mondo!", NEW_MAX_MSG_SIZE );
        REQUIRE( cres == NEW_MAX_MSG_SIZE, "failed in writing a msg with size DEFAULT_MAX_MSG_SIZE/2!" );

        cres = write( fd, "ciao mondo!", NEW_MAX_MSG_SIZE + 1 );
        REQUIRE( cres == -1, "succeeded in writing a msg with size greater than max!" );

        cleanup_device( fd );

        /* reset max msg size to default value */
        cres = ioctl( fd, MAILSLOT_SET_MAX_MSG_SIZE, DEFAULT_MAX_MSG_SIZE );
        REQUIRE( cres == 0, "failed to set max data unit size!" );

        printf( GREEN_STR( "[OK]\n" ) );
    }

    {/* waiting write test */
        printf("Testing waiting write...     "); /* expecting empty slot */

        cres = fill_device( fd, "hello world!", 12 );
        REQUIRE( cres == 1, "failed to fill device!" );

        pid = fork();
        REQUIRE( pid >= 0, "failed to fork!" );

        if ( pid == 0 ) { /* child */
            sleep(2);
            cres = read( fd, buffer, 12 );
            REQUIRE( cres == 12, "failed in reading a msg from child!" );
            return;
        } else { /* parent */
            cres = write( fd, "ciao mondo!", 12 );
            REQUIRE( cres == 12, "failed in writing a message from waiting parent!" );
            cleanup_device( fd );
        }

        printf( GREEN_STR( "[OK]\n" ) );
    }

    {/* non-waiting write test */
        printf("Testing non-waiting write... "); /* expecting empty slot and non-blocking io! */

        set_nonblocking( fd, 1 );

        /* still blocking, but we do not exceed the max slot size, hence it should not fail! */
        cres = fill_device( fd, "abc", 4 );
        REQUIRE( cres == 1, "failed to fill device!" );

        cres = write( fd, "ciao mondo!", 12 ); /* WRITING TO FULL FILLED SLOT! */
        REQUIRE( cres == -1, "succeeded in writing to a full filled slot!" );

        cleanup_device( fd );

        printf( GREEN_STR( "[OK]\n" ) );
    }

    {/* read test */
        printf("Testing read...              "); /* expecting empty slot! */

        cres = fill_device( fd, "hello world!", 12 );
        REQUIRE( cres == 1, "failed to fill device!" );

        cres = read( fd, buffer, 12 );
        REQUIRE( cres == 12, "failed in reading a msg!" );

        cres = read( fd, buffer, 11 );
        REQUIRE( cres == -1, "succeeded in reading a msg with size greater than the buffer size!" );

        cres = read( fd, buffer, 0 );
        REQUIRE( cres == 0, "succeeded in reading a msg to a 0-size buffer!" );

        cres = read( fd, NULL, 12 );
        REQUIRE( cres == 0, "succeeded in reading a msg to a NULL buffer!" );

        cleanup_device( fd );

        printf( GREEN_STR( "[OK]\n" ) );
    }

    {/* waiting read test */
        printf("Testing waiting read...      "); /* expecting empty slot and blocking io! */

        pid = fork();
        REQUIRE( pid >= 0, "failed to fork!" );

        if ( pid == 0 ) { /* child */
            sleep(2);
            cres = write( fd, "ciao mondo!", 12 );
            REQUIRE( cres == 12, "failed in writing a message from child!" );
            return;
        } else { /* parent */
            cres = read( fd, buffer, 12 );
            REQUIRE( cres == 12, "failed in reading a msg from waiting parent!" );
        }

        printf( GREEN_STR( "[OK]\n" ) );
    }

    { /* non-waiting read test */
        printf("Testing non-waiting read...  "); /* expecting empty slot and non-blocking io! */

        cleanup_device( fd );

        set_nonblocking( fd, 1 );

        cres = read( fd, buffer, 12 );
        REQUIRE( cres == -1, "succeeded in reading a msg from an empty slot!" );

        set_nonblocking( fd, 0 );

        printf( GREEN_STR( "[OK]\n" ) );
    }

    printf( GREEN_STR( "All tests were successful! No error occured!\n" ) );
}

int main() {
    int fd = open( DEVICE_FILE, O_RDWR );

    printf( RED_STR( "###### Linux Mail Slots -- TEST SUITE ######\n" ) );

    if ( fd < 0 ) {
        printf( RED_STR("[ERROR] Couldn't open device file!\n") );
        return 0;
    }

    setbuf(stdout, NULL); /* disable buffering */

    test_mailslot( fd );

    close( fd );
    return 0;
}

int is_nonblocking( int fd ) {
    int flags = fcntl( fd, F_GETFL, 0 );
    return flags & O_NONBLOCK;
}

void set_nonblocking( int fd, int non_blocking ) {
    int flags = fcntl( fd, F_GETFL, 0 );
    if ( non_blocking ) {
        fcntl( fd, F_SETFL, flags | O_NONBLOCK );
    } else {
        fcntl( fd, F_SETFL, flags & ~O_NONBLOCK );
    }
}

int fill_device( int fd, char* buffer, size_t size ) {
    int i, cres = 1;
    for ( i = 0; i < MAX_SLOT_SIZE; ++i ) {
        cres = cres && ( write( fd, buffer, size ) != -1 );
    }
    return cres;
}

void cleanup_device( int fd ) {
    int n;
    char buffer[ 4096 ];
    set_nonblocking( fd, 1 );
    do {
        n = read( fd, buffer, 4096 );
    } while ( n >= 0 );
    set_nonblocking( fd, 0 );
}