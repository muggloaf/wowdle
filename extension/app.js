const mockRecommendations = [
  { word: 'THEIR', score: '0.94' },
  { word: 'ABOUT', score: '0.91' },
  { word: 'CRATE', score: '0.87' },
  { word: 'SLATE', score: '0.84' },
  { word: 'WHITE', score: '0.81' }
];

const guessInput = document.querySelector('#guess-input');
const feedbackInput = document.querySelector('#feedback-input');
const feedbackTiles = document.querySelector('#feedback-tiles');
const heroGuess = document.querySelector('#hero-guess');
const recommendationList = document.querySelector('#recommendation-list');
const submitButton = document.querySelector('#submit-button');
const playButton = document.querySelector('#play-button');
const modeNote = document.querySelector('#mode-note');
const modes = document.querySelectorAll('.mode');
const candidateCount = document.querySelector('#candidate-count');
const analysisCount = document.querySelector('#analysis-count');
const scoreLabel = document.querySelector('#score-label');
const turnMark = document.querySelector('#turn-mark');
const boardPanel = document.querySelector('.board-panel');
const recommendationPanel = document.querySelector('.recommendation-panel');
const completionPanel = document.querySelector('#completion-panel');
const completionWord = document.querySelector('#completion-word');
const againButton = document.querySelector('#again-button');
const helpButton = document.querySelector('#help-button');
const closeHelp = document.querySelector('#close-help');
const helpDialog = document.querySelector('#help-dialog');

let currentMode = 'manual';
let submittedTurns = 0;
let recommendations = mockRecommendations;

function renderFeedbackTiles() {
  const feedback = feedbackInput.value.toLowerCase();
  feedbackTiles.innerHTML = '';

  for (let index = 0; index < 5; index += 1) {
    const tile = document.createElement('span');
    const mark = feedback[index];
    tile.className = `feedback-tile ${mark === 'x' || mark === 'y' || mark === 'g' ? mark : ''}`;
    tile.style.setProperty('--tile-delay', `${index * 45}ms`);
    feedbackTiles.appendChild(tile);
  }
}

function renderRecommendations() {
  recommendationList.innerHTML = recommendations.map((item) => `
    <li style="--row-delay: ${recommendations.indexOf(item) * 55}ms"><span>${item.word}</span><span>${item.score}</span></li>
  `).join('');
}

function updateTurnMark() {
  const turn = Math.min(submittedTurns + 1, 6);
  turnMark.textContent = `${String(turn).padStart(2, '0')} / 06`;
}

function animateGuess() {
  heroGuess.classList.remove('guess-arrival');
  void heroGuess.offsetWidth;
  heroGuess.classList.add('guess-arrival');
}

function showCompletion(word) {
  completionWord.textContent = word.toUpperCase();
  boardPanel.classList.add('hidden');
  recommendationPanel.classList.add('hidden');
  completionPanel.classList.remove('hidden');
  completionPanel.classList.add('completion-arrival');
}

async function sendToActiveGame(message) {
  if (typeof chrome === 'undefined' || !chrome.tabs) {
    throw new Error('auto mode only works inside the installed extension');
  }

  const tabs = await chrome.tabs.query({ active: true, currentWindow: true });
  const tab = tabs[0];
  if (!tab || !tab.id) {
    throw new Error('no active game tab found');
  }

  try {
    return await chrome.tabs.sendMessage(tab.id, message);
  } catch (error) {
    await chrome.scripting.executeScript({
      target: { tabId: tab.id },
      files: ['content.js'],
    });
    return chrome.tabs.sendMessage(tab.id, message);
  }
}

function applyBackendState(state) {
  if (!state || state.error) {
    return false;
  }

  recommendations = state.recommendations.map((item) => ({
    word: item.word.toUpperCase(),
    score: Number(item.score).toFixed(2),
  }));
  updateGuess(state.guess);
  updateTurnMark();
  animateGuess();
  scoreLabel.textContent = `combined ${recommendations[0].score}`;
  candidateCount.textContent = `${state.candidates} candidates`;
  analysisCount.textContent = state.candidates;
  renderRecommendations();
  return true;
}

