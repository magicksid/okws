// -*-c++-*-
/* $Id$ */

/*
 *
 * Copyright (C) 2002-2004 Maxwell Krohn (max@okcupid.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#pragma once

#include <sys/time.h>
#include "async.h"
#include "arpc.h"
#include "ahttp.h"
#include "okcgi.h"
#include "resp.h"
#include "ok.h"
#include "pslave.h"
#include "svq.h"
#include "okconst.h"
#include "arpc.h"
#include "rxx.h"
#include "tame.h"

#define OK_LQ_SIZE_D    100
#define OK_LQ_SIZE_LL   5
#define OK_LQ_SIZE_UL   1000


typedef enum { OKD_JAILED_NEVER = 0,
	       OKD_JAILED_DEPENDS = 1,
	       OKD_JAILED_ALWAYS = 2 } okd_jailtyp_t;

class okch_t;
typedef callback<void, okch_t *>::ref cb_okch_t;

struct okd_stats_t {
  void to_strbuf (strbuf &b) const;
  time_t _uptime;
  size_t _n_req;
  size_t _n_recv;
  size_t _n_sent;
  size_t _n_tot;
};

class ok_custom2_trig_t {
public:
  ok_custom2_trig_t () :
    _custom_res (New refcounted<ok_custom_res_set_t> ()) {}

  void add_err (const str &svc, ok_xstatus_typ_t t);
  void add_succ (const str &svc, const ok_custom_data_t &dat);
  ~ok_custom2_trig_t () { _cb (); }
  ptr<ok_custom_res_set_t> get_custom_res () { return _custom_res; }
  void setcb (cbv c) { _cb = c; }
private:
  cbv::ptr _cb;
  ptr<ok_custom_res_set_t> _custom_res;
};

class okd_t;
class okch_t : public ok_con_t {  // OK Child Handle
public:
  okch_t (okd_t *o, const str &e, okc_state_t st = OKC_STATE_NONE);
  ~okch_t ();
  void launch ();
  void clone (ahttpcon_wrapper_t<ahttpcon_clone> acw, CLOSURE);
  void send_con_to_service (ahttpcon_wrapper_t<ahttpcon_clone> acw, CLOSURE);
  void shutdown (oksig_t sig, cbv cb);

  void got_new_ctlx_fd (int fd, int p);
  void dispatch (ptr<bool> destroyed, svccb *b);
  void fdcon_eof (ptr<bool> destroyed);
  void kill ();
  void custom1_out (const ok_custom_data_t &x, evs_t ev, CLOSURE);
  void custom2_out (const ok_custom_data_t &in, ok_custom_res_union_t *out, 
		    evs_t ev, CLOSURE);

  void chld_eof ();
  void to_status_xdr (oksvc_status_t *out);

  inline int get_n_sent () const { return _n_sent; }
  void reset_n_sent () ;
  void reset_accounting ();
  inline int inc_n_sent () { return (_n_sent ++) ; }
  void awaken (evb_t ev, CLOSURE);
  void set_state (okc_state_t s) { state = s; }

  void stats_collect (okd_stats_t *s, evv_t ev, CLOSURE);

  void toggle_leak_checker (ok_diagnostic_cmd_t cmd, 
			    event<ok_xstatus_typ_t>::ref ev, CLOSURE);
  void toggle_profiler (ok_diagnostic_cmd_t cmd, 
			event<ok_xstatus_typ_t>::ref ev, CLOSURE);
  
  okd_t *myokd;
  int pid;
  const str servpath;       // GET <servpath> HTTP/1.1 (starts with '/')
  ihash_entry<okch_t> lnk;
protected:
  void handle_reenable_accept (svccb *sbp);
  void start_chld ();
private:
  void shutdown_cb1 (cbv cb);
  void closed_fd (u_int64_t gen);

  vec<ahttpcon_wrapper_t<ahttpcon_clone> > conqueue;

  okc_state_t state;
  ptr<bool> destroyed;
  bool srv_disabled;
  int per_svc_nfd_in_xit;   // per service number FD in transit
  int _n_sent;              // N sent since reboot
  time_t _last_restart;     // time when started;
  
  bool _too_busy;           // the server is potentially too busy to get more
  u_int64_t _generation_id; // which generation of service this is.

  // To be triggered when this service is ready to go.
  evv_t::ptr _ready_trigger; 
};

class okd_ssl_t {
public:
  okd_ssl_t (okd_t *o) : _okd (o), _fd (-1) {}
  ~okd_ssl_t () { hangup (); }
  bool init (int fd);
  void enable_accept () { toggle_accept (true); }
  void disable_accept () { toggle_accept (false); }
  void dispatch (svccb *sbp);
  void hangup ();
private:
  void toggle_accept (bool b, CLOSURE);
  okd_t *_okd;
  int _fd;
  ptr<axprt_unix> _x;
  ptr<aclnt> _cli;
  ptr<asrv> _srv;
};

class okd_t : public ok_httpsrv_t, public config_parser_t 
{
public:
  okd_t (const str &cf, int logfd_in, int okldfd_in, const str &cdd, 
	 okws1_port_t p, int pub2fd_in) : 
    ok_httpsrv_t (NULL, logfd_in, pub2fd_in),
    config_parser_t (),
    okd_usr (ok_okd_uname), okd_grp (ok_okd_gname),
    pubd (NULL), 
    configfile (cf),
    okldfd (okldfd_in),
    sdflag (false), sd2 (false), dcb (NULL), 
    sdattempt (0),
    cntr (0),
    coredumpdir (cdd),
    nfd_in_xit (0),
    reqid (0),
    _startup_time (time (NULL)),
    xtab (2),
    _socket_filename (okd_mgr_socket),
    _socket_mode (okd_mgr_socket_mode),
    _accept_ready (false),
    _ssl (this),
    _lazy_startup (false),
    _okd_nodelay (okd_tcp_nodelay)
  {
    listenport = p;
  }


  ~okd_t ();

  void abort ();

  void insert (okch_t *c) { servtab.insert (c); }
  void remove (okch_t *c) { servtab.remove (c); }

  void got_alias (vec<str> s, str loc, bool *errp);
  void got_regex_alias (vec<str> s, str loc, bool *errp);
  void got_err_doc (vec<str> s, str loc, bool *errp);
  void got_service (vec<str> s, str loc, bool *errp);
  void got_service2 (vec<str> s, str loc, bool *errp);
  void got_script (vec<str> s, str loc, bool *errp);

  void okld_dispatch (svccb *sbp);

  // Well, no one ever said event-driven programming was pretty
  void launch (CLOSURE);

  void launch_logd (evb_t ev, CLOSURE);

  void sclone (ahttpcon_wrapper_t<ahttpcon_clone> acw, str s, int status);
  void newserv (int fd);
  void newserv2 (int port, int nfd, sockaddr_in *sin, bool prx, 
		 const ssl_ctx_t *ssl);
  void shutdown (int sig);
  void awaken (str nm, evb_t ev, CLOSURE);

  typedef ihash<const str, okch_t, &okch_t::servpath, &okch_t::lnk> servtab_t;
  servtab_t servtab;
  qhash<str, str> aliases;

  struct regex_alias_t {
    regex_alias_t (str t, const rxx &x) : _target (t), _rxx (x) {}
    const str _target;
    rxx _rxx;
  };

  vec<regex_alias_t> regex_aliases;

  ok_usr_t okd_usr; 
  ok_grp_t okd_grp;

  helper_t *pubd;

  void open_mgr_socket ();

  // functions for the OKMGR interface
  void relaunch (svccb *sbp, CLOSURE);
  void custom1_in (svccb *sbp, CLOSURE);
  void custom2_in (svccb *sbp, CLOSURE);
  void toggle_leak_checker (svccb *sbp, CLOSURE);
  void toggle_profiler (svccb *sbp, CLOSURE);
  void okctl_get_stats (svccb *sbp);
  void turnlog (svccb *sbp, CLOSURE);

  void strip_privileges ();

  void kill_srvcs_cb ();
  bool in_shutdown () const { return sdflag; }
  void set_signals ();

  void send_errdoc_set (svccb *sbp);
  void req_errdoc_set_2 (svccb *sbp);

  void closed_fd ();

protected:
  // queueing stuff
  void enable_accept_guts ();
  void disable_accept_guts ();
  bool parse_file (const str &fn);
  bool post_config (const str &fn);
  bool lazy_startup () const { return _lazy_startup; }
  void render_stat_page (ptr<ahttpcon_clone> x, CLOSURE);
  ok_xstatus_typ_t reserve_child (const str &nm, bool lzy);

  /* statistics */
  void stats_collect (okd_stats_t *s, evv_t ev, CLOSURE);
  void render_stats_page (ptr<ahttpcon_clone> x, CLOSURE);
  void send_stats_reply (ptr<ahttpcon> x, const okd_stats_t &stats, htpv_t v,
			 evv_t ev, CLOSURE);

