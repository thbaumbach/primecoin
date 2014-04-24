Xolo's Primecoin Pool Miner
===========================

This is a pool mining version of primecoin, the so-called xolominer,
based on mikaelh's high performance version of Sunny King's Primecoin tree.

Features:
- pooled mining client
- user, password, miner-id, pool & developer fee integration

Usage
-----

`primeminer [-options]`

 * `-pooluser=[user]` Pool worker user name
 * `-poolpassword=[pass]` Pool worker password
 * `-poolip=[host]` Pool mining ip or host address
 * `-poolport=[port]` Pool mining port
 * `-genproclimit=[threads]` Number of CPU-Threads to use
 * `-poolshare=[chainlength]` Minimum chain length of submitted shares, if supported by pool, default 7
 * `-minerid=[0-65000]` A free-to-choose worker ID, if supported by pool, for statistical purpose
 * `-poolfee=[1-100]` Set pool fee ín percent, if supported by pool, default 2
 * `-devfeeid=[id]` Set the developer fee ID, if supported by pool, default 0

Building xolominer
==================

Use CMake to build. Usually: mkdir bin && cd bin && cmake .. && make

Dependencies:
 - libssl (SSL Support)
 - libboost (Boost C++ Library)
 - libgmp (GNU Multiprecision)

MinGW/Cygwin hint
-----------------

cmake -G "MSYS Makefiles" -DBOOST_ROOT=<BOOST_ROOT_DIR> -DOpenSSL_ROOT_DIR=<OPENSSL_ROOT_DIR> ..
 
Primecoin integration/staging tree
==================================

http://primecoin.org

Copyright (c) 2013-2014 Xolominer Developers

Copyright (c) 2013-2014 Primecoin Developers

Copyright (c) 2009-2014 Bitcoin Developers

Copyright (c) 2011-2014 PPCoin Developers

What is Primecoin?
------------------

Primecoin is an experimental cryptocurrency that introduces the first
scientific computing proof-of-work to cryptocurrency technology. Primecoin's
proof-of-work is an innovative design based on searching for prime number
chains, providing potential scientific value in addition to minting and
security for the network. Similar to Bitcoin, Primecoin enables instant payments
to anyone, anywhere in the world. It also uses peer-to-peer technology to
operate with no central authority: managing transactions and issuing money are
carried out collectively by the network. Primecoin is also the name of the open
source software which enables the use of this currency.

For more information, as well as an immediately useable, binary version of
the Primecoin client sofware, see http://primecoin.org.

License
-------

Primecoin's mining code is released under conditional MIT license. See  COPYING` for more
information.
