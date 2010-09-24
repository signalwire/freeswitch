#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <freetdm.h>
#include <ftdm_dso.h>
#include <dlfcn.h>
#include <execinfo.h>

#define ARRLEN(obj) (sizeof(obj)/sizeof(obj[0]))

struct dso_entry {
	char name[25];
	ftdm_dso_lib_t lib;
};

struct dso_entry loaded[10];

static void *(*real_dlopen)(const char *filename, int flag) = NULL;
static int (*real_dlclose)(void *handle) = NULL;

static void print_stack()
{
	void *stacktrace[100];
	char **symbols;
	int size;
	int i;
	size = backtrace(stacktrace, ARRLEN(stacktrace));
	symbols = backtrace_symbols(stacktrace, size);
	if (!symbols) {
		return;
	}
	for (i = 0; i < size; i++) {
		ftdm_log(FTDM_LOG_DEBUG, "%s\n", symbols[i]);
	}
	free(symbols);
}

void *dlopen(const char *filename, int flag)
{
	char *msg = NULL;
	void *handle = NULL;
	print_stack();
	if (real_dlopen == NULL) {
		dlerror();
		real_dlopen = dlsym(RTLD_NEXT, "dlopen");
		if ((msg = dlerror()) != NULL) {
			fprintf(stderr, "dlsym failed: %s\n", msg);
			exit(1);
		}
		fprintf(stderr, "Real dlopen at addr %p\n", real_dlopen);
	}
	handle = real_dlopen(filename, flag);
	if (!handle) {
		return NULL;
	}
	ftdm_log(FTDM_LOG_NOTICE, "Loaded %s with handle %p\n", filename, handle);
	return handle;
}

int dlclose(void *handle)
{
	char *msg = NULL;
	print_stack();
	if (real_dlclose == NULL) {
		dlerror();
		real_dlclose = dlsym(RTLD_NEXT, "dlclose");
		if ((msg = dlerror()) != NULL) {
			fprintf(stderr, "dlsym failed: %s\n", msg);
			exit(1);
		}
		fprintf(stderr, "Real dlclose at addr %p\n", real_dlclose);
	}
	ftdm_log(FTDM_LOG_NOTICE, "Unloading %p\n", handle);
	return real_dlclose(handle);
}

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