function updateGuess(word) {
  const normalized = word.toUpperCase().slice(0, 5);
  guessInput.value = normalized;
  heroGuess.textContent = normalized;
}

function submitSolverFeedback(guess, feedback) {
  if (feedback.length !== 5) {
    modeNote.textContent = 'Enter five feedback marks before submitting.';
    feedbackInput.focus();
    return;
  }

  if (feedback === 'ggggg') {
    showCompletion(guess);
    return;
  }

  const state = window.solverBackend.submit(
    guess.toLowerCase(),
    feedback,
  );

  if (state && state.error) {
    modeNote.textContent = 'That feedback leaves no possible answers. Check the tile colors and try again.';
    return;
  }

  submittedTurns += 1;
  if (!applyBackendState(state)) {
    candidateCount.textContent = `${Math.max(3, 216 - submittedTurns * 41)} candidates`;
    analysisCount.textContent = Math.max(3, 216 - submittedTurns * 41);
    updateTurnMark();
    animateGuess();
    modeNote.textContent = 'Preview update complete. The C++ solver bridge will provide the next recommendation.';
  } else {
    modeNote.textContent = 'Feedback submitted to the C++ solver.';
  }
  feedbackInput.value = '';
  renderFeedbackTiles();
}

function submitFeedback() {
  submitSolverFeedback(guessInput.value, feedbackInput.value);
}

function setMode(mode) {
  currentMode = mode;
  modes.forEach((button) => button.classList.toggle('active', button.dataset.mode === mode));
  const isAuto = mode === 'auto';
  modeNote.textContent = isAuto
    ? 'The extension will prepare the guess, but Play still requires your approval.'
    : 'Enter the colors you see on the game board, then submit.';
  submitButton.classList.toggle('hidden', isAuto);
  playButton.classList.toggle('hidden', !isAuto);
}

feedbackInput.addEventListener('input', () => {
  feedbackInput.value = feedbackInput.value.toLowerCase().replace(/[^xyg]/g, '').slice(0, 5);
  renderFeedbackTiles();
});

guessInput.addEventListener('input', () => updateGuess(guessInput.value));
submitButton.addEventListener('click', submitFeedback);
againButton.addEventListener('click', () => window.location.reload());
helpButton.addEventListener('click', () => helpDialog.showModal());
closeHelp.addEventListener('click', () => helpDialog.close());
playButton.addEventListener('click', async () => {
  const guess = guessInput.value;
  playButton.disabled = true;
  modeNote.textContent = `playing ${guess}...`;

  try {
    const result = await sendToActiveGame({
      type: 'WOWDLE_PLAY_GUESS',
      guess,
    });

    if (!result || result.error) {
      throw new Error(result?.error || 'the game did not return feedback');
    }

    playButton.textContent = 'guess played';
    playButton.classList.add('played');
    submitSolverFeedback(guess, result.feedback);
  } catch (error) {
    modeNote.textContent = error.message;
  } finally {
    playButton.disabled = false;
  }
});

modes.forEach((button) => button.addEventListener('click', () => setMode(button.dataset.mode)));

document.addEventListener('keydown', (event) => {
  if (event.key === 'Enter' && currentMode === 'manual') {
    submitFeedback();
  }
});

updateGuess(heroGuess.textContent);
renderFeedbackTiles();
renderRecommendations();

window.solverBackend.init().then((state) => {
  if (applyBackendState(state)) {
    document.querySelector('#backend-status').textContent = 'cpp solver bridge: connected';
    modeNote.textContent = 'C++ solver connected. Enter the colors from the game board.';
  } else if (window.solverBackendError) {
    document.querySelector('#backend-status').textContent = 'cpp bridge unavailable';
    modeNote.textContent = `Preview mode: ${window.solverBackendError}`;
  }
});
