const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const source = readFileSync(path.join(__dirname, '../extension/content.js'), 'utf8');
const states = { g: 'correct', y: 'present', x: 'absent' };

// a small DOM fixture keeps these interaction tests dependency-free.
class Element {
  constructor(tag, attributes = {}, text = '') {
    this.tagName = tag;
    this.attributes = { ...attributes };
    this.children = [];
    this.text = text;
    this.hidden = false;
    this.classList = { contains: (name) => (this.attributes.class || '').split(/\s+/).includes(name) };
  }
  append(child) {
    child.parentNode = this;
    child.parentElement = this.tagName.startsWith('#') ? null : this;
    this.children.push(child);
    return child;
  }
  get textContent() { return this.text + this.children.map((child) => child.textContent).join(''); }
  getAttribute(name) { return this.attributes[name] ?? null; }
  getRootNode() { return this.parentNode ? this.parentNode.getRootNode() : this; }
  getClientRects() { return this.hidden ? [] : [{}]; }
  matches(selector) {
    return selector.split(',').some((part) => {
      const rule = part.trim();
      if (rule === '*') return true;
      const tag = rule.match(/^[a-z][\w-]*/i)?.[0];
      if (tag && tag !== this.tagName) return false;
      for (const match of rule.matchAll(/\.([\w-]+)/g)) {
        if (!this.classList.contains(match[1])) return false;
      }
      for (const match of rule.matchAll(/\[([\w-]+)(?:(\*?=)"([^"]*)")?\]/g)) {
        const value = this.getAttribute(match[1]);
        if (value === null) return false;
        if (match[2] === '=' && value !== match[3]) return false;
        if (match[2] === '*=' && !value.includes(match[3])) return false;
      }
      return true;
    });
  }
  querySelectorAll(selector) {
    return this.children.flatMap((child) => [
      ...(child.matches(selector) ? [child] : []), ...child.querySelectorAll(selector),
    ]);
  }
  querySelector(selector) { return this.querySelectorAll(selector)[0] || null; }
  click() { this.onClick?.(); }
}

function fixture(adapter = 'modern', options = {}) {
  const document = new Element('#document');
  document.body = document.append(new Element('body'));
  const observers = new Set();
  const timers = new Set();
  const intervals = new Set();
  const listeners = [];
  const keys = [];
  const rows = [];
  let cursor = 0;
  let typed = '';
  let onEnter = options.onEnter;
  const notify = () => queueMicrotask(() => {
    for (const observer of [...observers]) observer.callback();
  });
  const host = document.body.append(new Element('main'));
  let board = host;
  if (adapter === 'legacy') {
    host.shadowRoot = new Element('#shadow-root');
    board = host.shadowRoot;
  }
  for (let index = 0; index < 6; index += 1) {
    const row = board.append(new Element(adapter === 'legacy' ? 'game-row' : 'div',
      adapter === 'modern' ? { class: 'Row-module_row__fixture' } : {}));
    rows.push(row);
    if (adapter === 'legacy') row.shadowRoot = new Element('#shadow-root');
  }

  function setRow(index, word = '', feedback = '', pending = false) {
    const row = rows[index];
    const contents = row.shadowRoot || row;
    contents.children = [];
    for (let position = 0; position < (options.length || 5); position += 1) {
      const letter = word[position] || '';
      const state = states[feedback[position]];
      if (adapter === 'wordplay' && state) {
        const tile = contents.append(new Element('div', { class: 'flip-container' + (pending ? '' : ' flip-hover') }));
        tile.append(new Element('div', { class: 'flip-front' }, letter.toUpperCase()));
        tile.append(new Element('div', { class: 'flip-back wp-cell-' + state }, letter.toUpperCase()));
      } else if (adapter === 'wordplay') {
        contents.append(new Element('div')).append(new Element('div', { class: 'wp-cell wp-cell-typing' }, letter));
      } else if (adapter === 'legacy') {
        const tile = contents.append(new Element('game-tile', { letter, evaluation: state || 'empty' }));
        tile.shadowRoot = new Element('#shadow-root');
        tile.shadowRoot.append(new Element('div', {
          class: 'tile', 'data-state': state && !pending ? state : letter ? 'tbd' : 'empty', 'data-animation': 'idle',
        }, letter));
      } else {
        contents.append(new Element('div', { class: 'Tile-module_tile__fixture', 'data-state': state || 'empty' }, letter));
      }
    }
    notify();
  }
  for (let index = 0; index < 6; index += 1) setRow(index);

  function acceptKey(key) {
    keys.push(key);
    if (options.ignoreInput) return;
    if (key === 'Enter') {
      if (onEnter) onEnter({ rows, setRow, notify, document, typed });
      else {
        setRow(cursor, typed, options.feedback || 'xyxgg');
        cursor += 1;
        typed = '';
      }
    } else {
      typed += key.toLowerCase();
      setRow(cursor, typed);
    }
  }
  document.dispatchEvent = (event) => {
    // the .org legacy game listens on keydown; wordplay also supports keyup.
    if (event.type === (options.inputEvent || (adapter === 'wordplay' ? 'keyup' : 'keydown'))) acceptKey(event.key);
  };
  if (options.buttons || adapter === 'wordplay') {
    for (const key of [...'abcdefghijklmnopqrstuvwxyz', 'Enter']) {
      const button = document.body.append(new Element('button', adapter === 'wordplay'
        ? { class: 'wp-key' } : { 'data-key': key.toLowerCase() }, key.toUpperCase()));
      button.onClick = () => acceptKey(key);
    }
  }
  const context = vm.createContext({
    document,
    chrome: { runtime: { onMessage: { addListener: (listener) => listeners.push(listener) } } },
    MutationObserver: class {
      constructor(callback) { this.callback = callback; }
      observe() { observers.add(this); }
      disconnect() { observers.delete(this); }
    },
    KeyboardEvent: class { constructor(type, data) { Object.assign(this, { type }, data); } },
    setTimeout(callback, delay) {
      const id = setTimeout(() => { timers.delete(id); callback(); }, Math.min(delay, 40));
      timers.add(id);
      return id;
    },
    clearTimeout(id) { clearTimeout(id); timers.delete(id); },
    setInterval(callback) { const id = setInterval(callback, 2); intervals.add(id); return id; },
    clearInterval(id) { clearInterval(id); intervals.delete(id); },
  });
  vm.runInContext(source, context);
  return {
    document, rows, setRow, keys, observers, timers, intervals, listeners,
    reinject: () => vm.runInContext(source, context),
    send: (message) => new Promise((resolve) => listeners[0](message, {}, (response) => resolve(JSON.parse(JSON.stringify(response))))),
    startAt(index, word = '') { cursor = index; typed = word; setRow(index, word); },
    setOnEnter(handler) { onEnter = handler; },
  };
}

function play(game, extra = {}) {
  return game.send({ type: 'WOWDLE_PLAY_GUESS', guess: 'crate', expectedHistory: [], ...extra });
}

function assertClean(game) {
  assert.equal(game.observers.size, 0);
  assert.equal(game.timers.size, 0);
  assert.equal(game.intervals.size, 0);
}

for (const adapter of ['modern', 'legacy', 'wordplay']) {
  test(adapter + ': reads only rendered history and current letters', async () => {
    const game = fixture(adapter);
    game.setRow(0, 'slate', 'xyxgg');
    game.setRow(1, 'cr');
    const board = await game.send({ type: 'WOWDLE_GET_BOARD' });
    assert.deepEqual(board.history, [{ guess: 'slate', feedback: 'xyxgg' }]);
    assert.equal(board.currentGuess, 'cr');
    assert.equal(board.maxGuesses, 6);
    assert.equal(board.status, 'playing');
    assert.deepEqual(game.keys, []);
  });
  test(adapter + ': types and verifies one submitted guess', async () => {
    const game = fixture(adapter);
    const result = await play(game);
    assert.equal(result.guess, 'crate');
    assert.equal(result.feedback, 'xyxgg');
    assert.deepEqual(result.history, [{ guess: 'crate', feedback: 'xyxgg' }]);
    assert.deepEqual(game.keys, ['c', 'r', 'a', 't', 'e', 'Enter']);
    assertClean(game);
  });
}

test('reinjection installs only one listener', () => {
  const game = fixture();
  game.reinject();
  assert.equal(game.listeners.length, 1);
});

test('checks exact history, not just the number of previous rows', async () => {
  const game = fixture();
  game.setRow(0, 'slate', 'xyxgg');
  const result = await play(game, { expectedHistory: [{ guess: 'stare', feedback: 'xyxgg' }] });
  assert.equal(result.code, 'HISTORY_MISMATCH');
  assert.deepEqual(game.keys, []);
});

test('refuses missing synchronization and malformed guesses', async () => {
  const game = fixture();
  assert.equal((await play(game, { expectedHistory: undefined })).code, 'HISTORY_MISMATCH');
  assert.equal((await play(game, { guess: 'bad!' })).code, 'INVALID_GUESS');
  assert.deepEqual(game.keys, []);
});

test('does not overwrite a partially entered word', async () => {
  const game = fixture();
  game.startAt(0, 'cr');
  assert.equal((await play(game)).code, 'PARTIAL_GUESS');
  assert.deepEqual(game.keys, []);
});

test('submits an already typed matching word without duplicating letters', async () => {
  const game = fixture();
  game.startAt(0, 'crate');
  assert.equal((await play(game)).feedback, 'xyxgg');
  assert.deepEqual(game.keys, ['Enter']);
  assertClean(game);
});

test('rejects simultaneous Play requests', async () => {
  const game = fixture();
  const first = play(game);
  assert.equal((await play(game)).code, 'BUSY');
  assert.equal((await first).guess, 'crate');
  assertClean(game);
});

test('waits for flip cards to reveal their feedback', async () => {
  const game = fixture('wordplay', { onEnter({ setRow, typed }) {
    setRow(0, typed, 'xyxgg', true);
    setTimeout(() => setRow(0, typed, 'xyxgg'), 5);
  } });
  assert.equal((await play(game)).feedback, 'xyxgg');
  assertClean(game);
});

test('reports rejected words and releases observers', async () => {
  const game = fixture('modern', { onEnter({ document, notify }) {
    document.body.append(new Element('div', { role: 'alert' }, 'Not in word list'));
    notify();
  } });
  const result = await play(game);
  assert.equal(result.code, 'REJECTED_GUESS');
  assert.match(result.error, /Not in word list/);
  assertClean(game);
});

test('detects rejection animations inside a shadow board', async () => {
  const game = fixture('legacy', { onEnter({ rows, notify }) {
    rows[0].attributes['data-animation'] = 'shake';
    notify();
  } });
  assert.equal((await play(game)).code, 'REJECTED_GUESS');
  assertClean(game);
});

test('.org legacy game-row invalid attribute reports a rejected guess', async () => {
  const game = fixture('legacy', { onEnter({ rows, notify }) {
    rows[0].attributes.invalid = '';
    notify();
  } });
  assert.equal((await play(game)).code, 'REJECTED_GUESS');
  assertClean(game);
});

test('.org shadow toast text is read when its host has no light DOM text', async () => {
  const game = fixture('legacy', { onEnter({ document, notify }) {
    const toast = document.body.append(new Element('game-toast', { text: 'Not in word list' }));
    toast.shadowRoot = new Element('#shadow-root');
    toast.shadowRoot.append(new Element('div', { class: 'toast' }, 'Not in word list'));
    notify();
  } });
  const result = await play(game);
  assert.equal(result.code, 'REJECTED_GUESS');
  assert.equal(result.error, 'Not in word list');
  assertClean(game);
});

test('.org host evaluation does not reveal feedback before visible tile colors', async () => {
  const game = fixture('legacy');
  game.setRow(0, 'crate', 'xyxgg', true);
  assert.equal((await game.send({ type: 'WOWDLE_GET_BOARD' })).code, 'ANIMATING');
  game.setRow(0, 'crate', 'xyxgg');
  const board = await game.send({ type: 'WOWDLE_GET_BOARD' });
  assert.deepEqual(board.history, [{ guess: 'crate', feedback: 'xyxgg' }]);
});

test('legacy hosts without the .org rendered-state element retain their feedback fallback', async () => {
  const game = fixture('legacy');
  game.setRow(0, 'crate', 'xyxgg');
  for (const tile of game.rows[0].shadowRoot.children) tile.shadowRoot.children[0].attributes = {};
  const board = await game.send({ type: 'WOWDLE_GET_BOARD' });
  assert.deepEqual(board.history, [{ guess: 'crate', feedback: 'xyxgg' }]);
});

test('.org Play waits for rendered shadow tiles to finish flipping', async () => {
  const game = fixture('legacy', { onEnter({ rows, setRow, typed, notify }) {
    setRow(0, typed, 'xyxgg', true);
    setTimeout(() => {
      setRow(0, typed, 'xyxgg');
      const last = rows[0].shadowRoot.children[4].shadowRoot.children[0];
      last.attributes['data-animation'] = 'flip-out';
      notify();
      setTimeout(() => { last.attributes['data-animation'] = 'idle'; notify(); }, 5);
    }, 5);
  } });
  assert.equal((await play(game)).feedback, 'xyxgg');
  assertClean(game);
});

test('.org arrow Enter button is clicked without a synthetic keyboard fallback', async () => {
  const game = fixture('legacy', { buttons: true });
  game.document.querySelector('button[data-key="enter"]').attributes['data-key'] = '↵';
  game.document.dispatchEvent = () => { throw new Error('keyboard fallback should not be used'); };
  assert.equal((await play(game)).feedback, 'xyxgg');
  assert.deepEqual(game.keys, ['c', 'r', 'a', 't', 'e', 'Enter']);
  assertClean(game);
});

test('.org open custom game modal blocks Play until closed', async () => {
  const game = fixture('legacy');
  const modal = game.document.body.append(new Element('game-modal', { open: '' }));
  assert.equal((await play(game)).code, 'DIALOG_OPEN');
  assert.deepEqual(game.keys, []);
  delete modal.attributes.open;
  assert.equal((await play(game)).feedback, 'xyxgg');
  assertClean(game);
});

test('does not read modern tile feedback during a flip animation', async () => {
  const game = fixture();
  game.setRow(0, 'crate', 'xyxgg');
  game.rows[0].children[4].attributes['data-animation'] = 'flip-in';
  assert.equal((await game.send({ type: 'WOWDLE_GET_BOARD' })).code, 'ANIMATING');
});

test('rejects gaps in the current row', async () => {
  const game = fixture('wordplay');
  game.setRow(0, 'c a');
  assert.equal((await game.send({ type: 'WOWDLE_GET_BOARD' })).code, 'PARTIAL_GUESS');
  assert.deepEqual(game.keys, []);
});

test('does not mistake another submitted word for the selected guess', async () => {
  const game = fixture('modern', { onEnter({ setRow }) { setRow(0, 'slate', 'xyxgg'); } });
  assert.equal((await play(game)).code, 'BOARD_CHANGED');
  assertClean(game);
});

test('typing and feedback timeouts clean up and unlock Play', async () => {
  const ignored = fixture('modern', { ignoreInput: true });
  assert.equal((await play(ignored)).code, 'TIMEOUT');
  assertClean(ignored);
  assert.equal((await play(ignored)).code, 'TIMEOUT');
  assertClean(ignored);
  const silent = fixture('modern', { onEnter() {} });
  assert.equal((await play(silent)).code, 'TIMEOUT');
  assertClean(silent);
});

test('detects completed and lost boards before entering letters', async () => {
  const won = fixture();
  won.setRow(0, 'crate', 'ggggg');
  assert.equal((await won.send({ type: 'WOWDLE_GET_BOARD' })).status, 'won');
  assert.equal((await play(won)).code, 'GAME_OVER');
  const lost = fixture();
  for (let row = 0; row < 6; row += 1) lost.setRow(row, 'crate', 'xxxxx');
  assert.equal((await lost.send({ type: 'WOWDLE_GET_BOARD' })).status, 'lost');
  assert.equal((await play(lost)).code, 'GAME_OVER');
  assert.deepEqual(won.keys.concat(lost.keys), []);
});

test('unsupported boards and non-five-letter boards return actionable errors', async () => {
  const unsupported = fixture();
  unsupported.document.body.children = [];
  assert.equal((await unsupported.send({ type: 'WOWDLE_GET_BOARD' })).code, 'UNSUPPORTED_BOARD');
  const six = fixture('wordplay', { length: 6 });
  assert.equal((await six.send({ type: 'WOWDLE_GET_BOARD' })).code, 'WORD_LENGTH');
});

test('a visible game dialog blocks Play', async () => {
  const game = fixture();
  game.document.body.append(new Element('dialog', { open: '' }));
  assert.equal((await play(game)).code, 'DIALOG_OPEN');
  assert.deepEqual(game.keys, []);
});
