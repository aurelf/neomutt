/**
 * @file
 * GUI present the user with a selectable list
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page menu_menu GUI present the user with a selectable list
 *
 * GUI present the user with a selectable list
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "commands.h"
#include "context.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"

char *SearchBuffers[MENU_MAX];

#define DIRECTION ((neg * 2) + 1)

#define MUTT_SEARCH_UP 1
#define MUTT_SEARCH_DOWN 2

/**
 * get_color - Choose a colour for a line of the index
 * @param index Index number
 * @param s     String of embedded colour codes
 * @retval num Colour pair in an integer
 *
 * Text is coloured by inserting special characters into the string, e.g.
 * #MT_COLOR_INDEX_AUTHOR
 */
static int get_color(int index, unsigned char *s)
{
  struct ColorLineList *color = NULL;
  struct ColorLine *np = NULL;
  struct Email *e = mutt_get_virt_email(Context->mailbox, index);
  int type = *s;

  switch (type)
  {
    case MT_COLOR_INDEX_AUTHOR:
      color = mutt_color_index_author();
      break;
    case MT_COLOR_INDEX_FLAGS:
      color = mutt_color_index_flags();
      break;
    case MT_COLOR_INDEX_SUBJECT:
      color = mutt_color_index_subject();
      break;
    case MT_COLOR_INDEX_TAG:
      STAILQ_FOREACH(np, mutt_color_index_tags(), entries)
      {
        if (mutt_strn_equal((const char *) (s + 1), np->pattern, strlen(np->pattern)))
          return np->pair;
        const char *transform = mutt_hash_find(TagTransforms, np->pattern);
        if (transform && mutt_strn_equal((const char *) (s + 1), transform, strlen(transform)))
        {
          return np->pair;
        }
      }
      return 0;
    default:
      return mutt_color(type);
  }

  STAILQ_FOREACH(np, color, entries)
  {
    if (mutt_pattern_exec(SLIST_FIRST(np->color_pattern),
                          MUTT_MATCH_FULL_ADDRESS, Context->mailbox, e, NULL))
      return np->pair;
  }

  return 0;
}

/**
 * print_enriched_string - Display a string with embedded colours and graphics
 * @param index    Index number
 * @param attr     Default colour for the line
 * @param s        String of embedded colour codes
 * @param do_color If true, apply colour
 */
