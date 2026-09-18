const state = { token: localStorage.getItem('player_token'), account: null, town: { revision: 0, payload: '' } };
const $ = (selector) => document.querySelector(selector);
const notice = (message, error = false) => { const el = $('#notice'); el.textContent = message; el.className = `notice ${error ? 'error' : 'success'}`; };
const json = async (url, options = {}) => {
  const headers = { 'Content-Type': 'application/json', ...(options.headers || {}) };
  if (state.token) headers.Authorization = `Bearer ${state.token}`;
  const response = await fetch(url, { ...options, headers });
  const body = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(body.error || `Request failed (${response.status})`);
  return body;
};
function showDashboard() { $('#login-card').hidden = true; $('#dashboard').hidden = false; $('#logout').hidden = false; }
function showLogin() { $('#login-card').hidden = false; $('#dashboard').hidden = true; $('#logout').hidden = true; }
function renderAccount(account) {
  $('#account-details').innerHTML = `<dt>Email</dt><dd>${escapeHtml(account.email || '—')}</dd><dt>Display name</dt><dd>${escapeHtml(account.display_name || '—')}</dd><dt>Account ID</dt><dd>${escapeHtml(String(account.id || '—'))}</dd>`;
}
function renderCurrencies(currencies) { $('#currencies').innerHTML = Object.entries(currencies || {}).map(([name, amount]) => `<div class="balance"><span>${escapeHtml(name)}</span><strong>${Number(amount).toLocaleString()}</strong></div>`).join('') || '<p class="muted">No balances yet.</p>'; }
function renderTown(town) { state.town = town; $('#town-revision').textContent = town.revision; $('#town-payload').value = town.payload || ''; }
function escapeHtml(value) { return String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }
async function loadDashboard() { const account = await json('/v1/me'); const currencies = await json('/v1/currencies'); const town = await json('/v1/town'); state.account = account; renderAccount(account); renderCurrencies(currencies.currencies); renderTown(town); showDashboard(); }
$('#login-form').addEventListener('submit', async (event) => { event.preventDefault(); const data = Object.fromEntries(new FormData(event.target)); try { const result = await json('/v1/auth/login', { method: 'POST', body: JSON.stringify(data) }); state.token = result.token; localStorage.setItem('player_token', state.token); await loadDashboard(); notice('Signed in.'); } catch (error) { notice(error.message, true); } });
$('#currency-form').addEventListener('submit', async (event) => { event.preventDefault(); const data = Object.fromEntries(new FormData(event.target)); try { const result = await json('/v1/currencies', { method: 'PUT', body: JSON.stringify({ currency: data.currency, amount: Number(data.amount) }) }); renderCurrencies(result.currencies); notice('Currency balance updated.'); } catch (error) { notice(error.message, true); } });
$('#refresh-town').addEventListener('click', async () => { try { renderTown(await json('/v1/town')); notice('Town refreshed.'); } catch (error) { notice(error.message, true); } });
$('#save-town').addEventListener('click', async () => { try { const town = await json('/v1/town', { method: 'PUT', body: JSON.stringify({ expected_revision: state.town.revision, payload: $('#town-payload').value }) }); renderTown({ ...state.town, revision: town.revision }); notice('Town saved.'); } catch (error) { notice(error.message === 'revision_conflict' ? 'Your town changed on another device. Refresh before saving.' : error.message, true); } });
$('#logout').addEventListener('click', () => { state.token = null; localStorage.removeItem('player_token'); showLogin(); notice('Signed out.'); });
if (state.token) loadDashboard().catch(() => { localStorage.removeItem('player_token'); state.token = null; showLogin(); }); else showLogin();
