// Exercise the actual embedded page script without sending commands to hardware.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const dir = path.join(__dirname, '../components/fourvrs_portal');
const page = fs.readFileSync(path.join(dir, 'AboutPage.h'), 'utf8');
const script = page.match(/<script>([\s\S]*?)<\/script>/)[1];
async function click(confirmed, response) {
  const elements = {}, calls = [];
  const context = {
    document: {hidden: true, getElementById: id => elements[id] ??= {}},
    confirm: () => confirmed, URLSearchParams, setTimeout: () => {},
    fetch: async (url, options) => {calls.push({url, options}); return response;},
  };
  vm.runInNewContext(script, context);
  await elements.restart.onclick();
  return {elements, calls};
}
(async () => {
  let result = await click(false);
  assert.equal(result.calls.length, 0, 'Cancel must not send a request');
  result = await click(true, {ok: true});
  assert.equal(result.calls.length, 1);
  const {url, options} = result.calls[0];
  assert.equal(url, '/system/restart');
  assert.equal(options.method, 'POST');
  assert.equal(options.body.get('confirm'), 'restart');
  assert.equal(options.body.get('token'), '__TOKEN__');
  assert.equal(result.elements.restart.disabled, true);
  result = await click(true, {ok: false, text: async () => 'Maintenance in progress'});
  assert.equal(result.elements.restart.disabled, false);
  assert.match(result.elements['restart-result'].textContent, /Maintenance in progress/);
  const style = fs.readFileSync(path.join(dir, 'UiShell.h'), 'utf8');
  for (const [name, key] of [['SettingsPage.h', 'settings'], ['UpdatesPage.h', 'updates']]) {
    assert.ok(fs.readFileSync(path.join(dir, name), 'utf8').includes(`data-page="${key}"`));
    assert.ok(style.includes(`body[data-page=${key}] a[href="/${key}"]`));
  }
  console.log('PASS: restart cancellation, confirmed POST, maintenance rejection and navigation selectors');
})().catch(error => {console.error(error); process.exitCode = 1;});
