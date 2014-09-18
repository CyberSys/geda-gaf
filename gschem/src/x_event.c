/* gEDA - GPL Electronic Design Automation
 * gschem - gEDA Schematic Capture
 * Copyright (C) 1998-2010 Ales Hvezda
 * Copyright (C) 1998-2011 gEDA Contributors (see ChangeLog for details)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <config.h>

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "gschem.h"
#include <gdk/gdkkeysyms.h>


/* used by mouse pan */
int start_pan_x, start_pan_y;
int throttle = 0;

/* used for the stroke stuff */
#ifdef HAVE_LIBSTROKE
static int DOING_STROKE = FALSE;
#endif /* HAVE_LIBSTROKE */

/*! \brief Redraws the view when widget is exposed.
 *
 *  \param [in] view      The GschemPageView.
 *  \param [in] event     The event structure.
 *  \param [in] w_current The GschemToplevel.
 *  \returns FALSE to propagate the event further.
 */

gint
x_event_expose(GschemPageView *view, GdkEventExpose *event, GschemToplevel *w_current)
{
  GschemPageGeometry *geometry;
  PAGE *page;

#if DEBUG
  printf("EXPOSE\n");
#endif

  g_return_val_if_fail (view != NULL, 0);
  g_return_val_if_fail (w_current != NULL, 0);

  page = gschem_page_view_get_page (view);
  geometry = gschem_page_view_get_page_geometry (view);

  if (page != NULL) {
    cairo_t *cr = gdk_cairo_create (GTK_WIDGET (view)->window);

    gdk_cairo_rectangle (cr, &(event->area));
    cairo_clip (cr);

    o_redraw_rects (w_current,
                    cr,
                    page,
                    geometry,
                    &(event->area),
                    1);

    cairo_destroy (cr);
  }

  return(0);
}



/*! \todo Finish function documentation!!!
 *  \brief
 *  \par Function Description
 *
 */
gint
x_event_raise_dialog_boxes (GschemPageView *view, GdkEventExpose *event, GschemToplevel *w_current)
{
  g_return_val_if_fail (w_current != NULL, 0);

  /* raise the dialog boxes if this feature is enabled */
  if (w_current->raise_dialog_boxes) {
    x_dialog_raise_all(w_current);
  }

  return 0;
}




/*! \todo Finish function documentation!!!
 *  \brief
 *  \par Function Description
 *
 */
