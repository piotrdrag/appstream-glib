/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>
#include <fnmatch.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <asb-plugin.h>

#define __APPSTREAM_GLIB_PRIVATE_H
#include <as-utils-private.h>

/**
 * asb_plugin_get_name:
 */
const gchar *
asb_plugin_get_name (void)
{
	return "desktop";
}

/**
 * asb_plugin_add_globs:
 */
void
asb_plugin_add_globs (AsbPlugin *plugin, GPtrArray *globs)
{
	asb_plugin_add_glob (globs, "/usr/share/applications/*.desktop");
	asb_plugin_add_glob (globs, "/usr/share/applications/kde4/*.desktop");
	asb_plugin_add_glob (globs, "/usr/share/pixmaps/*");
	asb_plugin_add_glob (globs, "/usr/share/icons/*");
	asb_plugin_add_glob (globs, "/usr/share/*/icons/*");
}

/**
 * _asb_plugin_check_filename:
 */
static gboolean
_asb_plugin_check_filename (const gchar *filename)
{
	if (fnmatch ("/usr/share/applications/*.desktop", filename, 0) == 0)
		return TRUE;
	if (fnmatch ("/usr/share/applications/kde4/*.desktop", filename, 0) == 0)
		return TRUE;
	return FALSE;
}

/**
 * asb_plugin_check_filename:
 */
gboolean
asb_plugin_check_filename (AsbPlugin *plugin, const gchar *filename)
{
	return _asb_plugin_check_filename (filename);
}

/**
 * asb_app_load_icon:
 */
static GdkPixbuf *
asb_app_load_icon (AsbApp *app,
		   const gchar *filename,
		   const gchar *logfn,
		   GError **error)
{
	GdkPixbuf *pixbuf = NULL;
	guint pixbuf_height;
	guint pixbuf_width;
	guint tmp_height;
	guint tmp_width;
	_cleanup_object_unref_ GdkPixbuf *pixbuf_src = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf_tmp = NULL;

	/* open file in native size */
	if (g_str_has_suffix (filename, ".svg")) {
		pixbuf_src = gdk_pixbuf_new_from_file_at_scale (filename,
								64, 64, TRUE,
								error);
	} else {
		pixbuf_src = gdk_pixbuf_new_from_file (filename, error);
	}
	if (pixbuf_src == NULL)
		return NULL;

	/* check size */
	if (gdk_pixbuf_get_width (pixbuf_src) < 32 ||
	    gdk_pixbuf_get_height (pixbuf_src) < 32) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "icon %s was too small %ix%i",
			     logfn,
			     gdk_pixbuf_get_width (pixbuf_src),
			     gdk_pixbuf_get_height (pixbuf_src));
		return NULL;
	}

	/* does the icon not have an alpha channel */
	if (!gdk_pixbuf_get_has_alpha (pixbuf_src)) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_INFO,
				 "icon %s does not have an alpha channel",
				 logfn);
	}

	/* don't do anything to an icon with the perfect size */
	pixbuf_width = gdk_pixbuf_get_width (pixbuf_src);
	pixbuf_height = gdk_pixbuf_get_height (pixbuf_src);
	if (pixbuf_width == 64 && pixbuf_height == 64)
		return g_object_ref (pixbuf_src);

	/* never scale up, just pad */
	if (pixbuf_width < 64 && pixbuf_height < 64) {
		asb_package_log (asb_app_get_package (app),
				 ASB_PACKAGE_LOG_LEVEL_INFO,
				 "icon %s padded to 64x64 as size %ix%i",
				 logfn, pixbuf_width, pixbuf_height);
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 64, 64);
		gdk_pixbuf_fill (pixbuf, 0x00000000);
		gdk_pixbuf_copy_area (pixbuf_src,
				      0, 0, /* of src */
				      pixbuf_width, pixbuf_height,
				      pixbuf,
				      (64 - pixbuf_width) / 2,
				      (64 - pixbuf_height) / 2);
		return pixbuf;
	}

	/* is the aspect ratio perfectly square */
	if (pixbuf_width == pixbuf_height) {
		pixbuf = gdk_pixbuf_scale_simple (pixbuf_src, 64, 64,
						  GDK_INTERP_HYPER);
		as_pixbuf_sharpen (pixbuf, 1, -0.5);
		return pixbuf;
	}

	/* create new square pixbuf with alpha padding */
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 64, 64);
	gdk_pixbuf_fill (pixbuf, 0x00000000);
	if (pixbuf_width > pixbuf_height) {
		tmp_width = 64;
		tmp_height = 64 * pixbuf_height / pixbuf_width;
	} else {
		tmp_width = 64 * pixbuf_width / pixbuf_height;
		tmp_height = 64;
	}
	pixbuf_tmp = gdk_pixbuf_scale_simple (pixbuf_src, tmp_width, tmp_height,
					      GDK_INTERP_HYPER);
	as_pixbuf_sharpen (pixbuf_tmp, 1, -0.5);
	gdk_pixbuf_copy_area (pixbuf_tmp,
			      0, 0, /* of src */
			      tmp_width, tmp_height,
			      pixbuf,
			      (64 - tmp_width) / 2,
			      (64 - tmp_height) / 2);
	return pixbuf;
}

