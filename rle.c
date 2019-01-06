#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define RLE_ESCAPE '\a'

void encode(FILE *fp) {
	uint8_t last = 0;
	uint8_t count = 0;

	while (!feof(fp)) {
		uint8_t c = fgetc(fp);

		if (feof(fp)){
			if (count > 0) {
				putchar(RLE_ESCAPE);
				putchar(count);
				putchar(last);
			}

			break;
		}

		if (count == 0) {
			count = 1;
			last  = c;
			continue;
		}

		if (c != last || count == 0xff) {
			if (count < 3 && last != RLE_ESCAPE) {
				for (unsigned k = 0; k < count; k++){
					putchar(last);
				}

			} else {
				putchar(RLE_ESCAPE);
				putchar(count);
				putchar(last);
			}

			count = 1;
			last  = c;

		} else {
			count++;
		}
	}
}

void decode(FILE *fp) {
	while (!feof(fp)) {
		uint8_t c = fgetc(fp);

		if (feof(fp))
			break;

		if (c == RLE_ESCAPE) {
			uint8_t count = fgetc(fp);
			uint8_t chr   = fgetc(fp);

			for (unsigned k = 0; k < count; k++) {
				putchar(chr);
			}

		} else {
			putchar(c);
		}
	}
}

int main(int argc, char *argv[]) {
	FILE *fp = stdin;

	if (argc < 2) {
		// TODO
		return 0;
	}

	if (strcmp(argv[1], "-e") == 0) {
		encode(fp);

	} else if (strcmp(argv[1], "-d") == 0) {
		decode(fp);

	} else {
		// TODO
	}

	return 0;
}
