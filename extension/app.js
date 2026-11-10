const get = (id) => document.getElementById(id);
const guessInput = get('guess-input');
const feedbackInput = get('feedback-input');
const modeNote = get('mode-note');
const backend = window.solverBackend;
const stageNames = {
  many: 'information + distance', moderate: 'balanced reduction',
  small: 'likelihood + reduction', tiny: 'candidate answers',
  final: 'final guess', single: 'one answer',
};
let currentMode = 'manual';
let state = null;
let busy = true;
let turnAnalysis = [];
const savedHistoryKey = 'wowdle-manual-history-v1';

function setBusy(value) {
  busy = value;
  const playing = backend.ready && state?.status === 'playing';
  for (const id of ['submit-button', 'play-button', 'sync-button', 'export-button', 'reset-button']) {
    get(id).disabled = value || !backend.ready;
  }
  get('submit-button').disabled ||= !playing;
  get('play-button').disabled ||= !playing;
  get('undo-button').disabled = value || !playing || !state.history.length || currentMode === 'auto';
  get('completion-undo-button').disabled = value || !backend.ready || !state?.history.length || currentMode === 'auto';
  guessInput.disabled = value || !playing;
  feedbackInput.disabled = value || !playing || currentMode === 'auto';
  document.querySelectorAll('.mode').forEach((button) => { button.disabled = value || !backend.ready; });
}

function renderFeedback() {
  get('feedback-tiles').replaceChildren();
  for (let i = 0; i < 5; ++i) {
    const tile = document.createElement('span');
    const mark = feedbackInput.value[i];
    tile.className = `feedback-tile ${'xyg'.includes(mark) ? mark : ''}`;
    tile.style.setProperty('--tile-delay', `${i * 45}ms`);
    get('feedback-tiles').appendChild(tile);
  }
}

function renderAnalysis(analysis) {
  const fields = {
    entropy: analysis && `${analysis.entropy.toFixed(3)} bits`,
    probability: analysis && `${(analysis.probability * 100).toFixed(2)}%`,
    expected: analysis && analysis.expected_candidates.toFixed(2),
    worst: analysis && String(analysis.worst_case),
    distance: analysis && analysis.distance.toFixed(3),
    risk: analysis && (analysis.risk > 0 ? 'information-only' : 'candidate'),
    score: analysis && analysis.combined_score.toFixed(3),
  };
  for (const [name, value] of Object.entries(fields)) get(`analysis-${name}`).textContent = value || '—';
  get('strategy-label').textContent = analysis ? stageNames[analysis.stage] : 'custom guess';
  get('score-label').textContent = analysis ? `combined ${analysis.combined_score.toFixed(3)}` : '—';
  get('analysis-weights').textContent = analysis
    ? Object.entries(analysis.weights).map(([key, value]) => `${key} ${value.toFixed(2)}`).join(' · ')
    : '';
}

function selectGuess(word) {
  guessInput.value = word.toUpperCase();
  get('hero-guess').textContent = guessInput.value || '-----';
  const selected = state?.recommendations.find((item) => item.word.toUpperCase() === guessInput.value);
  renderAnalysis(selected?.analysis);
  document.querySelectorAll('.suggestion').forEach((button) => {
    button.setAttribute('aria-pressed', String(button.dataset.word === guessInput.value));
  });
}

function addRow(list, word, value, choose = false) {
  const row = document.createElement('li');
  const label = document.createElement(choose ? 'button' : 'span');
  label.textContent = word.toUpperCase();
  if (choose) {
    label.className = 'suggestion';
    label.dataset.word = word.toUpperCase();
    label.addEventListener('click', () => { if (!busy) selectGuess(word); });
  }
  const score = document.createElement('span');
  score.textContent = value;
  row.append(label, score);
  list.appendChild(row);
}

function saveHistory() {
  if (currentMode !== 'manual') return;
  try { localStorage.setItem(savedHistoryKey, JSON.stringify(state.history)); } catch { /* storage may be disabled */ }
}

