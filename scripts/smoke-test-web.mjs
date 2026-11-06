// Opens the WebAssembly build in headless Chromium and fails on QML load errors
// or if the app never draws. Usage: node smoke-test-web.mjs <url> <screenshot.png>
import { chromium } from 'playwright';
const [url, shot] = process.argv.slice(2);
const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
const problems = [];
page.on('console', m => {
  const t = m.text();
  console.log(`[${m.type()}] ${t}`);
  if (/failed to load component|is not a type|Cannot assign to non-existent|Type \w+ unavailable|module .* is not installed/.test(t)) problems.push(t);
});
page.on('pageerror', e => { console.log('[pageerror]', e.message); problems.push(e.message); });
await page.goto(url);
await page.waitForTimeout(30000);
await page.screenshot({ path: shot });
// Qt draws into a canvas inside the app's container; it must exist and have a size.
const size = await page.evaluate(() => {
  const find = root => {               // Qt may render inside shadow DOM
    let best = null;
    for (const el of root.querySelectorAll('*')) {
      if (el.tagName === 'CANVAS' && (!best || el.width * el.height > best.width * best.height)) best = el;
      if (el.shadowRoot) { const c = find(el.shadowRoot); if (c && (!best || c.width * c.height > best.width * best.height)) best = c; }
    }
    return best;
  };
  const c = find(document);
  return c ? [c.width, c.height] : null;
});
await browser.close();
console.log('canvas size:', size);
if (problems.length || !size || size[0] < 100) {
  console.error('Web smoke test FAILED:\n' + problems.join('\n'));
  process.exit(1);
}
console.log('Web smoke test OK');
