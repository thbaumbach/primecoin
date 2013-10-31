Xolominer Documentation
=======================

This is the xolominer based on the primecoin high performance version of Sunny King's Primecoin tree.

Xolominer features:
- pooled mining client
- getwork-protocol (with longpoll-support)
- code integration into original client

Primecoin tree features:
 * Use GMP for bignum calculations in the mining threads
 * Replaced some bignum calculations with 64-bit arithmetic inside the sieve
 * Reduced the amount of memory allocations
 * L1 and L2 cache optimizations
 * Process only 10% of base primes when weaving the sieve
 * Configurable sieve size

Usage
-----

`primeminer [-options]`

 * `-pooluser=[user]` Pool worker user name
 * `-poolpassword=[pass]` Pool worker password
 * `-poolip=[host]` Pool mining ip or host address
 * `-poolport=[port]` Pool mining port
 * `-poolshare=[chainlength]` Minimum chain length of submitted shares, default 7
 * `-genproclimit=[threads]` Number of CPU-Threads to use (1-32)
 * `-minerid=[0-65000]` A free-to-choose worker ID
 * `-poolfee=[1-100]` Set pool fee ín percent, if supported by pool

Documentation overview
----------------------
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-msw.md)
- [Coding Guidelines](coding.md)
- [Release Process](release-process.md)
