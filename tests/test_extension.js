const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const appSource = readFileSync(path.join(__dirname, '../extension/app.js'), 'utf8');
const backendSource = readFileSync(path.join(__dirname, '../extension/backend.js'), 'utf8');
const historyKey = 'wowdle-manual-history-v1';
const clone = (value) => JSON.parse(JSON.stringify(value));
const flush = () => new Promise(setImmediate);

class Element {
  constructor(tag = 'div') {
    this.tagName = tag;
    this.children = [];
    this.listeners = {};
    this.dataset = {};
    this.attributes = {};
    this.classes = new Set();
    this.value = '';
    this.text = '';
    this.disabled = false;
    this.open = false;
    this.style = { setProperty() {} };
    this.classList = {
      add: (name) => this.classes.add(name),
      remove: (name) => this.classes.delete(name),
      contains: (name) => this.classes.has(name),
      toggle: (name, force) => {
        const add = force === undefined ? !this.classes.has(name) : force;
        if (add) this.classes.add(name); else this.classes.delete(name);
      },
    };
  }
  set className(value) { this.classes = new Set(value.split(/\s+/)); }
  get className() { return [...this.classes].join(' '); }
  set textContent(value) { this.text = String(value); this.children = []; }
  get textContent() { return this.text + this.children.map((child) => child.textContent).join(''); }
  setAttribute(name, value) { this.attributes[name] = value; }
  append(...children) { this.children.push(...children); }
  appendChild(child) { this.children.push(child); return child; }
  replaceChildren(...children) { this.children = children; this.text = ''; }
  addEventListener(type, handler) { (this.listeners[type] ||= []).push(handler); }
  async trigger(type, event = {}) {
    for (const handler of this.listeners[type] || []) await handler({ target: this, ...event });
  }
  click() { return this.disabled ? Promise.resolve() : this.trigger('click'); }
  focus() { this.focused = true; }
  showModal() { this.open = true; }
  close() { this.open = false; }
  querySelectorAll(selector) {
    return this.children.flatMap((child) => [
      ...(selector.startsWith('.') && child.classes.has(selector.slice(1)) ? [child] : []),
      ...child.querySelectorAll(selector),
    ]);
  }
}

function stateFor(history = []) {
  const status = history.at(-1)?.feedback === 'ggggg' ? 'won' : history.length >= 6 ? 'lost' : 'playing';
  const analysis = {
    entropy: 2, probability: 0.2, expected_candidates: 4, worst_case: 6,
    distance: 0.5, risk: 0, combined_score: 0.8, stage: 'moderate',
    weights: { information: 0.5, distance: 0.2, likelihood: 0.1, risk: 0.1, reduction: 0.1 },
  };
  return {
    status, history: clone(history), candidates: history.length ? 10 : 100,
    guesses_remaining: 6 - history.length,
    guess: status === 'won' ? history.at(-1).guess : 'crate',
    recommendations: status === 'playing'
      ? ['crate', 'slate', 'trace', 'arose', 'irate'].map((word) => ({ word, score: 0.8, analysis: { ...analysis } })) : [],
    answers: [{ word: 'crate', probability: 0.5 }, { word: 'slate', probability: 0.5 }],
  };
}

