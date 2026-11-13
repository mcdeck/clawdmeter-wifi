#include "settings_server.h"
#include "provisioning.h"
#include "hook_event.h"
#include "clawd_prefs.h"
#include "clawd_director.h"
#include "feature_pref.h"
#include "clock_pref.h"
#include "pace_pref.h"
#include "button_pref.h"
#include "theme_pref.h"
#include "theme_presets.h"
#include "tz_list.h"
#include "brightness.h"
#include "chime_pref.h"
#include "ui.h"
#include "wifi_poller.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <Arduino.h>

static WebServer s_server(80);
static bool      s_running = false;
static bool      s_restart_pending = false;

// ---- HTML template -------------------------------------------------------

static const char SETTINGS_HTML[] = R"html(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ClawdMeter Settings</title>
<style>
body{font-family:sans-serif;margin:0;background:#1a1a1a;color:#e8e0d4}
.page-wrap{display:flex;gap:0;max-width:720px;margin:40px auto;padding:0 20px;align-items:flex-start}
.nav-toc{flex-shrink:0;width:130px;position:sticky;top:20px;padding-right:24px}
.nav-toc a{display:block;font-size:.8em;color:#555;text-decoration:none;padding:5px 0 5px 10px;border-left:2px solid #2a2a2a;margin-bottom:2px;transition:color .15s,border-color .15s}
.nav-toc a:hover{color:#aaa;border-left-color:#555}
.nav-toc a.active{color:#e8e0d4;border-left-color:#d97757}
.content{flex:1;min-width:0}
@media(max-width:659px){.nav-toc{display:none}}
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
input[type="checkbox"],input[type="radio"]{width:auto;margin-right:6px;vertical-align:middle}
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
<div class="page-wrap">
<nav class="nav-toc">
<a href="#security">Security</a>
<a href="#clawd">Clawd</a>
<a href="#device">Device</a>
<a href="#connection">Connection</a>
</nav>
<div class="content">
<h1>ClawdMeter</h1>
<p class="sub">Settings &middot; <span id="host">clawdmeter.local</span></p>

%BANNER%

<section id="security">
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
</section>

%CLAWD_SECTION%

%DEVICE_SETTINGS%

%CONNECTION_SECTION%

</div>
</div>
<script>
document.getElementById('host').textContent = location.host;
function gen() {
  const a = new Uint8Array(16);
  crypto.getRandomValues(a);
  document.getElementById('evtok').value =
    Array.from(a).map(b => b.toString(16).padStart(2,'0')).join('');
}
var secs = document.querySelectorAll('section[id]');
function markActive(){
  var best = null, bst = -Infinity;
  secs.forEach(function(s){
    var t = s.getBoundingClientRect().top;
    if(t <= 80 && t > bst){bst=t;best=s.id;}
  });
  document.querySelectorAll('.nav-toc a').forEach(function(a){
    a.classList.toggle('active', a.getAttribute('href')==='#'+best);
  });
}
window.addEventListener('scroll',markActive,{passive:true});
markActive();
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
    bool fp_shows = feature_pref_shows();
    bool fp_scrn  = feature_pref_screensaver();
    bool fp_idle  = feature_pref_idle_acts();
    bool fp_react = feature_pref_reactions();
    char clawd[3072];
    snprintf(clawd, sizeof(clawd),
        "<section id=\"clawd\">"
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
        "<p class=\"note\" style=\"margin-top:14px\">Features (apply on top of liveliness)</p>"
        "<label><input type=\"checkbox\" name=\"feat_shows\"%s> Shows &mdash; earned performance after a busy stretch</label>"
        "<label><input type=\"checkbox\" name=\"feat_scrn\"%s> Screensaver &mdash; face rests when you&rsquo;re away</label>"
        "<label><input type=\"checkbox\" name=\"feat_idle\"%s> Idle acts &mdash; corner autonomy between tasks</label>"
        "<label><input type=\"checkbox\" name=\"feat_react\"%s> Reactions &mdash; notices prompts, waves on long turns</label>"
        "<br><button type=\"submit\">Save Clawd settings</button>"
        "</form></section>",
        lv==1?" selected":"", lv==2?" selected":"", lv==3?" selected":"",
        lv==4?" selected":"", lv==5?" selected":"",
        attn==120?" selected":"", attn==30?" selected":"",
        attn==10?" selected":"", attn==0?" selected":"",
        fp_shows?" checked":"", fp_scrn?" checked":"",
        fp_idle?" checked":"", fp_react?" checked":""
    );
    html.replace("%CLAWD_SECTION%", clawd);

    // ---- Device settings section ----
    String ds;
    ds.reserve(4096);
    ds += "<section id=\"device\"><h2>Device settings</h2>"
          "<form method=\"POST\" action=\"/save-settings\">";

    // Clock
    bool clk_on = clock_pref_on();
    int  clk_fmt = clock_pref_fmt();
    const char* clk_tz      = clock_pref_tz();
    const char* clk_tz_name = clock_pref_tz_name();
    ds += "<label><input type=\"checkbox\" name=\"clk_on\"";
    if (clk_on) ds += " checked";
    ds += "> Show clock (replaces &ldquo;Usage&rdquo; title)</label>";
    ds += "<div style=\"margin:8px 0\">"
          "<label style=\"display:inline\"><input type=\"radio\" name=\"clk_fmt\" value=\"24\"";
    if (clk_fmt != 12) ds += " checked";
    ds += "> 24-hour</label>&nbsp;&nbsp;"
          "<label style=\"display:inline\"><input type=\"radio\" name=\"clk_fmt\" value=\"12\"";
    if (clk_fmt == 12) ds += " checked";
    ds += "> 12-hour (AM/PM)</label></div>";

    // Timezone select grouped by region (prefix before '/')
    ds += "<select name=\"clk_tz\" style=\"margin-bottom:16px\">";
    char cur_region[32] = "";
    for (int i = 0; i < tz_list_count; i++) {
        // Extract region (prefix before first '/')
        const char* slash = strchr(tz_list[i].name, '/');
        char region[32] = "Other";
        if (slash) {
            int len = (int)(slash - tz_list[i].name);
            if (len < (int)sizeof(region)) {
                memcpy(region, tz_list[i].name, len);
                region[len] = '\0';
            }
        } else {
            strlcpy(region, tz_list[i].name, sizeof(region));
        }
        if (strcmp(region, cur_region) != 0) {
            if (cur_region[0]) ds += "</optgroup>";
            char grp[64];
            snprintf(grp, sizeof(grp), "<optgroup label=\"%s\">", region);
            ds += grp;
            strlcpy(cur_region, region, sizeof(cur_region));
        }
        bool sel = (clk_tz_name[0] != '\0')
            ? (strcmp(tz_list[i].name, clk_tz_name) == 0)
            : (strcmp(tz_list[i].posix, clk_tz) == 0);
        char opt[128];
        snprintf(opt, sizeof(opt), "<option value=\"%s\"%s>%s</option>",
                 tz_list[i].name, sel ? " selected" : "",
                 slash ? slash + 1 : tz_list[i].name);
        ds += opt;
    }
    if (cur_region[0]) ds += "</optgroup>";
    ds += "</select>";

    // Pace marker
    bool pace_on = pace_pref_get();
    ds += "<label><input type=\"checkbox\" name=\"pace_on\"";
    if (pace_on) ds += " checked";
    ds += "> Show pace ticks on usage bars</label>"
          "<p class=\"note\">Marks how much of the usage window has elapsed.</p>";

    // Buttons
    static const char* const btn_labels[] = {
        "None", "Space (voice PTT)", "Shift+Tab (mode toggle)", "Tab", "Enter", "Escape"
    };
    static const char* const slot_names[] = {
        "btn_k1", "btn_k2", "btn_k3"
    };
    static const char* const slot_labels[] = {
        "Primary button", "Secondary button", "PWR button"
    };
    for (int s = 0; s < 3; s++) {
        int cur = (int)button_pref_get((ButtonSlot)s);
        ds += "<label>";
        ds += slot_labels[s];
        ds += "<select name=\"";
        ds += slot_names[s];
        ds += "\">";
        for (int v = 0; v <= 5; v++) {
            ds += "<option value=\"";
            ds += v;
            ds += "\"";
            if (v == cur) ds += " selected";
            ds += ">";
            ds += btn_labels[v];
            ds += "</option>";
        }
        ds += "</select></label>";
    }

    // Theme
    const char* cur_theme = theme_pref_current_id();
    ds += "<label>Theme<select name=\"theme_id\">";
    for (int i = 0; i < theme_preset_count; i++) {
        char opt[128];
        snprintf(opt, sizeof(opt), "<option value=\"%s\"%s>%s</option>",
                 theme_presets[i].id,
                 strcmp(theme_presets[i].id, cur_theme) == 0 ? " selected" : "",
                 theme_presets[i].name);
        ds += opt;
    }
    ds += "</select></label>";

    // Brightness
    int brt = brightness_get();
    char brt_buf[128];
    snprintf(brt_buf, sizeof(brt_buf),
             "<label>Brightness<input type=\"range\" name=\"brt_lvl\" "
             "min=\"10\" max=\"255\" value=\"%d\"></label>", brt);
    ds += brt_buf;

    // Reset chime
    bool chime_on = chime_pref_get();
    ds += "<label><input type=\"checkbox\" name=\"chime\"";
    if (chime_on) ds += " checked";
    ds += "> Reset chime (rings when session window resets)</label>";

    ds += "<br><button type=\"submit\">Save device settings</button>"
          "</form></section>";
    html.replace("%DEVICE_SETTINGS%", ds);

    // ---- Connection section ----
    // Token and SSID are never pre-filled: a truncated or partial value
    // submitted back would silently overwrite the real credential in NVS.
    String ssid = provisioning_get_ssid();
    bool   has_token = provisioning_get_token().length() > 0;
    bool   no_rev = provisioning_no_revert();
    String cs;
    cs.reserve(1024);
    cs += "<section id=\"connection\"><h2>Connection</h2>"
          "<form method=\"POST\" action=\"/save-connection\">"
          "<label>SSID<input type=\"text\" name=\"ssid\" placeholder=\"";
    cs += ssid.length() ? ssid : "(not configured)";
    cs += "\" autocomplete=\"off\"></label>"
          "<label>Password<input type=\"password\" name=\"pass\" placeholder=\"leave blank to keep\" autocomplete=\"new-password\"></label>"
          "<label>Anthropic API Token<input type=\"password\" name=\"token\" placeholder=\"";
    cs += has_token ? "set \xe2\x80\x94 leave blank to keep" : "(not set)";
    cs += "\" autocomplete=\"off\"></label>"
          "<label><input type=\"checkbox\" name=\"safety_revert\"";
    if (!no_rev) cs += " checked";
    cs += "> Try new credentials for 30 s; revert if connection fails (recommended)</label>"
          "<p class=\"note\">Leave SSID and password blank to update the token only.</p>"
          "<button type=\"submit\">Save &amp; Connect</button>"
          "</form></section>";
    html.replace("%CONNECTION_SECTION%", cs);

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

static void handle_save_settings(void) {
    if (!check_web_auth()) return;

    // Clock — arg is now an IANA name (e.g. "Europe/Berlin"); look up its POSIX string
    bool clk_on  = s_server.hasArg("clk_on");
    int  clk_fmt = s_server.arg("clk_fmt") == "12" ? 12 : 24;
    String clk_tz_name_arg = s_server.arg("clk_tz");
    const char* posix = "UTC0";
    for (int i = 0; i < tz_list_count; i++) {
        if (strcmp(tz_list[i].name, clk_tz_name_arg.c_str()) == 0) {
            posix = tz_list[i].posix;
            break;
        }
    }
    clock_pref_set(clk_on, clk_fmt, posix, clk_tz_name_arg.c_str());

    // Pace
    pace_pref_set(s_server.hasArg("pace_on"));

    // Buttons
    button_pref_set(BTN_SLOT_PRIMARY,   s_server.arg("btn_k1").toInt());
    button_pref_set(BTN_SLOT_SECONDARY, s_server.arg("btn_k2").toInt());
    button_pref_set(BTN_SLOT_PWR,       s_server.arg("btn_k3").toInt());

    // Theme
    String theme_id = s_server.arg("theme_id");
    if (theme_id.length() > 0) theme_pref_set_by_name(theme_id.c_str());

    // Brightness
    int brt = s_server.arg("brt_lvl").toInt();
    if (brt < 10)  brt = 10;
    if (brt > 255) brt = 255;
    brightness_set((uint8_t)brt);

    // Chime
    chime_pref_set(s_server.hasArg("chime"));

    ui_settings_refresh();

    s_server.sendHeader("Location", "/?saved=1");
    s_server.send(303);
}

static void handle_save_connection(void) {
    if (!check_web_auth()) return;

    // Token: only save if the user typed something (never pre-filled, so
    // a non-empty value is always a real new token from the user).
    String token = s_server.arg("token");
    if (token.length() > 0) {
        provisioning_save_wifi(provisioning_get_ssid().c_str(),
                               provisioning_get_pass().c_str(),
                               token.c_str());
    }

    String new_ssid = s_server.arg("ssid");
    String new_pass = s_server.arg("pass");
    bool safety = s_server.hasArg("safety_revert");
    provisioning_set_no_revert(!safety);

    // Only attempt a reconnect if at least one credential field was provided
    // AND the resulting credentials differ from what is already stored.
    if (new_ssid.length() > 0 || new_pass.length() > 0) {
        if (new_ssid.length() == 0) new_ssid = provisioning_get_ssid();
        if (new_pass.length() == 0) new_pass = provisioning_get_pass();
        if (new_ssid != provisioning_get_ssid() || new_pass != provisioning_get_pass()) {
            provisioning_save_pending_wifi(new_ssid.c_str(), new_pass.c_str());
            // Defer restart until after the 303 response has been sent so
            // wifi_poller_restart() / WiFi.begin() can't drop the socket early.
            s_restart_pending = true;
        }
    }

    s_server.sendHeader("Location", "/?saved=1");
    s_server.send(303);
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
            feature_pref_set_shows(s_server.hasArg("feat_shows"));
            feature_pref_set_screensaver(s_server.hasArg("feat_scrn"));
            feature_pref_set_idle_acts(s_server.hasArg("feat_idle"));
            feature_pref_set_reactions(s_server.hasArg("feat_react"));
            director_apply_feature_prefs();
            s_server.sendHeader("Location", "/?saved=1");
            s_server.send(303);
        });
        s_server.on("/save-settings",         HTTP_POST, handle_save_settings);
        s_server.on("/save-connection",       HTTP_POST, handle_save_connection);
        s_server.on("/event",                 HTTP_POST, handle_event);
        s_server.on("/clawd",                 HTTP_POST, []() {
            if (!check_event_token()) return;
            String beat = s_server.arg("beat");
            if (beat.length() == 0) {
                s_server.send(400, "text/plain", "Missing beat parameter\n");
                return;
            }
            // attention accepts an optional msg= override so hook integrations
            // can show a meaningful string instead of the QA placeholder.
            if (beat == "attention") {
                String msg = s_server.arg("msg");
                director_alert(msg.length() > 0 ? msg.c_str() : "Clawdmeter test alert");
                s_server.send(204, "text/plain", "");
                return;
            }
            bool ok = director_debug(beat.c_str());
            s_server.send(ok ? 204 : 409, "text/plain", ok ? "" : "Beat refused (wrong state)\n");
        });
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
    if (s_restart_pending) {
        s_restart_pending = false;
        wifi_poller_restart();
    }
    if (!s_running) return;
    s_server.handleClient();
}
