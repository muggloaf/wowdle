class SolverBackend {
  constructor() {
    this.module = null;
    this.ready = false;
  }

  assetUrl(path) {
    if (typeof chrome !== 'undefined' && chrome.runtime?.getURL) {
      return chrome.runtime.getURL(path);
    }
    return path;
  }

  async loadWasmScript() {
    if (window.WowdleModuleFactory) {
      return;
    }

    await new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = this.assetUrl('wowdle.js');
      script.onload = resolve;
      script.onerror = reject;
      document.head.appendChild(script);
    });
  }

  async init() {
    try {
      await this.loadWasmScript();
      this.module = await window.WowdleModuleFactory();
      const words = await fetch(this.assetUrl('data/wordlist_fives.txt'))
        .then((response) => response.text());
      const weights = await fetch(this.assetUrl('data/word_frequencies_fives.txt'))
        .then((response) => response.text());
      this.module.ccall('wowdle_init', null, ['string', 'string'], [words, weights]);
      this.ready = true;
      return this.recommend();
    } catch (error) {
      window.solverBackendError = error instanceof Error ? error.message : String(error);
      console.info('cpp wasm backend is not loaded; using preview data', error);
      return null;
    }
  }

  recommend() {
    if (!this.ready) {
      return null;
    }
    return JSON.parse(this.module.ccall('wowdle_recommend', 'string', [], []));
  }

  submit(guess, feedback) {
    if (!this.ready) {
      return null;
    }
    return JSON.parse(this.module.ccall(
      'wowdle_submit',
      'string',
      ['string', 'string'],
      [guess, feedback],
    ));
  }
}

window.solverBackend = new SolverBackend();
