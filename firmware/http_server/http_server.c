/*
 * On-device HTTP control server. Wide, horizontally-scrollable table UI with
 * column headers. Single radio: scan/attack briefly drop the AP; the UI
 * auto-reconnects and Stop retries until it lands.
 *
 * Authorized security testing only.
 */
#include <string.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "control.h"
#include "wifi_controller.h"
#include "http_server.h"

static const char *TAG = "HTTP";
static httpd_handle_t s_server = NULL;

// Attributes use single quotes; JS uses template literals -> clean C string.
static const char PAGE[] =
"<!doctype html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>C5</title><style>"
"*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}"
"html,body{margin:0;background:#000;max-width:100%;overflow-x:hidden}"
"body{font-family:-apple-system,system-ui,sans-serif;color:#fff;font-size:15px}"
"#accent{height:3px;background:transparent;transition:background .3s}"
"@keyframes rain{0%{background-position:0 0}100%{background-position:300% 0}}"
"body.god{background:radial-gradient(circle at 50% -20%,#191100,#000 55%)}"
"body.god #accent{background:linear-gradient(90deg,#ffd700,#ff2d2d,#ff8c00,#ffe600,#2dff5a,#2de1ff,#2d6bff,#b02dff,#ffd700);background-size:300% 100%;animation:rain 1.6s linear infinite}"
"body.god .brand{background:linear-gradient(90deg,#ffd700,#fff6cf,#ffd700,#ff8c00);background-size:200% 100%;-webkit-background-clip:text;background-clip:text;color:transparent;animation:rain 2.2s linear infinite}"
"#bGod{background:linear-gradient(90deg,#3a2f00,#5a4700);border-color:#8a6d00;color:#ffe89a}"
"body.god #bGod{animation:rain 1.6s linear infinite;background:linear-gradient(90deg,#ffd700,#ff8c00,#ffd700);background-size:200% 100%;color:#1a1400}"
"header{position:sticky;top:0;background:#000;padding:12px 16px;z-index:9}"
".top{display:flex;align-items:center;gap:10px}"
".brand{font-weight:800;font-size:20px;letter-spacing:1px;margin-right:auto}"
".dot{width:10px;height:10px;border-radius:50%;background:#444;transition:.3s}"
".state{font-size:12px;color:#bbb;text-transform:uppercase;letter-spacing:1px}"
".bar{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}"
"button{flex:1;min-width:74px;background:#0d0d0d;color:#fff;border:1px solid #2a2a2a;padding:11px 8px;border-radius:11px;font-size:14px;font-weight:600}"
"button:active{background:#222}button:disabled{opacity:.35}"
".prime{background:#fff;color:#000;border-color:#fff}.danger{background:#fff;color:#000;border-color:#fff}"
"#msg{padding:8px 16px;font-size:13px;color:#9a9a9a;line-height:1.45;min-height:20px}"
".wrap{overflow-x:auto;-webkit-overflow-scrolling:touch;padding-bottom:8px}"
"table{border-collapse:collapse;width:max-content;min-width:100%;font-size:13px}"
"th,td{padding:10px 12px;text-align:left;white-space:nowrap;border-bottom:1px solid #161616}"
"th{color:#7a7a7a;font-size:10px;text-transform:uppercase;letter-spacing:.6px;font-weight:700;background:#080808}"
"td.nm{font-weight:600;cursor:pointer;max-width:210px;overflow:hidden;text-overflow:ellipsis}"
"td.mono{font-family:ui-monospace,monospace;color:#8a8a8a;font-size:11px}"
"td.sent{color:#ef4444;font-weight:700}"
"td.sec{font-size:11px;font-weight:600;color:#bbb}td.sec.op{color:#ef4444}td.sec.wp3{color:#4ade80}"
"input[type=checkbox]{width:19px;height:19px;accent-color:#fff}"
".badge{font:700 10px/1 ui-monospace,monospace;padding:4px 6px;border-radius:5px;border:1px solid #333}"
".b5{background:#fff;color:#000}.b24{background:#000;color:#fff}"
".lock{font:700 9px/1 ui-monospace,monospace;border:1px solid #666;color:#bbb;padding:2px 4px;border-radius:4px;margin-left:5px}"
".safe{font:700 9px/1 ui-monospace,monospace;background:#0d3d1a;color:#4ade80;border:1px solid #1a5a2a;padding:2px 4px;border-radius:4px;margin-left:5px}"
".wifi{display:inline-flex;align-items:flex-end;gap:2px;height:16px}"
".wifi>i{width:3px;border-radius:1px;display:block}"
"tr.drow>td{background:#0a0a0a;white-space:normal}"
".devs{display:flex;flex-wrap:wrap;gap:6px}"
".dc{font:600 11px/1.3 ui-monospace,monospace;background:#111;border:1px solid #262626;border-radius:7px;padding:7px 9px;color:#ccc}"
".dc b{color:#fff}.dc i{font-style:normal;font-weight:700}"
".empty{color:#666;text-align:center;padding:40px 20px}"
".foot{color:#555;font-size:11px;text-align:center;padding:16px}"
"</style></head><body>"
"<div id='accent'></div>"
"<header><div class='top'><span class='brand'>C5</span>"
"<span id='dot' class='dot'></span><span id='state' class='state'>idle</span></div>"
"<div class='bar'><button class='prime' id='bScan' onclick='scan()'>Scan</button>"
"<button class='danger' id='bAtk' onclick='attack()' disabled>Attack (0)</button></div>"
"<div class='bar'><button onclick='protect()'>Protect</button>"
"<button id='bGod' onclick='god()'>God Mode</button>"
"<button id='bStop' onclick='stop()'>Stop</button></div></header>"
"<div id='msg'>Tap Scan to find networks. Your phone briefly disconnects, then the list appears automatically.</div>"
"<div id='list'><div class='empty'>No networks yet.</div></div>"
"<div class='foot'>Attack only networks you own or are authorized to test.</div>"
"<script>"
"let rows=[];"
"const MC={idle:'#444',scanning:'#3b82f6',devices:'#06b6d4',attacking:'#ef4444',god:'#d946ef',stopping:'#eab308'};"
"function j(u){return fetch(u).then(r=>r.json())}"
"function m(t){document.getElementById('msg').textContent=t}"
"function sc(r){return r>=-60?'#22c55e':r>=-75?'#eab308':'#ef4444'}"
"function wifi(r){let lvl=r>=-55?4:r>=-67?3:r>=-75?2:1,c=sc(r),b='';"
"for(let k=1;k<=4;k++)b+=`<i style='height:${3+k*3}px;background:${k<=lvl?c:'#2a2a2a'}'></i>`;return `<span class='wifi'>${b}</span>`}"
"function selCount(){return document.querySelectorAll('#list tbody input:checked').length}"
"function sel(){let boxes=document.querySelectorAll('#list tbody input'),c=[...boxes].filter(b=>b.checked).length;"
"let b=document.getElementById('bAtk');b.textContent='Attack ('+c+')';b.disabled=c==0;"
"let a=document.getElementById('selall');if(a){a.checked=c>0&&c===boxes.length;a.indeterminate=c>0&&c<boxes.length}}"
"function render(){let L=document.getElementById('list');"
"if(!rows.length){L.innerHTML=`<div class='empty'>No networks found. Tap Scan.</div>`;sel();return}"
"let h=`<div class='wrap'><table><thead><tr><th><input type='checkbox' id='selall' onchange='toggleAll(this.checked)'></th><th>SSID</th><th>Band</th><th>Security</th><th>Ch</th><th>Signal</th><th>dBm</th><th>Dev</th><th>Sent</th><th>BSSID</th></tr></thead><tbody>`;"
"h+=rows.map(n=>`<tr><td><input type='checkbox' data-i='${n.i}' onchange='sel()'></td>"
"<td class='nm' onclick='showDev(${n.i})'>${n.ssid}${n.prot?` <span class='lock'>PMF</span>`:''}${n.safe?` <span class='safe'>SAFE</span>`:''}</td>"
"<td><span class='badge ${n.band==='5G'?'b5':'b24'}'>${n.band}</span></td>"
"<td class='sec ${n.sec==='Open'?'op':(n.prot?'wp3':'')}'>${n.sec}</td>"
"<td>${n.ch}</td><td>${wifi(n.rssi)}</td><td>${n.rssi}</td><td id='v${n.i}'>${n.clients}</td>"
"<td class='sent' id='s${n.i}'>${n.tx||''}</td><td class='mono'>${n.bssid}</td></tr>"
"<tr class='drow' id='d${n.i}' style='display:none'><td colspan='10'></td></tr>`).join('');"
"h+=`</tbody></table></div>`;L.innerHTML=h;sel()}"
"function showDev(i){let r=document.getElementById('d'+i),c=r.firstElementChild;"
"if(r.style.display!=='none'){r.style.display='none';return}r.style.display='';c.textContent='loading…';"
"j('/api/devices?idx='+i).then(ms=>{c.innerHTML=ms.length?`<div class='devs'>`+ms.map(d=>"
"`<span class='dc'><b>${d.mac}</b><br>${d.priv?'random':'device'} &middot; <i style='color:${sc(d.rssi)}'>${d.rssi} dBm</i> &middot; ${d.pkts} pk</span>`).join('')+`</div>`"
":`No devices seen. Tap Devices first, then reopen.`}).catch(e=>{c.textContent='Reconnect and try again.'})}"
"function autoLoad(t,done){j('/api/list').then(r=>{rows=r;render();done(true)})"
".catch(e=>{if(t>0){m('Reconnecting to deauther-ctrl ...');setTimeout(()=>autoLoad(t-1,done),1500)}else done(false)})}"
"function scan(){let b=document.getElementById('bScan');b.disabled=true;b.textContent='Scanning…';setBar('scanning');"
"m('Scanning networks and their connected devices (~15-20s). Your phone disconnects briefly, then the list loads automatically.');"
"j('/api/scan').catch(e=>{});setTimeout(()=>autoLoad(30,ok=>{b.disabled=false;b.textContent='Scan';"
"m(ok?rows.length+' networks found. Tap an SSID to see its devices.':'Please reconnect to Wi-Fi deauther-ctrl, then tap Scan.')}),1500)}"
"function toggleAll(v){document.querySelectorAll('#list tbody input').forEach(c=>c.checked=v);sel()}"
"function attack(){let ids=[...document.querySelectorAll('#list input:checked')].map(c=>c.dataset.i);if(!ids.length)return;"
"let prot=ids.filter(i=>rows[i]&&rows[i].prot).length,one=ids.length===1&&rows[ids[0]]&&rows[ids[0]].band==='2.4G';"
"let note=one?'Single 2.4GHz target: phone stays connected, injecting non-stop.':'Wi-Fi drops in short bursts; Stop retries until it lands.';"
"if(prot)note+=' '+prot+' use PMF (WPA3) and will ignore deauth.';"
"setBar('attacking');m('Attacking '+ids.length+' network(s). '+note+' Watch Sent column and pkt/s.');j('/api/attack?sel='+ids.join(',')).catch(e=>{})}"
"function protect(){let ids=[...document.querySelectorAll('#list tbody input:checked')].map(c=>c.dataset.i);"
"if(!ids.length){m('Select networks first, then tap Protect to shield them from God Mode.');return}"
"j('/api/protect?sel='+ids.join(',')).then(x=>{j('/api/list').then(r=>{rows=r;render();m('Protection updated. SAFE networks are skipped by God Mode.')})}).catch(e=>{})}"
"function god(){setBar('god');m('God mode: deauthing every network except SAFE ones. Wi-Fi returns briefly so Stop can land.');j('/api/god').catch(e=>{})}"
"function stop(){let b=document.getElementById('bStop');b.disabled=true;b.textContent='Stopping…';setBar('stopping');"
"m('Stopping. Waiting for the device to come back (a few seconds).');"
"let done=t=>{b.disabled=false;b.textContent='Stop';m(t)};"
"let go=n=>{j('/api/stop').then(x=>{done('Stopped.');poll()}).catch(e=>{if(n>0)setTimeout(()=>go(n-1),700);else done('Still stopping. Reconnect to deauther-ctrl.')})};go(25)}"
"function setBar(mode){let g=mode==='god',c=MC[mode]||'#444';document.body.classList.toggle('god',g);"
"let a=document.getElementById('accent'),d=document.getElementById('dot');"
"a.style.background=(mode==='idle')?'transparent':(g?'':c);"
"d.style.background=g?'#ffd700':c;d.style.boxShadow=mode!=='idle'?'0 0 9px '+(g?'#ffd700':c):'none';"
"document.getElementById('state').textContent=mode}"
"function live(){j('/api/list').then(r=>{r.forEach(n=>{if(rows[n.i]){rows[n.i].tx=n.tx;rows[n.i].clients=n.clients;}"
"let sc=document.getElementById('s'+n.i);if(sc)sc.textContent=n.tx||'';"
"let vc=document.getElementById('v'+n.i);if(vc)vc.textContent=n.clients;})}).catch(e=>{})}"
"function poll(){j('/api/status').then(s=>{setBar(s.mode);"
"if(s.rate)document.getElementById('state').textContent=s.mode+' '+s.rate+' pkt/s';"
"if(s.mode!=='idle'&&rows.length)live()}).catch(e=>{})}"
"setInterval(poll,2000);poll();"
"</script></body></html>";

