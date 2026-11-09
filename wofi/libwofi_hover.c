#define _GNU_SOURCE
#include <dlfcn.h>
#include <gtk/gtk.h>

static gboolean on_flowbox_motion(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    GtkFlowBox *box = GTK_FLOW_BOX(widget);
    gint x = (gint)event->x;
    gint y = (gint)event->y;

    GdkWindow *box_win = gtk_widget_get_window(widget);
    if (event->window && box_win && event->window != box_win) {
        gint wx = 0, wy = 0, bx = 0, by = 0;
        gdk_window_get_origin(event->window, &wx, &wy);
        gdk_window_get_origin(box_win, &bx, &by);
        x += (wx - bx);
        y += (wy - by);
    }

    GtkFlowBoxChild *child = gtk_flow_box_get_child_at_pos(box, x, y);
    if (child && !gtk_flow_box_child_is_selected(child)) {
        gtk_flow_box_select_child(box, child);
    }
    return FALSE;
}

static gboolean on_child_enter(GtkWidget *widget, GdkEventCrossing *event, gpointer user_data) {
    GtkWidget *p = widget;
    while (p && !GTK_IS_FLOW_BOX_CHILD(p)) {
        p = gtk_widget_get_parent(p);
    }
    if (p && GTK_IS_FLOW_BOX_CHILD(p)) {
        GtkWidget *box = gtk_widget_get_parent(p);
        if (box && GTK_IS_FLOW_BOX(box)) {
            GtkFlowBoxChild *child = GTK_FLOW_BOX_CHILD(p);
            if (!gtk_flow_box_child_is_selected(child)) {
                gtk_flow_box_select_child(GTK_FLOW_BOX(box), child);
            }
        }
    }
    return FALSE;
}

static void on_hadj_changed(GtkAdjustment *adj, gpointer user_data) {
    if (gtk_adjustment_get_value(adj) != 0.0) {
        gtk_adjustment_set_value(adj, 0.0);
    }
}

static void hook_widget_tree(GtkWidget *w, gpointer data) {
    if (GTK_IS_SCROLLED_WINDOW(w)) {
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w), GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL);
        GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(w));
        if (hadj) {
            gtk_adjustment_set_value(hadj, 0.0);
            g_signal_connect(hadj, "value-changed", G_CALLBACK(on_hadj_changed), NULL);
        }
    }
    gtk_widget_add_events(w, GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK);
    g_signal_connect(w, "enter-notify-event", G_CALLBACK(on_child_enter), NULL);
    if (GTK_IS_CONTAINER(w)) {
        gtk_container_forall(GTK_CONTAINER(w), hook_widget_tree, NULL);
    }
}

GtkWidget* gtk_flow_box_new(void) {
    static GtkWidget* (*real_gtk_flow_box_new)(void) = NULL;
    if (!real_gtk_flow_box_new) {
        real_gtk_flow_box_new = dlsym(RTLD_NEXT, "gtk_flow_box_new");
    }
    GtkWidget *box = real_gtk_flow_box_new();
    gtk_widget_add_events(box, GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK);
    g_signal_connect(box, "motion-notify-event", G_CALLBACK(on_flowbox_motion), NULL);
    return box;
}

void gtk_widget_show_all(GtkWidget *widget) {
    static void (*real_gtk_widget_show_all)(GtkWidget*) = NULL;
    if (!real_gtk_widget_show_all) {
        real_gtk_widget_show_all = dlsym(RTLD_NEXT, "gtk_widget_show_all");
    }
    if (GTK_IS_FLOW_BOX_CHILD(widget) || GTK_IS_WINDOW(widget)) {
        hook_widget_tree(widget, NULL);
    }
    real_gtk_widget_show_all(widget);
}