gint
x_event_button_pressed(GschemPageView *page_view, GdkEventButton *event, GschemToplevel *w_current)
{
  PAGE *page = gschem_page_view_get_page (page_view);
  int w_x, w_y;
  int unsnapped_wx, unsnapped_wy;

  g_return_val_if_fail ((w_current != NULL), 0);

  scm_dynwind_begin (0);
  g_dynwind_window (w_current);

#if DEBUG
  printf("pressed button %d! \n", event->button);
  printf("event state: %d \n", event->state);
  printf("w_current state: %d \n", w_current->event_state);
  printf("Selection is:\n");
  o_selection_print_all(&(page->selection_list));
  printf("\n");
#endif

  gschem_page_view_SCREENtoWORLD (page_view, (int) event->x, (int) event->y,
                                  &unsnapped_wx, &unsnapped_wy);
  w_x = snap_grid (w_current, unsnapped_wx);
  w_y = snap_grid (w_current, unsnapped_wy);

  if (event->type == GDK_2BUTTON_PRESS &&
      (w_current->event_state == STARTSELECT ||
       w_current->event_state == SELECT)) {
    o_find_object(w_current, w_x, w_y, TRUE);
    if (o_select_selected (w_current)) {
       o_edit(w_current, geda_list_get_glist( page->selection_list ));
       i_set_state(w_current, SELECT);
       scm_dynwind_end ();
       return(0);
    }
  }

  w_current->SHIFTKEY   = (event->state & GDK_SHIFT_MASK  ) ? 1 : 0;
  w_current->CONTROLKEY = (event->state & GDK_CONTROL_MASK) ? 1 : 0;
  w_current->ALTKEY     = (event->state & GDK_MOD1_MASK) ? 1 : 0;

  /* Huge switch statement to evaluate state transitions. Jump to
   * end_button_pressed label to escape the state evaluation rather than
   * returning from the function directly. */

  if (event->button == 1) {
    switch(w_current->event_state) {

      case(SELECT):
        /* look for grips or fall through if not enabled */
        if (!o_grips_start(w_current, unsnapped_wx, unsnapped_wy)) {
          /* now go into normal SELECT */
          w_current->event_state = STARTSELECT;
          w_current->first_wx = w_current->second_wx = unsnapped_wx;
          w_current->first_wy = w_current->second_wy = unsnapped_wy;
        } else {
          /* a grip was found */
          w_current->event_state = GRIPS;
          w_current->inside_action = 1;
        }
        break;

      case(STARTCOPY):
        if (o_select_selected(w_current)) {
          o_copy_start(w_current, w_x, w_y);
          w_current->event_state = COPY;
          w_current->inside_action = 1;
        }
        break;

      case(STARTMCOPY):
        if (o_select_selected(w_current)) {
          o_copy_start(w_current, w_x, w_y);
          w_current->event_state = MCOPY;
          w_current->inside_action = 1;
        }
        break;

      case(STARTMOVE):
        if (o_select_selected(w_current)) {
          o_move_start(w_current, w_x, w_y);
          w_current->event_state = MOVE;
          w_current->inside_action = 1;
        }
        break;

      case(STARTPASTE):
        o_buffer_paste_start(w_current, w_x, w_y, w_current->buffer_number);
        w_current->event_state = ENDPASTE;
        w_current->inside_action = 1;
        break;

      case(DRAWLINE):
        o_line_start(w_current, w_x, w_y);
        w_current->event_state = ENDLINE;
        w_current->inside_action = 1;
        break;

      case(ENDLINE):
        o_line_end(w_current, w_x, w_y);
        w_current->inside_action = 0;
        w_current->event_state = DRAWLINE;
        break;

    case DRAWPATH:
      o_path_start (w_current, w_x, w_y);
      w_current->event_state = ENDPATH;
      w_current->inside_action = TRUE;
      break;

    case PATHCONT:
      o_path_continue (w_current, w_x, w_y);
      w_current->event_state = ENDPATH;
      w_current->inside_action = TRUE;
      break;

      case(DRAWBOX):
        o_box_start(w_current, w_x, w_y);
        w_current->event_state = ENDBOX;
        w_current->inside_action = 1;
        break;

      case(ENDBOX):
        o_box_end(w_current, w_x, w_y);
        w_current->inside_action = 0;
        w_current->event_state = DRAWBOX;
        break;

      case(DRAWPICTURE):
        o_picture_start(w_current, w_x, w_y);
        w_current->event_state = ENDPICTURE;
        w_current->inside_action = 1;
        break;

      case(ENDPICTURE):
        o_picture_end(w_current, w_x, w_y);
        w_current->inside_action = 0;
        w_current->event_state = DRAWPICTURE;
        break;

      case(DRAWCIRCLE):
        o_circle_start(w_current, w_x, w_y);
        w_current->event_state = ENDCIRCLE;
        w_current->inside_action = 1;
        break;

      case(ENDCIRCLE):
        o_circle_end(w_current, w_x, w_y);
        w_current->inside_action = 0;
        w_current->event_state = DRAWCIRCLE;
        break;

      case(DRAWARC):
        o_arc_start(w_current, w_x, w_y);
        w_current->event_state = ENDARC;
        w_current->inside_action = 1;
        break;

      case(ENDARC):
        o_arc_end1(w_current, w_x, w_y);
        w_current->inside_action = 0;
        w_current->event_state = DRAWARC;
        break;

      case(DRAWPIN):
        o_pin_start(w_current, w_x, w_y);
        w_current->event_state = ENDPIN;
        w_current->inside_action = 1;
        break;

      case(ENDPIN):
        o_pin_end(w_current, w_x, w_y);
        w_current->inside_action = 0;
        w_current->event_state = DRAWPIN;
        break;

      case(STARTDRAWNET):  /*! \todo change state name? */
        o_net_start(w_current, w_x, w_y);
        w_current->inside_action = 1;
        w_current->event_state=DRAWNET;

        break;

      case(STARTDRAWBUS):
        o_bus_start(w_current, w_x, w_y);
        w_current->inside_action = 1;
        w_current->event_state=DRAWBUS;

        break;

      case(DRAWNET):
      case(NETCONT):
        /* Only continue the net if net end worked */
        if (o_net_end(w_current, w_x, w_y)) {
          o_net_start(w_current, w_current->first_wx, w_current->first_wy);
          w_current->event_state=NETCONT;
        } else { /* cleanup and start a new net */
          o_net_invalidate_rubber (w_current);
          o_net_reset(w_current);
          i_set_state(w_current, STARTDRAWNET);
          w_current->inside_action = 0;
        }
        break;

      case(DRAWBUS):
      case(BUSCONT):
        /* Only continue the net if net end worked */
        if (o_bus_end(w_current, w_x, w_y)) {
          o_bus_start(w_current, w_current->first_wx, w_current->first_wy);
          w_current->event_state=BUSCONT;
        } else {
          w_current->inside_action=0;
          i_set_state(w_current, STARTDRAWBUS);
        }
        break;
      case(ENDCOMP):
        o_place_end(w_current, w_x, w_y, w_current->continue_component_place,
                    NULL, "%add-objects-hook");
        if (!w_current->continue_component_place) {
          w_current->inside_action = 0;
          i_set_state(w_current, SELECT);
          i_update_toolbar(w_current);
        }
        break;

      case(ENDPASTE):
        o_place_end(w_current, w_x, w_y, FALSE, NULL, "%paste-objects-hook");
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;

      case(ENDROTATEP):
        o_rotate_world_update(w_current, w_x, w_y, 90,
          geda_list_get_glist(page->selection_list ));

        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;

      case(ENDMIRROR):
        o_mirror_world_update(w_current, w_x, w_y,
                              geda_list_get_glist(
                                page->selection_list ));

        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;

      case(ENDTEXT):
        o_place_end(w_current, w_x, w_y, FALSE, NULL, "%add-objects-hook");
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;


      case(STARTPAN):
        gschem_page_view_pan (page_view, w_x, w_y);
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;

      case(ZOOMBOXSTART):
        a_zoom_box_start(w_current, unsnapped_wx, unsnapped_wy);
        w_current->event_state = ZOOMBOXEND;
        w_current->inside_action = 1;
        break;

    }
  } else if (event->button == 2) {

    /* try this out and see how it behaves */
    if (w_current->inside_action) {
      if (!(w_current->event_state == ENDCOMP ||
            w_current->event_state == ENDTEXT ||
            w_current->event_state == ENDMOVE ||
            w_current->event_state == ENDCOPY ||
            w_current->event_state == ENDMCOPY ||
            w_current->event_state == ENDPASTE )) {
        i_callback_cancel(w_current, 0, NULL);
      }
      goto end_button_pressed;
    }

    switch(w_current->middle_button) {

      case(ACTION):
                                /* determine here if copy or move */
                                /* for now do move only */
                                /* make sure the list is not empty */
      if (o_select_selected(w_current)) {

        /* don't want to search if shift */
        /* key is depresed */
        if (!w_current->SHIFTKEY) {
          o_find_object(w_current, unsnapped_wx, unsnapped_wy, TRUE);
        }
      } else {
        o_select_unselect_all(w_current);
        /* don't want to search if shift */
        /* key is depresed */
        if (!w_current->SHIFTKEY) {
          o_find_object(w_current, unsnapped_wx, unsnapped_wy, TRUE);
        }
      }

      if (!o_select_selected(w_current)) {
        /* this means the above find did not
         * find anything */
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        goto end_button_pressed;
      }

      if (w_current->ALTKEY) {
        o_copy_start(w_current, w_x, w_y);
        w_current->inside_action = 1;
        i_set_state(w_current, COPY);
      } else {
        o_move_start(w_current, w_x, w_y);
        w_current->inside_action = 1;
        i_set_state(w_current, MOVE);
      }
      break;

      case(REPEAT):
      if (w_current->last_callback != NULL) {
        (*w_current->last_callback)(w_current, 0, NULL);
      }
      break;
#ifdef HAVE_LIBSTROKE
      case(STROKE):
      DOING_STROKE=TRUE;
      break;
#endif /* HAVE_LIBSTROKE */

      case(MID_MOUSEPAN_ENABLED):
      w_current->event_state = MOUSEPAN; /* start */
      w_current->inside_action = 1;
      w_current->doing_pan = TRUE;
      start_pan_x = (int) event->x;
      start_pan_y = (int) event->y;
      throttle=0;
      break;
    }

  } else if (event->button == 3) {
    if (!w_current->inside_action) {
      if (w_current->third_button == POPUP_ENABLED) {
        i_update_menus(w_current);  /* update menus before popup  */
        do_popup(w_current, event);
      } else {
        w_current->event_state = MOUSEPAN; /* start */
        w_current->inside_action = 1;
        w_current->doing_pan = TRUE;
        start_pan_x = (int) event->x;
        start_pan_y = (int) event->y;
        throttle=0;
      }
    } else { /* this is the default cancel */
      switch (w_current->event_state) {
        case(STARTDRAWNET):
        case(DRAWNET):
        case(NETCONT):
          w_current->inside_action = 0;
          i_set_state(w_current, STARTDRAWNET);
          o_net_invalidate_rubber (w_current);
          o_net_reset(w_current);
          break;

        case(STARTDRAWBUS):
        case(DRAWBUS):
        case(BUSCONT):
          w_current->inside_action = 0;
          i_set_state(w_current, STARTDRAWBUS);
          o_bus_invalidate_rubber (w_current);
          break;

        case(DRAWPIN):
        case(ENDPIN):
          w_current->inside_action = 0;
          i_set_state(w_current, DRAWPIN);
          o_pin_invalidate_rubber (w_current);
          break;

        case(DRAWLINE):
        case(ENDLINE):
          w_current->inside_action = 0;
          i_set_state(w_current, DRAWLINE);
          o_line_invalidate_rubber (w_current);
          break;

      case DRAWPATH:
      case PATHCONT:
      case ENDPATH:
        w_current->inside_action = 0;
        i_set_state (w_current, DRAWPATH);
        o_path_invalidate_rubber (w_current);
        break;

        case(DRAWBOX):
        case(ENDBOX):
          w_current->inside_action = 0;
          i_set_state(w_current, DRAWBOX);
          o_box_invalidate_rubber (w_current);
          break;

        case(DRAWPICTURE):
        case(ENDPICTURE):
          w_current->inside_action = 0;
          i_set_state(w_current, DRAWPICTURE);
          o_picture_invalidate_rubber (w_current);
          break;

        case(DRAWCIRCLE):
        case(ENDCIRCLE):
          w_current->inside_action = 0;
          i_set_state(w_current, DRAWCIRCLE);
          o_circle_invalidate_rubber (w_current);
          break;

        case(DRAWARC):
        case(ENDARC):
          w_current->inside_action = 0;
          i_set_state(w_current, DRAWARC);
          o_arc_invalidate_rubber (w_current);
          break;

        default:
          i_callback_cancel(w_current, 0, NULL);
          break;
      }
      i_update_toolbar(w_current);
    }
  }

 end_button_pressed:
  scm_dynwind_end ();

  return(0);
}