static void print_enriched_string(int index, int attr, unsigned char *s, bool do_color)
{
  wchar_t wc;
  size_t k;
  size_t n = mutt_str_len((char *) s);
  mbstate_t mbstate;

  memset(&mbstate, 0, sizeof(mbstate));
  while (*s)
  {
    if (*s < MUTT_TREE_MAX)
    {
      if (do_color)
#if defined(HAVE_COLOR) && defined(HAVE_USE_DEFAULT_COLORS)
        /* Combining tree fg color and another bg color requires having
         * use_default_colors, because the other bg color may be undefined. */
        mutt_curses_set_attr(mutt_color_combine(mutt_color(MT_COLOR_TREE), attr));
#else
        mutt_curses_set_color(MT_COLOR_TREE);
#endif

      const bool c_ascii_chars = cs_subset_bool(NeoMutt->sub, "ascii_chars");
      while (*s && (*s < MUTT_TREE_MAX))
      {
        switch (*s)
        {
          case MUTT_TREE_LLCORNER:
            if (c_ascii_chars)
              mutt_window_addch('`');
#ifdef WACS_LLCORNER
            else
              add_wch(WACS_LLCORNER);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\224"); /* WACS_LLCORNER */
            else
              mutt_window_addch(ACS_LLCORNER);
#endif
            break;
          case MUTT_TREE_ULCORNER:
            if (c_ascii_chars)
              mutt_window_addch(',');
#ifdef WACS_ULCORNER
            else
              add_wch(WACS_ULCORNER);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\214"); /* WACS_ULCORNER */
            else
              mutt_window_addch(ACS_ULCORNER);
#endif
            break;
          case MUTT_TREE_LTEE:
            if (c_ascii_chars)
              mutt_window_addch('|');
#ifdef WACS_LTEE
            else
              add_wch(WACS_LTEE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\234"); /* WACS_LTEE */
            else
              mutt_window_addch(ACS_LTEE);
#endif
            break;
          case MUTT_TREE_HLINE:
            if (c_ascii_chars)
              mutt_window_addch('-');
#ifdef WACS_HLINE
            else
              add_wch(WACS_HLINE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\200"); /* WACS_HLINE */
            else
              mutt_window_addch(ACS_HLINE);
#endif
            break;
          case MUTT_TREE_VLINE:
            if (c_ascii_chars)
              mutt_window_addch('|');
#ifdef WACS_VLINE
            else
              add_wch(WACS_VLINE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\202"); /* WACS_VLINE */
            else
              mutt_window_addch(ACS_VLINE);
#endif
            break;
          case MUTT_TREE_TTEE:
            if (c_ascii_chars)
              mutt_window_addch('-');
#ifdef WACS_TTEE
            else
              add_wch(WACS_TTEE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\254"); /* WACS_TTEE */
            else
              mutt_window_addch(ACS_TTEE);
#endif
            break;
          case MUTT_TREE_BTEE:
            if (c_ascii_chars)
              mutt_window_addch('-');
#ifdef WACS_BTEE
            else
              add_wch(WACS_BTEE);
#else
            else if (CharsetIsUtf8)
              mutt_window_addstr("\342\224\264"); /* WACS_BTEE */
            else
              mutt_window_addch(ACS_BTEE);
#endif
            break;
          case MUTT_TREE_SPACE:
            mutt_window_addch(' ');
            break;
          case MUTT_TREE_RARROW:
            mutt_window_addch('>');
            break;
          case MUTT_TREE_STAR:
            mutt_window_addch('*'); /* fake thread indicator */
            break;
          case MUTT_TREE_HIDDEN:
            mutt_window_addch('&');
            break;
          case MUTT_TREE_EQUALS:
            mutt_window_addch('=');
            break;
          case MUTT_TREE_MISSING:
            mutt_window_addch('?');
            break;
        }
        s++;
        n--;
      }
      if (do_color)
        mutt_curses_set_attr(attr);
    }
    else if (*s == MUTT_SPECIAL_INDEX)
    {
      s++;
      if (do_color)
      {
        if (*s == MT_COLOR_INDEX)
        {
          attrset(attr);
        }
        else
        {
          if (get_color(index, s) == 0)
          {
            attron(attr);
          }
          else
          {
            attron(get_color(index, s));
          }
        }
      }
      s++;
      n -= 2;
    }
    else if ((k = mbrtowc(&wc, (char *) s, n, &mbstate)) > 0)
    {
      mutt_window_addnstr((char *) s, k);
      s += k;
      n -= k;
    }
    else
      break;
  }
}

/**
 * make_entry - Create string to display in a Menu (the index)
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param menu Current Menu
 * @param i    Selected item
 */
static void make_entry(struct Menu *menu, char *buf, size_t buflen, int i)
{
  if (!ARRAY_EMPTY(&menu->dialog))
  {
    mutt_str_copy(buf, NONULL(*ARRAY_GET(&menu->dialog, i)), buflen);
    menu_set_index(menu, -1); /* hide menubar */
  }
  else
    menu->make_entry(menu, buf, buflen, i);
}

/**
 * menu_pad_string - Pad a string with spaces for display in the Menu
 * @param menu   Current Menu
 * @param buf    Buffer containing the string
 * @param buflen Length of the buffer
 *
 * @note The string is padded in-place.
 */
static void menu_pad_string(struct Menu *menu, char *buf, size_t buflen)
{
  char *scratch = mutt_str_dup(buf);
  const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
  const char *const c_arrow_string =
      cs_subset_string(NeoMutt->sub, "arrow_string");
  int shift = c_arrow_cursor ? mutt_strwidth(c_arrow_string) + 1 : 0;
  int cols = menu->win_index->state.cols - shift;

  mutt_simple_format(buf, buflen, cols, cols, JUSTIFY_LEFT, ' ', scratch,
                     mutt_str_len(scratch), true);
  buf[buflen - 1] = '\0';
  FREE(&scratch);
}

/**
 * menu_redraw_full - Force the redraw of the Menu
 * @param menu Current Menu
 */
void menu_redraw_full(struct Menu *menu)
{
  mutt_curses_set_color(MT_COLOR_NORMAL);
  mutt_window_clear(menu->win_index);

  window_redraw(RootWindow, true);
  menu->pagelen = menu->win_index->state.rows;

  mutt_show_error();

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
}

/**
 * menu_redraw_status - Force the redraw of the status bar
 * @param menu Current Menu
 */
void menu_redraw_status(struct Menu *menu)
{
  if (!menu || !menu->win_ibar)
    return;

  char buf[256];

  snprintf(buf, sizeof(buf), "-- NeoMutt: %s", menu->title);
  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_move(menu->win_ibar, 0, 0);
  mutt_paddstr(menu->win_ibar->state.cols, buf);
  mutt_curses_set_color(MT_COLOR_NORMAL);
  menu->redraw &= ~REDRAW_STATUS;
}

/**
 * menu_redraw_index - Force the redraw of the index
 * @param menu Current Menu
 */
void menu_redraw_index(struct Menu *menu)
{
  char buf[1024];
  bool do_color;
  int attr;

  for (int i = menu->top; i < (menu->top + menu->pagelen); i++)
  {
    if (i < menu->max)
    {
      attr = menu->color(menu, i);

      make_entry(menu, buf, sizeof(buf), i);
      menu_pad_string(menu, buf, sizeof(buf));

      mutt_curses_set_attr(attr);
      mutt_window_move(menu->win_index, 0, i - menu->top);
      do_color = true;

      const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
      const char *const c_arrow_string =
          cs_subset_string(NeoMutt->sub, "arrow_string");
      if (i == menu->current)
      {
        mutt_curses_set_color(MT_COLOR_INDICATOR);
        if (c_arrow_cursor)
        {
          mutt_window_addstr(c_arrow_string);
          mutt_curses_set_attr(attr);
          mutt_window_addch(' ');
        }
        else
          do_color = false;
      }
      else if (c_arrow_cursor)
      {
        /* Print space chars to match the screen width of `$arrow_string` */
        mutt_window_printf("%*s", mutt_strwidth(c_arrow_string) + 1, "");
      }

      print_enriched_string(i, attr, (unsigned char *) buf, do_color);
    }
    else
    {
      mutt_curses_set_color(MT_COLOR_NORMAL);
      mutt_window_clearline(menu->win_index, i - menu->top);
    }
  }
  mutt_curses_set_color(MT_COLOR_NORMAL);
  menu->redraw = REDRAW_NO_FLAGS;

  notify_send(menu->notify, NT_MENU, 0, NULL);
}

/**
 * menu_redraw_motion - Force the redraw of the list part of the menu
 * @param menu Current Menu
 */
void menu_redraw_motion(struct Menu *menu)
{
  char buf[1024];

  if (!ARRAY_EMPTY(&menu->dialog))
  {
    menu->redraw &= ~REDRAW_MOTION;
    return;
  }

  /* Note: menu->color() for the index can end up retrieving a message
   * over imap (if matching against ~h for instance).  This can
   * generate status messages.  So we want to call it *before* we
   * position the cursor for drawing. */
  const int old_color = menu->color(menu, menu->oldcurrent);
  mutt_window_move(menu->win_index, 0, menu->oldcurrent - menu->top);
  mutt_curses_set_attr(old_color);

  const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
  const char *const c_arrow_string =
      cs_subset_string(NeoMutt->sub, "arrow_string");
  if (c_arrow_cursor)
  {
    /* clear the arrow */
    /* Print space chars to match the screen width of `$arrow_string` */
    mutt_window_printf("%*s", mutt_strwidth(c_arrow_string) + 1, "");

    make_entry(menu, buf, sizeof(buf), menu->oldcurrent);
    menu_pad_string(menu, buf, sizeof(buf));
    mutt_window_move(menu->win_index, mutt_strwidth(c_arrow_string) + 1,
                     menu->oldcurrent - menu->top);
    print_enriched_string(menu->oldcurrent, old_color, (unsigned char *) buf, true);

    /* now draw it in the new location */
    mutt_curses_set_color(MT_COLOR_INDICATOR);
    mutt_window_mvaddstr(menu->win_index, 0, menu->current - menu->top, c_arrow_string);
  }
  else
  {
    /* erase the current indicator */
    make_entry(menu, buf, sizeof(buf), menu->oldcurrent);
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->oldcurrent, old_color, (unsigned char *) buf, true);

    /* now draw the new one to reflect the change */
    const int cur_color = menu->color(menu, menu->current);
    make_entry(menu, buf, sizeof(buf), menu->current);
    menu_pad_string(menu, buf, sizeof(buf));
    mutt_curses_set_color(MT_COLOR_INDICATOR);
    mutt_window_move(menu->win_index, 0, menu->current - menu->top);
    print_enriched_string(menu->current, cur_color, (unsigned char *) buf, false);
  }
  menu->redraw &= REDRAW_STATUS;
  mutt_curses_set_color(MT_COLOR_NORMAL);

  notify_send(menu->notify, NT_MENU, 0, NULL);
}

/**
 * menu_redraw_current - Redraw the current menu
 * @param menu Current Menu
 */
void menu_redraw_current(struct Menu *menu)
{
  char buf[1024];
  int attr = menu->color(menu, menu->current);

  mutt_window_move(menu->win_index, 0, menu->current - menu->top);
  make_entry(menu, buf, sizeof(buf), menu->current);
  menu_pad_string(menu, buf, sizeof(buf));

  mutt_curses_set_color(MT_COLOR_INDICATOR);
  const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
  const char *const c_arrow_string =
      cs_subset_string(NeoMutt->sub, "arrow_string");
  if (c_arrow_cursor)
  {
    mutt_window_addstr(c_arrow_string);
    mutt_curses_set_attr(attr);
    mutt_window_addch(' ');
    menu_pad_string(menu, buf, sizeof(buf));
    print_enriched_string(menu->current, attr, (unsigned char *) buf, true);
  }
  else
    print_enriched_string(menu->current, attr, (unsigned char *) buf, false);
  menu->redraw &= REDRAW_STATUS;
  mutt_curses_set_color(MT_COLOR_NORMAL);

  notify_send(menu->notify, NT_MENU, 0, NULL);
}

/**
 * menu_redraw_prompt - Force the redraw of the message window
 * @param menu Current Menu
 */
static void menu_redraw_prompt(struct Menu *menu)
{
  if (!menu || ARRAY_EMPTY(&menu->dialog))
    return;

  if (OptMsgErr)
  {
    mutt_sleep(1);
    OptMsgErr = false;
  }

  if (ErrorBufMessage)
    mutt_clear_error();

  mutt_window_mvaddstr(MessageWindow, 0, 0, menu->prompt);
  mutt_window_clrtoeol(MessageWindow);

  notify_send(menu->notify, NT_MENU, 0, NULL);
}

/**
 * menu_check_recenter - Recentre the menu on screen
 * @param menu Current Menu
 */
void menu_check_recenter(struct Menu *menu)
{
  const short c_menu_context = cs_subset_number(NeoMutt->sub, "menu_context");
  const bool c_menu_move_off = cs_subset_bool(NeoMutt->sub, "menu_move_off");
  const bool c_menu_scroll = cs_subset_bool(NeoMutt->sub, "menu_scroll");

  int c = MIN(c_menu_context, (menu->pagelen / 2));
  int old_top = menu->top;

  if (!c_menu_move_off && (menu->max <= menu->pagelen)) /* less entries than lines */
  {
    if (menu->top != 0)
    {
      menu->top = 0;
      menu->redraw |= REDRAW_INDEX;
    }
  }
  else
  {
    if (c_menu_scroll || (menu->pagelen <= 0) || (c < c_menu_context))
    {
      if (menu->current < (menu->top + c))
        menu->top = menu->current - c;
      else if (menu->current >= (menu->top + menu->pagelen - c))
        menu->top = menu->current - menu->pagelen + c + 1;
    }
    else
    {
      if (menu->current < menu->top + c)
      {
        menu->top -= (menu->pagelen - c) * ((menu->top + menu->pagelen - 1 - menu->current) /
                                            (menu->pagelen - c)) -
                     c;
      }
      else if ((menu->current >= (menu->top + menu->pagelen - c)))
      {
        menu->top +=
            (menu->pagelen - c) * ((menu->current - menu->top) / (menu->pagelen - c)) - c;
      }
    }
  }

  if (!c_menu_move_off) /* make entries stick to bottom */
    menu->top = MIN(menu->top, menu->max - menu->pagelen);
  menu->top = MAX(menu->top, 0);

  if (menu->top != old_top)
    menu->redraw |= REDRAW_INDEX;
}

/**
 * menu_jump - Jump to another item in the menu
 * @param menu Current Menu
 *
 * Ask the user for a message number to jump to.
 */
static void menu_jump(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  mutt_unget_event(LastKey, 0);
  char buf[128] = { 0 };
  if ((mutt_get_field(_("Jump to: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS,
                      false, NULL, NULL) == 0) &&
      (buf[0] != '\0'))
  {
    int n = 0;
    if ((mutt_str_atoi(buf, &n) == 0) && (n > 0) && (n < (menu->max + 1)))
    {
      menu_set_index(menu, n - 1); // msg numbers are 0-based
    }
    else
      mutt_error(_("Invalid index number"));
  }
}

/**
 * menu_next_line - Move the view down one line, keeping the selection the same
 * @param menu Current Menu
 */
void menu_next_line(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  const short c_menu_context = cs_subset_number(NeoMutt->sub, "menu_context");
  const bool c_menu_move_off = cs_subset_bool(NeoMutt->sub, "menu_move_off");
  int c = MIN(c_menu_context, (menu->pagelen / 2));

  if (((menu->top + 1) < (menu->max - c)) &&
      (c_menu_move_off ||
       ((menu->max > menu->pagelen) && (menu->top < (menu->max - menu->pagelen)))))
  {
    menu->top++;
    if ((menu->current < (menu->top + c)) && (menu->current < (menu->max - 1)))
      menu_set_index(menu, menu->current + 1);
    menu->redraw = REDRAW_INDEX;
  }
  else
    mutt_message(_("You can't scroll down farther"));
}

/**
 * menu_prev_line - Move the view up one line, keeping the selection the same
 * @param menu Current Menu
 */
void menu_prev_line(struct Menu *menu)
{
  if (menu->top < 1)
  {
    mutt_message(_("You can't scroll up farther"));
    return;
  }

  const short c_menu_context = cs_subset_number(NeoMutt->sub, "menu_context");
  int c = MIN(c_menu_context, (menu->pagelen / 2));

  menu->top--;
  if ((menu->current >= (menu->top + menu->pagelen - c)) && (menu->current > 1))
    menu_set_index(menu, menu->current - 1);
  menu->redraw = REDRAW_INDEX;
}

/**
 * menu_length_jump - Calculate the destination of a jump
 * @param menu    Current Menu
 * @param jumplen Number of lines to jump
 *
 * * pageup:   jumplen == -pagelen
 * * pagedown: jumplen == pagelen
 * * halfup:   jumplen == -pagelen/2
 * * halfdown: jumplen == pagelen/2
 */
static void menu_length_jump(struct Menu *menu, int jumplen)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  const short c_menu_context = cs_subset_number(NeoMutt->sub, "menu_context");
  const bool c_menu_move_off = cs_subset_bool(NeoMutt->sub, "menu_move_off");

  const int neg = (jumplen >= 0) ? 0 : -1;
  const int c = MIN(c_menu_context, (menu->pagelen / 2));

  /* possible to scroll? */
  int tmp;
  int index = menu->current;
  if ((DIRECTION * menu->top) <
      (tmp = (neg ? 0 : (menu->max /* -1 */) - (menu->pagelen /* -1 */))))
  {
    menu->top += jumplen;

    /* jumped too long? */
    if ((neg || !c_menu_move_off) && ((DIRECTION * menu->top) > tmp))
      menu->top = tmp;

    /* need to move the cursor? */
    if ((DIRECTION *
         (tmp = (menu->current - (menu->top + (neg ? (menu->pagelen - 1) - c : c))))) < 0)
    {
      index -= tmp;
    }

    menu->redraw = REDRAW_INDEX;
  }
  else if ((menu->current != (neg ? 0 : menu->max - 1)) && ARRAY_EMPTY(&menu->dialog))
  {
    index += jumplen;
  }
  else
  {
    mutt_message(neg ? _("You are on the first page") : _("You are on the last page"));
  }

  // Range check
  index = MIN(index, menu->max - 1);
  index = MAX(index, 0);
  menu_set_index(menu, index);
}

/**
 * menu_next_page - Move the focus to the next page in the menu
 * @param menu Current Menu
 */
void menu_next_page(struct Menu *menu)
{
  menu_length_jump(menu, MAX(menu->pagelen /* - MenuOverlap */, 0));
}

/**
 * menu_prev_page - Move the focus to the previous page in the menu
 * @param menu Current Menu
 */
void menu_prev_page(struct Menu *menu)
{
  menu_length_jump(menu, 0 - MAX(menu->pagelen /* - MenuOverlap */, 0));
}

/**
 * menu_half_down - Move the focus down half a page in the menu
 * @param menu Current Menu
 */
void menu_half_down(struct Menu *menu)
{
  menu_length_jump(menu, (menu->pagelen / 2));
}

/**
 * menu_half_up - Move the focus up half a page in the menu
 * @param menu Current Menu
 */
void menu_half_up(struct Menu *menu)
{
  menu_length_jump(menu, 0 - (menu->pagelen / 2));
}

/**
 * menu_top_page - Move the focus to the top of the page
 * @param menu Current Menu
 */
void menu_top_page(struct Menu *menu)
{
  if (menu->current == menu->top)
    return;

  menu_set_index(menu, menu->top);
}

/**
 * menu_bottom_page - Move the focus to the bottom of the page
 * @param menu Current Menu
 */
void menu_bottom_page(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  int index = menu->top + menu->pagelen - 1;
  if (index > (menu->max - 1))
    index = menu->max - 1;
  menu_set_index(menu, index);
}

/**
 * menu_middle_page - Move the focus to the centre of the page
 * @param menu Current Menu
 */
void menu_middle_page(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  int i = menu->top + menu->pagelen;
  if (i > (menu->max - 1))
    i = menu->max - 1;

  menu_set_index(menu, menu->top + (i - menu->top) / 2);
}

/**
 * menu_first_entry - Move the focus to the first entry in the menu
 * @param menu Current Menu
 */
void menu_first_entry(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu_set_index(menu, 0);
}

/**
 * menu_last_entry - Move the focus to the last entry in the menu
 * @param menu Current Menu
 */
void menu_last_entry(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu_set_index(menu, menu->max - 1);
}

/**
 * menu_current_top - Move the current selection to the top of the window
 * @param menu Current Menu
 */
void menu_current_top(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu->top = menu->current;
  menu->redraw = REDRAW_INDEX;
}

/**
 * menu_current_middle - Move the current selection to the centre of the window
 * @param menu Current Menu
 */
void menu_current_middle(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu->top = menu->current - (menu->pagelen / 2);
  if (menu->top < 0)
    menu->top = 0;
  menu->redraw = REDRAW_INDEX;
}

/**
 * menu_current_bottom - Move the current selection to the bottom of the window
 * @param menu Current Menu
 */
void menu_current_bottom(struct Menu *menu)
{
  if (menu->max == 0)
  {
    mutt_error(_("No entries"));
    return;
  }

  menu->top = menu->current - menu->pagelen + 1;
  if (menu->top < 0)
    menu->top = 0;
  menu->redraw = REDRAW_INDEX;
}

/**
 * menu_next_entry - Move the focus to the next item in the menu
 * @param menu Current Menu
 */
static void menu_next_entry(struct Menu *menu)
{
  if (menu->current < (menu->max - 1))
  {
    menu_set_index(menu, menu->current + 1);
  }
  else
    mutt_message(_("You are on the last entry"));
}

/**
 * menu_prev_entry - Move the focus to the previous item in the menu
 * @param menu Current Menu
 */
static void menu_prev_entry(struct Menu *menu)
{
  if (menu->current)
  {
    menu_set_index(menu, menu->current - 1);
  }
  else
    mutt_message(_("You are on the first entry"));
}

/**
 * default_color - Get the default colour for a line of the menu - Implements Menu::color()
 */
static int default_color(struct Menu *menu, int line)
{
  return mutt_color(MT_COLOR_NORMAL);
}

/**
 * generic_search - Search a menu for a item matching a regex - Implements Menu::search()
 */
static int generic_search(struct Menu *menu, regex_t *rx, int line)
{
  char buf[1024];

  make_entry(menu, buf, sizeof(buf), line);
  return regexec(rx, buf, 0, NULL, 0);
}

/**
 * menu_init - Initialise all the Menus
 */
void menu_init(void)
{
  for (int i = 0; i < MENU_MAX; i++)
    SearchBuffers[i] = NULL;
}

/**
 * menu_color_observer - Listen for colour changes affecting the menu - Implements ::observer_t
 */
static int menu_color_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_COLOR)
    return 0;

  struct EventColor *ev_c = nc->event_data;

  int c = ev_c->color;

  // MT_COLOR_MAX is sent on `uncolor *`
  bool simple = (c == MT_COLOR_INDEX_COLLAPSED) || (c == MT_COLOR_INDEX_DATE) ||
                (c == MT_COLOR_INDEX_LABEL) || (c == MT_COLOR_INDEX_NUMBER) ||
                (c == MT_COLOR_INDEX_SIZE) || (c == MT_COLOR_INDEX_TAGS) ||
                (c == MT_COLOR_MAX);
  bool lists = (c == MT_COLOR_ATTACH_HEADERS) || (c == MT_COLOR_BODY) ||
               (c == MT_COLOR_HEADER) || (c == MT_COLOR_INDEX) ||
               (c == MT_COLOR_INDEX_AUTHOR) || (c == MT_COLOR_INDEX_FLAGS) ||
               (c == MT_COLOR_INDEX_SUBJECT) || (c == MT_COLOR_INDEX_TAG) ||
               (c == MT_COLOR_MAX);

  // The changes aren't relevant to the index menu
  if (!simple && !lists)
    return 0;

  // Colour deleted from a list
  struct Mailbox *m = ctx_mailbox(Context);
  if ((nc->event_subtype == NT_COLOR_RESET) && lists && m)
  {
    // Force re-caching of index colors
    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;
      e->pair = 0;
    }
  }

  struct Menu *menu = nc->global_data;
  menu->redraw = REDRAW_FULL;

  return 0;
}

