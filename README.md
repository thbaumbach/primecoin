Xolo's Primecoin Pool Miner
===========================

This is a pool mining version of primecoin, the so-called xolominer,
based on mikaelh's high performance version of Sunny King's Primecoin tree.

Features:
- pooled mining client
- getwork-protocol (with longpoll-support)
- code integration into original client

See forked project for more information.

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

Building xolominer
==================

See [Build Notes](doc/README.md) for detailled information on compiling.

Dependencies:
 - libssl (SSL Support)
 - libdb4.8 (Berkeley DB)
 - libboost (Boost C++ Library)
 - libgmp (GNU Multiprecision)
 - miniupnpc (UPnP Support)

Primecoin integration/staging tree
==================================

http://primecoin.org

Copyright (c) 2013 Primecoin Developers

Copyright (c) 2009-2013 Bitcoin Developers

Copyright (c) 2011-2013 PPCoin Developers

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

Primecoin is released under conditional MIT license. See  COPYING` for more
information.

Development process
-------------------

Developers work in their own trees, then submit pull requests when they think
their feature or bug fix is ready.

If it is a simple/trivial/non-controversial change, then one of the Bitcoin
development team members simply pulls it.

If it is a *more complicated or potentially controversial* change, then the patch
submitter will be asked to start a discussion (if they haven't already) on the
ppcoin/primecoin forum (http://ppcointalk.org).

The patch will be accepted if there is broad consensus that it is a good thing.
Developers should expect to rework and resubmit patches if the code doesn't
match the project's coding conventions (see `doc/coding.txt`) or are
controversial.

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/primecoin/primecoin/tags) are
created regularly to indicate new official, stable release versions of
Primecoin.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test. Please be patient and help out, and
remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write unit tests for new code, and to
submit new unit tests for old code.

Unit tests for the core code are in `src/test/`. To compile and run them:

    cd src; make -f makefile.unix test

Unit tests for the GUI code are in `src/qt/test/`. To compile and run them:

    qmake BITCOIN_QT_TEST=1 -o Makefile.test bitcoin-qt.pro
    make -f Makefile.test
    ./bitcoin-qt_test

### Manual Quality Assurance (QA) Testing

Large changes should have a test plan, and should be tested by somebody other
than the developer who wrote the code.

See https://github.com/bitcoin/QA/ for how to create a test plan.
