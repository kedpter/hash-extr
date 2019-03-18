/*
   BLAKE2 reference source code package - reference C implementations

   Written in 2012 by Samuel Neves <sneves@dei.uc.pt>

   To the extent possible under law, the author(s) have dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.

   You should have received a copy of the CC0 Public Domain Dedication along with
   this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/
#pragma once
#ifndef __BLAKE2_IMPL_H__
#define __BLAKE2_IMPL_H__

#include <stdint.h>

inline static uint32_t load32( const void *src )
{
#if ARCH_LITTLE_ENDIAN
  return *( uint32_t * )( src );
#else
  const uint8_t *p = ( uint8_t * )src;
  uint32_t w = *p++;
  w |= ( uint32_t )( *p++ ) <<  8;
  w |= ( uint32_t )( *p++ ) << 16;
  w |= ( uint32_t )( *p++ ) << 24;
  return w;
#endif
}

inline static uint64_t load64( const void *src )
{
#if ARCH_LITTLE_ENDIAN
  return *( uint64_t * )( src );
#else
  const uint8_t *p = ( uint8_t * )src;
  uint64_t w = *p++;
  w |= ( uint64_t )( *p++ ) <<  8;
  w |= ( uint64_t )( *p++ ) << 16;
  w |= ( uint64_t )( *p++ ) << 24;
  w |= ( uint64_t )( *p++ ) << 32;
  w |= ( uint64_t )( *p++ ) << 40;
  w |= ( uint64_t )( *p++ ) << 48;
  w |= ( uint64_t )( *p++ ) << 56;
  return w;
#endif
}

inline static void store32( void *dst, uint32_t w )
{
#if ARCH_LITTLE_ENDIAN
  *( uint32_t * )( dst ) = w;
#else
  uint8_t *p = ( uint8_t * )dst;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w;
#endif
}

inline static void store64( void *dst, uint64_t w )
{
#if ARCH_LITTLE_ENDIAN
  *( uint64_t * )( dst ) = w;
#else
  uint8_t *p = ( uint8_t * )dst;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w;
#endif
}

inline static uint64_t load48( const void *src )
{
  const uint8_t *p = ( const uint8_t * )src;
  uint64_t w = *p++;
  w |= ( uint64_t )( *p++ ) <<  8;
  w |= ( uint64_t )( *p++ ) << 16;
  w |= ( uint64_t )( *p++ ) << 24;
  w |= ( uint64_t )( *p++ ) << 32;
  w |= ( uint64_t )( *p++ ) << 40;
  return w;
}

inline static void store48( void *dst, uint64_t w )
{
  uint8_t *p = ( uint8_t * )dst;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w;
}

inline static uint32_t rotl32( const uint32_t w, const unsigned c )
{
  return ( w << c ) | ( w >> ( 32 - c ) );
}

inline static uint64_t rotl64( const uint64_t w, const unsigned c )
{
  return ( w << c ) | ( w >> ( 64 - c ) );
}

inline static uint32_t rotr32( const uint32_t w, const unsigned c )
{
  return ( w >> c ) | ( w << ( 32 - c ) );
}

inline static uint64_t rotr64( const uint64_t w, const unsigned c )
{
  return ( w >> c ) | ( w << ( 64 - c ) );
}

#if 0 /* For our use, this is not wanted */
/* prevents compiler optimizing out memset() */
inline static void secure_zero_memory( void *v, size_t n )
{
  volatile uint8_t *p = ( volatile uint8_t * )v;

  while( n-- ) *p++ = 0;
}
#endif

#endif
