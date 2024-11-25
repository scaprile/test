// Copyright (c) 2024 Cesanta Software Limited
// All rights reserved

#include "mongoose.h"

#include "servers.h"

static const char *s_root_dir = ".";


// resources
struct res_s {
  struct mg_connection *c;
  const char *addr;
  struct mg_tls_opts opts;
  bool must_close;
  bool connected;
  uint8_t close_this;
  uint8_t id;
};

// event handler
static void fn(struct mg_connection *c, int ev, void *ev_data) {
  struct res_s *res = (struct res_s *) c->fn_data;
  if (ev == MG_EV_OPEN && c->is_listening == 1) {
    MG_INFO(("SERVER %u is listening", res->id));
  } else if (ev == MG_EV_ACCEPT) {
    MG_INFO(("SERVER %u accepted a connection", res->id));
    if (mg_url_is_ssl(res->addr)) {
      mg_tls_init(c, &res->opts);
    }
  } else if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    MG_INFO(("SERVER %u got a request for %.*s", res->id, hm->uri.len, hm->uri.buf));
    if (mg_match(hm->uri, mg_str("/api/stats"), NULL)) {
      struct mg_connection *t;
      mg_printf(c, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
      mg_http_printf_chunk(c, "ID PROTO TYPE      LOCAL           REMOTE\n");
      for (t = c->mgr->conns; t != NULL; t = t->next) {
        mg_http_printf_chunk(c, "%-3lu %4s %s %M %M\n", t->id,
                             t->is_udp ? "UDP" : "TCP",
                             t->is_listening  ? "LISTENING"
                             : t->is_accepted ? "ACCEPTED "
                                              : "CONNECTED",
                             mg_print_ip, &t->loc, mg_print_ip, &t->rem);
      }
      mg_http_printf_chunk(c, "");  // Don't forget the last empty chunk
    } else if (mg_match(hm->uri, mg_str("/api/close/*"), NULL)) {
      uint8_t target = hm->uri.buf[11] - '0'; // one digit, hardcoded to URI
      bool ok = target < 10;
      if (ok) {
        MG_INFO(("SERVER %u received request to close server %u", res->id, target));
        res->close_this = target;
      }
      mg_http_reply(c, 200, "", "{%m: %m}", MG_ESC("result"), MG_ESC(ok ? "OK" : "ERROR"));
    } else {
      struct mg_http_serve_opts opts;
      memset(&opts, 0, sizeof(opts));
      opts.root_dir = s_root_dir;
      mg_http_serve_dir(c, ev_data, &opts);
    }
  } else if (ev == MG_EV_CLOSE) {
    MG_INFO(("SERVER %u %sdisconnected", res->id, c->is_listening ? "listener " : ""));
  } else if (ev == MG_EV_ERROR) {
    MG_INFO(("SERVER %u %serror: %s", res->id, c->is_listening ? "listener " : "", (char *) ev_data));
  } else if (ev == MG_EV_POLL) {
    if (c->is_listening) { // don't close the listener !
      if (res->must_close && !res->connected) // monitor for child connections
        res->must_close = false; // all disconnected
      res->connected = false;
    } else {
      res->connected = true;
      if (res->must_close) {
        c->is_closing = 1;
        MG_INFO(("SERVER %u closing on command", res->id));
      }
    }
  }
}


int main(void) {
  struct mg_mgr mgr;  // Event manager
  struct res_s res[SERVERS];

  mg_log_set(MG_LL_INFO);  // Set log level
  mg_mgr_init(&mgr);       // Initialize event manager

  for (unsigned int i = 0; i < SERVERS; i++) {
    res[i].id = (uint8_t) i;
    res[i].c = NULL;
    res[i].addr = s_https_addr[i];
    res[i].must_close = false;
    res[i].connected = false;
    res[i].close_this = 0xFF;
    memset(&(res[i].opts), 0, sizeof(res[i].opts));
    res[i].opts.cert = mg_unpacked("/certs/ss_server.pem");
    res[i].opts.key = mg_unpacked("/certs/ss_server.pem");
    res[i].c = mg_http_listen(&mgr, res[i].addr, fn, &res[i]);
    MG_INFO(("SERVER %u start: %s", res[i].id, res[i].c != NULL ? "OK" : "*FAIL*"));
  }

  while (true) {
    mg_mgr_poll(&mgr, 10);   // Infinite event loop, blocks for upto 100ms
                             // unless there is network activity
    for (unsigned int i = 0; i < SERVERS; i++) {
      if (res[i].c == NULL || res[i].close_this == 0xFF) continue;
      MG_INFO(("MGR parsed request from server %u to close server %u", i, res[i].close_this));
      if (res[i].close_this < SERVERS) {
        res[res[i].close_this].must_close = true;
        MG_INFO(("MGR sent must_close command to server %u", res[i].close_this));
        // change anything you must change inside "res" for that server;
        // all connections will be closed,
        // new connections will use these new parameters.
      } else {
        MG_ERROR(("MGR says server %u does not exist", res[i].close_this));
      }
      res[i].close_this = 0xFF;
    }
  }
  mg_mgr_free(&mgr);         // Free resources
  return 0;
}
