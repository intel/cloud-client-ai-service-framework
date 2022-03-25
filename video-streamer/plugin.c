/* Copyright (C) 2021 Intel Corporation */

#include "ccai_stream_plugin.h"
#include "ccai_stream_utils.h"
#include "cs.h"

#include <dlfcn.h>
#include <glib.h>


static const char* PLUGINDIRS[] = {
	"plugins/",
	"/usr/lib/ccai_stream/plugins/",
};

struct ccai_stream_plugin {
	void *handle;
	struct ccai_stream_plugin_desc *desc;
};

static GSList *plugins = NULL;

static int priority_compare(gconstpointer a, gconstpointer b)
{
	const struct ccai_stream_plugin *p1 = a;
	const struct ccai_stream_plugin *p2 = b;

	return p2->desc->priority - p1->desc->priority;
}

static int add_plugin(void *handle, struct ccai_stream_plugin_desc *d)
{
	struct ccai_stream_plugin *plugin;

	if (d->init == NULL)
		return -1;

	if (g_str_equal(d->version, CCAI_STREAM_PLUGIN_VERSION) == FALSE) {
		CS_E("Version mismatch for %s\n", d->name);
		return -1;
	}

	CS_D("Loading %s plugin\n", d->name);

	// check plugin if already loaded.
	GSList* p = plugins;
	while (p) {
		struct ccai_stream_plugin * cs_p =
			(struct ccai_stream_plugin *)p->data;
		if (strncmp(cs_p->desc->name, d->name, PATH_MAX) == 0) {
			CS_E("plugin %s already loaded.\n", d->name);
			return -1;
		}
		p = p->next;
	}

	if ((plugin = g_try_new0(struct ccai_stream_plugin, 1)) == NULL)
		return -1;

	plugin->handle = handle;
	plugin->desc = d;

	plugins = g_slist_insert_sorted(plugins, plugin, priority_compare);

	return 0;
}

int plugin_init(const char *enable, const char *disable)
{
	GDir *dir;
	const char *file;
	char *filename;
	void *handle;
	struct ccai_stream_plugin_desc *desc;
	struct ccai_stream_plugin *plugin;
	GSList *list;

	for (int i = 0; i < sizeof(PLUGINDIRS)/sizeof(char*); i++) {
		CS_D("plugin dir=%s\n", PLUGINDIRS[i]);
		dir = g_dir_open(PLUGINDIRS[i], 0, NULL);
		if (!dir)
			continue;
		while ((file = g_dir_read_name(dir)) != NULL) {
			if (g_str_has_prefix(file, "lib") == TRUE ||
			    g_str_has_suffix(file, ".so") == FALSE)
				continue;

			filename = g_build_filename(PLUGINDIRS[i], file, NULL);

			CS_D("filename=%s\n", filename);
			if ((handle = dlopen(filename, RTLD_NOW)) == NULL) {
				CS_E("can't load plugin %s: %s\n", filename,
				     dlerror());
				g_free(filename);
				continue;
			}

			g_free(filename);

			if ((desc = dlsym(handle, "ccai_stream_plugin_desc"))
			     == NULL) {
				CS_E("can't load plugin desc: %s\n", dlerror());
				dlclose(handle);
				continue;
			}

			if (add_plugin(handle, desc) != 0)
				dlclose(handle);
		}

		g_dir_close(dir);
	}

	for (list = plugins; list; list = list->next) {
		plugin = list->data;
		if (plugin->desc->init() != 0) {
			CS_E("failed to init %s plugin\n", plugin->desc->name);
			continue;
		}
	}

	return 0;
}