function applyState(next) {
  state = next;
  const playing = state.status === 'playing';
  document.querySelector('.board-panel').classList.toggle('hidden', !playing);
  document.querySelector('.recommendation-panel').classList.toggle('hidden', !playing);
  get('completion-panel').classList.toggle('hidden', playing);
  get('completion-panel').classList.toggle('lost', state.status === 'lost');
  get('turn-mark').textContent = `${String(Math.min(state.history.length + 1, 6)).padStart(2, '0')} / 06`;
  get('candidate-count').textContent = `${state.candidates} candidates`;
  get('analysis-count').textContent = `${state.candidates} · ${state.guesses_remaining} turns left`;
  get('recommendation-list').replaceChildren();
  get('answer-list').replaceChildren();
  get('history-list').replaceChildren();
  for (const item of state.recommendations) {
    addRow(get('recommendation-list'), item.word, item.score.toFixed(3), true);
  }
  for (const answer of state.answers) {
    addRow(get('answer-list'), answer.word, `${(answer.probability * 100).toFixed(2)}%`);
  }
  for (const turn of state.history) addRow(get('history-list'), turn.guess, turn.feedback.toUpperCase());
  selectGuess(state.guess);
  feedbackInput.value = '';
  renderFeedback();
  if (!playing) {
    const won = state.status === 'won';
    get('completion-label').textContent = won ? 'puzzle complete' : 'out of turns';
    get('completion-title').textContent = won ? 'you got it' : 'six guesses used';
    get('completion-word').textContent = won ? state.guess.toUpperCase() : '';
    get('completion-note').textContent = won
      ? `Solved in ${state.history.length} ${state.history.length === 1 ? 'guess' : 'guesses'}.`
      : `${state.candidates} possible ${state.candidates === 1 ? 'answer remains' : 'answers remain'}.`;
  }
  get('hero-guess').classList.remove('guess-arrival');
  void get('hero-guess').offsetWidth;
  get('hero-guess').classList.add('guess-arrival');
  saveHistory();
  setBusy(busy);
}

function recordTurn(guess, feedback) {
  const item = state.recommendations.find((entry) => entry.word === guess);
  return {
    turn: state.history.length + 1, candidate_count: state.candidates,
    guesses_remaining: state.guesses_remaining, selected_guess: guess, feedback,
    analysis: item?.analysis || null,
  };
}

function submitFeedback(guess = guessInput.value, feedback = feedbackInput.value) {
  const word = guess.toLowerCase();
  const marks = feedback.toLowerCase();
  if (!/^[a-z]{5}$/.test(word)) throw new Error('Enter a five-letter guess.');
  if (!/^[xyg]{5}$/.test(marks)) throw new Error('Enter five feedback marks using g, y, and x.');
  const log = recordTurn(word, marks);
  const next = backend.submit(word, marks);
  turnAnalysis.push(log);
  applyState(next);
  modeNote.textContent = 'Feedback submitted.';
}

async function activeGameTab() {
  if (typeof chrome === 'undefined' || !chrome.tabs) {
    throw new Error('Auto mode requires the installed Chrome extension.');
  }
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id || !tab.url) throw new Error('Open the Wordle game tab, then sync.');
  const url = new URL(tab.url);
  const supported = url.protocol === 'https:' && (
    url.hostname === 'wordleunlimited.com' || url.hostname.endsWith('.wordleunlimited.com') ||
    url.hostname === 'wordleunlimited.org' || url.hostname.endsWith('.wordleunlimited.org') ||
    (url.hostname === 'www.nytimes.com' && url.pathname.startsWith('/games/wordle')));
  if (!supported) throw new Error('Open NYT Wordle, wordleunlimited.com, or wordleunlimited.org, then sync.');
  return tab.id;
}

async function gameMessage(tabId, message) {
  let result;
  try {
    result = await chrome.tabs.sendMessage(tabId, message);
  } catch (error) {
    // only retry a missing listener; an interrupted Play must never be repeated.
    if (message.type !== 'WOWDLE_GET_BOARD' || !/receiving end|establish connection/i.test(error.message)) throw error;
    await chrome.scripting.executeScript({ target: { tabId }, files: ['content.js'] });
    result = await chrome.tabs.sendMessage(tabId, message);
  }
  if (!result || result.error) throw new Error(result?.error || 'The game did not return a board.');
  return result;
}

function sameHistory(first, second) {
  return first.length === second.length && first.every((turn, i) =>
    turn.guess === second[i].guess && turn.feedback === second[i].feedback);
}

async function syncBoard() {
  const tabId = await activeGameTab();
  const board = await gameMessage(tabId, { type: 'WOWDLE_GET_BOARD' });
  if (board.maxGuesses !== 6) throw new Error('Auto requires a five-letter board with six guesses.');
  if (!sameHistory(board.history, state.history)) {
    applyState(backend.restore(board.history));
    turnAnalysis = [];
  }
  modeNote.textContent = state.status === 'playing'
    ? 'Board synced. Review the guess, then press Play.' : 'This game has ended. Start another puzzle in the game tab.';
  return { tabId, board };
}

async function run(action) {
  if (busy || !backend.ready) return;
  setBusy(true);
  try { await action(); } catch (error) { modeNote.textContent = error.message; }
  finally { setBusy(false); }
}

async function playGuess() {
  const before = state.history;
  const guess = guessInput.value.toLowerCase();
  if (!/^[a-z]{5}$/.test(guess)) throw new Error('Enter a five-letter guess.');
  const { tabId, board } = await syncBoard();
  if (!sameHistory(before, board.history) || state.status !== 'playing') return;
  modeNote.textContent = `Playing ${guess.toUpperCase()}...`;
  const result = await gameMessage(tabId, {
    type: 'WOWDLE_PLAY_GUESS', guess, expectedHistory: state.history,
  });
  submitFeedback(guess, result.feedback);
}

