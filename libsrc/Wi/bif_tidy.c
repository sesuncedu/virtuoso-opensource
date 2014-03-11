/*
 *  bif_tidy.c
 *
 *  $Id$
 *
 *  Build in Functions for tidying HTML pages
 *
 *  This file is part of the OpenLink Software Virtuoso Open-Source (VOS)
 *  project.
 *
 *  Copyright (C) 1998-2014 OpenLink Software
 *
 *  This project is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; only version 2 of the License, dated June 1991.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef USE_TIDYLIB
#define OLD_TIDY
#endif

#include "libutil.h"
#include "sqlnode.h"
#include "sqlbif.h"
#include "wi.h"
#include "Dk.h"
#ifndef WIN32
#define __USE_MISC 1 /* hack for platform.h defining ulong & uint */
#endif
#ifdef OLD_TIDY
#include "Tidy/html.h"
#else
#include <tidy/tidy.h>
#include <tidy/buffio.h>
#endif
#ifndef WIN32
#undef __USE_MISC
#endif

static dk_mutex_t *tidy_mtx;

#ifndef OLD_TIDY
static void * TIDY_CALL
tidy_malloc (size_t len)
{
  if (len >= MAX_BOX_LENGTH)
    return NULL;
  return t_alloc_box (len, DV_CUSTOM);
}

static void * TIDY_CALL
tidy_realloc (void * buf, size_t len)
{
  int buf_size = IS_BOX_POINTER (buf) ? box_length (buf) : 0;
  int copy_size = buf_size > len ? len : buf_size;
  void *new;
  if (len >= MAX_BOX_LENGTH)
    return NULL;
  new = t_alloc_box (len, DV_CUSTOM);
  if (buf && copy_size)
    memcpy (new, buf, copy_size);
  return new;
}

static void TIDY_CALL
tidy_free (void * buf)
{
  /* void, will release on MP_DONE */
}

static void TIDY_CALL
tidy_panic (const char * err)
{
  /* log_error ("Tidy panic: %s", err); */
  sqlr_new_error ("42000", "TIDYE", "Tidy panic: %s", err);
}

#define READING_NAME 1
#define READING_VALUE 2

static int
tidy_parse_config (TidyDoc doc, caddr_t config_str)
{
  dk_session_t * ses;
  volatile int rc = -1, i = 0;
  char name[64] = {0}, value[8192] = {0}, stat = READING_NAME;

  ses = strses_allocate ();
  ses->dks_in_buffer = config_str;
  ses->dks_in_fill = box_length (config_str) - 1;

  CATCH_READ_FAIL (ses)
    {
      char c;
      for (;;)
	{
	  c = session_buffered_read_char (ses);
	  if (READING_VALUE == stat && (c == '\r' || c == '\n'))
	    {
	      value[i] = 0;
	      rc = tidyOptParseValue (doc, name, value);
	      i = 0;
	      stat = READING_NAME;
	      continue;
	    }
	  if (isspace (c))
	    continue;
	  if (READING_NAME == stat && c == ':') /* delimiter */
	    {
	      name[i] = 0;
	      i = 0;
	      stat = READING_VALUE;
	      continue;
	    }
	  if (READING_NAME == stat)
	    name[i++] = c;
	  if (READING_VALUE == stat)
	    value[i++] = c;
	  /* check for overflow */
	  if (READING_NAME == stat && i >= sizeof (name))
	    break;
	  if (READING_VALUE == stat && i >= sizeof (value))
	    break;
	}
    }
  FAILED
    {
      if (READING_VALUE == stat)
	{
	  value[i] = 0;
	  rc = tidyOptParseValue (doc, name, value);
	}
    }
  END_READ_FAIL (ses);

  ses->dks_in_buffer = NULL;
  dk_free_box (ses);
  return 0;
}
#endif