/*! \todo Finish function documentation!!!
 *  \brief
 *  \par Function Description
 *
 */
gint
x_event_button_released (GschemPageView *page_view, GdkEventButton *event, GschemToplevel *w_current)
{
  int unsnapped_wx, unsnapped_wy;
  int w_x, w_y;

  g_return_val_if_fail ((page_view != NULL), 0);
  g_return_val_if_fail ((w_current != NULL), 0);

#if DEBUG
  printf("released! %d \n", w_current->event_state);
#endif

  w_current->SHIFTKEY   = (event->state & GDK_SHIFT_MASK  ) ? 1 : 0;
  w_current->CONTROLKEY = (event->state & GDK_CONTROL_MASK) ? 1 : 0;
  w_current->ALTKEY     = (event->state & GDK_MOD1_MASK) ? 1 : 0;

  gschem_page_view_SCREENtoWORLD (page_view, (int) event->x, (int) event->y,
                                  &unsnapped_wx, &unsnapped_wy);
  w_x = snap_grid (w_current, unsnapped_wx);
  w_y = snap_grid (w_current, unsnapped_wy);

  /* Huge switch statement to evaluate state transitions. Jump to
   * end_button_released label to escape the state evaluation rather
   * than returning from the function directly. */
  scm_dynwind_begin (0);
  g_dynwind_window (w_current);

  if (event->button == 1) {
    switch(w_current->event_state) {
      case(SELECT):
        /* do nothing */
        break;
      case(MOVE):
        w_current->event_state = ENDMOVE;
        break;

      case(COPY):
        w_current->event_state = ENDCOPY;
        break;

      case(MCOPY):
        w_current->event_state = ENDMCOPY;
        break;
      case(GRIPS):
        o_grips_end(w_current),
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;
      case(ENDMOVE):
        o_move_end(w_current);
        /* having this stay in copy was driving me nuts*/
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;

      case(ENDCOPY):
        o_copy_end(w_current);
        /* having this stay in copy was driving me nuts*/
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;

      case(ENDMCOPY):
        o_copy_multiple_end(w_current);
        /* having this stay in copy was driving me nuts*/
        w_current->inside_action = 1;
        /* Keep the state and the inside_action, as the copy has not finished. */
        i_set_state(w_current, ENDMCOPY);
        i_update_toolbar(w_current);
        o_undo_savestate_old(w_current, UNDO_ALL);
        break;

      case(SBOX):
        o_select_box_end(w_current, unsnapped_wx, unsnapped_wy);
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;

      case(ZOOMBOXEND):
        a_zoom_box_end(w_current, unsnapped_wx, unsnapped_wy);
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;

      case(STARTSELECT):
        /* first look for grips */
        if (!o_grips_start(w_current, unsnapped_wx, unsnapped_wy)) {
                                /* now go looking for objects to select */
          o_find_object(w_current, unsnapped_wx, unsnapped_wy, TRUE);
          w_current->event_state = SELECT;
          w_current->inside_action = 0;
        } else {
                                /* an grip was found */
          w_current->event_state = GRIPS;
          w_current->inside_action = 1;
        }
        break;

    case ENDPATH:
      if (o_path_end (w_current, w_x, w_y)) {
        w_current->event_state = PATHCONT;
        w_current->inside_action = TRUE;
      } else {
        w_current->event_state = DRAWPATH;
        w_current->inside_action = FALSE;
      }
      break;
    }
  } else if (event->button == 2) {

    if (w_current->inside_action) {
      if (w_current->event_state == ENDCOMP ||
          w_current->event_state == ENDTEXT ||
          w_current->event_state == ENDMOVE ||
          w_current->event_state == ENDCOPY ||
          w_current->event_state == ENDMCOPY ||
          w_current->event_state == ENDPASTE ) {

        if (w_current->event_state == ENDMOVE) {
          o_move_invalidate_rubber (w_current, FALSE);
        } else {
          o_place_invalidate_rubber (w_current, FALSE);
        }
        w_current->rubber_visible = 0;

        o_place_rotate(w_current);

        if (w_current->event_state == ENDCOMP) {
          o_complex_place_changed_run_hook (w_current);
        }

        if (w_current->event_state == ENDMOVE) {
          o_move_invalidate_rubber (w_current, TRUE);
        } else {
          o_place_invalidate_rubber (w_current, TRUE);
        }
        w_current->rubber_visible = 1;
        goto end_button_released;
      }
    }

    switch(w_current->middle_button) {
      case(ACTION):
      switch(w_current->event_state) {
        case(MOVE):
        o_move_end(w_current);
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;

        case(COPY):
        o_copy_end(w_current);
        w_current->inside_action = 0;
        i_set_state(w_current, SELECT);
        i_update_toolbar(w_current);
        break;
      }
      break;

#ifdef HAVE_LIBSTROKE
      case(STROKE):
      DOING_STROKE = FALSE;
      x_stroke_translate_and_execute (w_current);
      break;
#endif /* HAVE_LIBSTROKE */

      case(MID_MOUSEPAN_ENABLED):
      w_current->doing_pan=FALSE;
      o_invalidate_all (w_current);
      if (w_current->undo_panzoom) {
        o_undo_savestate_old(w_current, UNDO_VIEWPORT_ONLY);
      }
      /* this needs to be REDONE */
      /* if you mouse pan, you will be thrown out of the current mode. */
      /* not good */
      w_current->inside_action = 0;
      i_set_state(w_current, SELECT);
      i_update_toolbar(w_current);
      break;
    }

  } else if (event->button == 3) {
    if (w_current->doing_pan) { /* just for ending a mouse pan */
      w_current->doing_pan=FALSE;
      o_invalidate_all (w_current);

      if (w_current->undo_panzoom) {
        o_undo_savestate_old(w_current, UNDO_VIEWPORT_ONLY);
      }
      /* this needs to be REDONE */
      /* if you mouse pan, you will be thrown out of the current mode. */
      /* not good */
      w_current->inside_action = 0;
      i_set_state(w_current, SELECT);
      i_update_toolbar(w_current);
    }
  }
 end_button_released:
  scm_dynwind_end ();

  return(0);
}

