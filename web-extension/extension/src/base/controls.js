"use strict";
const browserApi = globalThis.browser || globalThis.chrome;
const element = (id) => document.getElementById(id);
async function refresh() {
  const result = await browserApi.runtime.sendMessage({dragonAction:"status"});
  element("connection").textContent = result.connected ? "Connected" : "Not connected";
  element("extensionId").textContent = result.id || browserApi.runtime.id;
  element("error").textContent = result.error || (result.connected ? "" : "Install Dragon and register the browser connection. Open Setup guide for instructions.");
  for (const id of ["capture","showDialog","shareCookies"]) element(id).checked = !!result.settings?.[id];
  element("excludedHosts").value = result.settings?.excludedHosts || "";
}
element("save").addEventListener("click", async () => {
  const settings = {excludedHosts:element("excludedHosts").value};
  for (const id of ["capture","showDialog","shareCookies"]) settings[id] = element(id).checked;
  try {
    const result = await browserApi.runtime.sendMessage({dragonAction:"save",settings});
    element("saved").textContent = result.ok ? "Settings saved." : "Could not save settings.";
  } catch { element("error").textContent = "Could not save settings. Reload the extension."; }
});
element("open").addEventListener("click", async () => {
  try { const r=await browserApi.runtime.sendMessage({dragonAction:"open"});if(!r.accepted)throw Error(); }
  catch { element("error").textContent="Dragon is not connected. Open Setup guide."; }
});
element("help").addEventListener("click", () => browserApi.tabs.create({url:browserApi.runtime.getURL("setup.html")}));
element("recover").addEventListener("click", async () => {
  await browserApi.runtime.sendMessage({dragonAction:"recover"});
  await refresh();
});
refresh().catch(() => { element("connection").textContent="Not connected";element("error").textContent="Reload the extension and check the installation."; });