async function appFixture(options = {}) {
  const document = new Element('#document');
  document.body = document.appendChild(new Element('body'));
  const ids = new Map();
  document.getElementById = (id) => {
    if (!ids.has(id)) ids.set(id, document.body.appendChild(new Element()));
    return ids.get(id);
  };
  document.createElement = (tag) => new Element(tag);
  for (const name of ['board-panel', 'recommendation-panel']) {
    document.getElementById(name).className = name;
  }
  document.querySelector = (selector) => document.querySelectorAll(selector)[0];
  const modes = ['manual', 'auto'].map((mode) => {
    const button = document.body.appendChild(new Element('button'));
    button.className = 'mode';
    button.dataset.mode = mode;
    return button;
  });
  const storage = options.storage || new Map();
  const calls = [];
  let current = stateFor();
  const backend = {
    ready: false,
    async init() {
      calls.push(['init']);
      if (options.initError) throw new Error(options.initError);
      this.ready = true;
      return current;
    },
    submit(guess, feedback) {
      calls.push(['submit', guess, feedback]);
      if (options.submitError) throw new Error(options.submitError);
      current = stateFor([...current.history, { guess, feedback }]);
      return current;
    },
    restore(history) {
      calls.push(['restore', clone(history)]);
      current = stateFor(history);
      return current;
    },
  };
  const messages = [];
  const injections = [];
  let board = { supported: true, history: [], currentGuess: '', status: 'playing', maxGuesses: 6 };
  const chrome = {
    tabs: {
      async query() { return [{ id: 7, url: options.tabUrl || 'https://wordleunlimited.com/' }]; },
      async sendMessage(tabId, message) {
        messages.push({ tabId, ...clone(message) });
        if (options.onMessage) return options.onMessage(message, messages.length);
        if (message.type === 'WOWDLE_GET_BOARD') return clone(board);
        return { ...clone(board), guess: message.guess, feedback: 'ggggg' };
      },
    },
    scripting: { async executeScript(details) { injections.push(details); } },
  };
  const context = vm.createContext({
    document, window: { solverBackend: backend }, chrome, URL, Blob,
    localStorage: {
      getItem: (key) => storage.get(key) ?? null,
      setItem: (key, value) => storage.set(key, value),
    },
  });
  vm.runInContext(appSource, context);
  await flush();
  return {
    get: document.getElementById, document, backend, calls, storage, modes, messages, injections,
    setBoard(value) { board = { ...board, ...value }; },
    async key(key, target = document.body) {
      let prevented = false;
      await document.trigger('keydown', { key, target, preventDefault() { prevented = true; } });
      await flush();
      return prevented;
    },
  };
}

test('missing C++ backend disables solver actions and shows no preview recommendations', async () => {
  const app = await appFixture({ initError: 'missing wowdle.wasm' });
  for (const id of ['submit-button', 'play-button', 'sync-button', 'export-button', 'guess-input', 'feedback-input']) {
    assert.equal(app.get(id).disabled, true, id);
  }
  assert.equal(app.get('recommendation-list').children.length, 0);
  assert.equal(app.get('candidate-count').textContent, 'unavailable');
  assert.match(app.get('backend-status').textContent, /unavailable/);
  assert.match(app.get('mode-note').textContent, /missing wowdle.wasm/);
  await app.get('submit-button').click();
  assert.deepEqual(app.calls, [['init']]);
});

test('invalid feedback and inconsistent feedback preserve the current turn', async () => {
  const app = await appFixture({ submitError: 'No candidates match that feedback.' });
  app.get('feedback-input').value = 'xy';
  await app.get('submit-button').click();
  assert.equal(app.calls.filter(([name]) => name === 'submit').length, 0);
  assert.equal(app.get('turn-mark').textContent, '01 / 06');
  app.get('feedback-input').value = 'xxxxx';
  await app.get('submit-button').click();
  assert.match(app.get('mode-note').textContent, /No candidates/);
  assert.equal(app.get('turn-mark').textContent, '01 / 06');
  assert.equal(app.get('candidate-count').textContent, '100 candidates');
  assert.equal(app.get('feedback-input').value, 'xxxxx');
  assert.deepEqual(JSON.parse(app.storage.get(historyKey)), []);
});

test('c opens choices, numeric keys select, and recommendation buttons select guesses', async () => {
  const app = await appFixture();
  assert.equal(await app.key('c'), true);
  assert.equal(app.get('change-dialog').open, true);
  assert.equal(await app.key('2'), true);
  assert.equal(app.get('guess-input').value, 'SLATE');
  assert.equal(app.get('hero-guess').textContent, 'SLATE');
  assert.equal(app.get('change-dialog').open, false);
  assert.equal(app.get('feedback-input').focused, true);
  await app.get('recommendation-list').children[2].children[0].click();
  assert.equal(app.get('guess-input').value, 'TRACE');
  assert.equal(await app.key('c', app.get('guess-input')), false);
  assert.equal(app.get('change-dialog').open, false);
});

