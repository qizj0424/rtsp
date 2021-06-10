#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <adk_usbcamera.h>


extern int Ucamera_Init(int FrmNum, int FrmMaxSize);

#define CUSTOM_GAOSIBEIER  0

/* use for the lib for special custom */
#define INDEX          CUSTOM_GAOSIBEIER


typedef struct custom_info_t {
	int cid;
	char sn[16];
	char name[32];
} custom_info;

custom_info user_infos[16] = {
	[CUSTOM_GAOSIBEIER] = {
		.cid = 10010,
		.sn = "0tfmtmi0q9ruh8wh",
		.name = "gaosibeier",
	},
};

int Ucamera_Init_Adk(int FrmNum, int FrmMaxSize, char *license_path)
{
	FILE *license_fd = NULL;
	char *line_str = NULL;
	char key[64] = {0};
	char value[64] = {0};
	int line_num = 0;
	custom_info license;
	if ((license_fd = fopen(license_path, "r+")) == NULL) {
		printf("open license file error!\n");
		return -1;
	}

	line_str = malloc(256 * sizeof(char));
	if(line_str == NULL){
		printf("malloc line_str is failed\n\n");
		return -1;
	}
	while (!feof(license_fd)) {
		if (fscanf(license_fd, "%[^\n]", line_str) < 0)
			break;
		fseek(license_fd , 1, SEEK_CUR);
		line_num++;

		if (sscanf(line_str, "%[^:]:%[^\n]", key, value) != 2) {
			printf("warning: skip read  %s\n", line_str);
			fseek(license_fd, 1, SEEK_CUR);
			continue;
		}

		char *ch = strchr(key, ' ');
		if (ch) *ch = 0;

		if (strcmp(key, "cid") == 0) {
			license.cid = atoi(value);
		} else if(strcmp(key, "sn") == 0){
			strncpy(license.sn, value, sizeof(license.sn));
		} else if(strcmp(key, "name") == 0){
			strncpy(license.name, value, sizeof(license.name));
		} else {
			printf("Invalid license information: %s\n", key);
		}
	}
	free(line_str);

	printf("The library right is only for %d(custom cid).\n", user_infos[INDEX].cid);

	if(strcmp(license.name, user_infos[INDEX].name) != 0) {
		printf("license name is not matched!\n");
		return -1;
	}
	if(strcmp(license.sn, user_infos[INDEX].sn) != 0) {
		printf("license sn is not matched!\n");
		return -1;
	}

	if(license.cid != user_infos[INDEX].cid) {
		printf("license cid is not matched!\n");
		return -1;
	}

	return Ucamera_Init(FrmNum, FrmMaxSize);
}
