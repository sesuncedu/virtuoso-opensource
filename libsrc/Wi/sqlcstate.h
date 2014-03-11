/*
 *  sqlcstate.h
 *
 *  $Id$
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

#ifndef SQLCSTATE_H
#define SQLCSTATE_H

typedef struct sql_compile_state_s /* serialized in parse_sem */
{
  oid_t scs_v_u_id;
  oid_t scs_v_g_id;
  char scs_sql_line_loc_text[1000];
  char scs_sql_err_text[2000];
  char scs_sql_err_state[6];
  char scs_sql_err_native[1000];
  int scs_parse_not_char_c_escape;
  int scs_parse_utf8_execs;
  int scs_parse_pldbg;
  caddr_t scs_pl_file;
  int 	scs_pl_file_offs;
  dk_set_t scs_sql3_breaks;
  dk_set_t scs_sql3_pbreaks;
  dk_set_t scs_sql3_ppbreaks;
  caddr_t scs_sqlp_udt_current_type;
  int scs_sqlp_udt_current_type_lang;
  sql_tree_t *scs_parse_tree;
  sql_tree_t *scs_global_trans;
  char *scs_sql_text;
  int scs_param_inx;
  int scs_sqlp_have_infoschema_views;
  char * scs_inside_view;
  char	scs_count_qr_global_refs; /*   qr global ssl's will be counted as refs in cv_refd_slots etc. */
  char	scs_inside_sem;
  sql_comp_t *	scs_current_sc;
  sql_comp_t *	scs_top_sc;
} sql_compile_state_t;


#define top_sc global_scs->scs_top_sc
#define v_u_id global_scs->scs_v_u_id
#define v_g_id global_scs->scs_v_g_id
#define sql_line_loc_text	global_scs->scs_sql_line_loc_text
#define sql_err_text		global_scs->scs_sql_err_text
#define sql_err_state		global_scs->scs_sql_err_state
#define sql_err_native		global_scs->scs_sql_err_native
#define parse_not_char_c_escape	global_scs->scs_parse_not_char_c_escape
#define parse_utf8_execs	global_scs->scs_parse_utf8_execs
#define parse_pldbg		global_scs->scs_parse_pldbg
#define pl_file			global_scs->scs_pl_file
#define pl_file_offs		global_scs->scs_pl_file_offs
#define sql3_breaks		global_scs->scs_sql3_breaks
#define sql3_pbreaks		global_scs->scs_sql3_pbreaks
#define sql3_ppbreaks		global_scs->scs_sql3_ppbreaks
#define sqlp_udt_current_type	global_scs->scs_sqlp_udt_current_type
#define sqlp_udt_current_type_lang global_scs->scs_sqlp_udt_current_type_lang
#define parse_tree		global_scs->scs_parse_tree
#define global_trans		global_scs->scs_global_trans
#define sqlc_sql_text		global_scs->scs_sql_text
#define param_inx		global_scs->scs_param_inx
#define sqlp_have_infoschema_views	global_scs->scs_sqlp_have_infoschema_views
#define inside_view global_scs->scs_inside_view
#define sqlg_count_qr_global_refs global_scs->scs_count_qr_global_refs
#define sqlc_inside_sem global_scs->scs_inside_sem
#define sqlc_current_sc global_scs->scs_current_sc

#define SET_SCS(scs) \
  THREAD_CURRENT_THREAD->thr_sql_scs = (void*)scs

#define global_scs ((sql_compile_state_t*) (THREAD_CURRENT_THREAD->thr_sql_scs))

#define SCS_STATE_FRAME		sql_compile_state_t * save_scs; sql_compile_state_t scs

#define SCS_STATE_PUSH \
        { \
	  save_scs = global_scs; \
          memset (&scs, 0, sizeof (sql_compile_state_t)); \
	  SET_SCS (&scs); \
	}

#define SCS_STATE_POP \
  SET_SCS (save_scs)


#endif