/**
 * menu_config_observer - Listen for config changes affecting the menu - Implements ::observer_t
 */
static int menu_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;

  const struct ConfigDef *cdef = ec->he->data;
  ConfigRedrawFlags flags = cdef->type & R_REDRAW_MASK;

  if (flags == R_REDRAW_NO_FLAGS)
    return 0;

  struct Menu *menu = nc->global_data;
  if ((menu->type == MENU_MAIN) && (flags & R_INDEX))
    menu->redraw |= REDRAW_FULL;
  if ((menu->type == MENU_PAGER) && (flags & R_PAGER))
    menu->redraw |= REDRAW_FULL;
  if (flags & R_PAGER_FLOW)
  {
    menu->redraw |= REDRAW_FULL | REDRAW_FLOW;
  }

  if (flags & R_RESORT_SUB)
    OptSortSubthreads = true;
  if (flags & R_RESORT)
    OptNeedResort = true;
  if (flags & R_RESORT_INIT)
    OptResortInit = true;
  if (flags & R_TREE)
    OptRedrawTree = true;

  if (flags & R_MENU)
    menu->redraw |= REDRAW_FULL;

  return 0;
}

/**
 * menu_recalc - Recalculate the Window data - Implements MuttWindow::recalc()
 */
static int menu_recalc(struct MuttWindow *win)
{
  if (win->type != WT_MENU)
    return 0;

  // struct Menu *menu = win->wdata;

  win->actions |= WA_REPAINT;
  return 0;
}

