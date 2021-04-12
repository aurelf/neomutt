/**
 * @file
 * Config used by libnotmuch
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page nm_config Config used by libnotmuch
 *
 * Config used by libnotmuch
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "notmuch/private.h"

/**
 * is_valid_notmuch_url - Checks that a URL is in required form.
 * @retval true url in form notmuch://[absolute path]
 * @retval false url is not in required form.
 */
static bool is_valid_notmuch_url(const char *url)
{
  return mutt_istr_startswith(url, NmUrlProtocol) && (url[NmUrlProtocolLen] == '/');
}

/**
 * nm_default_url_validator - Ensure nm_default_url is of the form notmuch://[absolute path] - Implements ConfigDef::validator()
 */
static int nm_default_url_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                                    intptr_t value, struct Buffer *err)
{
  const char *url = (const char *) value;
  if (!is_valid_notmuch_url(url))
  {
    mutt_buffer_printf(
        err, _("nm_default_url must be: notmuch://<absolute path> . Current: %s"), url);
    return CSR_ERR_INVALID;
  }

  return CSR_SUCCESS;
}

static struct ConfigDef NotmuchVars[] = {
  // clang-format off
  { "nm_db_limit", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "(notmuch) Default limit for Notmuch queries"
  },
  { "nm_default_url", DT_STRING, 0, 0, nm_default_url_validator,
    "(notmuch) Path to the Notmuch database"
  },
  { "nm_exclude_tags", DT_STRING, 0, 0, NULL,
    "(notmuch) Exclude messages with these tags"
  },
  { "nm_flagged_tag", DT_STRING, IP "flagged", 0, NULL,
    "(notmuch) Tag to use for flagged messages"
  },
  { "nm_open_timeout", DT_NUMBER|DT_NOT_NEGATIVE, 5, 0, NULL,
    "(notmuch) Database timeout"
  },
  { "nm_query_type", DT_STRING, IP "messages", 0, NULL,
    "(notmuch) Default query type: 'threads' or 'messages'"
  },
  { "nm_query_window_current_position", DT_NUMBER, 0, 0, NULL,
    "(notmuch) Position of current search window"
  },
  { "nm_query_window_current_search", DT_STRING, 0, 0, NULL,
    "(notmuch) Current search parameters"
  },
  { "nm_query_window_duration", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "(notmuch) Time duration of the current search window"
  },
  { "nm_query_window_timebase", DT_STRING, IP "week", 0, NULL,
    "(notmuch) Units for the time duration"
  },
  { "nm_record_tags", DT_STRING, 0, 0, NULL,
    "(notmuch) Tags to apply to the 'record' mailbox (sent mail)"
  },
  { "nm_replied_tag", DT_STRING, IP "replied", 0, NULL,
    "(notmuch) Tag to use for replied messages"
  },
  { "nm_unread_tag", DT_STRING, IP "unread", 0, NULL,
    "(notmuch) Tag to use for unread messages"
  },
  { "vfolder_format", DT_STRING|DT_NOT_EMPTY|R_INDEX, IP "%2C %?n?%4n/&     ?%4m %f", 0, NULL,
    "(notmuch) printf-like format string for the browser's display of virtual folders"
  },
  { "virtual_spool_file", DT_BOOL, false, 0, NULL,
    "(notmuch) Use the first virtual mailbox as a spool file"
  },

  { "nm_default_uri",    DT_SYNONYM, IP "nm_default_url",     },
  { "virtual_spoolfile", DT_SYNONYM, IP "virtual_spool_file", },

  { NULL },
  // clang-format on
};

/**
 * config_init_notmuch - Register notmuch config variables - Implements ::module_init_config_t
 */
bool config_init_notmuch(struct ConfigSet *cs)
{
  return cs_register_variables(cs, NotmuchVars, 0);
}
