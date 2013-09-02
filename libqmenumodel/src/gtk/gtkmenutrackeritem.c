/*
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkmenutrackeritem.h"

/**
 * SECTION:gtkmenutrackeritem
 * @Title: GtkMenuTrackerItem
 * @Short_description: Small helper for model menu items
 *
 * A #GtkMenuTrackerItem is a small helper class used by #GtkMenuTracker to
 * represent menu items. It has one of three classes: normal item, separator,
 * or submenu.
 *
 * If an item is one of the non-normal classes (submenu, separator), only the
 * label of the item needs to be respected. Otherwise, all the properties
 * of the item contribute to the item's appearance and state.
 *
 * Implementing the appearance of the menu item is up to toolkits, and certain
 * toolkits may choose to ignore certain properties, like icon or accel. The
 * role of the item determines its accessibility role, along with its
 * decoration if the GtkMenuTrackerItem::toggled property is true. As an
 * example, if the item has the role %GTK_MENU_TRACKER_ITEM_ROLE_CHECK and
 * GtkMenuTrackerItem::toggled is %FALSE, its accessible role should be that of
 * a check menu item, and no decoration should be drawn. But if
 * GtkMenuTrackerItem::toggled is %TRUE, a checkmark should be drawn.
 *
 * All properties except for the two class-determining properties,
 * GtkMenuTrackerItem::is-separator and GtkMenuTrackerItem::has-submenu are
 * allowed to change, so listen to the notify signals to update your item's
 * appearance. When using a GObject library, this can conveniently be done
 * with g_object_bind_property() and #GBinding, and this is how this is
 * implemented in GTK+; the appearance side is implemented in #GtkModelMenuItem.
 *
 * When an item is clicked, simply call gtk_menu_tracker_item_activated() in
 * response. The #GtkMenuTrackerItem will take care of everything related to
 * activating the item and will itself update the state of all items in
 * response.
 *
 * Submenus are a special case of menu item. When an item is a submenu, you
 * should create a submenu for it with gtk_menu_tracker_new_item_for_submenu(),
 * and apply the same menu tracking logic you would for a toplevel menu.
 * Applications using submenus may want to lazily build their submenus in
 * response to the user clicking on it, as building a submenu may be expensive.
 *
 * Thus, the submenu has two special controls -- the submenu's visibility
 * should be controlled by the GtkMenuTrackerItem::submenu-shown property,
 * and if a user clicks on the submenu, do not immediately show the menu,
 * but call gtk_menu_tracker_item_request_submenu_shown() and wait for the
 * GtkMenuTrackerItem::submenu-shown property to update. If the user navigates,
 * the application may want to be notified so it can cancel the expensive
 * operation that it was using to build the submenu. Thus,
 * gtk_menu_tracker_item_request_submenu_shown() takes a boolean parameter.
 * Use %TRUE when the user wants to open the submenu, and %FALSE when the
 * user wants to close the submenu.
 */

typedef GObjectClass GtkMenuTrackerItemClass;

struct _GtkMenuTrackerItem
{
  GObject parent_instance;

  GtkActionObservable *observable;
  gchar *action_namespace;
  GMenuItem *item;
  GtkMenuTrackerItemRole role : 4;
  guint is_separator : 1;
  guint can_activate : 1;
  guint sensitive : 1;
  guint toggled : 1;
  guint submenu_shown : 1;
  guint submenu_requested : 1;
  GVariant *action_state;
};

enum {
  PROP_0,
  PROP_IS_SEPARATOR,
  PROP_HAS_SUBMENU,
  PROP_LABEL,
  PROP_ICON,
  PROP_SENSITIVE,
  PROP_VISIBLE,
  PROP_ROLE,
  PROP_TOGGLED,
  PROP_ACCEL,
  PROP_SUBMENU_SHOWN,
  PROP_ACTION_NAME,
  PROP_ACTION_STATE,
  N_PROPS
};

static GParamSpec *gtk_menu_tracker_item_pspecs[N_PROPS];

static void gtk_menu_tracker_item_init_observer_iface (GtkActionObserverInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GtkMenuTrackerItem, gtk_menu_tracker_item, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTION_OBSERVER, gtk_menu_tracker_item_init_observer_iface))

