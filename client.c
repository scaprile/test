// Copyright (c) 2024 Cesanta Software Limited
// All rights reserved

#include "mongoose.h"

#include "servers.h"
#define CONNS 10

static const uint64_t s_timeout_ms = 500;  // Connect timeout in milliseconds
static const uint64_t s_next_ms = 997;     // next request time in milliseconds

// resources
struct res_s {
  unsigned int id;
  unsigned int count;
  struct mg_mgr *mgr;
  char url[50];
  struct mg_tls_opts opts;
  bool connected;
  bool *pconnected;
};

// Send request
static void sreq(struct mg_connection *c, const char *url) {
    struct mg_str host = mg_url_host(url);
    mg_printf(c,
              "%s %s HTTP/1.0\r\n"
              "Host: %.*s\r\n"
              "Content-Type: octet-stream\r\n"
              "Content-Length: %d\r\n"
              "\r\n",
              "GET", mg_url_uri(url), (int) host.len,
              host.buf, 0);
}

// event handler
static void fn(struct mg_connection *c, int ev, void *ev_data) {
  struct res_s *res = (struct res_s *) c->fn_data;
  if (ev == MG_EV_OPEN) {
    // Connection created. Store connect expiration time in c->data
    *(uint64_t *) c->data = mg_millis() + s_timeout_ms;
    MG_INFO(("CLIENT %u is connecting", res->id));
  } else if (ev == MG_EV_CONNECT) {
    MG_INFO(("CLIENT %u connected to %s", res->id, res->url));
    if (mg_url_is_ssl(res->url)) {
      mg_tls_init(c, &res->opts);
    }
    sreq(c, res->url);
    *(uint64_t *) c->data = mg_millis() + s_next_ms;
  } else if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    char lastchar = res->url[strlen(res->url) - 1];
    MG_INFO(("CLIENT %u got a response: %.*s", res->id, 20, hm->body.buf));
    if(lastchar >= '0' && lastchar <= '9') { // close request
      c->is_draining = 1;        // Tell mongoose to close this connection
    } // else stay open until the server closes
  } else if (ev == MG_EV_CLOSE) {
    MG_INFO(("CLIENT %u disconnected", res->id));
    free(res);
  } else if (ev == MG_EV_ERROR) {
    MG_INFO(("CLIENT %u error: %s", res->id, (char *) ev_data));
  } else if (ev == MG_EV_POLL) {
    *res->pconnected = true;
    if (mg_millis() > *(uint64_t *) c->data) {
      if (c->is_connecting || c->is_resolving) {
        mg_error(c, "Connect timeout");
        MG_INFO(("CLIENT %u connect failed %s", res->id, (char *) ev_data));
      } else {
        sreq(c, res->url);
        *(uint64_t *) c->data = mg_millis() + s_next_ms;
      }
    }
  }
}

// Timer function - connect to server
static void timer_fn(void *arg) {
  struct res_s *res = (struct res_s *) arg;
  if (res->count >= CONNS) {
    if (!res->connected) // monitor for connections
      res->count = 0;    // to restart
    res->connected = false;
    if (res->count == CONNS && res->id != 0) {
      struct res_s *cres = (struct res_s *) malloc(sizeof(*res));
      memcpy(cres, res, sizeof(*cres));
      cres->id = 100 * res->id + res->count;
      cres->pconnected = &res->connected;
      sprintf(cres->url + strlen(cres->url), "/api/close/%u", res->id - 1);
      mg_http_connect(res->mgr, cres->url, fn, cres);
      ++res->count;
    }
  } else {
    struct res_s *cres = (struct res_s *) malloc(sizeof(*res));
    memcpy(cres, res, sizeof(*cres));
    cres->id = 100 * res->id + res->count;
    cres->pconnected = &res->connected;
    strcat(cres->url, "/api/stats");
    mg_http_connect(res->mgr, cres->url, fn, cres);
    ++res->count;
  }
}


int main(void) {
  struct mg_mgr mgr;              // Event manager
  struct res_s res[SERVERS];

  mg_log_set(MG_LL_INFO);  // Set log level
  mg_mgr_init(&mgr);       // Initialize event manager

  for (unsigned int i = 0; i < SERVERS; i++) {
    res[i].id = i;
    res[i].count = 0;
    res[i].mgr = &mgr;
    strcpy(res[i].url, s_https_addr[i]);
    memset(&(res[i].opts), 0, sizeof(res[i].opts));
    res[i].opts.ca = mg_unpacked("/certs/ss_ca.pem");
    res[i].opts.name = mg_url_host(res[i].url);
    mg_timer_add(&mgr, (113 * i + 1) + 11, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_fn, &res[i]);
  }

  while (true) mg_mgr_poll(&mgr, 10);

  mg_mgr_free(&mgr);                        // Free resources
  return 0;
}
