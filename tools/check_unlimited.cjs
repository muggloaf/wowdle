const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const root = path.join(__dirname, '..');
let chromium;
try {
  ({ chromium } = require(path.join(root, '.build/browser-check/node_modules/playwright-core')));
} catch {
  console.error('Install the optional test dependency: npm install --prefix .build/browser-check --no-save --package-lock=false playwright-core');
  process.exit(1);
}

async function solverModule() {
  const context = {
    window: {}, document: { currentScript: { src: 'http://localhost/extension/wowdle.js' } },
    console, WebAssembly, TextDecoder, TextEncoder, URL, setTimeout, clearTimeout,
    fetch: async () => new Response(fs.readFileSync(path.join(root, 'extension/wowdle.wasm')), {
      headers: { 'Content-Type': 'application/wasm' },
    }),
  };
  vm.createContext(context);
  vm.runInContext(fs.readFileSync(path.join(root, 'extension/wowdle.js'), 'utf8'), context);
  return context.WowdleModuleFactory();
}

async function main() {
  const browser = await chromium.launch({ channel: 'chrome', headless: true });
  try {
    const page = await browser.newPage();
    await page.goto('https://wordleunlimited.org/', { waitUntil: 'domcontentloaded' });
    await page.locator('game-app').waitFor();
    await page.locator('game-row').first().waitFor();
    await page.addScriptTag({ content: `
      window.chrome = { runtime: { onMessage: { addListener(listener) {
        window.__wowdleTestListener = listener;
      } } } };
    ` });
    await page.addScriptTag({ content: fs.readFileSync(path.join(root, 'extension/content.js'), 'utf8') });
    const send = (message) => page.evaluate((value) => new Promise((resolve) => {
      window.__wowdleTestListener(value, {}, resolve);
    }), message);
    const initial = await send({ type: 'WOWDLE_GET_BOARD' });
    assert.equal(initial.supported, true);
    assert.deepEqual(initial.history, []);
    assert.equal(initial.maxGuesses, 6);

    const modal = page.locator('game-modal[open]');
    if (await modal.count()) {
      const blocked = await send({ type: 'WOWDLE_PLAY_GUESS', guess: 'crate', expectedHistory: [] });
      assert.equal(blocked.code, 'DIALOG_OPEN');
      await modal.locator('.close-icon').click();
      await modal.waitFor({ state: 'detached' });
    }
    const rejected = await send({ type: 'WOWDLE_PLAY_GUESS', guess: 'zzzzz', expectedHistory: [] });
    assert.equal(rejected.code, 'REJECTED_GUESS', JSON.stringify(rejected));
    for (let i = 0; i < 5; ++i) await page.locator('button[data-key="←"]').click();
    await page.locator('game-row[invalid]').waitFor({ state: 'detached' });
    // the toast belongs to the rejected test word; wait for it to leave the DOM.
    await page.locator('game-toast').waitFor({ state: 'detached' });
    console.log('live .org: board sync, welcome dialog, and rejected guess verified');

    const module = await solverModule();
    module.ccall('wowdle_init', null, ['string', 'string'], [
      fs.readFileSync(path.join(root, 'extension/data/wordlist_fives.txt'), 'utf8'),
      fs.readFileSync(path.join(root, 'extension/data/word_frequencies_fives.txt'), 'utf8'),
    ]);
    let state = JSON.parse(module.ccall('wowdle_recommend', 'string', [], []));
    for (let turn = 1; state.status === 'playing'; ++turn) {
      const result = await send({ type: 'WOWDLE_PLAY_GUESS', guess: state.guess, expectedHistory: state.history });
      assert.equal(result.error, undefined, JSON.stringify(result));
      assert.equal(result.history.length, turn);
      assert.equal(result.guess, state.guess);
      assert.match(result.feedback, /^[gyx]{5}$/);
      // read only rendered tiles and generated feedback; the secret is never inspected.
      console.log(`turn ${turn}: ${state.guess} -> ${result.feedback}`);
      state = JSON.parse(module.ccall('wowdle_submit', 'string', ['string', 'string'], [state.guess, result.feedback]));
      assert.equal(state.error, undefined, JSON.stringify(state));
    }
    assert.equal(state.status, 'won', 'the live smoke game reached the six-turn limit');
    console.log(`live .org: C++ solver won in ${state.history.length} guesses`);
  } finally {
    await browser.close();
  }
}

main().catch((error) => { console.error(error); process.exitCode = 1; });