/*! \todo Finish function documentation!!!
 *  \brief
 *  \par Function Description
 *
 */
gint
x_event_motion (GschemPageView *page_view, GdkEventMotion *event, GschemToplevel *w_current)
{
  int pdiff_x, pdiff_y;
  int w_x, w_y;
  int unsnapped_wx, unsnapped_wy;
  int skip_event=0;
  GdkEvent *test_event;

  g_return_val_if_fail ((w_current != NULL), 0);

  w_current->SHIFTKEY   = (event->state & GDK_SHIFT_MASK  ) ? 1 : 0;
  w_current->CONTROLKEY = (event->state & GDK_CONTROL_MASK) ? 1 : 0;
  w_current->ALTKEY     = (event->state & GDK_MOD1_MASK) ? 1 : 0;

#if DEBUG
  /*  printf("MOTION!\n");*/
#endif

#ifdef HAVE_LIBSTROKE
  if (DOING_STROKE == TRUE) {
    x_stroke_record (w_current, event->x, event->y);
    return(0);
  }
#endif /* HAVE_LIBSTROKE */

  /* skip the moving event if there are other moving events in the
     gdk event queue (Werner)
     Only skip the event if is the same event and no buttons or modifier
     keys changed*/
  if ((test_event = gdk_event_get()) != NULL) {
    if (test_event->type == GDK_MOTION_NOTIFY
        && ((GdkEventMotion *) test_event)->state == event->state) {
      skip_event= 1;
    }
    gdk_event_put(test_event); /* put it back in front of the queue */
    gdk_event_free(test_event);
    if (skip_event == 1)
      return 0;
  }

  gschem_page_view_SCREENtoWORLD (page_view, (int) event->x, (int) event->y,
                                  &unsnapped_wx, &unsnapped_wy);
  w_x = snap_grid (w_current, unsnapped_wx);
  w_y = snap_grid (w_current, unsnapped_wy);

  if (w_current->cowindow) {
    coord_display_update(w_current, (int) event->x, (int) event->y);
  }
  if (w_current->third_button == MOUSEPAN_ENABLED || w_current->middle_button == MID_MOUSEPAN_ENABLED) {
    if((w_current->event_state == MOUSEPAN) &&
       w_current->inside_action) {
         pdiff_x = (int) event->x - start_pan_x;
         pdiff_y = (int) event->y - start_pan_y;

         if (!(throttle % 5)) {
           gschem_page_view_pan_mouse(page_view,
                                      w_current,
                                      pdiff_x*w_current->mousepan_gain,
                                      pdiff_y*w_current->mousepan_gain);

           start_pan_x = (int) event->x;
           start_pan_y = (int) event->y;
         }
         throttle++;
         return(0);
       }
  }

  /* Huge switch statement to evaluate state transitions. Jump to
   * end_motion label to escape the state evaluation rather
   * than returning from the function directly. */
  scm_dynwind_begin (0);
  g_dynwind_window (w_current);

  switch(w_current->event_state) {

    case(SELECT):
    /* do nothing */
    break;

    case(GRIPS):
      o_grips_motion(w_current, w_x, w_y);
    break;

    case(STARTSELECT):
    /* If the shift or control keys are pressed, that means the user definately wants to drag out a
     * selection box.  Otherwise, if there is not a selected object under the cursor, look for one
     * that could be selected and start moving it.
     */
    if (w_current->SHIFTKEY || w_current->CONTROLKEY
            || (!o_find_selected_object(w_current, w_current->first_wx, w_current->first_wy)
                && (!o_find_object(w_current, w_current->first_wx, w_current->first_wy, TRUE)
                    || !o_select_selected(w_current)))) {
      if (o_select_box_start(w_current, unsnapped_wx, unsnapped_wy)) {
        w_current->event_state = SBOX;
        w_current->inside_action = 1;
      }
      break;
    } else {
      /* Start moving the selected object(s) */
      o_move_start(w_current, w_x, w_y);
      w_current->event_state = ENDMOVE;
      w_current->inside_action = 1;
      /* Fall through bottom of case to finish the move */
    }
    /* Fall through to handle move */
    case(ENDMOVE):
    case(MOVE):
    if (w_current->inside_action)
      o_move_motion (w_current, w_x, w_y);
    break;

    case(ENDLINE):
    if (w_current->inside_action)
      o_line_motion (w_current, w_x, w_y);
    break;

  case PATHCONT:
  case ENDPATH:
    if (w_current->inside_action)
      o_path_motion (w_current, w_x, w_y);
    break;

    case(ENDBOX):
    if (w_current->inside_action)
      o_box_motion ( w_current, w_x, w_y);
    break;

    case(ENDPICTURE):
    if (w_current->inside_action)
      o_picture_motion ( w_current, w_x, w_y);
    break;

    case(ENDCIRCLE):
    if (w_current->inside_action)
      o_circle_motion (w_current, w_x, w_y);
    break;

    case(ENDARC):
    if (w_current->inside_action)
      o_arc_motion (w_current, w_x, w_y, ARC_RADIUS);
    break;


    case(STARTDRAWNET):
    if(gschem_options_get_magnetic_net_mode (w_current->options)) {
      o_net_start_magnetic(w_current, w_x, w_y);
    }
    break;

    case(DRAWNET):
    case(NETCONT):
    if (w_current->inside_action)
      o_net_motion (w_current, w_x, w_y);
    break;

    case(DRAWBUS):
    case(BUSCONT):
    if (w_current->inside_action)
      o_bus_motion (w_current, w_x, w_y);
    break;

    case(ENDPIN):
    if (w_current->inside_action)
      o_pin_motion (w_current, w_x, w_y);
    break;

    case(COPY):
    case(MCOPY):
    case(ENDCOPY):
    case(ENDMCOPY):
    case(ENDCOMP):
    case(ENDPASTE):
    case(ENDTEXT):
    o_place_motion (w_current, w_x, w_y);
    break;

    case(SBOX):
    if (w_current->inside_action)
    o_select_box_motion (w_current, unsnapped_wx, unsnapped_wy);
    break;

    case(ZOOMBOXEND):
    if (w_current->inside_action)
      a_zoom_box_motion (w_current, unsnapped_wx, unsnapped_wy);
    break;

  }

  scm_dynwind_end ();

  return(0);
}

