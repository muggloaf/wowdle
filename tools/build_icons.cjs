const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
let chromium;
try {
  ({ chromium } = require(path.join(root, '.build/browser-check/node_modules/playwright-core')));
} catch {
  console.error('Install the optional renderer: npm install --prefix .build/browser-check --no-save --package-lock=false playwright-core');
  process.exit(1);
}

async function main() {
  const source = fs.readFileSync(path.join(root, 'extension/icons/wowdle.svg'), 'utf8');
  const browser = await chromium.launch({ channel: 'chrome', headless: true });
  try {
    const page = await browser.newPage({ deviceScaleFactor: 1 });
    await page.setContent(`<style>html,body{margin:0;background:transparent}svg{display:block;width:100vw;height:100vh}</style>${source}`);
    for (const size of [16, 24, 32, 48, 128]) {
      await page.setViewportSize({ width: size, height: size });
      await page.screenshot({ path: path.join(root, `extension/icons/icon${size}.png`), omitBackground: true });
    }
    console.log('Rendered wowdle PNG icons: 16, 24, 32, 48, 128 px');
  } finally {
    await browser.close();
  }
}

main().catch((error) => { console.error(error); process.exitCode = 1; });