GType
gtk_menu_tracker_item_role_get_type (void)
{
  static gsize gtk_menu_tracker_item_role_type;

  if (g_once_init_enter (&gtk_menu_tracker_item_role_type))
    {
      static const GEnumValue values[] = {
        { GTK_MENU_TRACKER_ITEM_ROLE_NORMAL, "GTK_MENU_TRACKER_ITEM_ROLE_NORMAL", "normal" },
        { GTK_MENU_TRACKER_ITEM_ROLE_CHECK, "GTK_MENU_TRACKER_ITEM_ROLE_CHECK", "check" },
        { GTK_MENU_TRACKER_ITEM_ROLE_RADIO, "GTK_MENU_TRACKER_ITEM_ROLE_RADIO", "radio" },
        { 0, NULL, NULL }
      };
      GType type;

      type = g_enum_register_static ("GtkMenuTrackerItemRole", values);

      g_once_init_leave (&gtk_menu_tracker_item_role_type, type);
    }

  return gtk_menu_tracker_item_role_type;
}

static void
gtk_menu_tracker_item_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkMenuTrackerItem *self = GTK_MENU_TRACKER_ITEM (object);

  switch (prop_id)
    {
    case PROP_IS_SEPARATOR:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_is_separator (self));
      break;
    case PROP_HAS_SUBMENU:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_has_submenu (self));
      break;
    case PROP_LABEL:
      g_value_set_string (value, gtk_menu_tracker_item_get_label (self));
      break;
    case PROP_ICON:
      g_value_set_object (value, gtk_menu_tracker_item_get_icon (self));
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_sensitive (self));
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_visible (self));
      break;
    case PROP_ROLE:
      g_value_set_enum (value, gtk_menu_tracker_item_get_role (self));
      break;
    case PROP_TOGGLED:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_toggled (self));
      break;
    case PROP_ACCEL:
      g_value_set_string (value, gtk_menu_tracker_item_get_accel (self));
      break;
    case PROP_SUBMENU_SHOWN:
      g_value_set_boolean (value, gtk_menu_tracker_item_get_submenu_shown (self));
      break;
    case PROP_ACTION_NAME:
      g_value_take_string (value, gtk_menu_tracker_item_get_action_name (self));
    case PROP_ACTION_STATE:
      g_value_set_variant (value, self->action_state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtk_menu_tracker_item_finalize (GObject *object)
{
  GtkMenuTrackerItem *self = GTK_MENU_TRACKER_ITEM (object);

  g_free (self->action_namespace);

  if (self->observable)
    g_object_unref (self->observable);

  if (self->action_state)
    g_variant_unref (self->action_state);

  g_object_unref (self->item);

  G_OBJECT_CLASS (gtk_menu_tracker_item_parent_class)->finalize (object);
}

static void
gtk_menu_tracker_item_init (GtkMenuTrackerItem * self)
{
}

static void
gtk_menu_tracker_item_class_init (GtkMenuTrackerItemClass *class)
{
  class->get_property = gtk_menu_tracker_item_get_property;
  class->finalize = gtk_menu_tracker_item_finalize;

  gtk_menu_tracker_item_pspecs[PROP_IS_SEPARATOR] =
    g_param_spec_boolean ("is-separator", "", "", FALSE, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_HAS_SUBMENU] =
    g_param_spec_boolean ("has-submenu", "", "", FALSE, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_LABEL] =
    g_param_spec_string ("label", "", "", NULL, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_ICON] =
    g_param_spec_object ("icon", "", "", G_TYPE_ICON, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_SENSITIVE] =
    g_param_spec_boolean ("sensitive", "", "", FALSE, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_VISIBLE] =
    g_param_spec_boolean ("visible", "", "", FALSE, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_ROLE] =
    g_param_spec_enum ("role", "", "",
                       GTK_TYPE_MENU_TRACKER_ITEM_ROLE, GTK_MENU_TRACKER_ITEM_ROLE_NORMAL,
                       G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_TOGGLED] =
    g_param_spec_boolean ("toggled", "", "", FALSE, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_ACCEL] =
    g_param_spec_string ("accel", "", "", NULL, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_SUBMENU_SHOWN] =
    g_param_spec_boolean ("submenu-shown", "", "", FALSE, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_ACTION_NAME] =
    g_param_spec_boolean ("action-name", "", "", FALSE, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);
  gtk_menu_tracker_item_pspecs[PROP_ACTION_STATE] =
    g_param_spec_boolean ("action-state", "", "", FALSE, G_PARAM_STATIC_STRINGS | G_PARAM_READABLE);

  g_object_class_install_properties (class, N_PROPS, gtk_menu_tracker_item_pspecs);
}

static void
gtk_menu_tracker_item_action_added (GtkActionObserver   *observer,
                                    GtkActionObservable *observable,
                                    const gchar         *action_name,
                                    const GVariantType  *parameter_type,
                                    gboolean             enabled,
                                    GVariant            *state)
{
  GtkMenuTrackerItem *self = GTK_MENU_TRACKER_ITEM (observer);
  GVariant *action_target;

  action_target = g_menu_item_get_attribute_value (self->item, G_MENU_ATTRIBUTE_TARGET, NULL);

  self->can_activate = (action_target == NULL && parameter_type == NULL) ||
                        (action_target != NULL && parameter_type != NULL &&
                        g_variant_is_of_type (action_target, parameter_type));

  if (!self->can_activate)
    {
      if (action_target)
        g_variant_unref (action_target);
      return;
    }

  self->sensitive = enabled;

  if (action_target != NULL && state != NULL)
    {
      self->toggled = g_variant_equal (state, action_target);
      self->role = GTK_MENU_TRACKER_ITEM_ROLE_RADIO;
    }

  else if (state != NULL && g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    {
      self->toggled = g_variant_get_boolean (state);
      self->role = GTK_MENU_TRACKER_ITEM_ROLE_CHECK;
    }

  g_object_freeze_notify (G_OBJECT (self));

  if (self->sensitive)
    g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_SENSITIVE]);

  if (self->toggled)
    g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_TOGGLED]);

  if (self->role != GTK_MENU_TRACKER_ITEM_ROLE_NORMAL)
    g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_ROLE]);

  if (state != NULL)
    {
      self->action_state = g_variant_ref (state);
      g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_ACTION_STATE]);
    }

  g_object_thaw_notify (G_OBJECT (self));

  if (action_target)
    g_variant_unref (action_target);
}