caddr_t
bif_tidy_html (caddr_t * qst, caddr_t * err_ret, state_slot_t ** args)
{
  caddr_t html_input = bif_string_arg (qst, args, 0, "tidy_html");
  caddr_t config_input = bif_string_arg (qst, args, 1, "tidy_html");
  caddr_t html_output = NULL;
  int res;
#ifdef OLD_TIDY
  tidy_io_t tidy_errout;
  tidy_errout.tio_data.lm_memblock = NULL;
  tidy_errout.tio_data.lm_length = 0;
  tidy_errout.tio_pos = 0;
  mutex_enter (tidy_mtx);
  errout = &tidy_errout;
  res = do_tidy (html_input, config_input, &html_output);
  errout = NULL;
  mutex_leave (tidy_mtx);
  if (NULL != tidy_errout.tio_data.lm_memblock)
    dk_free_box (tidy_errout.tio_data.lm_memblock);
  if ((NULL == html_output) || (2 == res))	/* errors */
    {
      dk_free_box (html_output);
      sqlr_new_error ("42000", "HT076", "HTML Tidy failed, try tidy_list_errors(...) to get more information");
    }
#else
  TidyBuffer output;
  TidyBuffer errbuf;
  TidyDoc doc;
  mem_pool_t * mp = THR_TMP_POOL;

  if (mp) /* do not crash if MP exists */
    {
      SET_THR_TMP_POOL (NULL);
      log_error ("non-empty MP in bif_tidy");
    }
  MP_START ();
  QR_RESET_CTX
  {
    doc = tidyCreate ();
    tidyBufInit (&output);
    tidyBufInit (&errbuf);
    tidySetErrorBuffer (doc, &errbuf);
    /* cannot load cfg file here, must parse the config string */
    tidy_parse_config (doc, config_input);
    res = tidyParseString (doc, html_input);
    if (res >= 0)
      res = tidyCleanAndRepair (doc);
    if (res >= 0)
      res = tidyRunDiagnostics (doc);
    if (res > 1)
      res = (tidyOptSetBool (doc, TidyForceOutput, yes) ? res : -1);
    if (res >= 0)
      res = tidySaveBuffer (doc, &output);
    if (res >= 0)
      html_output = box_dv_short_string ((char *) output.bp);
  }
  QR_RESET_CODE
  {
    caddr_t err;
    POP_QR_RESET;
    err = thr_get_error_code (THREAD_CURRENT_THREAD);
    MP_DONE ();
      if (mp) /* restore */
	{
	  SET_THR_TMP_POOL (mp);
	}
    sqlr_resignal (err);
  }
  END_QR_RESET;
  MP_DONE ();
  if (mp)
    {
      SET_THR_TMP_POOL (mp);
    }
  /*
     tidyBufFree( &output );
     tidyBufFree( &errbuf );
     tidyRelease (doc);
   */
  if (res < 0)
    {
      dk_free_box (html_output);
      sqlr_new_error ("42000", "HT076", "HTML Tidy failed, try tidy_list_errors(...) to get more information");
    }
#endif
  return html_output;
}

caddr_t
bif_tidy_list_errors (caddr_t * qst, caddr_t * err_ret, state_slot_t ** args)
{
  caddr_t html_input = bif_string_arg (qst, args, 0, "tidy_list_errors");
  caddr_t config_input = bif_string_arg (qst, args, 1, "tidy_list_errors");
  caddr_t errlist = NULL;
#ifdef OLD_TIDY
  tidy_io_t tidy_errout;
  tidy_errout.tio_data.lm_memblock = NULL;
  tidy_errout.tio_data.lm_length = 0;
  tidy_errout.tio_pos = 0;
  mutex_enter (tidy_mtx);
  errout = &tidy_errout;
  do_tidy (html_input, config_input, null);
  errout = NULL;
  mutex_leave (tidy_mtx);
  if (NULL == tidy_errout.tio_data.lm_memblock)
    errlist = box_dv_short_string ("");
  else
    {
      errlist = box_dv_short_nchars (tidy_errout.tio_data.lm_memblock, tidy_errout.tio_pos);
      dk_free_box (tidy_errout.tio_data.lm_memblock);
    }
#else
  int res = -1;
  TidyBuffer errbuf;
  TidyDoc doc;

  MP_START ();
  QR_RESET_CTX
  {
    doc = tidyCreate ();
    tidyBufInit (&errbuf);
    tidySetErrorBuffer (doc, &errbuf);
    /* cannot load cfg file here, must parse the config string */
    tidy_parse_config (doc, config_input);
    res = tidyParseString (doc, html_input);
    if (res >= 0)
      res = tidyCleanAndRepair (doc);
    if (res >= 0)
      res = tidyRunDiagnostics (doc);
    if (res > 1)
      res = (tidyOptSetBool (doc, TidyForceOutput, yes) ? res : -1);
    if (res >= 0)
      errlist = box_dv_short_string ((char *) errbuf.bp);
    else
      errlist = box_dv_short_string ("");
  }
  QR_RESET_CODE
  {
    caddr_t err;
    POP_QR_RESET;
    err = thr_get_error_code (THREAD_CURRENT_THREAD);
    MP_DONE ();
    sqlr_resignal (err);
  }
  END_QR_RESET;
  MP_DONE ();
  /*
     tidyBufFree( &errbuf );
     tidyRelease (doc);
   */
#endif
  return errlist;
}

caddr_t
bif_tidy_external (caddr_t * qst, caddr_t * err_ret, state_slot_t ** args)
{
#ifdef OLD_TIDY
  return box_num (0);
#else
  return box_num (1);
#endif
}

int bif_tidy_init(void)
{
  tidy_mtx = mutex_allocate ();
  bif_define ("tidy_html", bif_tidy_html);
  bif_define ("tidy_list_errors", bif_tidy_list_errors);
  bif_define ("tidy_external", bif_tidy_external);
#ifndef OLD_TIDY
  tidySetMallocCall (tidy_malloc);
  tidySetReallocCall (tidy_realloc);
  tidySetFreeCall (tidy_free);
  tidySetPanicCall (tidy_panic);
#endif
  return 0;
}