static esp_err_t h_root(httpd_req_t *r){
    httpd_resp_set_type(r, "text/html");
    return httpd_resp_send(r, PAGE, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t send_json(httpd_req_t *r, const char *s, int len){
    httpd_resp_set_type(r, "application/json");
    return httpd_resp_send(r, s, len < 0 ? HTTPD_RESP_USE_STRLEN : len);
}
static esp_err_t h_scan(httpd_req_t *r){
    char b[48]; return send_json(r, b, snprintf(b, sizeof(b), "{\"count\":%d}", control_scan()));
}
static esp_err_t h_clients(httpd_req_t *r){
    char b[48]; return send_json(r, b, snprintf(b, sizeof(b), "{\"total\":%d}", control_count_clients()));
}
static esp_err_t h_list(httpd_req_t *r){
    static char json[10240]; return send_json(r, json, control_list_json(json, sizeof(json)));
}
static esp_err_t h_status(httpd_req_t *r){
    char b[80]; return send_json(r, b, snprintf(b, sizeof(b), "{\"mode\":\"%s\",\"rate\":%lu}",
                                 control_mode_str(), (unsigned long)control_rate()));
}
static esp_err_t h_devices(httpd_req_t *r){
    char q[32], v[12]; int idx = -1;
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) == ESP_OK &&
        httpd_query_key_value(q, "idx", v, sizeof(v)) == ESP_OK) idx = atoi(v);
    static char json[2048]; return send_json(r, json, control_devices_json(idx, json, sizeof(json)));
}
static esp_err_t h_attack(httpd_req_t *r){
    char q[512], v[300]; int idx[WIFI_MAX_NETWORKS], k = 0;
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) == ESP_OK &&
        httpd_query_key_value(q, "sel", v, sizeof(v)) == ESP_OK) {
        char *s = v;
        while (*s && k < WIFI_MAX_NETWORKS) {
            while (*s == ',' || *s == ' ') s++;
            if (!*s) break;
            idx[k++] = atoi(s);
            while (*s && *s != ',') s++;
        }
    }
    int got = control_attack_selection(idx, k);
    char b[48]; return send_json(r, b, snprintf(b, sizeof(b), "{\"armed\":%d}", got > 0 ? got : 0));
}
static esp_err_t h_protect(httpd_req_t *r){
    char q[512], v[300]; int idx[WIFI_MAX_NETWORKS], k = 0;
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) == ESP_OK &&
        httpd_query_key_value(q, "sel", v, sizeof(v)) == ESP_OK) {
        char *s = v;
        while (*s && k < WIFI_MAX_NETWORKS) {
            while (*s == ',' || *s == ' ') s++;
            if (!*s) break;
            idx[k++] = atoi(s);
            while (*s && *s != ',') s++;
        }
    }
    control_protect(idx, k);
    return send_json(r, "{\"ok\":true}", -1);
}
static esp_err_t h_god(httpd_req_t *r){ control_god();  return send_json(r, "{\"ok\":true}", -1); }
static esp_err_t h_stop(httpd_req_t *r){ control_stop(); return send_json(r, "{\"ok\":true}", -1); }

void http_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 12;
    if (httpd_start(&s_server, &cfg) != ESP_OK) { ESP_LOGE(TAG, "httpd_start failed"); return; }

    const httpd_uri_t uris[] = {
        { .uri = "/",            .method = HTTP_GET, .handler = h_root },
        { .uri = "/api/scan",    .method = HTTP_GET, .handler = h_scan },
        { .uri = "/api/clients", .method = HTTP_GET, .handler = h_clients },
        { .uri = "/api/list",    .method = HTTP_GET, .handler = h_list },
        { .uri = "/api/status",  .method = HTTP_GET, .handler = h_status },
        { .uri = "/api/devices", .method = HTTP_GET, .handler = h_devices },
        { .uri = "/api/attack",  .method = HTTP_GET, .handler = h_attack },
        { .uri = "/api/protect", .method = HTTP_GET, .handler = h_protect },
        { .uri = "/api/god",     .method = HTTP_GET, .handler = h_god },
        { .uri = "/api/stop",    .method = HTTP_GET, .handler = h_stop },
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
        httpd_register_uri_handler(s_server, &uris[i]);
    ESP_LOGI(TAG, "HTTP control server up at http://192.168.4.1/");
}
