/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2021-2026 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "kiwi.h"
#include "clk.h"
#include "misc.h"
#include "str.h"
#include "cfg.h"
#include "gps.h"
#include "rx.h"
#include "rx_util.h"
#include "shmem.h"
#include "rx_waterfall.h"
#include "ansi.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

void c2s_mon_init()
{
	for (int i=0; i < rx_chans; i++) {
        rx_chan_t *rxc = &rx_channels[i];
		ndesc_register(&rxc->rtn_snd_nd);
	}
}

void c2s_mon_setup(void *param)
{
	conn_t *conn_mon = (conn_t *) param;
}

void c2s_mon(void *param)
{
	int n, i, j;
	conn_t *conn_mon = (conn_t *) param;
	bool init = false;
	conn_mon->camped_rx = -1;
	rx_common_init(conn_mon);
	nbuf_t *nb = NULL;

	while (TRUE) {
        int camped_rx = conn_mon->camped_rx;
        if (nb) web_to_app_done(conn_mon, nb);
        n = web_to_app(conn_mon, &nb);
            
        if (n) {
            char *cmd = nb->buf;
            cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

            // SECURITY: this must be first for auth check
            if (rx_common_cmd(STREAM_MONITOR, conn_mon, cmd)) {
                if (!init) {
                    if (kiwi_str_begins_with(cmd, "SET auth")) {
                    
                        // STREAM_SOUND converted into a STREAM_MONITOR
                        // because SND URL contained "/?camp"
                        // So let STREAM_MONITOR know to proceed by sending us "MON_CAMP={rx}"
                        // We then respond with "camp={ok},{rx_channel}" to confirm ready to recv msgs
                        send_msg(conn_mon, false, "MSG monitor");
                        init = true;
                    }
                }
                continue;
            }
        
            //cprintf(conn_mon, "CAMP: Q=%d <%s>\n", conn_mon->queued? 1:0, cmd);
        
            int rx;
            if (sscanf(cmd, "SET MON_CAMP=%d", &rx) == 1) {
                //cprintf(conn_mon, "CAMP: SET MON_CAMP=%d\n", rx);

                // remove any previous camping
                bool stop = false;
                for (i = 0; i < MAX_RX_CHANS; i++) {
                    rx_chan_t *rxc = &rx_channels[i];
                    for (j = 0; j < n_camp; j++) {
                        conn_t *c = rxc->camp_conn[j];
                        if (c == conn_mon) {
                            c->camp_init = false;
                            rxc->camp_conn[j] = NULL;
                            rxc->n_camp--;
                            //cprintf(conn_mon, "CAMP: rem rx%d slot=%d/%d\n", i, rxc->camp_id[i], j+1, n_camp);
                            stop = true;
                        }
                    }
                }
            
                if (rx < 0 || rx >= rx_chans || stop) send_msg(conn_mon, false, "MSG audio_camp=1,0");      // audio disconnect
                conn_mon->camped_rx = -1;
                if (rx < 0 || rx >= rx_chans) continue;

                int okay = 0;
                rx_chan_t *rxc = &rx_channels[rx];
                if (rxc->n_camp < n_camp) {
                    for (i = 0; i < n_camp; i++) {
                        conn_t *c = rxc->camp_conn[i];
                        if (c != NULL) continue;
                        rxc->camp_conn[i] = conn_mon;
                        rxc->camp_id[i] = conn_mon->remote_port;
                        //cprintf(conn_mon, "CAMP: add rx%d id=%d slot=%d/%d\n", rx, rxc->camp_id[i], i+1, n_camp);
                        okay = 1;
                        conn_mon->camped_rx = rx;
                        break;
                    }
                    rxc->n_camp++;
                }
            
                cprintf(conn_mon, "MON camp=%d rx=%d\n", okay, rx);
                send_msg(conn_mon, false, "MSG camp=%d,%d", okay, rx);
                continue;
            }

            if (sscanf(cmd, "SET MON_QSET=%d", &i) == 1) {
                conn_mon->queued = (i != 0);
                conn_mon->arrival = conn_mon->queued? timer_sec() : 0;
                continue;
            }

            if (strcmp(cmd, "SET MON_QPOS") == 0) {
                int pos = 0, waiters = 0;
                conn_t *c = conns;
                for (i = 0; i < N_CONNS; i++, c++) {
                    if (c->type == STREAM_MONITOR && c->queued && c->arrival) {
                        if (c->arrival <= conn_mon->arrival)
                            pos++;
                        waiters++;
                    }
                }

                int rx_free = rx_chan_free_count(RX_COUNT_ALL);
                int chan_no_pwd = rx_chan_no_pwd(PWD_CHECK_YES);
                bool chan_avail;
            
                if (chan_no_pwd) {
                    int chan_need_pwd = rx_chans - chan_no_pwd;
            
                    // Allow if minimum number of channels needing password remains.
                    // This minimum number is reduced by the number of already privileged connections.
                    int already_privileged_conns = rx_count_server_conns(LOCAL_OR_PWD_PROTECTED_USERS);
                    int chans_keep_free = chan_need_pwd - already_privileged_conns;
                    if (chans_keep_free < 0) chans_keep_free = 0;
                    //cprintf(conn_mon, "CAMP: QPOS=%d chan_no_pwd=%d chan_need_pwd=%d already_privileged_conns=%d chans_keep_free/rx_free=%d/%d\n",
                    //    pos, chan_no_pwd, chan_need_pwd, already_privileged_conns, chans_keep_free, rx_free);

                    // NB: ">" not ">=" since we will consume a channel immediately upon reload
                    chan_avail = (rx_free > chans_keep_free);
                } else {
                    chan_avail = (rx_free > 0);
                }
            
                bool redirect = false;
                if (!chan_avail) {
                    char *url_redirect = (char *) admcfg_string("url_redirect", NULL, CFG_REQUIRED);
                    if (url_redirect != NULL && *url_redirect != '\0') {
                        redirect = true;
                    }
                    admcfg_string_free(url_redirect);
                }

                static u4_t lockout;
                bool locked = (lockout > timer_sec())? 1:0;
            
                //int reload = (!locked && pos == 1 && (chan_avail || redirect))? 1:0;
                int reload = (!locked && pos == 1 && chan_avail)? 1:0;
                if (reload) lockout = timer_sec() + 10;
                //cprintf(conn_mon, "CAMP: QPOS=%d waiters=%d rx_chans=%d rx_free=%d locked=%d chan_avail=%d redirect=%d reload=%d\n",
                //    pos, waiters, rx_chans, rx_free, locked, chan_avail, redirect, reload);

                send_msg(conn_mon, false, "MSG qpos=%d,%d,%d", pos, waiters, reload);
                continue;
            }
            
            if (strncmp(cmd, "SET rev_bin=", 12) == 0) {
                //cprintf(conn_mon, "MON %d<%s>\n", nb->len, kiwi_str_ASCII_static(nb->buf, 0, nb->len));
                if (camped_rx == -1) {
                    cprintf(conn_mon, "MON no camped chan yet? size=%d\n", nb->len);
                    continue;
                }
        
                // forward to camped channel's audio stream if requested
	            wf_inst_t *wf = &WF_SHMEM->wf_inst[camped_rx];
	            if (!wf) {
                    cprintf(conn_mon, "MON no wf? rx=%d\n", camped_rx);
	                continue;
	            }
                rx_chan_t *rxc = &rx_channels[camped_rx];
                if (wf->want_rtn_snd) {
                    if (!wf->have_rtn_snd) {
                        // first time have returned sound
                        ndesc_init(&rxc->rtn_snd_nd, NULL);
                        ndesc_t *nd = &rxc->rtn_snd_nd;
                        if (nd->magic_b != NDESC_MAGIC_B) {
                            printf("camped_rx=%d\n", camped_rx);
                            panic("mon ndesc");
                        }
                        wf->have_rtn_snd = true;
                        ext_send_msg(camped_rx, false, "EXT have_rtn_snd");
                    }
                    if (rxc == NULL || nb == NULL) continue;   // gone away
                    if (ndesc_valid(&rxc->rtn_snd_nd))
                        nbuf_allocq(&rxc->rtn_snd_nd, nb->buf, nb->len);
                    //real_printf(GREEN "%d:%d" NORM " ", camped_rx, nb->len); fflush(stdout);
                } else {
                    //real_printf(RED "%d:no-want" NORM " ", camped_rx); fflush(stdout);
                }
                continue;
            }
        
            if (strncmp(cmd, "SET rev_txt=", 12) == 0) {
                //kiwi_str_decode_inplace(cmd);
                //cprintf(conn_mon, "MON %d \"%s\"\n", strlen(cmd)-12, cmd);
                //real_printf(CYAN "%d:%d" NORM " ", camped_rx, nb->len-12); fflush(stdout);
                if (camped_rx == -1) {
                    //cprintf(conn_mon, "MON no camped chan yet? %d \"%s\"\n", strlen(cmd)-12, cmd);
                    cprintf(conn_mon, "MON no camped chan yet? size=%d\n", strlen(cmd)-12);
                    continue;
                }
                
                // NB: conn of the camped channel
                conn_t *conn = rx_channels[camped_rx].conn;
                if (conn && conn->ext_cmd != NULL) {
                    conn->ext_cmd(CMD_NO_KEY, cmd, camped_rx);
                } else {
                    // no one registered to receive rev_txt as a cmd
                    //cprintf(conn_mon, "MON no receiver for: %d \"%s\"\n", strlen(cmd)-12, cmd);
                    cprintf(conn_mon, "MON no receiver size=%d\n", strlen(cmd)-12);
                }
                continue;
            }

            //cprintf(conn_mon, "c2s_mon: unknown command: sl=%d %d|%d|%d [%s] ip=%s ==================================\n",
            //    strlen(cmd), cmd[0], cmd[1], cmd[2], cmd, conn_mon->remote_ip);

            continue;       // keep checking until no cmds in queue
        }
		
        // detect camped connection has gone away
		if (conn_mon->camped_rx != -1) {
            rx_chan_t *rxc = &rx_channels[conn_mon->camped_rx];
            conn_t *c = rxc->conn;
            if (c == NULL || !c->valid || c->type != STREAM_SOUND) {
                /*
                if (c == NULL)
                    cprintf(conn_mon, "CAMP: channel gone rx%d c=NULL id=?/%d slot=%d/%d\n",
                        conn_mon->camped_rx, rxc->camp_id[i], i+1, n_camp);
                else
                    cprintf(conn_mon, "CAMP: channel gone rx%d type=%d id=%d/%d slot=%d/%d\n",
                        conn_mon->camped_rx, c->type, c->remote_port, rxc->camp_id[i], i+1, n_camp);
                */
                send_msg(conn_mon, false, "MSG camp_disconnect");
                conn_mon->camped_rx = -1;
                conn_mon->camp_init = false;
            }
		}
		
		conn_mon->keep_alive = conn_mon->internal_connection? 0 : (timer_sec() - conn_mon->keepalive_time);
		bool keepalive_expired = (conn_mon->keep_alive > KEEPALIVE_SEC);
		if (keepalive_expired || conn_mon->kick) {
			rx_server_remove(conn_mon);
			panic("shouldn't return");
		}
		
		TaskSleepReasonMsec("mon-cmd", 200);
	}
}
