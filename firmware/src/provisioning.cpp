#include "provisioning.h"
#include <Preferences.h>
#include <freertos/semphr.h>

#define NVS_NS "clawdmeter"

static SemaphoreHandle_t s_mutex = nullptr;
static String s_token;
static String s_ssid;
static String s_pass;
static String s_web_pass;
static String s_event_token;

static void save_and_cache(const char* key, const char* value, String& cache) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString(key, value);
    prefs.end();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    cache = value;
    xSemaphoreGive(s_mutex);
    Serial.printf("prov: %s saved\n", key);
}

void provisioning_init(void) {
    s_mutex = xSemaphoreCreateMutex();
    Preferences prefs;
    prefs.begin(NVS_NS, true);  // read-only
    s_token       = prefs.getString("token",       "");
    s_ssid        = prefs.getString("ssid",        "");
    s_pass        = prefs.getString("pass",        "");
    s_web_pass    = prefs.getString("web_pass",    "");
    s_event_token = prefs.getString("event_token", "");
    prefs.end();
    Serial.printf("prov: token=%s ssid=%s pass=%s webpass=%s event_token=%s\n",
        s_token.length()       ? s_token.substring(0, 20).c_str() : "(none)",
        s_ssid.length()        ? s_ssid.c_str()                   : "(none)",
        s_pass.length()        ? "***"                            : "(none)",
        s_web_pass.length()    ? "set"                            : "(none)",
        s_event_token.length() ? "set"                            : "(none)");
}

void provisioning_handle_cmd(const char* cmd) {
    // Split on first space: verb [value]
    const char* sp = strchr(cmd, ' ');
    String verb = sp ? String(cmd).substring(0, sp - cmd) : String(cmd);
    const char* value = sp ? sp + 1 : "";

    if (verb == "token") {
        if (*value == '\0') { Serial.println("prov: usage: token <value>"); return; }
        save_and_cache("token", value, s_token);
    } else if (verb == "ssid") {
        if (*value == '\0') { Serial.println("prov: usage: ssid <value>"); return; }
        save_and_cache("ssid", value, s_ssid);
    } else if (verb == "pass") {
        if (*value == '\0') { Serial.println("prov: usage: pass <value>"); return; }
        save_and_cache("pass", value, s_pass);
    } else if (verb == "webpass") {
        save_and_cache("web_pass", value, s_web_pass);
        Serial.printf("prov: web_pass %s\n", s_web_pass.length() ? "set" : "cleared");
    } else if (verb == "event_token") {
        save_and_cache("event_token", value, s_event_token);
        Serial.printf("prov: event_token %s\n", s_event_token.length() ? "set" : "cleared");
    } else if (verb == "status") {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        String tok = s_token;
        String sid = s_ssid;
        bool hp    = s_pass.length() > 0;
        bool hwp   = s_web_pass.length() > 0;
        bool het   = s_event_token.length() > 0;
        xSemaphoreGive(s_mutex);
        Serial.printf("token:       %s\n", tok.length() ? tok.substring(0, 20).c_str() : "(none)");
        Serial.printf("ssid:        %s\n", sid.length() ? sid.c_str()                  : "(none)");
        Serial.printf("pass:        %s\n", hp  ? "***" : "(none)");
        Serial.printf("webpass:     %s\n", hwp ? "set" : "(none)");
        Serial.printf("event_token: %s\n", het ? "set" : "(none)");
    } else if (verb == "clear") {
        Preferences prefs;
        prefs.begin(NVS_NS, false);
        prefs.clear();
        prefs.end();
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_token = "";
        s_ssid  = "";
        s_pass  = "";
        xSemaphoreGive(s_mutex);
        Serial.println("prov: cleared");
    } else {
        Serial.printf("prov: unknown command: %s\n", cmd);
        Serial.println("prov: commands: token|ssid|pass|webpass|event_token|status|clear");
    }
}

bool provisioning_has_wifi(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool r = s_ssid.length() > 0 && s_pass.length() > 0;
    xSemaphoreGive(s_mutex);
    return r;
}

bool provisioning_has_token(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool r = s_token.length() > 0;
    xSemaphoreGive(s_mutex);
    return r;
}

String provisioning_get_ssid(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    String r = s_ssid;
    xSemaphoreGive(s_mutex);
    return r;
}

String provisioning_get_pass(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    String r = s_pass;
    xSemaphoreGive(s_mutex);
    return r;
}

String provisioning_get_token(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    String r = s_token;
    xSemaphoreGive(s_mutex);
    return r;
}

String provisioning_get_webpass(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    String r = s_web_pass;
    xSemaphoreGive(s_mutex);
    return r;
}

String provisioning_get_event_token(void) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    String r = s_event_token;
    xSemaphoreGive(s_mutex);
    return r;
}

void provisioning_save_webpass(const char* pass) {
    save_and_cache("web_pass", pass ? pass : "", s_web_pass);
}

void provisioning_save_event_token(const char* token) {
    save_and_cache("event_token", token ? token : "", s_event_token);
}

void provisioning_save_wifi(const char* ssid, const char* pass, const char* token) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    if (token && *token) prefs.putString("token", token);
    prefs.end();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_ssid = ssid;
    s_pass = pass;
    if (token && *token) s_token = token;
    xSemaphoreGive(s_mutex);
    Serial.printf("prov: saved ssid=%s pass=*** token=%s\n",
        ssid, (token && *token) ? "set" : "unchanged");
}
