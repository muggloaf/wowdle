(() => {
  const stateToMark = {
    correct: 'g',
    present: 'y',
    absent: 'x',
  };

  function tilesInRow(row) {
    const gameTiles = [...row.querySelectorAll('game-tile')];
    if (gameTiles.length) {
      return gameTiles;
    }

    return [...row.querySelectorAll('[data-state]')];
  }

  function boardRows() {
    const rows = [...document.querySelectorAll('game-row')];
    if (rows.length) {
      return rows.map(tilesInRow).filter((tiles) => tiles.length >= 5);
    }

    const tiles = [...document.querySelectorAll('game-tile')];
    const grouped = [];
    for (let index = 0; index < tiles.length; index += 5) {
      const row = tiles.slice(index, index + 5);
      if (row.length === 5) {
        grouped.push(row);
      }
    }
    return grouped;
  }

  function completedFeedback() {
    return boardRows()
      .map((tiles) => tiles.slice(0, 5).map((tile) => stateToMark[tile.dataset.state]))
      .filter((marks) => marks.every(Boolean));
  }

  function typeGuess(guess) {
    for (const letter of guess.toLowerCase()) {
      document.dispatchEvent(new KeyboardEvent('keydown', {
        key: letter,
        code: `Key${letter.toUpperCase()}`,
        bubbles: true,
      }));
    }
    document.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Enter',
      code: 'Enter',
      bubbles: true,
    }));
  }

  function waitForFeedback(rowCount) {
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        observer.disconnect();
        reject(new Error('timed out waiting for the Wordle result'));
      }, 15000);

      const check = () => {
        const rows = completedFeedback();
        if (rows.length > rowCount) {
          clearTimeout(timeout);
          observer.disconnect();
          resolve(rows[rows.length - 1].join(''));
        }
      };

      const observer = new MutationObserver(check);
      observer.observe(document.body, {
        subtree: true,
        childList: true,
        attributes: true,
        attributeFilter: ['data-state'],
      });
      check();
    });
  }

  chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (message.type !== 'WOWDLE_PLAY_GUESS') {
      return false;
    }

    const previousRows = completedFeedback().length;
    typeGuess(message.guess);
    waitForFeedback(previousRows)
      .then((feedback) => sendResponse({ feedback }))
      .catch((error) => sendResponse({ error: error.message }));
    return true;
  });
})();