/*! \brief Updates the GschemToplevel and display when drawing area is configured.
 *  \par Function Description
 *  This is the callback function connected to the configure event of
 *  the drawing area of the main window.
 *
 *  It updates the size of the backingstore for the associated
 *  toplevel structure (creates a new pixmap) and re-pans each of its
 *  pages to keep their contents centered in the drawing area.
 *
 *  When the window is maximised, the zoom of every page is changed to
 *  best fit the previously displayed area of the page in the new
 *  area. Otherwise the current zoom level is left unchanged.
 *
 *  \param [in] widget    The drawing area which received the signal.
 *  \param [in] event     The event structure of signal configure-event.
 *  \param [in] user_data The toplevel environment as user data.
 *  \returns FALSE to propagate the event further.
 */
gboolean
x_event_configure (GschemPageView    *page_view,
                   GdkEventConfigure *event,
                   gpointer           user_data)
{
  GschemToplevel *w_current = GSCHEM_TOPLEVEL (user_data);
  TOPLEVEL *toplevel = gschem_page_view_get_toplevel (page_view);
  GList *iter;
  PAGE *p_current;
  gint old_win_width, old_win_height, new_win_width, new_win_height;
  gdouble relativ_zoom_factor = 1.0;

  g_assert (toplevel != NULL);

  if (gschem_page_view_get_page (page_view) == NULL) {
    /* don't want to call this if the current page isn't setup yet */
    return FALSE;
  }

  old_win_width  = w_current->win_width;
  old_win_height = w_current->win_height;
  new_win_width  = event->width;
  new_win_height = event->height;

  if (old_win_width  == new_win_width &&
      old_win_height == new_win_height) {
    /* the size of the drawing area has not changed */
    /* nothing to do here */
    return FALSE;
  }

  w_current->drawable = w_current->window;

  /* update the GschemToplevel with new size of drawing area */
  w_current->win_width   = toplevel->width  = new_win_width;
  w_current->win_height  = toplevel->height = new_win_height;


  /* in the case the user has maximised the window (hence the */
  /* configure event) fit the view by playing with zoom level */
  if (gdk_window_get_state (
        (gtk_widget_get_toplevel (
          GTK_WIDGET (page_view)))->window) & GDK_WINDOW_STATE_MAXIMIZED) {
    gdouble width_ratio, height_ratio;

    /* tweak relative_zoom to better fit page in maximized window */
    width_ratio  = ((gdouble)new_win_width)  / ((gdouble)old_win_width);
    height_ratio = ((gdouble)new_win_height) / ((gdouble)old_win_height);
    /* keep smallest ratio as relative zoom factor when panning */
    relativ_zoom_factor =
      (width_ratio < height_ratio) ? width_ratio : height_ratio;

  }

  /* save current page */
  //old_page_current = gschem_page_view_get_page (page_view);

  /* re-pan each page of the TOPLEVEL */
  for ( iter = geda_list_get_glist( toplevel->pages );
        iter != NULL;
        iter = g_list_next( iter ) ) {

    gdouble cx, cy;
    p_current = (PAGE *)iter->data;

    /* doing this the aspectratio is kept when changing (hw)*/
    cx = ((gdouble)(p_current->left + p_current->right))  / 2;
    cy = ((gdouble)(p_current->top  + p_current->bottom)) / 2;
    //s_page_goto (toplevel, p_current);
    //gschem_toplevel_page_changed (w_current);
    a_pan_general (w_current, p_current, cx, cy, relativ_zoom_factor, A_PAN_DONT_REDRAW);

  }
  /* restore current page to saved value */
  //s_page_goto (toplevel, old_page_current);
  //gschem_toplevel_page_changed (w_current);

  /* redraw the current page and update UI */
  gschem_page_view_invalidate_all (page_view);
  gschem_page_view_update_scroll_adjustments (page_view);

  return FALSE;
}