static void
gtk_menu_tracker_item_action_enabled_changed (GtkActionObserver   *observer,
                                              GtkActionObservable *observable,
                                              const gchar         *action_name,
                                              gboolean             enabled)
{
  GtkMenuTrackerItem *self = GTK_MENU_TRACKER_ITEM (observer);

  if (!self->can_activate)
    return;

  if (self->sensitive == enabled)
    return;

  self->sensitive = enabled;

  g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_SENSITIVE]);
}

static void
gtk_menu_tracker_item_action_state_changed (GtkActionObserver   *observer,
                                            GtkActionObservable *observable,
                                            const gchar         *action_name,
                                            GVariant            *state)
{
  GtkMenuTrackerItem *self = GTK_MENU_TRACKER_ITEM (observer);
  GVariant *action_target;
  gboolean was_toggled;

  if (!self->can_activate)
    return;

  action_target = g_menu_item_get_attribute_value (self->item, G_MENU_ATTRIBUTE_TARGET, NULL);
  was_toggled = self->toggled;

  if (action_target)
    {
      self->toggled = g_variant_equal (state, action_target);
      g_variant_unref (action_target);
    }

  else if (g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    self->toggled = g_variant_get_boolean (state);

  else
    self->toggled = FALSE;

  if (self->toggled != was_toggled)
    g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_TOGGLED]);

  if (self->action_state)
    g_variant_unref (self->action_state);
  self->action_state = g_variant_ref (state);
  g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_ACTION_STATE]);
}