/**
 * menu_repaint - Repaint the Window - Implements MuttWindow::repaint()
 */
static int menu_repaint(struct MuttWindow *win)
{
  if (win->type != WT_MENU)
    return 0;

  // struct Menu *menu = win->wdata;
  // menu_redraw(menu);
  // menu->redraw = REDRAW_NO_FLAGS;

  return 0;
}

/**
 * menu_window_observer - Listen for Window changes affecting the menu - Implements ::observer_t
 */
static int menu_window_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (nc->event_subtype != NT_WINDOW_STATE)
    return 0;

  struct Menu *menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  struct MuttWindow *win = ev_w->win;

  menu->pagelen = win->state.rows;
  menu->redraw = REDRAW_FULL;

  return 0;
}

/**
 * menu_add_dialog_row - Add a row to a Menu
 * @param menu Menu to add to
 * @param row  Row of text to add
 */
void menu_add_dialog_row(struct Menu *menu, const char *row)
{
  ARRAY_SET(&menu->dialog, menu->max, mutt_str_dup(row));
  menu->max++;
}

/**
 * search - Search a menu
 * @param menu Menu to search
 * @param op   Search operation, e.g. OP_SEARCH_NEXT
 * @retval >=0 Index of matching item
 * @retval -1  Search failed, or was cancelled
 */
