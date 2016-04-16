#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/**
 *  Only little-endian case, pack data and assign with bus size alignment
 */

///////////////////////////////////
typedef unsigned long       ulong_t;
typedef unsigned long       reg_uint_t;

////////////////////////////////////
void*
duff_copy(char* to, char* from, size_t count)
{
    size_t n = (count+7) >> 3;
    switch( count & 0x7 )
    {
        case 0: do{ *to++ = *from++;
        case 7:     *to++ = *from++;
        case 6:     *to++ = *from++;
        case 5:     *to++ = *from++;
        case 4:     *to++ = *from++;
        case 3:     *to++ = *from++;
        case 2:     *to++ = *from++;
        case 1:     *to++ = *from++;
                }while(--n > 0);
    }
    return (void*)to;
}

#define BUS_SIZE        4

#define SHIFT_ASSIGN(pDest_align, pSrc_align, pSrc_cache, src_shift, word_size)   \
    do{ ulong_t  dest_word = 0;                                                   \
        dest_word      = *(pSrc_cache) >> (src_shift << 3);                       \
        *(pSrc_cache)  = *pSrc_align++;                                           \
        dest_word     |= *(pSrc_cache) << ((word_size - src_shift) << 3);         \
        *pDest_align++ = dest_word;                                               \
    }while(0)

void* my_copy(char *to, char *from, size_t count)
{
    char    *pDest = to;
    char    *pSrc = from;
    int     bus_size = sizeof(void*), align_mask = 0;
    int     shift_bytes = 0;

    align_mask = bus_size - 1;

    // less than 8
    if( count < 8 )
    {
        switch( count )
        {
            case 7: *pDest++ = *pSrc++;
            case 6: *pDest++ = *pSrc++;
            case 5: *pDest++ = *pSrc++;
            case 4: *pDest++ = *pSrc++;
            case 3: *pDest++ = *pSrc++;
            case 2: *pDest++ = *pSrc++;
            case 1: *pDest++ = *pSrc++;
            case 0:
            default: break;
        }
        return (void*)to;
    }

    // alignment dest_addr
    while( (ulong_t)pDest & align_mask )
    {
        *pDest++ = *pSrc++;
        count--;
    }

    if( (shift_bytes = (ulong_t)pSrc & align_mask) )
    {
        // pack source and aligned assign
        ulong_t         *pAlign_src = (ulong_t*)((ulong_t)pSrc & ~align_mask);
        ulong_t         *pAlign_dest = (ulong_t*)((ulong_t)pDest & ~align_mask);
        ulong_t         src_cache = 0;
        size_t          length = (bus_size == 4) ? count >> 2 : count >> 3;

        src_cache = *pAlign_src++;

        while( length & 7 )
        {
            SHIFT_ASSIGN(pAlign_dest, pAlign_src, &src_cache, shift_bytes, bus_size);
            length--;
        }

        length = length >> 3;
        while( length-- )
        {
            SHIFT_ASSIGN(pAlign_dest, pAlign_src, &src_cache, shift_bytes, bus_size);
            SHIFT_ASSIGN(pAlign_dest, pAlign_src, &src_cache, shift_bytes, bus_size);
            SHIFT_ASSIGN(pAlign_dest, pAlign_src, &src_cache, shift_bytes, bus_size);
            SHIFT_ASSIGN(pAlign_dest, pAlign_src, &src_cache, shift_bytes, bus_size);
            SHIFT_ASSIGN(pAlign_dest, pAlign_src, &src_cache, shift_bytes, bus_size);
            SHIFT_ASSIGN(pAlign_dest, pAlign_src, &src_cache, shift_bytes, bus_size);
            SHIFT_ASSIGN(pAlign_dest, pAlign_src, &src_cache, shift_bytes, bus_size);
            SHIFT_ASSIGN(pAlign_dest, pAlign_src, &src_cache, shift_bytes, bus_size);
        }

        pSrc  = (char*)pAlign_src + (shift_bytes - bus_size);
        pDest = (char*)pAlign_dest;
    }
    else
    {
        ulong_t         *pAlign_src = (ulong_t*)pSrc;
        ulong_t         *pAlign_dest = (ulong_t*)pDest;
        size_t          length = (bus_size == 4) ? count >> 2 : count >> 3;

        while( length & 7 )
        {
            *pAlign_dest++ = *pAlign_src++;
            length--;
        }

        length = length >> 3;

        while( length-- )
        {
            *pAlign_dest++ = *pAlign_src++;
            *pAlign_dest++ = *pAlign_src++;
            *pAlign_dest++ = *pAlign_src++;
            *pAlign_dest++ = *pAlign_src++;
            *pAlign_dest++ = *pAlign_src++;
            *pAlign_dest++ = *pAlign_src++;
            *pAlign_dest++ = *pAlign_src++;
            *pAlign_dest++ = *pAlign_src++;

        }

        pSrc  = (char*)pAlign_src;
        pDest = (char*)pAlign_dest;
    }

    // tail of non-aligned
    switch( count & align_mask )
    {
        case 7: *pDest++ = *pSrc++;
        case 6: *pDest++ = *pSrc++;
        case 5: *pDest++ = *pSrc++;
        case 4: *pDest++ = *pSrc++;
        case 3: *pDest++ = *pSrc++;
        case 2: *pDest++ = *pSrc++;
        case 1: *pDest++ = *pSrc++;
        case 0:
        default: break;
    }

    return (void*)to;
}