test('manual history survives reopening and Undo persists the previous state', async () => {
  const history = [{ guess: 'slate', feedback: 'xyxgg' }];
  const storage = new Map([[historyKey, JSON.stringify(history)]]);
  const app = await appFixture({ storage });
  assert.deepEqual(app.calls.find(([name]) => name === 'restore'), ['restore', history]);
  assert.equal(app.get('turn-mark').textContent, '02 / 06');
  assert.deepEqual(JSON.parse(storage.get(historyKey)), history);
  app.get('feedback-input').value = 'xxxxx';
  await app.get('submit-button').click();
  assert.equal(JSON.parse(storage.get(historyKey)).length, 2);
  await app.get('undo-button').click();
  assert.deepEqual(JSON.parse(storage.get(historyKey)), history);
  assert.equal(app.get('turn-mark').textContent, '02 / 06');
});

test('manual terminal feedback can be undone without resetting earlier turns', async () => {
  const history = [{ guess: 'slate', feedback: 'xyxgg' }];
  const app = await appFixture({ storage: new Map([[historyKey, JSON.stringify(history)]]) });
  app.get('feedback-input').value = 'ggggg';
  await app.get('submit-button').click();
  assert.equal(app.get('completion-panel').classList.contains('hidden'), false);
  assert.equal(app.get('completion-undo-button').disabled, false);
  await app.get('completion-undo-button').click();
  assert.equal(app.get('completion-panel').classList.contains('hidden'), true);
  assert.deepEqual(JSON.parse(app.storage.get(historyKey)), history);
  assert.equal(app.get('turn-mark').textContent, '02 / 06');
});

test('auto rejects a board whose turn limit differs from the solver', async () => {
  const app = await appFixture();
  app.setBoard({ maxGuesses: 7 });
  await app.modes[1].click();
  await flush();
  assert.match(app.get('mode-note').textContent, /six guesses/);
  assert.equal(app.messages.filter((message) => message.type === 'WOWDLE_PLAY_GUESS').length, 0);
});

test('auto accepts both Unlimited domains and their subdomains', async () => {
  for (const domain of ['wordleunlimited.com', 'www.wordleunlimited.com', 'wordleunlimited.org', 'www.wordleunlimited.org']) {
    const app = await appFixture({ tabUrl: `https://${domain}/` });
    await app.modes[1].click();
    await flush();
    assert.equal(app.messages[0].type, 'WOWDLE_GET_BOARD', domain);
    assert.match(app.get('mode-note').textContent, /Board synced/, domain);
  }
});

test('Unlimited permissions and content-script matches cover both domains', () => {
  const manifest = JSON.parse(readFileSync(path.join(__dirname, '../extension/manifest.json'), 'utf8'));
  for (const domain of ['wordleunlimited.com', 'wordleunlimited.org']) {
    for (const pattern of [`https://${domain}/*`, `https://*.${domain}/*`]) {
      assert.ok(manifest.host_permissions.includes(pattern), pattern);
      assert.ok(manifest.content_scripts.some((script) => script.matches.includes(pattern)), pattern);
    }
  }
});

test('auto sync updates solver history without overwriting the saved manual session', async () => {
  const manual = [{ guess: 'arose', feedback: 'xxxxx' }];
  const storage = new Map([[historyKey, JSON.stringify(manual)]]);
  const app = await appFixture({ storage });
  const gameHistory = [{ guess: 'slate', feedback: 'xyxgg' }];
  app.setBoard({ history: gameHistory });
  await app.modes[1].click();
  assert.deepEqual(app.calls.at(-1), ['restore', gameHistory]);
  assert.equal(app.get('turn-mark').textContent, '02 / 06');
  assert.deepEqual(JSON.parse(storage.get(historyKey)), manual);
  assert.equal(app.get('feedback-input').disabled, true);
});

