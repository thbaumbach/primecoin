// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_SERIALIZE_H
#define BITCOIN_SERIALIZE_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cassert>
#include <limits>
#include <cstring>
#include <cstdio>

#include <boost/type_traits/is_fundamental.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/tuple/tuple_io.hpp>

/*#include "allocators.h"
#include "version.h"

typedef long long  int64;
typedef unsigned long long  uint64;

class CScript;
class CDataStream;
class CAutoFile;*/
static const unsigned int MAX_SIZE = 0x02000000;

// Used to bypass the rule against non-const reference to temporary
// where it makes sense with wrappers such as CFlatData or CTxDB
template<typename T>
inline T& REF(const T& val)
{
    return const_cast<T&>(val);
}

/////////////////////////////////////////////////////////////////
//
// Templates for serializing to anything that looks like a stream,
// i.e. anything that supports .read(char*, int) and .write(char*, int)
//

#define IMPLEMENT_SERIALIZE(statements)    \
    unsigned int GetSerializeSize(int nType, int nVersion) const  \
    {                                           \
        CSerActionGetSerializeSize ser_action;  \
        const bool fGetSize = true;             \
        const bool fWrite = false;              \
        const bool fRead = false;               \
        unsigned int nSerSize = 0;              \
        ser_streamplaceholder s;                \
        assert(fGetSize||fWrite||fRead); /* suppress warning */ \
        s.nType = nType;                        \
        s.nVersion = nVersion;                  \
        {statements}                            \
        return nSerSize;                        \
    }                                           \
    template<typename Stream>                   \
    void Serialize(Stream& s, int nType, int nVersion) const  \
    {                                           \
        CSerActionSerialize ser_action;         \
        const bool fGetSize = false;            \
        const bool fWrite = true;               \
        const bool fRead = false;               \
        unsigned int nSerSize = 0;              \
        assert(fGetSize||fWrite||fRead); /* suppress warning */ \
        {statements}                            \
    }                                           \
    template<typename Stream>                   \
    void Unserialize(Stream& s, int nType, int nVersion)  \
    {                                           \
        CSerActionUnserialize ser_action;       \
        const bool fGetSize = false;            \
        const bool fWrite = false;              \
        const bool fRead = true;                \
        unsigned int nSerSize = 0;              \
        assert(fGetSize||fWrite||fRead); /* suppress warning */ \
        {statements}                            \
    }

#define READWRITE(obj)      (nSerSize += ::SerReadWrite(s, (obj), nType, nVersion, ser_action))

//
// Basic types
//
#define WRITEDATA(s, obj)   s.write((char*)&(obj), sizeof(obj))
#define READDATA(s, obj)    s.read((char*)&(obj), sizeof(obj))

