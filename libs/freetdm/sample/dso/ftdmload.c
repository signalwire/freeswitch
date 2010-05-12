#include <stdio.h>
#include <stdlib.h>
#include <freetdm.h>
#include <ftdm_dso.h>

#define ARRLEN(obj) (sizeof(obj)/sizeof(obj[0]))

struct dso_entry {
	char name[25];
	ftdm_dso_lib_t lib;
};

struct dso_entry loaded[10];

int load(char *name)
{
	char path[255];
	char *err;
	struct dso_entry *entry = NULL;
	int i;
	
	for (i = 0; i < ARRLEN(loaded); i++) {
		if (!loaded[i].lib) {
			entry = &loaded[i];
			break;
		}
	}

	if (!entry) {
		ftdm_log(FTDM_LOG_CRIT, "Cannot load more libraries\n");
		return -1;
	}

	ftdm_build_dso_path(name, path, sizeof(path));
	ftdm_log(FTDM_LOG_DEBUG, "Loading %s!\n", path);
	entry->lib = ftdm_dso_open(path, &err);
	if (!entry->lib) {
		ftdm_log(FTDM_LOG_CRIT, "Cannot load library '%s': %s\n", path, err);
		return -1;
	}
	strncpy(entry->name, name, sizeof(entry->name)-1);
	entry->name[sizeof(entry->name)-1] = 0;
	return 0;
}

int unload(char *name)
{
	int i;
	struct dso_entry *entry = NULL;
	ftdm_log(FTDM_LOG_DEBUG, "Unloading %s!\n", name);
	for (i = 0; i < ARRLEN(loaded); i++) {
		if (loaded[i].lib && !strcasecmp(loaded[i].name, name)) {
			entry = &loaded[i];
			break;
		}
	}

	if (!entry) {
		ftdm_log(FTDM_LOG_CRIT, "Library %s not found\n", name);
		return -1;
	}

	ftdm_dso_destroy(&entry->lib);
	entry->lib = NULL;
	return 0;
}

int main(int argc, char *argv[])
{
	char cmdline[255];
	char name[255];
	
	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);

	ftdm_cpu_monitor_disable();

	if (ftdm_global_init() != FTDM_SUCCESS) {
		fprintf(stderr, "Error loading FreeTDM\n");
		exit(-1);
	}

	memset(loaded, 0, sizeof(loaded));

	printf("CLI> ");
	while (fgets(cmdline, sizeof(cmdline), stdin)) {
		if (sscanf(cmdline, "load=%s\n", name) == 1) {
			load(name);
		} else if (sscanf(cmdline, "unload=%s\n", name) == 1) {
			unload(name);
		} else if (!strncasecmp(cmdline, "exit", sizeof("exit")-1)) {
			printf("Quitting ...\n");
			sleep(1);
			break;
		} else {
			fprintf(stderr, "load=<name> | unload=<name> | exit\n");
		}
		printf("\nCLI> ");
	}


	ftdm_global_destroy();

	printf("Done, press any key to die!\n");

	getchar();
	return 0;
}