test('Play resyncs changed history for review before sending another guess', async () => {
  const app = await appFixture();
  await app.modes[1].click();
  const history = [{ guess: 'slate', feedback: 'xyxgg' }];
  app.setBoard({ history });
  await app.get('play-button').click();
  assert.equal(app.messages.filter((message) => message.type === 'WOWDLE_PLAY_GUESS').length, 0);
  assert.equal(app.get('turn-mark').textContent, '02 / 06');
  await app.get('play-button').click();
  const sent = app.messages.filter((message) => message.type === 'WOWDLE_PLAY_GUESS');
  assert.equal(sent.length, 1);
  assert.deepEqual(sent[0].expectedHistory, history);
  assert.equal(sent[0].guess, 'crate');
  assert.equal(app.get('completion-title').textContent, 'you got it');
});

test('missing listener recovery retries GET_BOARD once and never repeats Play', async () => {
  let first = true;
  const app = await appFixture({ onMessage(message) {
    if (first) {
      first = false;
      throw new Error('Could not establish connection. Receiving end does not exist.');
    }
    if (message.type === 'WOWDLE_PLAY_GUESS') throw new Error('Receiving end does not exist.');
    return { history: [], currentGuess: '', status: 'playing', maxGuesses: 6 };
  } });
  await app.modes[1].click();
  assert.equal(app.messages.length, 2);
  assert.equal(app.injections.length, 1);
  await app.get('play-button').click();
  assert.equal(app.messages.filter((message) => message.type === 'WOWDLE_PLAY_GUESS').length, 1);
  assert.equal(app.injections.length, 1);
  assert.equal(app.get('turn-mark').textContent, '01 / 06');
  assert.match(app.get('mode-note').textContent, /Receiving end/);
});

test('unrelated messaging failures and unrelated tabs do not trigger script injection', async () => {
  const unavailable = await appFixture({ onMessage() { throw new Error('The tab was closed.'); } });
  await unavailable.modes[1].click();
  assert.equal(unavailable.injections.length, 0);
  const otherTab = await appFixture({ tabUrl: 'https://example.com/' });
  await otherTab.modes[1].click();
  assert.equal(otherTab.messages.length, 0);
  assert.match(otherTab.get('mode-note').textContent, /Open .*wordleunlimited/);
});

function backendFixture(options = {}) {
  const calls = [];
  const module = { ccall(name, resultType, types, values) {
    calls.push({ name, resultType, types: clone(types), values: clone(values) });
    if (name === 'wowdle_init') return;
    return JSON.stringify(options.result || stateFor());
  } };
  const context = vm.createContext({
    window: { WowdleModuleFactory: async () => module },
    fetch: async () => ({ ok: options.httpError ? false : true, status: options.httpError || 200, text: async () => 'crate\nslate\n' }),
  });
  vm.runInContext(backendSource, context);
  return { backend: context.window.solverBackend, calls };
}

test('backend rejects missing assets instead of switching to preview data', async () => {
  const { backend, calls } = backendFixture({ httpError: 404 });
  await assert.rejects(backend.init(), /could not load.*404/);
  assert.equal(backend.ready, false);
  assert.equal(calls.length, 0);
  assert.throws(() => backend.submit('crate', 'xxxxx'), /unavailable/);
});

test('backend initialization errors clear readiness and solver errors reach callers', async () => {
  const unavailable = backendFixture({ result: { error: 'empty dictionary' } });
  await assert.rejects(unavailable.backend.init(), /empty dictionary/);
  assert.equal(unavailable.backend.ready, false);
  const available = backendFixture();
  await available.backend.init();
  assert.equal(available.backend.ready, true);
  available.backend.module.ccall = () => JSON.stringify({ error: 'inconsistent feedback' });
  assert.throws(() => available.backend.submit('crate', 'xxxxx'), /inconsistent feedback/);
});

test('backend restore forwards exact turn history to C++', async () => {
  const { backend, calls } = backendFixture();
  await backend.init();
  backend.restore([{ guess: 'slate', feedback: 'xyxgg' }, { guess: 'crate', feedback: 'ggggg' }]);
  assert.deepEqual(calls.at(-1), {
    name: 'wowdle_restore', resultType: 'string', types: ['string'],
    values: ['slate xyxgg\ncrate ggggg'],
  });
});