static int search(struct Menu *menu, int op)
{
  int rc = 0, wrap = 0;
  int search_dir;
  regex_t re;
  char buf[128];
  char *search_buf = ((menu->type < MENU_MAX)) ? SearchBuffers[menu->type] : NULL;

  if (!(search_buf && *search_buf) || ((op != OP_SEARCH_NEXT) && (op != OP_SEARCH_OPPOSITE)))
  {
    mutt_str_copy(buf, search_buf && (search_buf[0] != '\0') ? search_buf : "",
                  sizeof(buf));
    if ((mutt_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ?
                            _("Search for: ") :
                            _("Reverse search for: "),
                        buf, sizeof(buf), MUTT_CLEAR, false, NULL, NULL) != 0) ||
        (buf[0] == '\0'))
    {
      return -1;
    }
    if (menu->type < MENU_MAX)
    {
      mutt_str_replace(&SearchBuffers[menu->type], buf);
      search_buf = SearchBuffers[menu->type];
    }
    menu->search_dir =
        ((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ? MUTT_SEARCH_DOWN : MUTT_SEARCH_UP;
  }

  search_dir = (menu->search_dir == MUTT_SEARCH_UP) ? -1 : 1;
  if (op == OP_SEARCH_OPPOSITE)
    search_dir = -search_dir;

  if (search_buf)
  {
    uint16_t flags = mutt_mb_is_lower(search_buf) ? REG_ICASE : 0;
    rc = REG_COMP(&re, search_buf, REG_NOSUB | flags);
  }

  if (rc != 0)
  {
    regerror(rc, &re, buf, sizeof(buf));
    mutt_error("%s", buf);
    return -1;
  }

  rc = menu->current + search_dir;
search_next:
  if (wrap)
    mutt_message(_("Search wrapped to top"));
  while ((rc >= 0) && (rc < menu->max))
  {
    if (menu->search(menu, &re, rc) == 0)
    {
      regfree(&re);
      return rc;
    }

    rc += search_dir;
  }

  const bool c_wrap_search = cs_subset_bool(NeoMutt->sub, "wrap_search");
  if (c_wrap_search && (wrap++ == 0))
  {
    rc = (search_dir == 1) ? 0 : menu->max - 1;
    goto search_next;
  }
  regfree(&re);
  mutt_error(_("Not found"));
  return -1;
}

/**
 * menu_dialog_translate_op - Convert menubar movement to scrolling
 * @param i Action requested, e.g. OP_NEXT_ENTRY
 * @retval num Action to perform, e.g. OP_NEXT_LINE
 */
static int menu_dialog_translate_op(int i)
{
  switch (i)
  {
    case OP_NEXT_ENTRY:
      return OP_NEXT_LINE;
    case OP_PREV_ENTRY:
      return OP_PREV_LINE;
    case OP_CURRENT_TOP:
    case OP_FIRST_ENTRY:
      return OP_TOP_PAGE;
    case OP_CURRENT_BOTTOM:
    case OP_LAST_ENTRY:
      return OP_BOTTOM_PAGE;
    case OP_CURRENT_MIDDLE:
      return OP_MIDDLE_PAGE;
  }

  return i;
}

/**
 * menu_dialog_dokey - Check if there are any menu key events to process
 * @param menu Current Menu
 * @param ip   KeyEvent ID
 * @retval  0 An event occurred for the menu, or a timeout
 * @retval -1 There was an event, but not for menu
 */
static int menu_dialog_dokey(struct Menu *menu, int *ip)
{
  struct KeyEvent ch;
  char *p = NULL;

  do
  {
    ch = mutt_getch();
  } while (ch.ch == -2);

  if (ch.ch < 0)
  {
    *ip = -1;
    return 0;
  }

  if (ch.ch && (p = strchr(menu->keys, ch.ch)))
  {
    *ip = OP_MAX + (p - menu->keys + 1);
    return 0;
  }
  else
  {
    if (ch.op == OP_NULL)
      mutt_unget_event(ch.ch, 0);
    else
      mutt_unget_event(0, ch.op);
    return -1;
  }
}

/**
 * menu_redraw - Redraw the parts of the screen that have been flagged to be redrawn
 * @param menu Menu to redraw
 * @retval OP_NULL   Menu was redrawn
 * @retval OP_REDRAW Full redraw required
 */
int menu_redraw(struct Menu *menu)
{
  if (menu->custom_redraw)
  {
    menu->custom_redraw(menu);
    return OP_NULL;
  }

  /* See if all or part of the screen needs to be updated.  */
  if (menu->redraw & REDRAW_FULL)
  {
    menu_redraw_full(menu);
    /* allow the caller to do any local configuration */
    return OP_REDRAW;
  }

  if (ARRAY_EMPTY(&menu->dialog))
    menu_check_recenter(menu);

  if (menu->redraw & REDRAW_STATUS)
    menu_redraw_status(menu);
  if (menu->redraw & REDRAW_INDEX)
    menu_redraw_index(menu);
  else if (menu->redraw & REDRAW_MOTION)
    menu_redraw_motion(menu);
  else if (menu->redraw == REDRAW_CURRENT)
    menu_redraw_current(menu);

  if (!ARRAY_EMPTY(&menu->dialog))
    menu_redraw_prompt(menu);

  return OP_NULL;
}

/**
 * menu_loop - Menu event loop
 * @param menu Current Menu
 * @retval num An event id that the menu can't process
 */
int menu_loop(struct Menu *menu)
{
  static int last_position = -1;
  int op = OP_NULL;

  if (menu->max && menu->is_mailbox_list)
  {
    if (last_position > (menu->max - 1))
      last_position = -1;
    else if (last_position >= 0)
      menu_set_index(menu, last_position);
  }

  while (true)
  {
    /* Clear the tag prefix unless we just started it.  Don't clear
     * the prefix on a timeout (op==-2), but do clear on an abort (op==-1) */
    if (menu->tagprefix && (op != OP_TAG_PREFIX) &&
        (op != OP_TAG_PREFIX_COND) && (op != -2))
    {
      menu->tagprefix = false;
    }

    mutt_curses_set_cursor(MUTT_CURSOR_INVISIBLE);

    if (menu_redraw(menu) == OP_REDRAW)
      return OP_REDRAW;

    /* give visual indication that the next command is a tag- command */
    if (menu->tagprefix)
    {
      mutt_window_mvaddstr(MessageWindow, 0, 0, "tag-");
      mutt_window_clrtoeol(MessageWindow);
    }

    const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
    const bool c_braille_friendly =
        cs_subset_bool(NeoMutt->sub, "braille_friendly");

    /* move the cursor out of the way */
    if (c_arrow_cursor)
      mutt_window_move(menu->win_index, 2, menu->current - menu->top);
    else if (c_braille_friendly)
      mutt_window_move(menu->win_index, 0, menu->current - menu->top);
    else
    {
      mutt_window_move(menu->win_index, menu->win_index->state.cols - 1,
                       menu->current - menu->top);
    }

    mutt_refresh();

    /* try to catch dialog keys before ops */
    if (!ARRAY_EMPTY(&menu->dialog) && (menu_dialog_dokey(menu, &op) == 0))
      return op;

    const bool c_auto_tag = cs_subset_bool(NeoMutt->sub, "auto_tag");
    op = km_dokey(menu->type);
    if ((op == OP_TAG_PREFIX) || (op == OP_TAG_PREFIX_COND))
    {
      if (menu->tagprefix)
      {
        menu->tagprefix = false;
        mutt_window_clearline(MessageWindow, 0);
        continue;
      }

      if (menu->tagged)
      {
        menu->tagprefix = true;
        continue;
      }
      else if (op == OP_TAG_PREFIX)
      {
        mutt_error(_("No tagged entries"));
        op = -1;
      }
      else /* None tagged, OP_TAG_PREFIX_COND */
      {
        mutt_flush_macro_to_endcond();
        mutt_message(_("Nothing to do"));
        op = -1;
      }
    }
    else if (menu->tagged && c_auto_tag)
      menu->tagprefix = true;

    mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);

    if (SigWinch)
    {
      SigWinch = 0;
      mutt_resize_screen();
      clearok(stdscr, true); /* force complete redraw */
    }

    if (op < 0)
    {
      if (menu->tagprefix)
        mutt_window_clearline(MessageWindow, 0);
      continue;
    }

    if (ARRAY_EMPTY(&menu->dialog))
      mutt_clear_error();

    /* Convert menubar movement to scrolling */
    if (!ARRAY_EMPTY(&menu->dialog))
      op = menu_dialog_translate_op(op);

    switch (op)
    {
      case OP_NEXT_ENTRY:
        menu_next_entry(menu);
        break;
      case OP_PREV_ENTRY:
        menu_prev_entry(menu);
        break;
      case OP_HALF_DOWN:
        menu_half_down(menu);
        break;
      case OP_HALF_UP:
        menu_half_up(menu);
        break;
      case OP_NEXT_PAGE:
        menu_next_page(menu);
        break;
      case OP_PREV_PAGE:
        menu_prev_page(menu);
        break;
      case OP_NEXT_LINE:
        menu_next_line(menu);
        break;
      case OP_PREV_LINE:
        menu_prev_line(menu);
        break;
      case OP_FIRST_ENTRY:
        menu_first_entry(menu);
        break;
      case OP_LAST_ENTRY:
        menu_last_entry(menu);
        break;
      case OP_TOP_PAGE:
        menu_top_page(menu);
        break;
      case OP_MIDDLE_PAGE:
        menu_middle_page(menu);
        break;
      case OP_BOTTOM_PAGE:
        menu_bottom_page(menu);
        break;
      case OP_CURRENT_TOP:
        menu_current_top(menu);
        break;
      case OP_CURRENT_MIDDLE:
        menu_current_middle(menu);
        break;
      case OP_CURRENT_BOTTOM:
        menu_current_bottom(menu);
        break;
      case OP_SEARCH:
      case OP_SEARCH_REVERSE:
      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
        if (menu->custom_search)
          return op;
        else if (menu->search && ARRAY_EMPTY(&menu->dialog)) /* Searching dialogs won't work */
        {
          int index = search(menu, op);
          if (index != -1)
            menu_set_index(menu, index);
        }
        else
          mutt_error(_("Search is not implemented for this menu"));
        break;

      case OP_JUMP:
        if (!ARRAY_EMPTY(&menu->dialog))
          mutt_error(_("Jumping is not implemented for dialogs"));
        else
          menu_jump(menu);
        break;

      case OP_ENTER_COMMAND:
        mutt_enter_command();
        window_set_focus(menu->win_index);
        window_redraw(RootWindow, false);
        break;

      case OP_TAG:
        if (menu->tag && ARRAY_EMPTY(&menu->dialog))
        {
          const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");

          if (menu->tagprefix && !c_auto_tag)
          {
            for (int i = 0; i < menu->max; i++)
              menu->tagged += menu->tag(menu, i, 0);
            menu->redraw |= REDRAW_INDEX;
          }
          else if (menu->max != 0)
          {
            int j = menu->tag(menu, menu->current, -1);
            menu->tagged += j;
            if (j && c_resolve && (menu->current < (menu->max - 1)))
            {
              menu_set_index(menu, menu->current + 1);
            }
            else
              menu->redraw |= REDRAW_CURRENT;
          }
          else
            mutt_error(_("No entries"));
        }
        else
          mutt_error(_("Tagging is not supported"));
        break;

      case OP_SHELL_ESCAPE:
        if (mutt_shell_escape())
        {
          mutt_mailbox_check(ctx_mailbox(Context), MUTT_MAILBOX_CHECK_FORCE);
        }
        break;

      case OP_WHAT_KEY:
        mutt_what_key();
        break;

      case OP_CHECK_STATS:
        mutt_check_stats(ctx_mailbox(Context));
        break;

      case OP_REDRAW:
        clearok(stdscr, true);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_HELP:
        mutt_help(menu->type);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_NULL:
        km_error_key(menu->type);
        break;

      case OP_END_COND:
        break;

      default:
        if (menu->is_mailbox_list)
          last_position = menu->current;
        return op;
    }
  }
  /* not reached */
}