static void
gtk_menu_tracker_item_action_removed (GtkActionObserver   *observer,
                                      GtkActionObservable *observable,
                                      const gchar         *action_name)
{
  GtkMenuTrackerItem *self = GTK_MENU_TRACKER_ITEM (observer);

  if (!self->can_activate)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  if (self->sensitive)
    {
      self->sensitive = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_SENSITIVE]);
    }

  if (self->toggled)
    {
      self->toggled = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_TOGGLED]);
    }

  if (self->role != GTK_MENU_TRACKER_ITEM_ROLE_NORMAL)
    {
      self->role = GTK_MENU_TRACKER_ITEM_ROLE_NORMAL;
      g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_ROLE]);
    }

  if (self->action_state != NULL)
    {
      g_variant_unref (self->action_state);
      g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_ACTION_STATE]);
    }

  g_object_thaw_notify (G_OBJECT (self));
}

static void
gtk_menu_tracker_item_init_observer_iface (GtkActionObserverInterface *iface)
{
  iface->action_added = gtk_menu_tracker_item_action_added;
  iface->action_enabled_changed = gtk_menu_tracker_item_action_enabled_changed;
  iface->action_state_changed = gtk_menu_tracker_item_action_state_changed;
  iface->action_removed = gtk_menu_tracker_item_action_removed;
}

GtkMenuTrackerItem *
_gtk_menu_tracker_item_new (GtkActionObservable *observable,
                            GMenuModel          *model,
                            gint                 item_index,
                            const gchar         *action_namespace,
                            gboolean             is_separator)
{
  GtkMenuTrackerItem *self;
  const gchar *action_name;

  g_return_val_if_fail (GTK_IS_ACTION_OBSERVABLE (observable), NULL);
  g_return_val_if_fail (G_IS_MENU_MODEL (model), NULL);

  self = g_object_new (GTK_TYPE_MENU_TRACKER_ITEM, NULL);
  self->item = g_menu_item_new_from_model (model, item_index);
  self->action_namespace = g_strdup (action_namespace);
  self->observable = g_object_ref (observable);
  self->is_separator = is_separator;

  if (!is_separator && g_menu_item_get_attribute (self->item, "action", "&s", &action_name))
    {
      GActionGroup *group = G_ACTION_GROUP (observable);
      const GVariantType *parameter_type;
      gboolean enabled;
      GVariant *state;
      gboolean found;

      state = NULL;

      if (action_namespace)
        {
          gchar *full_action;

          full_action = g_strjoin (".", action_namespace, action_name, NULL);
          gtk_action_observable_register_observer (self->observable, full_action, GTK_ACTION_OBSERVER (self));
          found = g_action_group_query_action (group, full_action, &enabled, &parameter_type, NULL, NULL, &state);
          g_free (full_action);
        }
      else
        {
          gtk_action_observable_register_observer (self->observable, action_name, GTK_ACTION_OBSERVER (self));
          found = g_action_group_query_action (group, action_name, &enabled, &parameter_type, NULL, NULL, &state);
        }

      if (found)
        gtk_menu_tracker_item_action_added (GTK_ACTION_OBSERVER (self), observable, NULL, parameter_type, enabled, state);
      else
        gtk_menu_tracker_item_action_removed (GTK_ACTION_OBSERVER (self), observable, NULL);

      if (state)
        g_variant_unref (state);
    }
  else
    self->sensitive = TRUE;

  return self;
}

GtkActionObservable *
_gtk_menu_tracker_item_get_observable (GtkMenuTrackerItem *self)
{
  return self->observable;
}

/**
 * gtk_menu_tracker_item_get_is_separator:
 * @self: A #GtkMenuTrackerItem instance
 *
 * Returns whether the menu item is a separator. If so, only
 * certain properties may need to be obeyed. See the documentation
 * for #GtkMenuTrackerItem.
 */
gboolean
gtk_menu_tracker_item_get_is_separator (GtkMenuTrackerItem *self)
{
  return self->is_separator;
}

/**
 * gtk_menu_tracker_item_get_has_submenu:
 * @self: A #GtkMenuTrackerItem instance
 *
 * Returns whether the menu item has a submenu. If so, only
 * certain properties may need to be obeyed. See the documentation
 * for #GtkMenuTrackerItem.
 */