/**
 * asb_app_find_icon:
 */
static GdkPixbuf *
asb_app_find_icon (AsbApp *app,
		   const gchar *tmpdir,
		   const gchar *something,
		   GError **error)
{
	guint i;
	guint j;
	guint k;
	guint m;
	const gchar *pixmap_dirs[] = { "pixmaps", "icons", NULL };
	const gchar *theme_dirs[] = { "hicolor", "oxygen", NULL };
	const gchar *supported_ext[] = { ".png",
					 ".gif",
					 ".svg",
					 ".xpm",
					 "",
					 NULL };
	const gchar *sizes[] = { "64x64",
				 "128x128",
				 "96x96",
				 "256x256",
				 "scalable",
				 "48x48",
				 "32x32",
				 "24x24",
				 "16x16",
				 NULL };
	const gchar *types[] = { "actions",
				 "animations",
				 "apps",
				 "categories",
				 "devices",
				 "emblems",
				 "emotes",
				 "filesystems",
				 "intl",
				 "mimetypes",
				 "places",
				 "status",
				 "stock",
				 NULL };

	/* is this an absolute path */
	if (something[0] == '/') {
		_cleanup_free_ gchar *tmp;
		tmp = g_build_filename (tmpdir, something, NULL);
		if (!g_file_test (tmp, G_FILE_TEST_EXISTS)) {
			g_set_error (error,
				     ASB_PLUGIN_ERROR,
				     ASB_PLUGIN_ERROR_FAILED,
				     "specified icon '%s' does not exist",
				     something);
			return NULL;
		}
		return asb_app_load_icon (app, tmp, something, error);
	}

	/* icon theme apps */
	for (k = 0; theme_dirs[k] != NULL; k++) {
		for (i = 0; sizes[i] != NULL; i++) {
			for (m = 0; types[m] != NULL; m++) {
				for (j = 0; supported_ext[j] != NULL; j++) {
					_cleanup_free_ gchar *log;
					_cleanup_free_ gchar *tmp;
					log = g_strdup_printf ("/usr/share/icons/"
							       "%s/%s/%s/%s%s",
							       theme_dirs[k],
							       sizes[i],
							       types[m],
							       something,
							       supported_ext[j]);
					tmp = g_build_filename (tmpdir, log, NULL);
					if (g_file_test (tmp, G_FILE_TEST_EXISTS))
						return asb_app_load_icon (app, tmp, log, error);
				}
			}
		}
	}

	/* pixmap */
	for (i = 0; pixmap_dirs[i] != NULL; i++) {
		for (j = 0; supported_ext[j] != NULL; j++) {
			_cleanup_free_ gchar *log;
			_cleanup_free_ gchar *tmp;
			log = g_strdup_printf ("/usr/share/%s/%s%s",
					       pixmap_dirs[i],
					       something,
					       supported_ext[j]);
			tmp = g_build_filename (tmpdir, log, NULL);
			if (g_file_test (tmp, G_FILE_TEST_EXISTS))
				return asb_app_load_icon (app, tmp, log, error);
		}
	}

	/* failed */
	g_set_error (error,
		     ASB_PLUGIN_ERROR,
		     ASB_PLUGIN_ERROR_FAILED,
		     "Failed to find icon %s", something);
	return NULL;
}

/**
 * asb_plugin_process_filename:
 */