/**
 * menu_get_current_type - Get the type of the current Window
 * @retval enum Menu Type, e.g. #MENU_PAGER
 */
enum MenuType menu_get_current_type(void)
{
  struct MuttWindow *win = window_get_dialog();
  while (win && win->focus)
    win = win->focus;

  if (!win)
    return MENU_GENERIC;

  if (win->type != WT_MENU)
    return MENU_GENERIC;

  struct Menu *menu = win->wdata;
  if (!menu)
    return MENU_GENERIC;

  return menu->type;
}

/**
 * menu_free_window - Destroy a Menu Window - Implements MuttWindow::wdata_free()
 */
static void menu_free_window(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Menu *menu = *ptr;

  notify_observer_remove(NeoMutt->notify, menu_config_observer, menu);
  notify_observer_remove(menu->win_index->notify, menu_window_observer, menu);
  mutt_color_observer_remove(menu_color_observer, menu);
  notify_free(&menu->notify);

  if (menu->mdata && menu->mdata_free)
    menu->mdata_free(menu, &menu->mdata); // Custom function to free private data

  char **line = NULL;
  ARRAY_FOREACH(line, &menu->dialog)
  {
    FREE(line);
  }
  ARRAY_FREE(&menu->dialog);

  FREE(ptr);
}

/**
 * menu_new_window - Create a new Menu Window
 * @param type Menu type, e.g. #MENU_PAGER
 * @retval ptr New MuttWindow wrapping a Menu
 */
