// --------------------------------------------------------------------
// Various basic hash functions for strings and integers
// --------------------------------------------------------------------

#ifndef UTILS_HASH_FUNCTIONS
#define UTILS_HASH_FUNCTIONS

#include <tr1/cstdint>
#include <string>

namespace utils {

// --------------------------------------------------------------------
// Domagoj's hash functions
// --------------------------------------------------------------------
int inthash(const int key);
int64_t int64hash(const int64_t key);
int32_t int32hash(const int32_t key);
int64_t stringhash(const std::string& str);

// --------------------------------------------------------------------
// String hashing
// --------------------------------------------------------------------
unsigned int RSHash  (const std::string& str);
unsigned int JSHash  (const std::string& str);
// Method recommended in Aho, Sethi, Ullman book
unsigned int PJWHash (const std::string& str);
unsigned int ELFHash (const std::string& str);
unsigned int BKDRHash(const std::string& str);
unsigned int SDBMHash(const std::string& str);
unsigned int DJBHash (const std::string& str);
unsigned int DEKHash (const std::string& str);
unsigned int APHash  (const std::string& str);
// Domagoj's simple hash - weighted sum of characters.
unsigned int DBHash  (const std::string& str);
// Merge of PJW & DB hash, more efficient than using the two separately
int64_t PJWDBHash    (const std::string& str);

// --------------------------------------------------------------------
// Integer hashing
// --------------------------------------------------------------------
int64_t         TWLongHash  (const int64_t key);
int64_t         MULongHash  (const int64_t key);
unsigned int RJHash      (const unsigned int key);
unsigned int TWHash      (const unsigned int key);

} // End of utils namespace

#endif // UTILS_HASH_FUNCTIONS