gboolean
gtk_menu_tracker_item_get_has_submenu (GtkMenuTrackerItem *self)
{
  GMenuModel *link;

  link = g_menu_item_get_link (self->item, G_MENU_LINK_SUBMENU);

  if (link)
    {
      g_object_unref (link);
      return TRUE;
    }
  else
    return FALSE;
}

const gchar *
gtk_menu_tracker_item_get_label (GtkMenuTrackerItem *self)
{
  const gchar *label = NULL;

  g_menu_item_get_attribute (self->item, G_MENU_ATTRIBUTE_LABEL, "&s", &label);

  return label;
}

/**
 * gtk_menu_tracker_item_get_icon:
 *
 * Returns: (transfer full):
 */
GIcon *
gtk_menu_tracker_item_get_icon (GtkMenuTrackerItem *self)
{
  GVariant *icon_data;
  GIcon *icon;

  icon_data = g_menu_item_get_attribute_value (self->item, "icon", NULL);

  if (icon_data == NULL)
    return NULL;

  icon = g_icon_deserialize (icon_data);
  g_variant_unref (icon_data);

  return icon;
}

gboolean
gtk_menu_tracker_item_get_sensitive (GtkMenuTrackerItem *self)
{
  return self->sensitive;
}

gboolean
gtk_menu_tracker_item_get_visible (GtkMenuTrackerItem *self)
{
  return TRUE;
}

GtkMenuTrackerItemRole
gtk_menu_tracker_item_get_role (GtkMenuTrackerItem *self)
{
  return self->role;
}

gboolean
gtk_menu_tracker_item_get_toggled (GtkMenuTrackerItem *self)
{
  return self->toggled;
}

const gchar *
gtk_menu_tracker_item_get_accel (GtkMenuTrackerItem *self)
{
  const gchar *accel = NULL;

  g_menu_item_get_attribute (self->item, "accel", "&s", &accel);

  return accel;
}

GMenuModel *
_gtk_menu_tracker_item_get_submenu (GtkMenuTrackerItem *self)
{
  return g_menu_item_get_link (self->item, "submenu");
}

gchar *
_gtk_menu_tracker_item_get_submenu_namespace (GtkMenuTrackerItem *self)
{
  const gchar *namespace;

  if (g_menu_item_get_attribute (self->item, "action-namespace", "&s", &namespace))
    {
      if (self->action_namespace)
        return g_strjoin (".", self->action_namespace, namespace, NULL);
      else
        return g_strdup (namespace);
    }
  else
    return g_strdup (self->action_namespace);
}

gboolean
gtk_menu_tracker_item_get_should_request_show (GtkMenuTrackerItem *self)
{
  return g_menu_item_get_attribute (self->item, "submenu-action", "&s", NULL);
}

gboolean
gtk_menu_tracker_item_get_submenu_shown (GtkMenuTrackerItem *self)
{
  return self->submenu_shown;
}

/**
 * gtk_menu_tracker_item_get_action_name:
 * @self: A #GtkMenuTrackerItem instance
 *
 * Returns a newly-allocated string containing the name of the action
 * associated with this menu item.
 *
 * Returns: (transfer full): the action name, free it with g_free()
 */
gchar *
gtk_menu_tracker_item_get_action_name (GtkMenuTrackerItem *self)
{
  const gchar *action_name;

  if (!g_menu_item_get_attribute (self->item, G_MENU_ATTRIBUTE_ACTION, "&s", &action_name))
    return NULL;

  if (self->action_namespace)
    return g_strjoin (".", self->action_namespace, action_name, NULL);
  else
    return g_strdup (action_name);
}

GVariant *
gtk_menu_tracker_item_get_action_state (GtkMenuTrackerItem *self)
{
  if (self->action_state != NULL)
    return g_variant_ref (self->action_state);

  return NULL;
}

static void
gtk_menu_tracker_item_set_submenu_shown (GtkMenuTrackerItem *self,
                                         gboolean            submenu_shown)
{
  if (submenu_shown == self->submenu_shown)
    return;

  self->submenu_shown = submenu_shown;
  g_object_notify_by_pspec (G_OBJECT (self), gtk_menu_tracker_item_pspecs[PROP_SUBMENU_SHOWN]);
}

