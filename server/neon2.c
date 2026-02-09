#include "neon2.h"
#include <argon2.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>

#define ENCODED 256
#define HASHLEN 32
#define SALTLEN 16

char* hash(char plaintext[]) {
	uint8_t hash[HASHLEN];
	uint8_t salt[SALTLEN];
	char encoded[ENCODED];

	int rn = getrandom(salt, sizeof(uint8_t) * SALTLEN, GRND_NONBLOCK);
	if (rn == -1) {
		printf("Error when attempting to create salt.\n");
		return "";
	}

	uint8_t *passwd = (uint8_t *) strdup(plaintext);
	uint32_t passwdlen = strlen((char *) passwd);

	uint32_t iterations = 2;
	uint32_t hashmem = (1<<16);
	uint32_t parallelism = 1;

	int rc = argon2id_hash_encoded(iterations, hashmem, parallelism, passwd, passwdlen, salt, SALTLEN, HASHLEN, encoded, ENCODED);

	if (ARGON2_OK != rc) {
		printf("Error: %s\n", argon2_error_message(rc));
		return "";
	}

	return strdup(encoded);
}

int verify(const char* encodedHash, const char* passwd) {
	const uint32_t passwdlen = strlen(passwd);
	int rc = argon2id_verify(encodedHash, passwd, passwdlen);
	if (ARGON2_OK != rc) {
		printf("Verification failed: %s\n", argon2_error_message(rc));
		return 0;
	}
	return 1;
}

// int main(int argc, char* argv[]) {
// 	if (argc != 2 && argc != 3) {
// 		printf("Usage: neon string_to_hash_here\n");
// 		printf("       neon encoded_hash password_to_verify\n");
// 		exit(1);
// 	}

// 	if (argc == 2)
// 		printf("%s\n", hash(argv[1]));
// 	else {
// 		if (verify(argv[1], argv[2]))
// 			printf("Verification Successful.\n");
// 	}
	
// 	return 0;
// }

// gcc neon2.c -largon2 -Isrc -o neon2
