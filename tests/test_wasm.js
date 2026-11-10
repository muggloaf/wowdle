const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const root = path.join(__dirname, '..');
let modulePromise;
function loadModule() {
  if (!modulePromise) {
    const context = {
      window: {}, document: { currentScript: { src: 'http://localhost/extension/wowdle.js' } },
      console, WebAssembly, TextDecoder, TextEncoder, URL, setTimeout, clearTimeout,
      fetch: async (url) => {
        assert.equal(new URL(url).pathname, '/extension/wowdle.wasm');
        return new Response(fs.readFileSync(path.join(root, 'extension/wowdle.wasm')), {
          headers: { 'Content-Type': 'application/wasm' },
        });
      },
    };
    vm.createContext(context);
    vm.runInContext(fs.readFileSync(path.join(root, 'extension/wowdle.js'), 'utf8'), context);
    modulePromise = context.WowdleModuleFactory();
  }
  return modulePromise;
}

test('compiled WASM returns parseable analysis and preserves state after C++ exceptions', async () => {
  const module = await loadModule();
  const call = (name, ...values) => JSON.parse(module.ccall(name, 'string', values.map(() => 'string'), values));
  module.ccall('wowdle_init', null, ['string', 'string'], ['cater caper baker', 'cater 3 caper 2 baker 1']);
  const initial = call('wowdle_recommend');
  assert.equal(initial.candidates, 3);
  assert.equal(initial.guesses_remaining, 6);
  assert.equal(initial.status, 'playing');
  assert.ok(initial.recommendations.every((item) => Number.isFinite(item.score)));
  assert.ok(Math.abs(initial.answers.reduce((sum, item) => sum + item.probability, 0) - 1) < 1e-10);
  assert.ok(call('wowdle_submit', 'cater', 'xxxxx').error);
  assert.deepEqual(call('wowdle_recommend'), initial);
  const filtered = call('wowdle_submit', 'cater', 'ggxgg');
  assert.equal(filtered.guess, 'caper');
  assert.equal(filtered.candidates, 1);
  assert.deepEqual(call('wowdle_restore', 'cater ggxgg'), filtered);
  assert.equal(call('wowdle_submit', 'caper', 'ggggg').status, 'won');
  assert.deepEqual(call('wowdle_restore', ''), initial);
});

test('compiled WASM solves a duplicate-letter game using the shipped dictionaries', async () => {
  const module = await loadModule();
  module.ccall('wowdle_init', null, ['string', 'string'], [
    fs.readFileSync(path.join(root, 'extension/data/wordlist_fives.txt'), 'utf8'),
    fs.readFileSync(path.join(root, 'extension/data/word_frequencies_fives.txt'), 'utf8'),
  ]);
  let state = JSON.parse(module.ccall('wowdle_recommend', 'string', [], []));
  assert.equal(state.candidates, 16131);
  assert.equal(state.skipped_words, 21);
  const final = JSON.parse(module.ccall('wowdle_restore', 'string', ['string'], [
    Array(5).fill('zzzzz xxxxx').join('\n'),
  ]));
  assert.equal(final.guesses_remaining, 1);
  assert.ok(final.candidates > 500);
  assert.equal(final.analysis.stage, 'final');
  assert.equal(final.guess, final.answers[0].word);
  assert.equal(final.recommendations.length, 5);
  state = JSON.parse(module.ccall('wowdle_restore', 'string', ['string'], ['']));
  const answer = 'apple';
  for (let turn = 0; turn < 6 && state.status === 'playing'; ++turn) {
    // independent feedback oracle for this integration test only.
    const feedback = Array(5).fill('x');
    const unused = [...answer];
    [...state.guess].forEach((letter, index) => {
      if (letter === unused[index]) { feedback[index] = 'g'; unused[index] = null; }
    });
    [...state.guess].forEach((letter, index) => {
      const found = unused.indexOf(letter);
      if (feedback[index] !== 'g' && found >= 0) { feedback[index] = 'y'; unused[found] = null; }
    });
    state = JSON.parse(module.ccall('wowdle_submit', 'string', ['string', 'string'], [state.guess, feedback.join('')]));
    assert.equal(state.error, undefined);
  }
  assert.equal(state.status, 'won');
  assert.equal(state.guess, answer);
});