void
gtk_menu_tracker_item_activated (GtkMenuTrackerItem *self)
{
  const gchar *action_name;
  GVariant *action_target;

  g_return_if_fail (GTK_IS_MENU_TRACKER_ITEM (self));

  if (!self->can_activate)
    return;

  g_menu_item_get_attribute (self->item, G_MENU_ATTRIBUTE_ACTION, "&s", &action_name);
  action_target = g_menu_item_get_attribute_value (self->item, G_MENU_ATTRIBUTE_TARGET, NULL);

  if (self->action_namespace)
    {
      gchar *full_action;

      full_action = g_strjoin (".", self->action_namespace, action_name, NULL);
      g_action_group_activate_action (G_ACTION_GROUP (self->observable), full_action, action_target);
      g_free (full_action);
    }
  else
    g_action_group_activate_action (G_ACTION_GROUP (self->observable), action_name, action_target);

  if (action_target)
    g_variant_unref (action_target);
}

void
gtk_menu_tracker_item_change_state (GtkMenuTrackerItem *self,
                                    GVariant           *value)
{
  const gchar *action_name;

  g_return_if_fail (GTK_IS_MENU_TRACKER_ITEM (self));

  g_menu_item_get_attribute (self->item, G_MENU_ATTRIBUTE_ACTION, "&s", &action_name);

  if (self->action_namespace)
    {
      gchar *full_action;

      full_action = g_strjoin (".", self->action_namespace, action_name, NULL);
      g_action_group_change_action_state (G_ACTION_GROUP (self->observable), full_action, g_variant_ref(value));
      g_free (full_action);
    }
  else
    g_action_group_change_action_state (G_ACTION_GROUP (self->observable), action_name, g_variant_ref(value));
}

typedef struct
{
  GtkMenuTrackerItem *item;
  gchar              *submenu_action;
  gboolean            first_time;
} GtkMenuTrackerOpener;