/*! \todo Finish function documentation!!!
 *  \brief
 *  \par Function Description
 *
 *  \note
 *  this is used during an open command
 *  to setup the correct sizes
 */
void x_manual_resize(GschemToplevel *w_current)
{
  TOPLEVEL *toplevel = gschem_toplevel_get_toplevel (w_current);
  PAGE *page = gschem_page_view_get_page (GSCHEM_PAGE_VIEW (w_current->drawing_area));

  /* of the actual win window (drawing_area) */
  w_current->win_width  = w_current->drawing_area->allocation.width;
  w_current->win_height = w_current->drawing_area->allocation.height;

#if DEBUG
  printf("manual: %d %d\n", w_current->win_width, w_current->win_height);
#endif

  toplevel->width = w_current->win_width;
  toplevel->height = w_current->win_height;

  /* need to do this every time you change width / height */
  set_window(toplevel,
             page,
             page->left,
             page->right,
             page->top,
             page->bottom);

#if DEBUG
  printf("Window aspect: %f\n",
         (float) w_current->win_width / (float) w_current->win_height);
  /* No longer used?
     printf("w: %d h: %d\n", width, height); */
  printf("aw: %d ah: %d\n", w_current->win_width, w_current->win_height);
#endif
}

/*! \todo Finish function documentation!!!
 *  \brief
 *  \par Function Description
 *
 */