private:
  void custom1_in (const ok_custom_arg_t &x, evs_t cb);

  void newmgrsrv (ptr<axprt_stream> x);
  void check_runas ();

  // shutdown functions
  void kill_srvcs (oksig_t sig);
  void stop_listening ();
  void shutdown_retry ();
  void shutdown2 ();
  void shutdown3 ();
  void shutdown_cb1 ();

  void got_child_fd (int fd, const okws_svc_descriptor_t &d);
  bool listen_from_ssl (int fd);

  str configfile;
  int okldfd;
  
  bool bdlnch;
  u_int chkcnt;
  u_int sdcbcnt;
  bool sdflag, sd2;
  timecb_t *dcb;
  bool jailed;
  int sdattempt;
  int cntr;

  ptr<axprt_unix> _okld_x;
  ptr<asrv> _okld_srv;
  ptr<aclnt> _okld_cli;

  str coredumpdir;
  xpub_errdoc_set_t xeds;

  int nfd_in_xit;       // number of FDs in transit
  u_int reqid;
  time_t _startup_time;
  ahttp_tab_t xtab;

  qhash<int, okws1_port_t> portmap;
  vec<int> listenfds;
  str _socket_filename;
  str _socket_group, _socket_owner;
  int _socket_mode;

  str _config_grp, _config_user;
  bool _accept_ready;
  okd_ssl_t _ssl;
  bool _lazy_startup;
  str _stat_page_url;
  bool _okd_nodelay;
};

class okd_mgrsrv_t 
{
public:
  okd_mgrsrv_t (ptr<axprt_stream> xx, okd_t *o);
  void dispatch (svccb *b);
private:
  void repub (svccb *b, int v);
  void turnlog (svccb *b);
  ptr<asrv> srv;
  ptr<axprt_stream> x;
  okd_t *myokd;
};

class common_404_t {
public:
  common_404_t ();
  bool operator[] (const str &s) { return tab[s]; }
  bhash<str> tab;
};