function chooseDialog() {
  if (busy || !state?.recommendations.length) return;
  get('guess-choices').replaceChildren();
  state.recommendations.forEach((item, i) => {
    const button = document.createElement('button');
    button.textContent = `${i + 1}. ${item.word.toUpperCase()} · ${item.score.toFixed(3)}`;
    button.addEventListener('click', () => {
      selectGuess(item.word);
      get('change-dialog').close();
      feedbackInput.focus();
    });
    get('guess-choices').appendChild(button);
  });
  get('change-dialog').showModal();
}

function reset() {
  turnAnalysis = [];
  applyState(backend.restore([]));
  modeNote.textContent = currentMode === 'auto'
    ? 'Start a new puzzle in the game tab, then sync board.' : 'New solver session.';
}

get('submit-button').addEventListener('click', () => run(() => submitFeedback()));
get('play-button').addEventListener('click', () => run(playGuess));
get('sync-button').addEventListener('click', () => run(syncBoard));
function undoTurn() {
  applyState(backend.restore(state.history.slice(0, -1)));
  turnAnalysis.pop();
  modeNote.textContent = 'Last turn removed.';
}
get('undo-button').addEventListener('click', () => run(undoTurn));
get('completion-undo-button').addEventListener('click', () => run(undoTurn));
get('reset-button').addEventListener('click', () => run(reset));
get('again-button').addEventListener('click', () => run(reset));
get('help-button').addEventListener('click', () => get('help-dialog').showModal());
get('close-help').addEventListener('click', () => get('help-dialog').close());
get('close-change').addEventListener('click', () => get('change-dialog').close());
get('export-button').addEventListener('click', () => {
  const blob = new Blob([JSON.stringify({ history: state.history, turns: turnAnalysis, state }, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = 'wowdle-analysis.json';
  link.click();
  URL.revokeObjectURL(url);
});
guessInput.addEventListener('input', () => selectGuess(guessInput.value.replace(/[^a-z]/gi, '').slice(0, 5)));
feedbackInput.addEventListener('input', () => {
  feedbackInput.value = feedbackInput.value.toLowerCase().replace(/[^xyg]/g, '').slice(0, 5);
  renderFeedback();
});
document.querySelectorAll('.mode').forEach((button) => button.addEventListener('click', () => run(async () => {
  currentMode = button.dataset.mode;
  document.querySelectorAll('.mode').forEach((item) => item.classList.toggle('active', item === button));
  const auto = currentMode === 'auto';
  get('submit-button').classList.toggle('hidden', auto);
  get('play-button').classList.toggle('hidden', !auto);
  get('sync-button').classList.toggle('hidden', !auto);
  modeNote.textContent = auto ? 'Reading game board...' : 'Enter the feedback from your game, then submit.';
  if (auto) await syncBoard();
})));
document.addEventListener('keydown', (event) => {
  if (event.ctrlKey || event.altKey || event.metaKey || busy) return;
  if (get('change-dialog').open) {
    const index = Number(event.key) - 1;
    if (index >= 0 && index < state.recommendations.length && /^[1-5]$/.test(event.key)) {
      event.preventDefault();
      get('guess-choices').children[index].click();
    }
    return;
  }
  if (get('help-dialog').open) return;
  if (event.key.toLowerCase() === 'c' && event.target !== guessInput) {
    event.preventDefault();
    chooseDialog();
  } else if (event.key === 'Enter' && currentMode === 'manual' &&
             (event.target === feedbackInput || event.target === guessInput || event.target === document.body)) {
    event.preventDefault();
    run(() => submitFeedback());
  } else if (event.key === 'ArrowRight' && currentMode === 'auto' &&
             event.target !== guessInput && event.target !== feedbackInput) {
    event.preventDefault();
    run(playGuess);
  }
});

setBusy(true);
renderFeedback();
backend.init().then((initial) => {
  let saved;
  try { saved = JSON.parse(localStorage.getItem(savedHistoryKey) || '[]'); } catch { saved = []; }
  applyState(initial);
  if (Array.isArray(saved) && saved.length) {
    try { applyState(backend.restore(saved)); } catch (error) { modeNote.textContent = error.message; }
  }
  get('backend-status').textContent = 'cpp solver bridge: connected';
  if (!modeNote.textContent || modeNote.textContent.startsWith('Loading')) {
    modeNote.textContent = 'Enter the colors from your game, then submit.';
  }
}).catch((error) => {
  get('backend-status').textContent = 'cpp solver bridge: unavailable';
  get('score-label').textContent = 'unavailable';
  get('candidate-count').textContent = 'unavailable';
  modeNote.textContent = error.message;
}).finally(() => setBusy(false));
