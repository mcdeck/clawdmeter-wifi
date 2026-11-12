#include "settings_server.h"
#include "provisioning.h"
#include "hook_event.h"
#include "clawd_prefs.h"
#include "clawd_director.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <Arduino.h>

static WebServer s_server(80);
static bool      s_running = false;

// ---- HTML template -------------------------------------------------------

static const char SETTINGS_HTML[] = R"html(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ClawdMeter Settings</title>
<style>
body{font-family:sans-serif;max-width:520px;margin:40px auto;padding:0 20px;
     background:#1a1a1a;color:#e8e0d4}
h1{margin-bottom:4px}
.sub{color:#888;font-size:.9em;margin:0 0 32px}
h2{font-size:.85em;color:#888;text-transform:uppercase;letter-spacing:.08em;
   margin:28px 0 14px;padding-bottom:8px;border-bottom:1px solid #2a2a2a}
label{display:block;font-size:.85em;color:#888;margin-bottom:4px}
.info{font-size:.8em;margin-bottom:10px}
.info.ok{color:#7abf8e}
.info.dim{color:#555}
input{width:100%;box-sizing:border-box;padding:10px 12px;border:1px solid #444;
     border-radius:6px;background:#252525;color:#e8e0d4;font-size:1em}
select{width:100%;box-sizing:border-box;padding:10px 12px;border:1px solid #444;
       border-radius:6px;background:#252525;color:#e8e0d4;font-size:1em;margin-bottom:12px}
.row{display:flex;gap:8px;align-items:stretch;margin-bottom:4px}
.row input{margin:0;flex:1;min-width:0}
button{padding:10px 18px;background:#d97757;border:none;border-radius:6px;
       color:#fff;font-size:.95em;cursor:pointer;font-weight:600;white-space:nowrap}
button.sec{background:#333}
.note{font-size:.8em;color:#555;line-height:1.5;margin:6px 0 20px}
.banner{padding:10px 14px;border-radius:6px;margin-bottom:20px;font-size:.9em}
.banner.ok{background:#1e3a2a;color:#7abf8e}
code{background:#252525;padding:2px 5px;border-radius:3px;font-size:.9em}
form{margin:0 0 8px}
</style>
</head>
<body>
<h1>ClawdMeter</h1>
<p class="sub">Settings &middot; <span id="host">clawdmeter.local</span></p>

%BANNER%

<h2>Security</h2>

<form method="POST" action="/settings/webpass">
  <label>Web UI Password</label>
  <p class="info %WEB_PASS_CLASS%">%WEB_PASS_STATUS%</p>
  <div class="row">
    <input type="password" name="web_pass"
           placeholder="New password (empty = remove protection)"
           autocomplete="new-password">
    <button type="submit">Save</button>
  </div>
  <p class="note">
    Protects this settings page with HTTP Basic Auth (username: <code>admin</code>).<br>
    To reset if locked out: serial console &rarr; <code>webpass</code> (clears password).<br>
    <em>Credentials are sent in plaintext on your local network (HTTP, not HTTPS).</em>
  </p>
</form>

<form method="POST" action="/settings/eventtoken">
  <label>Event API Token</label>
  <div class="row">
    <input type="text" name="event_token" id="evtok"
           placeholder="Leave empty to allow unauthenticated POSTs"
           value="%EVENT_TOKEN%">
    <button type="button" class="sec" onclick="gen()">Generate</button>
    <button type="submit">Save</button>
  </div>
  <p class="note">
    Bearer token for <code>POST /event</code> from Claude Code hooks.<br>
    Copy the token into your hook command:<br>
    <code>curl -sX POST http://clawdmeter.local/event \<br>
    &nbsp;&nbsp;-H "Authorization: Bearer &lt;token&gt;" \<br>
    &nbsp;&nbsp;-d "t=p&amp;es=1&amp;n=1&amp;a=2"</code>
  </p>
</form>

%CLAWD_SECTION%

<script>
document.getElementById('host').textContent = location.host;
function gen() {
  const a = new Uint8Array(16);
  crypto.getRandomValues(a);
  document.getElementById('evtok').value =
    Array.from(a).map(b => b.toString(16).padStart(2,'0')).join('');
}
</script>
</body>
</html>)html";

// ---- Auth helpers --------------------------------------------------------

static bool check_web_auth(void) {
    String pass = provisioning_get_webpass();
    if (pass.length() == 0) return true;
    if (s_server.authenticate("admin", pass.c_str())) return true;
    s_server.requestAuthentication(BASIC_AUTH, "ClawdMeter");
    return false;
}

static bool check_event_token(void) {
    String tok = provisioning_get_event_token();
    if (tok.length() == 0) return true;
    String auth = s_server.header("Authorization");
    if (auth.startsWith("Bearer ") && auth.substring(7) == tok) return true;
    s_server.send(401, "text/plain", "Unauthorized");
    return false;
}

// ---- Route handlers ------------------------------------------------------

static String build_page(const char* banner) {
    String html = SETTINGS_HTML;
    bool   has_pass = provisioning_get_webpass().length() > 0;
    html.replace("%BANNER%", banner ? banner : "");
    html.replace("%WEB_PASS_CLASS%",  has_pass ? "ok"  : "dim");
    html.replace("%WEB_PASS_STATUS%", has_pass ? "Currently protected" : "Currently no password");
    html.replace("%EVENT_TOKEN%", provisioning_get_event_token());

    // Build Clawd section with live NVS values pre-selected
    int lv   = liveliness_get();
    int attn = clawd_attn_anim();
    char clawd[2048];
    snprintf(clawd, sizeof(clawd),
        "<section>"
        "<h2>Clawd</h2>"
        "<form method=\"post\" action=\"/settings/clawd\">"
        "<label>Liveliness<br>"
        "<select name=\"liveliness\">"
        "<option value=\"1\"%s>1 &mdash; No-nonsense</option>"
        "<option value=\"2\"%s>2 &mdash; Mostly quiet</option>"
        "<option value=\"3\"%s>3 &mdash; Some nonsense (default)</option>"
        "<option value=\"4\"%s>4 &mdash; Lively</option>"
        "<option value=\"5\"%s>5 &mdash; All-nonsense</option>"
        "</select></label>"
        "<label>Attention animation<br>"
        "<select name=\"attn_anim\">"
        "<option value=\"120\"%s>120 s (default)</option>"
        "<option value=\"30\"%s>30 s</option>"
        "<option value=\"10\"%s>10 s</option>"
        "<option value=\"0\"%s>Off</option>"
        "</select></label>"
        "<button type=\"submit\">Save Clawd settings</button>"
        "</form></section>",
        lv==1?" selected":"", lv==2?" selected":"", lv==3?" selected":"",
        lv==4?" selected":"", lv==5?" selected":"",
        attn==120?" selected":"", attn==30?" selected":"",
        attn==10?" selected":"", attn==0?" selected":""
    );
    html.replace("%CLAWD_SECTION%", clawd);

    return html;
}

static void handle_root(void) {
    if (!check_web_auth()) return;
    bool saved = s_server.hasArg("saved");
    const char* banner = saved
        ? "<div class='banner ok'>&#10003; Saved.</div>"
        : nullptr;
    String html = build_page(banner);
    s_server.send(200, "text/html; charset=utf-8", html);
}

static void handle_save_webpass(void) {
    if (!check_web_auth()) return;
    String pass = s_server.arg("web_pass");
    provisioning_save_webpass(pass.c_str());
    s_server.sendHeader("Location", "/?saved=1");
    s_server.send(302, "text/plain", "");
}

static void handle_save_eventtoken(void) {
    if (!check_web_auth()) return;
    String tok = s_server.arg("event_token");
    provisioning_save_event_token(tok.c_str());
    s_server.sendHeader("Location", "/?saved=1");
    s_server.send(302, "text/plain", "");
}

// POST /event  body (URL-encoded): t=p&es=42&n=1&a=2
//   t  — event type: p=prompt, s=stop, b=back
//   es — event sequence number
//   n  — session count (-1 or omit = unknown)
//   a  — activity level 0–3 (-1 or omit = unknown)
static void handle_event(void) {
    if (!check_event_token()) return;

    String t  = s_server.arg("t");
    String es = s_server.arg("es");
    String n  = s_server.arg("n");
    String a  = s_server.arg("a");

    char type = '\0';
    if (t == "p") type = 'p';
    else if (t == "s") type = 's';
    else if (t == "b") type = 'b';

    g_hook_event.type     = type;
    g_hook_event.seq      = (uint32_t)es.toInt();
    g_hook_event.sessions = n.length() ? (int)n.toInt() : -1;
    g_hook_event.activity = a.length() ? (int)a.toInt() : -1;
    g_hook_event.pending  = true;

    s_server.send(204, "text/plain", "");
}

// ---- Lifecycle -----------------------------------------------------------

void settings_server_init(void) {
    // Register the Authorization header for collection before begin().
    static const char* s_hdrs[] = {"Authorization"};
    s_server.collectHeaders(s_hdrs, 1);

    static bool s_routes_done = false;
    if (!s_routes_done) {
        s_routes_done = true;
        s_server.on("/",                      HTTP_GET,  handle_root);
        s_server.on("/settings/webpass",      HTTP_POST, handle_save_webpass);
        s_server.on("/settings/eventtoken",   HTTP_POST, handle_save_eventtoken);
        s_server.on("/settings/clawd",        HTTP_POST, []() {
            if (!check_web_auth()) return;
            int lv   = s_server.arg("liveliness").toInt();
            int attn = s_server.arg("attn_anim").toInt();
            liveliness_set(lv);
            director_set_liveliness(lv);  // propagate to running director immediately
            attn_anim_set(attn);
            s_server.sendHeader("Location", "/?saved=1");
            s_server.send(303);
        });
        s_server.on("/event",                 HTTP_POST, handle_event);
        s_server.onNotFound([]() {
            s_server.sendHeader("Location", "/");
            s_server.send(302, "text/plain", "");
        });
    }
}

void settings_server_start(void) {
    if (s_running) return;
    s_running = true;

    if (MDNS.begin("clawdmeter")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("settings: mDNS started — http://clawdmeter.local");
    } else {
        Serial.println("settings: mDNS failed");
    }

    s_server.begin();
    Serial.printf("settings: server started, IP=%s\n",
                  WiFi.localIP().toString().c_str());
}

void settings_server_stop(void) {
    if (!s_running) return;
    s_running = false;
    s_server.stop();
    MDNS.end();
    Serial.println("settings: server stopped");
}

void settings_server_tick(void) {
    if (!s_running) return;
    s_server.handleClient();
}