gint x_event_enter(GtkWidget *widget, GdkEventCrossing *event,
                   GschemToplevel *w_current)
{
  g_return_val_if_fail ((w_current != NULL), 0);
  /* do nothing or now */
  return(0);
}

/*! \brief Get a snapped pointer position in world coordinates
 *
 *  \par Function Description
 *  Queries GTK for the mouse location in world coordinates,
 *  then snaps it to the grid.
 *
 * \param [in]  w_current  The GschemToplevel object.
 * \param [in]  page_view  The GschemPageView object.
 * \param [out] wx         Return location for the snapped X coordinate.
 * \param [out] wy         Return location for the snapped Y coordiante.
 */
static void
get_snapped_pointer (GschemToplevel *w_current, GschemPageView *page_view, int *wx, int *wy)
{
  int sx, sy;
  int unsnapped_wx, unsnapped_wy;

  gtk_widget_get_pointer (GTK_WIDGET (page_view), &sx, &sy);
  gschem_page_view_SCREENtoWORLD (page_view, sx, sy, &unsnapped_wx, &unsnapped_wy);
  *wx = snap_grid (w_current, unsnapped_wx);
  *wy = snap_grid (w_current, unsnapped_wy);
}

/*! \brief Callback to handle key events in the drawing area.
 *  \par Function Description
 *
 *  GTK+ callback function (registered in x_window_setup_draw_events() ) which
 *  handles key press and release events from the GTK+ system.
 *
 * \param [in] widget     the widget that generated the event
 * \param [in] event      the event itself
 * \param      w_current  the toplevel environment
 * \returns TRUE if the event has been handled.
 */
gboolean
x_event_key (GschemPageView *page_view, GdkEventKey *event, GschemToplevel *w_current)
{
  gboolean retval = FALSE;
  int wx, wy;
  int shift_key = 0;
  int control_key = 0;
  int pressed;

#if DEBUG
  printf("x_event_key_pressed: Pressed key %i.\n", event->keyval);
#endif

  /* update the state of the modifiers */
  w_current->ALTKEY     = (event->state & GDK_MOD1_MASK)    ? 1 : 0;
  w_current->SHIFTKEY   = (event->state & GDK_SHIFT_MASK)   ? 1 : 0;
  w_current->CONTROLKEY = (event->state & GDK_CONTROL_MASK) ? 1 : 0;

  pressed = (event->type == GDK_KEY_PRESS) ? 1 : 0;

  switch (event->keyval) {
    case GDK_Alt_L:
    case GDK_Alt_R:
      w_current->ALTKEY = pressed;
      break;

    case GDK_Shift_L:
    case GDK_Shift_R:
      shift_key = 1;
      w_current->SHIFTKEY = pressed;
      break;

    case GDK_Control_L:
    case GDK_Control_R:
      control_key = 1;
      w_current->CONTROLKEY = pressed;
      break;
  }


  /* Huge switch statement to evaluate state transitions. Jump to
   * end_key label to escape the state evaluation rather
   * than returning from the function directly. */
  scm_dynwind_begin (0);
  g_dynwind_window (w_current);

  switch (w_current->event_state) {
    case ENDLINE:
      if (control_key) {
        get_snapped_pointer (w_current, page_view, &wx, &wy);
        o_line_motion (w_current, wx, wy);
      }
      break;
    case STARTDRAWNET:
      if (control_key) {
        get_snapped_pointer (w_current, page_view, &wx, &wy);
        o_net_start_magnetic(w_current, wx, wy);
      }
      break;
    case DRAWNET:
    case NETCONT:
      if (shift_key || control_key) {
        get_snapped_pointer (w_current, page_view, &wx, &wy);
        o_net_motion (w_current, wx, wy);
      }
      break;
    case DRAWBUS:
    case BUSCONT:
      if (control_key) {
        get_snapped_pointer (w_current, page_view, &wx, &wy);
        o_bus_motion (w_current, wx, wy);
      }
      break;
    case ENDMOVE:
      if (control_key) {
        get_snapped_pointer (w_current, page_view, &wx, &wy);
        o_move_motion (w_current, wx, wy);
      }
      break;
    case ENDCOMP:   /* FIXME: This state shouldn't respond to modifier keys */
    case ENDPASTE:  /* FIXME: This state shouldn't respond to modifier keys */
    case ENDTEXT:   /* FIXME: This state shouldn't respond to modifier keys */
    case ENDCOPY:
    case ENDMCOPY:
      if (control_key) {
        get_snapped_pointer (w_current, page_view, &wx, &wy);
        o_place_motion (w_current, wx, wy);
      }
      break;
  }

  if (pressed)
    retval = g_keys_execute (w_current, event) ? TRUE : FALSE;

  scm_dynwind_end ();

  return retval;
}


/*! \todo Finish function documentation!!!
 *  \brief
 *  \par Function Description
 *
 */
