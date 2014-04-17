#include "HashFunctions.h"
#include "CompilerPortability.h"
#include "Rand.h"
#include "Utilities.h"
#include <climits>

namespace utils {

const int SizeOfLongLong = sizeof(int64_t) * CHAR_BIT;
const int SizeOfUnsigned = sizeof(unsigned int) * CHAR_BIT;

int64_t longrotr(int64_t x, int places) {
    return (x >> places) | (x << (SizeOfLongLong - places));
}

int64_t longrotl(int64_t x, int places) {
    return (x << places) | (x >> (SizeOfLongLong - places));
}

unsigned int rotr(unsigned int x, int places) {
    return (x >> places) | (x << (SizeOfUnsigned - places));
}

unsigned int rotl(unsigned int x, int places) {
    return (x << places) | (x >> (SizeOfUnsigned - places));
}

int inthash(const int key) {
    return sizeof(int) == 4 ? int32hash(key) : int64hash(key);
}

int64_t int64hash(const int64_t key) {
    static THREADLOCAL int64_t BaseHash = 0;
    static THREADLOCAL bool BaseHashInitialized = false;
    
    if (!BaseHashInitialized) {
        int64_t x = 0;
        while (x == 0) { 
            x = rnd::i64rand(); 
        }
        BaseHash = x;
        BaseHashInitialized = true;
    }   

    return TWLongHash(key + BaseHash);
}

int32_t int32hash(const int32_t key) {
    static THREADLOCAL int32_t BaseHash = 0;
    static THREADLOCAL bool BaseHashInitialized = false;
    
    if (!BaseHashInitialized) {
        int32_t x = 0;
        while (x == 0) { 
            x = rnd::i32rand(); 
        }
        BaseHash = x;
        BaseHashInitialized = true;
    }   

    return TWHash(key + BaseHash);
}

int64_t stringhash(const std::string& str) {
    return PJWDBHash(str);
}

// RS Hash Function 
unsigned int RSHash(const std::string& str) {
    unsigned int b    = 378551;
    unsigned int a    = 63689;
    unsigned int hash = 0;
    for(unsigned int i = 0; i < str.length(); i++) {
        hash = hash * a + str[i];
        a    = a * b;
    }
    return hash;
}

// JS Hash Function 
unsigned int JSHash(const std::string& str) {
    unsigned int hash = 1315423911;
    for (std::string::const_iterator i = str.begin(); i != str.end(); 
        i++) {
        hash ^= ((hash << 5) + (unsigned)*i + (hash >> 2));
    }
    return hash;
}

// P. J. Weinberger Hash Function 
unsigned int PJWHash(const std::string& str) {
    unsigned int BitsInUnsignedInt = 
        (unsigned int)(sizeof(unsigned int) * CHAR_BIT);
    unsigned int ThreeQuarters     = 
        (unsigned int)((BitsInUnsignedInt  * 3) / 4);
    unsigned int OneEighth         = 
        (unsigned int)(BitsInUnsignedInt / CHAR_BIT);
    unsigned int HighBits          = 
        (unsigned int)(0xFFFFFFFF) << (BitsInUnsignedInt - OneEighth);
    unsigned int hash              = 0;
    unsigned int test              = 0;

    for (std::string::const_iterator i = str.begin(); i != str.end(); 
        i++) {
        hash = (hash << OneEighth) + (unsigned)*i;
        test = hash & HighBits;
        if(test != 0) {
            hash = (( hash ^ (test >> ThreeQuarters)) & (~HighBits));
        }
    }

    return hash;
}

// ELF Hash Function 
unsigned int ELFHash(const std::string& str) {
    unsigned int hash = 0;
    unsigned int x    = 0;
    for (std::string::const_iterator i = str.begin(); i != str.end(); 
        i++) {
        hash = (hash << 4) + (unsigned)*i;
        x = hash & 0xF0000000L;
        if(x != 0) {
            hash ^= (x >> 24);
            hash &= ~x;
        }
    }
    return hash;
}

// BKDR Hash Function
unsigned int BKDRHash(const std::string& str) {
    unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;

    for (std::string::const_iterator i = str.begin(); i != str.end(); 
        i++) {
        hash = (hash * seed) + (unsigned)*i;
    }
    return hash;
}

// SDBM Hash Function
unsigned int SDBMHash(const std::string& str) {
    unsigned int hash = 0;
    for (std::string::const_iterator i = str.begin(); i != str.end(); 
        i++) {
        hash = (unsigned)*i + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

// DJB Hash Function
unsigned int DJBHash(const std::string& str) {
    unsigned int hash = 5381;
    for (std::string::const_iterator i = str.begin(); i != str.end(); 
        i++) {
        hash = ((hash << 5) + hash) + (unsigned)*i;
    }
    return hash;
}

// DEK Hash Function
unsigned int DEKHash(const std::string& str) {
    unsigned int hash = static_cast<unsigned int>(str.length());
    for (std::string::const_iterator i = str.begin(); i != str.end(); 
        i++) {
        hash = ((hash << 5) ^ (hash >> 27)) ^ (unsigned)*i;
    }
    return hash;
}


// AP Hash Function
unsigned int APHash(const std::string& str) {
    unsigned int hash = 0;
    for(unsigned int i = 0; i < str.length(); i++) {
        hash ^= ((i & 1) == 0) ? 
            (  (hash <<  7) ^ str[i] ^ (hash >> 3)) :
            (~((hash << 11) ^ str[i] ^ (hash >> 5)));
    }
    return hash;
}

unsigned int DBHash(const std::string &str) {
    unsigned int hash = 0;
    for(unsigned int i = 0; i < str.length(); i++) {
        // Cut in blocks of 4 characters, sum the blocks, and shift the
        // sum.
        if (i % 4 == 0) {
            hash <<= 1;
        }
        hash += (unsigned)str[i];
    }
    return hash;
}

// Merge of PJW and DB hash
int64_t PJWDBHash(const std::string& str) {
    const unsigned BitsInUnsignedInt = 
        (unsigned int)(sizeof(unsigned int) * CHAR_BIT);
    const unsigned ThreeQuarters     = 
        (unsigned int)((BitsInUnsignedInt  * 3) / 4);
    const unsigned OneEighth         = 
        (unsigned int)(BitsInUnsignedInt / CHAR_BIT);
    const unsigned HighBits          = 
        (unsigned int)(0xFFFFFFFF) << (BitsInUnsignedInt - OneEighth);

    int64_t hash = 0;
    int64_t test = 0;
    int64_t sum  = 0;

    for (unsigned i = 0; i < str.length(); i++) {
        const int64_t C = (int64_t)str[i];
        hash = (hash << OneEighth) + C;
        if((test = hash & HighBits)  != 0) {
            hash = (( hash ^ (test >> ThreeQuarters)) & (~HighBits));
        }
        if (i % 4 == 0) {
            sum <<= 1;
        }
        sum += C;
    }

    return sum * hash;
}

// Thomas Wang's 64 bit Mix Function
int64_t TWLongHash (const int64_t key) {
    int64_t hash = key;
    hash += ~(hash << 32);
    hash ^= longrotr(hash, 22);
    hash += ~(hash << 13);
    hash ^= longrotr(hash, 8);
    hash += (hash << 3);
    hash ^= longrotr(hash, 15);
    hash += ~(hash << 27);
    hash ^= longrotr(hash, 31);
    return hash;
}

// Multiplication hashing
int64_t MULongHash (const int64_t key) {
    int64_t c1 = 0x6e5ea73858134343LL;
    int64_t c2 = 0xb34e8f99a2ec9ef5LL;
    int64_t hash = key;
    hash ^= longrotr((c1 ^ hash),32);
    hash *= c1;
    hash ^= longrotr((c2 ^ hash),31);
    hash *= c2;
    hash ^= longrotr((c1 ^ hash),32);
    return hash;
}

// Robert Jenkins' 32 bit Mix Function
unsigned int RJHash (const unsigned int key) {
    unsigned int hash = key;
    hash += (hash << 12);
    hash ^= (hash >> 22);
    hash += (hash << 4);
    hash ^= (hash >> 9);
    hash += (hash << 10);
    hash ^= (hash >> 2);
    hash += (hash << 7);
    hash ^= (hash >> 12);
    return hash;
}

// Thomas Wang's 32 bit Mix Function
unsigned int TWHash (const unsigned int key) {
    unsigned int hash = key;
    hash += ~(hash << 15);
    hash ^=  rotr(hash,10);
    hash +=  (hash << 3);
    hash ^=  rotr(hash,6);
    hash += ~(hash << 11);
    hash ^=  rotr(hash,16);
    return hash;
}

} // End of utils namespace