void *
my_memset(void *dst, int c, size_t count)
{
    register int            bus_size = sizeof(void*), align_mask = 0;
    register unsigned int   bit_shift_num = 0;
    register ulong_t        value;
    register size_t         length;
    register unsigned char  *pDest = (unsigned char*)dst;

    align_mask = bus_size - 1;
    c = c & 0xFF;

	/**
	 * If not enough words, just fill bytes.  A count >= 2 words
	 * guarantees that at least one of them is 'complete' after
	 * any necessary alignment.  For instance:
	 *
	 *	|-----------|-----------|-----------|
	 *	|00|01|02|03|04|05|06|07|08|09|0A|00|
	 *	          ^---------------------^
	 *		 dst		 dst+count-1
	 */
	if( count < 3 * bus_size )
    {
		while( count )
		{
			*pDest++ = c;
			--count;
		}
		return (void*)dst;
	}

	// generate pattern
	value = c;
	length = align_mask;
	do {
        value = (value << 8) | (value & 0xFF);
	}while( length-- );

	// Align destination by filling in bytes.
	if( (length = (ulong_t)pDest & align_mask) )
    {
		length = bus_size - length;
		count -= length;
		do {
			*pDest++ = c;
		} while ( --length );
	}

	bit_shift_num = (bus_size == 4) ? 2 : 3;

	// Fill words.
	length = count >> bit_shift_num;
	do {
		*(ulong_t*)pDest = value;
		pDest += bus_size;
	} while ( --length );

	// Mop up trailing bytes
	if( (length = count & align_mask) )
    {
		do {
			*pDest++ = c;
		} while( --length );
    }

    return (void*)dst;
}

////////////////////////////////////
int main()
{
    int             i;
    unsigned char   *pBuf_src = 0, *pBuf_dest = 0;
    int             buf_size = 10240;
    int             move_len = 261;

    pBuf_src = malloc(buf_size);
    for(i = 0; i < buf_size; i++)
        *(pBuf_src + i) = i + 1;

    pBuf_dest = malloc(buf_size);


    for(i = 0; i < 101; i++)
    {
        int     j;
        memset(pBuf_dest, 0x0, buf_size);

    #if 1
        my_copy(pBuf_dest + i, pBuf_src, move_len);
        for(j = 0; j < move_len; j++)
        {
            if( *(pBuf_dest + i + j) != *(pBuf_src + j) )
            {
                printf("diff %02d-th: %02x, %02x\n",
                       j, *(pBuf_dest + i + j), *(pBuf_src + j));
            }
        }
    #else
        my_memset(pBuf_dest + i, 0xaa, move_len);
        for(j = 0; j < move_len; j++)
        {
            if( *(pBuf_dest + i + j) != 0xaa )
            {
                printf("diff %02d-th: %02x\n",
                       j, *(pBuf_dest + i + j));
            }
        }
    #endif
        printf("\n");
    }


    if( pBuf_src )      free(pBuf_src);
    if( pBuf_dest )     free(pBuf_dest);

    return 0;
}
