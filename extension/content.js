(() => {
  if (globalThis.__wowdleContentBridge) return;
  globalThis.__wowdleContentBridge = true;

  const marks = { correct: 'g', present: 'y', absent: 'x', green: 'g', yellow: 'y', gray: 'x', grey: 'x' };
  let playing = false;

  function fail(code, message) {
    const error = new Error(message);
    error.code = code;
    throw error;
  }

  function rootsWithin(root = document) {
    const roots = [root];
    for (let index = 0; index < roots.length; index += 1) {
      if (roots[index].shadowRoot) roots.push(roots[index].shadowRoot);
      for (const element of roots[index].querySelectorAll('*')) {
        if (element.shadowRoot) roots.push(element.shadowRoot);
      }
    }
    return [...new Set(roots)];
  }

  function findAll(selector, root = document) {
    return rootsWithin(root).flatMap((scope) => [...scope.querySelectorAll(selector)]);
  }

  function visible(element) {
    return !element.hidden && element.getAttribute('aria-hidden') !== 'true'
      && element.getClientRects().length > 0;
  }

  function rowTiles(row, adapter) {
    if (adapter === 'wordplay') return findAll('.flip-container, .wp-cell', row);
    const gameTiles = findAll('game-tile', row);
    return gameTiles.length ? gameTiles : findAll('[data-state], [evaluation]', row);
  }

  function locateBoard() {
    const groups = new Map();
    const add = (row, adapter) => {
      if (!row || !visible(row)) return;
      const parent = row.parentElement || row.getRootNode();
      if (!groups.has(parent)) groups.set(parent, { adapter, rows: [] });
      const group = groups.get(parent);
      if (!group.rows.includes(row)) group.rows.push(row);
    };
    findAll('game-row, [class*="Row-module_row__"], [data-testid="row"]')
      .forEach((row) => add(row, 'wordle'));

    // wordplay uses flip cards for completed cells and wrappers for editable cells.
    findAll('.flip-container, .wp-cell').forEach((tile) => {
      add(tile.matches('.flip-container') ? tile.parentElement : tile.parentElement?.parentElement, 'wordplay');
    });
    const boards = [...groups.values()].filter((group) => group.rows.length >= 6 && group.rows.length <= 10);
    const board = boards.find((group) => group.rows.every((row) => rowTiles(row, group.adapter).length === 5));
    if (!board) {
      if (boards.length) fail('WORD_LENGTH', 'Auto mode needs a five-letter board. Switch the game to five letters.');
      fail('UNSUPPORTED_BOARD', 'No supported board found. Open a five-letter Wordle game, close its welcome screen, then try again.');
    }
    return board;
  }

  function readTile(tile) {
    const flip = tile.matches('.flip-container');
    const rendered = tile.matches('game-tile') ? tile.shadowRoot?.querySelector('.tile[data-state]') : null;
    const face = flip ? tile.querySelector('.flip-front') : tile;
    const text = (tile.getAttribute('letter') || tile.getAttribute('data-letter') || face?.textContent || '').trim().toLowerCase();
    const letter = /^[a-z]$/.test(text) ? text : '';
    let mark;
    // legacy tiles receive evaluation attributes before their visible colors flip.
    let pending = !!(rendered && marks[tile.getAttribute('evaluation')] && !marks[rendered.getAttribute('data-state')]);
    const stateNodes = rendered ? [rendered] : [tile, ...findAll('[data-state], [evaluation], .flip-back', tile)];
    for (const node of stateNodes) {
      const state = node.getAttribute('data-state') || node.getAttribute('evaluation');
      mark = mark || marks[state];
      for (const name of ['correct', 'present', 'absent']) {
        if (node.classList.contains('wp-cell-' + name)) mark = marks[name];
      }
    }
    const animationNode = rendered || tile;
    const animation = animationNode.getAttribute('data-animation');
    const animating = ['flip-in', 'flip-out'].includes(animation)
      || animationNode.getAnimations?.({ subtree: true }).some((item) => item.playState === 'running');
    if (mark && (animating || (flip && !tile.classList.contains('flip-hover')))) {
      pending = true;
      mark = undefined;
    }
    return { letter, mark, pending };
  }

  function readBoard() {
    const board = locateBoard();
    const history = [];
    let currentGuess = '';
    let foundCurrent = false;
    let invalid = false;
    for (const row of board.rows) {
      const tiles = rowTiles(row, board.adapter).map(readTile);
      if (tiles.every((tile) => tile.letter && tile.mark)) {
        if (foundCurrent) fail('BOARD_CHANGED', 'The board is changing. Wait for the tiles to finish, then try again.');
        history.push({ guess: tiles.map((tile) => tile.letter).join(''), feedback: tiles.map((tile) => tile.mark).join('') });
      } else {
        if (tiles.some((tile) => tile.mark || tile.pending)) fail('ANIMATING', 'Wait for the tile colors to finish, then try again.');
        const letters = tiles.map((tile) => tile.letter).join('');
        if (!foundCurrent) {
          if (tiles.map((tile) => tile.letter || ' ').join('').trim() !== letters) {
            fail('PARTIAL_GUESS', 'The current row has gaps. Clear it in the game before pressing Play.');
          }
          currentGuess = letters;
          invalid = row.getAttribute('data-animation') === 'shake'
            || (row.matches('game-row') && row.getAttribute('invalid') !== null)
            || findAll('.cell-wiggle, .wp-cell-invalid, [data-state="invalid"], [data-animation="shake"]', row).length > 0;
          foundCurrent = true;
        } else if (letters) {
          fail('BOARD_CHANGED', 'The board has more than one unfinished row. Finish or clear the game first.');
        }
      }
    }
    const status = history.some((turn) => turn.feedback === 'ggggg') ? 'won'
      : history.length === board.rows.length ? 'lost' : 'playing';
    return { supported: true, history, currentGuess, maxGuesses: board.rows.length, status, invalid, adapter: board.adapter };
  }

  function sameHistory(first, second) {
    return first.length === second.length && first.every((turn, index) =>
      typeof second[index]?.guess === 'string' && typeof second[index]?.feedback === 'string'
      && turn.guess === second[index].guess.toLowerCase() && turn.feedback === second[index].feedback.toLowerCase());
  }

  function rejection() {
    const alerts = findAll('[role="alert"], game-toast, [class*="Toast-module_toast"], .Toastify__toast, .wordplay-message');
    const messages = alerts.map((element) => ({
      element,
      text: element.matches('game-toast')
        ? element.shadowRoot?.querySelector('.toast')?.textContent || element.getAttribute('text') || element.textContent
        : element.textContent,
    }));
    const alert = messages.find((item) => visible(item.element)
      && /not (?:in (?:the )?(?:word )?list|a valid word)|invalid word|not enough letters|must contain|must use|hard mode|is not a word|must be .*letters long/i.test(item.text));
    return alert?.text.trim();
  }

  function waitFor(check, timeout, timeoutMessage) {
    return new Promise((resolve, reject) => {
      let finished = false;
      let observer;
      let timer;
      let poll;
      const finish = (error, value) => {
        if (finished) return;
        finished = true;
        observer?.disconnect();
        clearTimeout(timer);
        clearInterval(poll);
        if (error) reject(error); else resolve(value);
      };
      const inspect = () => {
        if (finished) return;
        try {
          const result = check();
          if (result !== undefined) finish(null, result);
        } catch (error) {
          if (error.code !== 'ANIMATING') finish(error);
        }
      };
      observer = new MutationObserver(inspect);
      for (const root of rootsWithin()) observer.observe(root, { subtree: true, childList: true, attributes: true, characterData: true });
      timer = setTimeout(() => {
        const error = new Error(timeoutMessage);
        error.code = 'TIMEOUT';
        finish(error);
      }, timeout);
      // polling catches shadow roots mounted after observation began.
      poll = setInterval(inspect, 75);
      inspect();
    });
  }

  function pressKey(key, adapter) {
    const buttons = adapter === 'wordplay' ? findAll('button.wp-key') : findAll('button[data-key]');
    const aliases = key === 'Enter' ? ['enter', '↵'] : [key.toLowerCase()];
    const button = buttons.find((item) => visible(item) && !item.disabled
      && aliases.includes(adapter === 'wordplay' ? item.textContent.trim().toLowerCase() : item.getAttribute('data-key')?.toLowerCase()));
    if (button) {
      button.click();
      return;
    }
    const code = key === 'Enter' ? 'Enter' : 'Key' + key.toUpperCase();
    const keyCode = key === 'Enter' ? 13 : key.toUpperCase().charCodeAt(0);
    for (const type of ['keydown', 'keyup']) {
      document.dispatchEvent(new KeyboardEvent(type, { key, code, keyCode, which: keyCode, bubbles: true, composed: true }));
    }
  }

  async function playGuess(message) {
    const guess = typeof message.guess === 'string' ? message.guess.toLowerCase() : '';
    if (!/^[a-z]{5}$/.test(guess)) fail('INVALID_GUESS', 'Choose a five-letter guess before pressing Play.');
    const before = readBoard();
    if (before.status !== 'playing') fail('GAME_OVER', 'This game is ' + (before.status === 'won' ? 'already solved' : 'out of guesses') + '. Start a new game first.');
    if (message.expectedHistory !== undefined) {
      if (!Array.isArray(message.expectedHistory) || !sameHistory(before.history, message.expectedHistory)) {
        fail('HISTORY_MISMATCH', 'The game changed since the solver synced. Sync the board again before playing.');
      }
    } else if (message.expectedRows !== before.history.length) {
      fail('HISTORY_MISMATCH', 'Sync the game board before pressing Play.');
    }
    if (before.currentGuess && before.currentGuess !== guess) {
      fail('PARTIAL_GUESS', 'The game row contains ' + before.currentGuess.toUpperCase() + '. Clear it in the game before pressing Play.');
    }
    const dialogs = findAll('dialog[open], game-modal[open], [role="dialog"][aria-modal="true"]');
    if (dialogs.some(visible)) fail('DIALOG_OPEN', 'Close the game dialog before pressing Play.');

    let typed = before.currentGuess;
    for (const letter of guess.slice(typed.length)) {
      const previous = typed;
      typed += letter;
      const expected = typed;
      const entered = waitFor(() => {
        const board = readBoard();
        if (!sameHistory(board.history, before.history)) fail('BOARD_CHANGED', 'The board changed while entering the guess. Sync it again.');
        if (board.currentGuess === expected) return true;
        if (board.currentGuess !== previous) fail('INPUT_CHANGED', 'The game row changed while entering the guess. Clear it and try again.');
      }, 1500, 'The game did not accept the letters. Close any game dialogs, clear the current row, then try again.');
      pressKey(letter, before.adapter);
      await entered;
    }

    const result = waitFor(() => {
      const board = readBoard();
      if (board.history.length > before.history.length) {
        const turn = board.history[before.history.length];
        if (board.history.length !== before.history.length + 1 || !sameHistory(board.history.slice(0, -1), before.history) || turn.guess !== guess) {
          fail('BOARD_CHANGED', 'A different guess appeared on the board. Sync the game again.');
        }
        return { ...board, guess, feedback: turn.feedback };
      }
      if (!sameHistory(board.history, before.history)) fail('BOARD_CHANGED', 'The game restarted while waiting for feedback. Sync it again.');
      const rejected = rejection();
      if (board.invalid || rejected) fail('REJECTED_GUESS', rejected || guess.toUpperCase() + ' was rejected by the game. Choose another guess and clear the game row.');
    }, 15000, 'No feedback arrived. The game may have rejected the guess or still have a dialog open. Check the game before trying again.');
    pressKey('Enter', before.adapter);
    return result;
  }

  chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (message?.type === 'WOWDLE_GET_BOARD') {
      try { sendResponse(readBoard()); } catch (error) { sendResponse({ error: error.message, code: error.code }); }
      return false;
    }
    if (message?.type !== 'WOWDLE_PLAY_GUESS') return false;
    if (playing) {
      sendResponse({ error: 'A guess is already being played. Wait for its result.', code: 'BUSY' });
      return false;
    }
    playing = true;
    playGuess(message).then((result) => {
      playing = false;
      sendResponse(result);
    }, (error) => {
      playing = false;
      sendResponse({ error: error.message, code: error.code || 'GAME_ERROR' });
    });
    return true;
  });
})();