gint x_event_scroll (GtkWidget *widget, GdkEventScroll *event,
                     GschemToplevel *w_current)
{
  GtkAdjustment *adj;
  gboolean pan_xaxis = FALSE;
  gboolean pan_yaxis = FALSE;
  gboolean zoom = FALSE;
  int pan_direction = 1;
  int zoom_direction = ZOOM_IN;

  g_return_val_if_fail ((w_current != NULL), 0);

  /* update the state of the modifiers */
  w_current->SHIFTKEY   = (event->state & GDK_SHIFT_MASK  ) ? 1 : 0;
  w_current->CONTROLKEY = (event->state & GDK_CONTROL_MASK) ? 1 : 0;
  w_current->ALTKEY     = (event->state & GDK_MOD1_MASK) ? 1 : 0;

  if (w_current->scroll_wheel == SCROLL_WHEEL_CLASSIC) {
    /* Classic gschem behaviour */
    zoom =      !w_current->CONTROLKEY && !w_current->SHIFTKEY;
    pan_yaxis = !w_current->CONTROLKEY &&  w_current->SHIFTKEY;
    pan_xaxis =  w_current->CONTROLKEY && !w_current->SHIFTKEY;
  } else {
    /* GTK style behaviour */
    zoom =       w_current->CONTROLKEY && !w_current->SHIFTKEY;
    pan_yaxis = !w_current->CONTROLKEY && !w_current->SHIFTKEY;
    pan_xaxis = !w_current->CONTROLKEY &&  w_current->SHIFTKEY;
  }

  /* If the user has a left/right scroll wheel, always scroll the y-axis */
  if (event->direction == GDK_SCROLL_LEFT ||
      event->direction == GDK_SCROLL_RIGHT) {
    zoom = FALSE;
    pan_yaxis = FALSE;
    pan_xaxis = TRUE;
  }

  /* You must have scrollbars enabled if you want to use the scroll wheel to pan */
  if (!w_current->scrollbars_flag) {
    pan_xaxis = FALSE;
    pan_yaxis = FALSE;
  }

  switch (event->direction) {
    case GDK_SCROLL_UP:
    case GDK_SCROLL_LEFT:
      pan_direction = -1;
      zoom_direction = ZOOM_IN;
      break;
    case GDK_SCROLL_DOWN:
    case GDK_SCROLL_RIGHT:
      pan_direction =  1;
      zoom_direction = ZOOM_OUT;
      break;
  }

  if (zoom) {
    /*! \todo Change "HOTKEY" TO new "MOUSE" specifier? */
    a_zoom(w_current, GSCHEM_PAGE_VIEW (widget), zoom_direction, HOTKEY, 0);
  }

  if (pan_xaxis) {
    adj = gschem_page_view_get_hadjustment (GSCHEM_PAGE_VIEW (w_current->drawing_area));
    g_return_val_if_fail (adj != NULL, TRUE);
    gtk_adjustment_set_value(adj, min(adj->value + pan_direction *
                                        (adj->page_increment /
                                         w_current->scrollpan_steps),
                                      adj->upper - adj->page_size));
  }

  if (pan_yaxis) {
    adj = gschem_page_view_get_vadjustment (GSCHEM_PAGE_VIEW (w_current->drawing_area));
    g_return_val_if_fail (adj != NULL, TRUE);
    gtk_adjustment_set_value(adj, min(adj->value + pan_direction *
                                        (adj->page_increment /
                                         w_current->scrollpan_steps),
                                      adj->upper - adj->page_size));
  }

  if (w_current->undo_panzoom && (zoom || pan_xaxis || pan_yaxis)) {
    o_undo_savestate_old(w_current, UNDO_VIEWPORT_ONLY);
  }

  /* Stop further processing of this signal */
  return TRUE;
}


/*! \brief get the pointer position of a given GschemToplevel
 *  \par Function Description
 *  This function gets the pointer position of the drawing area of the
 *  current workspace <b>GschemToplevel</b>. The flag <b>snapped</b> specifies
 *  whether the pointer position should be snapped to the current grid.
 *
 *  \param [in] w_current  The GschemToplevel object.
 *  \param [in] snapped    An option flag to specify the wished coords
 *  \param [out] wx        snapped/unsnapped world x coordinate
 *  \param [out] wy        snapped/unsnapped world y coordinate
 *
 *  \return Returns TRUE if the pointer position is inside the drawing area.
 *
 */
gboolean
x_event_get_pointer_position (GschemToplevel *w_current, gboolean snapped, gint *wx, gint *wy)
{
  GschemPageView *page_view = gschem_toplevel_get_current_page_view (w_current);
  int width;
  int height;
  int sx;
  int sy;
  int x;
  int y;

  g_return_val_if_fail (page_view != NULL, FALSE);
  g_return_val_if_fail (GTK_WIDGET (page_view)->window != NULL, FALSE);

  /* \todo The following line is depricated in GDK 2.24 */
  width = gdk_window_get_width(GTK_WIDGET(page_view)->window);
  height = gdk_window_get_height(GTK_WIDGET(page_view)->window);

  gtk_widget_get_pointer(GTK_WIDGET (page_view), &sx, &sy);

  /* check if we are inside the drawing area */
  if ((sx < 0) || (sx >= width) || (sy < 0) || (sy >= height)) {
    return FALSE;
  }

  gschem_page_view_SCREENtoWORLD (page_view, sx, sy, &x, &y);

  if (snapped) {
    x = snap_grid (w_current, x);
    y = snap_grid (w_current, y);
  }

  *wx = x;
  *wy = y;

  return TRUE;
}
