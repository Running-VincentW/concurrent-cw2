#include "prime.h"
#include "libc.h"

/*
 * Return whether x is prime or not
 *
 * Returns:
 *   1  - prime
 *   0  - undefined
 *   -1 - not prime
 */
int isPrime( uint32_t x ) {
  if ( !( x & 1 ) || ( x < 2 ) ) {
    return ( x == 2 );
  }

  for( uint32_t d = 3; ( d * d ) <= x ; d += 2 ) {
    if( !( x % d ) ) {
      return 0;
    }
  }

  return 1;
}

/*
 * Return the next prime after x, or x if x is prime
 */
int next_prime(int x) {
    while (isPrime(x) != 1) {
        x++;
    }
    return x;
}
