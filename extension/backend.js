class SolverBackend {
  constructor() {
    this.module = null;
    this.ready = false;
  }

  assetUrl(path) {
    return typeof chrome !== 'undefined' && chrome.runtime?.getURL
      ? chrome.runtime.getURL(path) : path;
  }

  async loadWasmScript() {
    if (window.WowdleModuleFactory) return;
    await new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = this.assetUrl('wowdle.js');
      script.onload = resolve;
      script.onerror = () => reject(new Error('could not load wowdle.js; rebuild the WASM backend'));
      document.head.appendChild(script);
    });
  }

  async init() {
    this.ready = false;
    await this.loadWasmScript();
    this.module = await window.WowdleModuleFactory({
      locateFile: (name) => this.assetUrl(name),
    });
    const loadText = async (path) => {
      const response = await fetch(this.assetUrl(path));
      if (!response.ok) throw new Error(`could not load ${path} (${response.status})`);
      return response.text();
    };
    const [words, weights] = await Promise.all([
      loadText('data/wordlist_fives.txt'),
      loadText('data/word_frequencies_fives.txt'),
    ]);
    this.module.ccall('wowdle_init', null, ['string', 'string'], [words, weights]);
    this.ready = true;
    try {
      return this.recommend();
    } catch (error) {
      this.ready = false;
      throw error;
    }
  }

  call(name, types = [], values = []) {
    if (!this.ready) throw new Error('the C++ solver is unavailable; rebuild the backend and reload');
    const state = JSON.parse(this.module.ccall(name, 'string', types, values));
    if (state.error) throw new Error(state.error);
    return state;
  }

  recommend() {
    return this.call('wowdle_recommend');
  }

  submit(guess, feedback) {
    return this.call('wowdle_submit', ['string', 'string'], [guess, feedback]);
  }

  restore(history) {
    const text = history.map((turn) => `${turn.guess} ${turn.feedback}`).join('\n');
    return this.call('wowdle_restore', ['string'], [text]);
  }
}

window.solverBackend = new SolverBackend();
