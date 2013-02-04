/*
    This file is part of darktable,
    copyright (c) 2012 aldric renaudin.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "common/darktable.h"
#include "common/debug.h"
#include "control/control.h"
#include "control/conf.h"
#include "common/styles.h"
#include "develop/develop.h"
#include "develop/masks.h"
#include "develop/imageop.h"
#include "libs/lib.h"
#include "gui/gtk.h"
#include "gui/styles.h"
#include "dtgtk/button.h"

DT_MODULE(1)


typedef struct dt_lib_masks_t
{
  /* vbox with managed history items */
  GtkWidget *vbox;
}
dt_lib_masks_t;


const char* name()
{
  return _("masks manager");
}

uint32_t views()
{
  return DT_VIEW_DARKROOM;
}

uint32_t container()
{
  return DT_UI_CONTAINER_PANEL_LEFT_CENTER;
}

int position()
{
  return 10;
}

static void _lib_masks_recreate_list(dt_lib_module_t *self)
{
  //const int bs = 12;
  //dt_lib_masks_t *d = (dt_lib_masks_t *)self->data;

  /* first destroy all buttons in list */
  gtk_container_foreach(GTK_CONTAINER(self->widget),(GtkCallback)gtk_widget_destroy,0);
  
  //dt_iop_module_t *iop = darktable.develop->gui_module;
  
  GtkTreeStore *treestore;
  GtkTreeIter toplevel, child;
  treestore = gtk_tree_store_new(1, G_TYPE_STRING);
  
  //first, we display the "all shapes" entry
  gtk_tree_store_append(treestore, &toplevel, NULL);
  gtk_tree_store_set(treestore, &toplevel,0, _("all created shapes"),-1);

  GList *forms = g_list_first(darktable.develop->forms);
  while (forms)
  {
    dt_masks_form_t *form = (dt_masks_form_t *)forms->data;
    gtk_tree_store_append(treestore, &child, &toplevel);
    gtk_tree_store_set(treestore, &child, 0, form->name, -1);
    forms = g_list_next(forms);
  }
  
  //now we display shapes iop by iop
  GList *iops = g_list_first(darktable.develop->iop);
  while(iops)
  {
    dt_iop_module_t *module = (dt_iop_module_t *)iops->data;
    if (module->flags() & IOP_FLAGS_SUPPORTS_BLENDING)
    {
      //we create the entry
      gtk_tree_store_append(treestore, &toplevel, NULL);
      gtk_tree_store_set(treestore, &toplevel,0, module->name(),-1);
      //ad we populate it
      for (int i=0; i<module->blend_params->forms_count; i++)
      {
        dt_masks_form_t *form = dt_masks_get_from_id(module->dev,module->blend_params->forms[i]);
        if (!form) continue;
        gtk_tree_store_append(treestore, &child, &toplevel);
        gtk_tree_store_set(treestore, &child, 0, form->name, -1);
      }
    }
    iops = g_list_next(iops);
  }
  
  GtkWidget *view = gtk_tree_view_new();
  GtkTreeViewColumn *col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(col, "shapes");
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_add_attribute(col, renderer, "text", 0);

  gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(treestore));
  g_object_unref(treestore);
  
  //GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
  gtk_widget_set_size_request(view, -1, 300);
  gtk_container_add(GTK_CONTAINER(sw), view);
  
  gtk_box_pack_start(GTK_BOX(self->widget), sw, TRUE, TRUE, 1);

  //g_signal_connect(selection, "changed", G_CALLBACK(on_changed), statusbar);
  
  /* show all widgets */
  gtk_widget_show_all(sw);
}
static void _lib_history_change_callback(gpointer instance, gpointer user_data)
{
  dt_lib_module_t *self = (dt_lib_module_t *)user_data;
  _lib_masks_recreate_list(self);
}

void gui_init(dt_lib_module_t *self)
{
  /* initialize ui widgets */
  dt_lib_masks_t *d = (dt_lib_masks_t *)g_malloc(sizeof(dt_lib_masks_t));
  self->data = (void *)d;
  memset(d,0,sizeof(dt_lib_masks_t));

  //dt_iop_module_t *iop = darktable.develop->gui_module;

  self->widget =  gtk_vbox_new (FALSE,2);
  
  //d->vbox = gtk_vbox_new(FALSE,0);
  //gtk_box_pack_start (GTK_BOX (self->widget),d->vbox,FALSE,FALSE,0);

  gtk_widget_show_all (self->widget);

  /* connect to history change signal for updating the history view */
  dt_control_signal_connect(darktable.signals, DT_SIGNAL_DEVELOP_HISTORY_CHANGE, G_CALLBACK(_lib_history_change_callback), self);

  // set proxy functions
  darktable.develop->proxy.masks.module = self;
  darktable.develop->proxy.masks.switch_module = _lib_masks_recreate_list;
}

void gui_cleanup(dt_lib_module_t *self)
{
  dt_control_signal_disconnect(darktable.signals, G_CALLBACK(_lib_history_change_callback), self);

  g_free(self->data);
  self->data = NULL;
}

// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.sh
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-space on;
