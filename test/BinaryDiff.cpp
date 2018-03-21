#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int binnary_diff(const char* file1, const char* file2)
{
	FILE* fp1 = fopen(file1, "rb");
	FILE* fp2 = fopen(file2, "rb");

	char packet1[4 * 1024];
	char packet2[4 * 1024];
	for(int i = 0; 1; i++)
	{
		int r1 = fread(packet1, 1, sizeof(packet1), fp1);
		int r2 = fread(packet2, 1, sizeof(packet2), fp2);
		int r = r1 < r2 ? r1 : r2;
		if (r < 1)
			break; // eof

		if (0 != memcmp(packet1, packet2, r))
		{
			for (int j = 0; j < r; j++)
			{
				if (packet1[j] != packet2[j])
					break;
			}
		}

		if(r1 != r2)
			break;
	}

	fclose(fp1);
	fclose(fp2);
	return 0;
}