static gboolean
asb_plugin_process_filename (AsbPlugin *plugin,
			     AsbPackage *pkg,
			     const gchar *filename,
			     GList **apps,
			     const gchar *tmpdir,
			     GError **error)
{
	const gchar *key;
	gboolean ret;
	_cleanup_free_ gchar *app_id = NULL;
	_cleanup_free_ gchar *full_filename = NULL;
	_cleanup_free_ gchar *icon_filename = NULL;
	_cleanup_object_unref_ AsbApp *app = NULL;
	_cleanup_object_unref_ GdkPixbuf *pixbuf = NULL;

	/* create app */
	app_id = g_path_get_basename (filename);
	app = asb_app_new (pkg, app_id);
	full_filename = g_build_filename (tmpdir, filename, NULL);
	ret = as_app_parse_file (AS_APP (app),
				 full_filename,
				 AS_APP_PARSE_FLAG_USE_HEURISTICS,
				 error);
	if (!ret)
		return FALSE;

	/* NoDisplay apps are never included */
	if (as_app_get_metadata_item (AS_APP (app), "NoDisplay") != NULL)
		asb_app_add_veto (app, "NoDisplay=true");

	/* Settings or DesktopSettings requires AppData */
	if (as_app_has_category (AS_APP (app), "Settings"))
		asb_app_add_requires_appdata (app, "Category=Settings");
	if (as_app_has_category (AS_APP (app), "DesktopSettings"))
		asb_app_add_requires_appdata (app, "Category=DesktopSettings");

	/* is the icon a stock-icon-name? */
	key = as_app_get_icon (AS_APP (app));
	if (key != NULL) {
		if (as_app_get_icon_kind (AS_APP (app)) == AS_ICON_KIND_STOCK) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_DEBUG,
					 "using stock icon %s", key);
		} else {
			_cleanup_error_free_ GError *error_local = NULL;

			/* is icon XPM or GIF */
			if (g_str_has_suffix (key, ".xpm"))
				asb_app_add_veto (app, "Uses XPM icon: %s", key);
			else if (g_str_has_suffix (key, ".gif"))
				asb_app_add_veto (app, "Uses GIF icon: %s", key);
			else if (g_str_has_suffix (key, ".ico"))
				asb_app_add_veto (app, "Uses ICO icon: %s", key);

			/* find icon */
			pixbuf = asb_app_find_icon (app, tmpdir, key, &error_local);
			if (pixbuf == NULL) {
				asb_app_add_veto (app, "Failed to find icon: %s",
						  error_local->message);
			} else {
				/* save in target directory */
				icon_filename = g_strdup_printf ("%s.png",
								 as_app_get_id (AS_APP (app)));
				as_app_set_icon (AS_APP (app), icon_filename, -1);
				as_app_set_icon_kind (AS_APP (app), AS_ICON_KIND_CACHED);
				asb_app_set_pixbuf (app, pixbuf);
			}
		}
	}

	/* add */
	asb_plugin_add_app (apps, AS_APP (app));
	return TRUE;
}

/**
 * asb_plugin_process:
 */
GList *
asb_plugin_process (AsbPlugin *plugin,
		    AsbPackage *pkg,
		    const gchar *tmpdir,
		    GError **error)
{
	gboolean ret;
	GError *error_local = NULL;
	GList *apps = NULL;
	guint i;
	gchar **filelist;

	filelist = asb_package_get_filelist (pkg);
	for (i = 0; filelist[i] != NULL; i++) {
		if (!_asb_plugin_check_filename (filelist[i]))
			continue;
		ret = asb_plugin_process_filename (plugin,
						   pkg,
						   filelist[i],
						   &apps,
						   tmpdir,
						   &error_local);
		if (!ret) {
			asb_package_log (pkg,
					 ASB_PACKAGE_LOG_LEVEL_INFO,
					 "Failed to process %s: %s",
					 filelist[i],
					 error_local->message);
			g_clear_error (&error_local);
		}
	}

	/* no desktop files we care about */
	if (apps == NULL) {
		g_set_error (error,
			     ASB_PLUGIN_ERROR,
			     ASB_PLUGIN_ERROR_FAILED,
			     "nothing interesting in %s",
			     asb_package_get_basename (pkg));
		return NULL;
	}
	return apps;
}