inline unsigned int GetSerializeSize(char a,           int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(signed char a,    int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(unsigned char a,  int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(signed short a,   int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(unsigned short a, int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(signed int a,     int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(unsigned int a,   int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(signed long a,    int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(unsigned long a,  int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(int64 a,          int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(uint64 a,         int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(float a,          int, int=0) { return sizeof(a); }
inline unsigned int GetSerializeSize(double a,         int, int=0) { return sizeof(a); }

template<typename Stream> inline void Serialize(Stream& s, char a,           int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, signed char a,    int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, unsigned char a,  int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, signed short a,   int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, unsigned short a, int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, signed int a,     int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, unsigned int a,   int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, signed long a,    int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, unsigned long a,  int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, int64 a,          int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, uint64 a,         int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, float a,          int, int=0) { WRITEDATA(s, a); }
template<typename Stream> inline void Serialize(Stream& s, double a,         int, int=0) { WRITEDATA(s, a); }

template<typename Stream> inline void Unserialize(Stream& s, char& a,           int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, signed char& a,    int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, unsigned char& a,  int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, signed short& a,   int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, unsigned short& a, int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, signed int& a,     int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, unsigned int& a,   int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, signed long& a,    int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, unsigned long& a,  int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, int64& a,          int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, uint64& a,         int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, float& a,          int, int=0) { READDATA(s, a); }
template<typename Stream> inline void Unserialize(Stream& s, double& a,         int, int=0) { READDATA(s, a); }

inline unsigned int GetSerializeSize(bool a, int, int=0)                          { return sizeof(char); }
template<typename Stream> inline void Serialize(Stream& s, bool a, int, int=0)    { char f=a; WRITEDATA(s, f); }
template<typename Stream> inline void Unserialize(Stream& s, bool& a, int, int=0) { char f; READDATA(s, f); a=f; }

//
// Compact size
//  size <  253        -- 1 byte
//  size <= USHRT_MAX  -- 3 bytes  (253 + 2 bytes)
//  size <= UINT_MAX   -- 5 bytes  (254 + 4 bytes)
//  size >  UINT_MAX   -- 9 bytes  (255 + 8 bytes)
//
inline unsigned int GetSizeOfCompactSize(uint64 nSize)
{
    if (nSize < 253)             return sizeof(unsigned char);
    else if (nSize <= std::numeric_limits<unsigned short>::max()) return sizeof(unsigned char) + sizeof(unsigned short);
    else if (nSize <= std::numeric_limits<unsigned int>::max())  return sizeof(unsigned char) + sizeof(unsigned int);
    else                         return sizeof(unsigned char) + sizeof(uint64);
}

template<typename Stream>
void WriteCompactSize(Stream& os, uint64 nSize)
{
    if (nSize < 253)
    {
        unsigned char chSize = nSize;
        WRITEDATA(os, chSize);
    }
    else if (nSize <= std::numeric_limits<unsigned short>::max())
    {
        unsigned char chSize = 253;
        unsigned short xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    else if (nSize <= std::numeric_limits<unsigned int>::max())
    {
        unsigned char chSize = 254;
        unsigned int xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    else
    {
        unsigned char chSize = 255;
        uint64 xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    return;
}

template<typename Stream>
uint64 ReadCompactSize(Stream& is)
{
    unsigned char chSize;
    READDATA(is, chSize);
    uint64 nSizeRet = 0;
    if (chSize < 253)
    {
        nSizeRet = chSize;
    }
    else if (chSize == 253)
    {
        unsigned short xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
    }
    else if (chSize == 254)
    {
        unsigned int xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
    }
    else
    {
        uint64 xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
    }
    if (nSizeRet > (uint64)MAX_SIZE)
        throw std::ios_base::failure("ReadCompactSize() : size too large");
    return nSizeRet;
}

//
// Forward declarations
//

// string
template<typename C> unsigned int GetSerializeSize(const std::basic_string<C>& str, int, int=0);
template<typename Stream, typename C> void Serialize(Stream& os, const std::basic_string<C>& str, int, int=0);
template<typename Stream, typename C> void Unserialize(Stream& is, std::basic_string<C>& str, int, int=0);

// vector
template<typename T, typename A> unsigned int GetSerializeSize_impl(const std::vector<T, A>& v, int nType, int nVersion, const boost::true_type&);
template<typename T, typename A> unsigned int GetSerializeSize_impl(const std::vector<T, A>& v, int nType, int nVersion, const boost::false_type&);
template<typename T, typename A> inline unsigned int GetSerializeSize(const std::vector<T, A>& v, int nType, int nVersion);
template<typename Stream, typename T, typename A> void Serialize_impl(Stream& os, const std::vector<T, A>& v, int nType, int nVersion, const boost::true_type&);
template<typename Stream, typename T, typename A> void Serialize_impl(Stream& os, const std::vector<T, A>& v, int nType, int nVersion, const boost::false_type&);
template<typename Stream, typename T, typename A> inline void Serialize(Stream& os, const std::vector<T, A>& v, int nType, int nVersion);
template<typename Stream, typename T, typename A> void Unserialize_impl(Stream& is, std::vector<T, A>& v, int nType, int nVersion, const boost::true_type&);
template<typename Stream, typename T, typename A> void Unserialize_impl(Stream& is, std::vector<T, A>& v, int nType, int nVersion, const boost::false_type&);
template<typename Stream, typename T, typename A> inline void Unserialize(Stream& is, std::vector<T, A>& v, int nType, int nVersion);

//
// string
//
template<typename C>
unsigned int GetSerializeSize(const std::basic_string<C>& str, int, int)
{
    return GetSizeOfCompactSize(str.size()) + str.size() * sizeof(str[0]);
}

template<typename Stream, typename C>
void Serialize(Stream& os, const std::basic_string<C>& str, int, int)
{
    WriteCompactSize(os, str.size());
    if (!str.empty())
        os.write((char*)&str[0], str.size() * sizeof(str[0]));
}

template<typename Stream, typename C>
void Unserialize(Stream& is, std::basic_string<C>& str, int, int)
{
    unsigned int nSize = ReadCompactSize(is);
    str.resize(nSize);
    if (nSize != 0)
        is.read((char*)&str[0], nSize * sizeof(str[0]));
}

//
// vector
//
template<typename T, typename A>
unsigned int GetSerializeSize_impl(const std::vector<T, A>& v, int nType, int nVersion, const boost::true_type&)
{
    return (GetSizeOfCompactSize(v.size()) + v.size() * sizeof(T));
}

template<typename T, typename A>
unsigned int GetSerializeSize_impl(const std::vector<T, A>& v, int nType, int nVersion, const boost::false_type&)
{
    unsigned int nSize = GetSizeOfCompactSize(v.size());
    for (typename std::vector<T, A>::const_iterator vi = v.begin(); vi != v.end(); ++vi)
        nSize += GetSerializeSize((*vi), nType, nVersion);
    return nSize;
}

template<typename T, typename A>
inline unsigned int GetSerializeSize(const std::vector<T, A>& v, int nType, int nVersion)
{
    return GetSerializeSize_impl(v, nType, nVersion, boost::is_fundamental<T>());
}


template<typename Stream, typename T, typename A>
void Serialize_impl(Stream& os, const std::vector<T, A>& v, int nType, int nVersion, const boost::true_type&)
{
    WriteCompactSize(os, v.size());
    if (!v.empty())
        os.write((char*)&v[0], v.size() * sizeof(T));
}

template<typename Stream, typename T, typename A>
void Serialize_impl(Stream& os, const std::vector<T, A>& v, int nType, int nVersion, const boost::false_type&)
{
    WriteCompactSize(os, v.size());
    for (typename std::vector<T, A>::const_iterator vi = v.begin(); vi != v.end(); ++vi)
        ::Serialize(os, (*vi), nType, nVersion);
}

template<typename Stream, typename T, typename A>
inline void Serialize(Stream& os, const std::vector<T, A>& v, int nType, int nVersion)
{
    Serialize_impl(os, v, nType, nVersion, boost::is_fundamental<T>());
}


template<typename Stream, typename T, typename A>
void Unserialize_impl(Stream& is, std::vector<T, A>& v, int nType, int nVersion, const boost::true_type&)
{
    // Limit size per read so bogus size value won't cause out of memory
    v.clear();
    unsigned int nSize = ReadCompactSize(is);
    unsigned int i = 0;
    while (i < nSize)
    {
        unsigned int blk = std::min(nSize - i, (unsigned int)(1 + 4999999 / sizeof(T)));
        v.resize(i + blk);
        is.read((char*)&v[i], blk * sizeof(T));
        i += blk;
    }
}

template<typename Stream, typename T, typename A>
void Unserialize_impl(Stream& is, std::vector<T, A>& v, int nType, int nVersion, const boost::false_type&)
{
    v.clear();
    unsigned int nSize = ReadCompactSize(is);
    unsigned int i = 0;
    unsigned int nMid = 0;
    while (nMid < nSize)
    {
        nMid += 5000000 / sizeof(T);
        if (nMid > nSize)
            nMid = nSize;
        v.resize(nMid);
        for (; i < nMid; i++)
            Unserialize(is, v[i], nType, nVersion);
    }
}

template<typename Stream, typename T, typename A>
inline void Unserialize(Stream& is, std::vector<T, A>& v, int nType, int nVersion)
{
    Unserialize_impl(is, v, nType, nVersion, boost::is_fundamental<T>());
}

#endif