static void
gtk_menu_tracker_opener_update (GtkMenuTrackerOpener *opener)
{
  GActionGroup *group = G_ACTION_GROUP (opener->item->observable);
  gboolean is_open = TRUE;

  /* We consider the menu as being "open" if the action does not exist
   * or if there is another problem (no state, wrong state type, etc.).
   * If the action exists, with the correct state then we consider it
   * open if we have ever seen this state equal to TRUE.
   *
   * In the event that we see the state equal to FALSE, we force it back
   * to TRUE.  We do not signal that the menu was closed because this is
   * likely to create UI thrashing.
   *
   * The only way the menu can have a true-to-false submenu-shown
   * transition is if the user calls _request_submenu_shown (FALSE).
   * That is handled in _free() below.
   */

  if (g_action_group_has_action (group, opener->submenu_action))
    {
      GVariant *state = g_action_group_get_action_state (group, opener->submenu_action);

      if (state)
        {
          if (g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
            is_open = g_variant_get_boolean (state);
          g_variant_unref (state);
        }
    }

  /* If it is already open, signal that.
   *
   * If it is not open, ask it to open.
   */
  if (is_open)
    gtk_menu_tracker_item_set_submenu_shown (opener->item, TRUE);

  if (!is_open || opener->first_time)
    {
      g_action_group_change_action_state (group, opener->submenu_action, g_variant_new_boolean (TRUE));
      opener->first_time = FALSE;
    }
}

static void
gtk_menu_tracker_opener_added (GActionGroup *group,
                               const gchar  *action_name,
                               gpointer      user_data)
{
  GtkMenuTrackerOpener *opener = user_data;

  if (g_str_equal (action_name, opener->submenu_action))
    gtk_menu_tracker_opener_update (opener);
}

static void
gtk_menu_tracker_opener_removed (GActionGroup *action_group,
                                 const gchar  *action_name,
                                 gpointer      user_data)
{
  GtkMenuTrackerOpener *opener = user_data;

  if (g_str_equal (action_name, opener->submenu_action))
    gtk_menu_tracker_opener_update (opener);
}

static void
gtk_menu_tracker_opener_changed (GActionGroup *action_group,
                                 const gchar  *action_name,
                                 GVariant     *new_state,
                                 gpointer      user_data)
{
  GtkMenuTrackerOpener *opener = user_data;

  if (g_str_equal (action_name, opener->submenu_action))
    gtk_menu_tracker_opener_update (opener);
}

static void
gtk_menu_tracker_opener_free (gpointer data)
{
  GtkMenuTrackerOpener *opener = data;

  g_signal_handlers_disconnect_by_func (opener->item->observable, gtk_menu_tracker_opener_added, opener);
  g_signal_handlers_disconnect_by_func (opener->item->observable, gtk_menu_tracker_opener_removed, opener);
  g_signal_handlers_disconnect_by_func (opener->item->observable, gtk_menu_tracker_opener_changed, opener);

  g_action_group_change_action_state (G_ACTION_GROUP (opener->item->observable),
                                      opener->submenu_action,
                                      g_variant_new_boolean (FALSE));

  gtk_menu_tracker_item_set_submenu_shown (opener->item, FALSE);

  g_free (opener->submenu_action);

  g_slice_free (GtkMenuTrackerOpener, opener);
}

static GtkMenuTrackerOpener *
gtk_menu_tracker_opener_new (GtkMenuTrackerItem *item,
                             const gchar        *submenu_action)
{
  GtkMenuTrackerOpener *opener;

  opener = g_slice_new (GtkMenuTrackerOpener);
  opener->first_time = TRUE;
  opener->item = item;

  if (item->action_namespace)
    opener->submenu_action = g_strjoin (".", item->action_namespace, submenu_action, NULL);
  else
    opener->submenu_action = g_strdup (submenu_action);

  g_signal_connect (item->observable, "action-added", G_CALLBACK (gtk_menu_tracker_opener_added), opener);
  g_signal_connect (item->observable, "action-removed", G_CALLBACK (gtk_menu_tracker_opener_removed), opener);
  g_signal_connect (item->observable, "action-state-changed", G_CALLBACK (gtk_menu_tracker_opener_changed), opener);

  gtk_menu_tracker_opener_update (opener);

  return opener;
}

void
gtk_menu_tracker_item_request_submenu_shown (GtkMenuTrackerItem *self,
                                             gboolean            shown)
{
  const gchar *submenu_action;
  gboolean has_submenu_action;

  if (shown == self->submenu_requested)
    return;

  has_submenu_action = g_menu_item_get_attribute (self->item, "submenu-action", "&s", &submenu_action);

  self->submenu_requested = shown;

  /* If we have a submenu action, start a submenu opener and wait
   * for the reply from the client. Otherwise, simply open the
   * submenu immediately.
   */
  if (has_submenu_action)
    {
      if (shown)
        g_object_set_data_full (G_OBJECT (self), "submenu-opener",
                                gtk_menu_tracker_opener_new (self, submenu_action),
                                gtk_menu_tracker_opener_free);
      else
        g_object_set_data (G_OBJECT (self), "submenu-opener", NULL);
    }
  else
    gtk_menu_tracker_item_set_submenu_shown (self, shown);
}

gboolean
gtk_menu_tracker_item_get_attribute (GtkMenuTrackerItem *self,
                                     const gchar        *attribute,
                                     const gchar        *format,
                                     ...)
{
  gboolean success = FALSE;
  GVariant *value;

  g_return_val_if_fail (GTK_IS_MENU_TRACKER_ITEM (self), FALSE);
  g_return_val_if_fail (attribute != NULL, FALSE);
  g_return_val_if_fail (format != NULL, FALSE);

  value = g_menu_item_get_attribute_value (self->item, attribute, NULL);
  if (value)
    {
      if (g_variant_check_format_string (value, format, TRUE))
        {
          va_list args;

          va_start (args, format);
          g_variant_get_va (value, format, NULL, &args);
          va_end (args);

          success = TRUE;
        }

      g_variant_unref (value);
    }

  return success;
}

GVariant *
gtk_menu_tracker_item_get_attribute_value (GtkMenuTrackerItem *self,
                                           const gchar        *attribute,
                                           const GVariantType *expected_type)
{
  return g_menu_item_get_attribute_value (self->item, attribute, expected_type);
}
