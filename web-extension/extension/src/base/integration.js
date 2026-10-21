/* Dragon Browser Integration — Brami Group. LGPL v3. */
(() => {
  "use strict";
  const api = globalThis.browser || globalThis.chrome;
  const host = "net.mobile_dragon.download_manager";
  const defaults = { capture: true, showDialog: true, shareCookies: true, excludedHosts: "" };
  const requests = new Map();
  const inFlight = new Set();
  const bypass = new Set();
  let transactions = Promise.resolve();
  const serial = (work) => {
    const result = transactions.then(work, work);
    transactions = result.catch(() => {});
    return result;
  };
  const settings = async () => ({ ...defaults, ...(await api.storage.local.get("dragonSettings")).dragonSettings });
  const native = (message) => new Promise((resolve, reject) => {
    let port;
    try { port = api.runtime.connectNative(host); }
    catch (error) { reject(error); return; }
    let settled = false;
    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      port.disconnect();
      reject(new Error("Dragon connection timed out"));
    }, 45000);
    port.onMessage.addListener((response) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      resolve(response);
      port.disconnect();
    });
    port.onDisconnect.addListener(() => {
      const error = api.runtime.lastError || port.error;
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      reject(new Error(error?.message || "Dragon connection closed"));
    });
    try { port.postMessage(message); }
    catch (error) {
      settled = true;
      clearTimeout(timer);
      port.disconnect();
      reject(error);
    }
  });
  const httpUrl = (value) => {
    try { const u = new URL(value); return ["http:", "https:"].includes(u.protocol) && !u.username && !u.password ? u : null; }
    catch { return null; }
  };
  const excluded = (url, config) => config.excludedHosts.split(/[\s,]+/).filter(Boolean).some((entry) => {
    const hostName = entry.trim().toLowerCase().replace(/^\*\./, "");
    return url.hostname === hostName || url.hostname.endsWith("." + hostName);
  });
  const noteFailure = async (message) => {
    await api.storage.local.set({ dragonLastError: message });
    await api.action.setBadgeText({ text: "!" });
    await api.action.setBadgeBackgroundColor({ color: "#a95623" });
    await api.action.setTitle({ title: "Dragon: " + message });
  };
  const clearFailure = async () => {
    await api.storage.local.remove("dragonLastError");
    await api.action.setBadgeText({ text: "" });
    await api.action.setTitle({ title: "Dragon Download Manager" });
  };
  const pending = async () => (await api.storage.local.get("dragonPending")).dragonPending || {};
  const updatePending = (id, record) => serial(async () => {
    const list = await pending();
    if (record) list[id] = record; else delete list[id];
    await api.storage.local.set({ dragonPending: list });
  });
  async function payload(item, config, requestId) {
    const url = httpUrl(item.finalUrl || item.url);
    if (!url) throw new Error("Only direct HTTP or HTTPS downloads can be sent.");
    let cookies = [];
    if (config.shareCookies) {
      // Only cookies matching this actual download URL leave the browser, to the local host.
      cookies = (await api.cookies.getAll({ url: url.href })).slice(0, 200);
    }
    const filename = (item.filename || "").replace(/\\/g, "/").split("/").pop();
    return { action: "download", requestId, url: url.href, filename,
      referrer: item.referrer || "", totalBytes: item.totalBytes || 0,
      userAgent: navigator.userAgent, cookies, showDialog: config.showDialog };
  }
  async function sendLink(url) {
    const config = await settings();
    const message = await payload({url}, config, crypto.randomUUID());
    const response = await native(message);
    if (!response?.accepted) throw new Error(response?.error || "Dragon did not accept the link.");
    await clearFailure();
    return response;
  }
  async function capture(item, config, recoveryRecord) {
    if (inFlight.has(item.id)) return;
    inFlight.add(item.id);
    let accepted = recoveryRecord?.accepted || false;
    const record = recoveryRecord || { requestId: crypto.randomUUID(), created: Date.now(), accepted: false };
    try {
      await updatePending(item.id, record);
      if (!item.paused) await api.downloads.pause(item.id);
      if (!accepted) {
        const message = await payload(item, config, record.requestId);
        let response;
        // Retry the same ID. Dragon stores it with the queue to avoid duplicate handoffs.
        for (let attempt = 0; attempt < 2; ++attempt) {
          try { response = await native(message); break; }
          catch (error) { if (attempt === 1) throw error; }
        }
        if (!response?.accepted || response.requestId !== record.requestId)
          throw new Error(response?.error || "Dragon did not confirm the download.");
        accepted = true;
        await updatePending(item.id, { ...record, accepted: true });
      }
      // Cancel the browser copy only after Dragon has saved the job durably.
      await api.downloads.cancel(item.id);
      await updatePending(item.id, null);
      await clearFailure();
    } catch (error) {
      if (!accepted) {
        bypass.add(item.id);
        try { await api.downloads.resume(item.id); } catch { /* User may have cancelled the download. */ }
        await updatePending(item.id, null);
        await noteFailure("Browser download kept: check that Dragon is installed and connected.");
      } else {
        await noteFailure("Saved in Dragon; the browser copy could not be cancelled. Check both lists.");
      }
    } finally { inFlight.delete(item.id); }
  }
  async function consider(item) {
    if (bypass.has(item.id) || inFlight.has(item.id) || item.state !== "in_progress" || item.paused || item.incognito || item.byExtensionId) return;
    if (item.danger && !["safe", "accepted", "allowlistedByPolicy"].includes(item.danger)) return;
    if (!item.filename || item.filename.toLowerCase().endsWith(".crdownload")) return;
    const url = httpUrl(item.finalUrl || item.url);
    if (!url || item.mime === "text/html") return;
    const config = await settings();
    if (!config.capture || excluded(url, config)) return;
    const request = requests.get(url.href) || requests.get(item.url);
    // A POST response, a blob or an unknown request cannot safely be replayed as a GET.
    if (!request || request.method !== "GET" || Date.now() - request.time > 30000) return;
    const existing = (await pending())[item.id];
    await capture(item, config, existing);
  }
  async function recover() {
    const list = await pending();
    const config = await settings();
    for (const [id, record] of Object.entries(list)) {
      const items = await api.downloads.search({id: Number(id)});
      const item = items[0];
      if (!item || item.state !== "in_progress") { await updatePending(id, null); continue; }
      if (!config.capture && !record.accepted) {
        try { await api.downloads.resume(item.id); } catch {}
        await updatePending(id, null);
        continue;
      }
      await capture(item, config, record);
    }
  }
  api.webRequest.onBeforeRequest.addListener((details) => {
    if (details.incognito) return;
    requests.set(details.url, {method: details.method, time: Date.now()});
    if (requests.size > 512) requests.delete(requests.keys().next().value);
  }, { urls: ["http://*/*", "https://*/*"] });
  api.downloads.onCreated.addListener((item) => { consider(item).catch(() => {}); });
  api.downloads.onChanged.addListener((change) => {
    if (!change.filename && !change.finalUrl && !change.paused) return;
    api.downloads.search({id: change.id}).then((items) => items[0] && consider(items[0])).catch(() => {});
  });
  api.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (!message?.dragonAction || sender.id !== api.runtime.id) return false;
    const handle = async () => {
      if (message.dragonAction === "status") {
        let connected = false;
        try { connected = !!(await native({action: "ping"}))?.accepted; } catch {}
        return {connected, id: api.runtime.id, settings: await settings(), error: (await api.storage.local.get("dragonLastError")).dragonLastError || ""};
      }
      if (message.dragonAction === "save") {
        const config = { ...defaults, ...message.settings };
        config.excludedHosts = String(config.excludedHosts || "").slice(0, 8192);
        await api.storage.local.set({dragonSettings: config});
        if (!config.capture) await recover();
        return {ok: true};
      }
      if (message.dragonAction === "open") return native({action: "open"});
      if (message.dragonAction === "recover") { await recover(); return {ok: true}; }
      throw new Error("Unknown Dragon action");
    };
    handle().then(sendResponse, () => sendResponse({error:"Dragon is not connected. See the installation guide."}));
    return true;
  });
  api.runtime.onInstalled.addListener(() => { recover().catch(() => {}); });
  api.runtime.onStartup.addListener(() => { recover().catch(() => {}); });
  recover().catch(() => {});
  globalThis.DragonIntegration = {sendLink, noteFailure, collect: async (text) => {
    const response = await native({action: "collect", text});
    if (!response?.accepted) throw new Error("Dragon did not accept the collection");
    return response;
  }, open: () => native({action: "open"})};
})();
