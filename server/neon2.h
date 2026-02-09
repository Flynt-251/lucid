
#include <argon2.h>
#include <sys/random.h>

char* hash(char plaintext[]);

int verify(const char* encodedHash, const char* passwd);