struct MuttWindow *menu_new_window(enum MenuType type)
{
  struct MuttWindow *win =
      mutt_window_new(WT_MENU, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct Menu *menu = mutt_mem_calloc(1, sizeof(struct Menu));

  menu->type = type;
  menu->redraw = REDRAW_FULL;
  menu->color = default_color;
  menu->search = generic_search;
  menu->notify = notify_new();
  menu->win_index = win;
  menu->pagelen = win->state.rows;

  win->recalc = menu_recalc;
  win->repaint = menu_repaint;
  win->wdata = menu;
  win->wdata_free = menu_free_window;
  notify_set_parent(menu->notify, win->notify);

  notify_observer_add(NeoMutt->notify, NT_CONFIG, menu_config_observer, menu);
  notify_observer_add(win->notify, NT_WINDOW, menu_window_observer, menu);
  mutt_color_observer_add(menu_color_observer, menu);

  return win;
}

/**
 * menu_get_index - Get the current selection in the Menu
 * @param menu Menu
 * @retval num Index of selection
 */
int menu_get_index(struct Menu *menu)
{
  if (!menu)
    return -1;

  return menu->current;
}

/**
 * menu_set_index - Set the current selection in the Menu
 * @param menu  Menu
 * @param index Item to select
 * @retval true Selection was changed
 */
bool menu_set_index(struct Menu *menu, int index)
{
  if (!menu)
    return false;

  if (index < -1)
    return false;
  if (index >= menu->max)
    return false;

  menu->oldcurrent = menu->current;
  menu->current = index;
  menu->redraw |= REDRAW_MOTION;
  return true;
}

/**
 * menu_queue_redraw - Queue a request for a redraw
 * @param menu  Menu
 * @param redraw Item to redraw, e.g. #REDRAW_CURRENT
 */
void menu_queue_redraw(struct Menu *menu, MuttRedrawFlags redraw)
{
  if (!menu)
    return;

  menu->redraw |= redraw;
  menu->win_index->actions |= WA_RECALC;
}