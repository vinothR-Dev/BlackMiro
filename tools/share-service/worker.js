// Miru share service (Cloudflare Worker).
//
// Lets anyone - no GitHub account needed - publish an encrypted board and get a
// share link. The app encrypts the board itself (AES-256 + HMAC, key only in the
// link's #fragment), so this service and GitHub only ever see ciphertext.
//
// Boards are written to a separate, public boards repository (never the app's
// code repository) with a fine-grained token that only this Worker holds:
//
//   <BOARDS_REPO>@<BOARDS_BRANCH>:boards/<user>/<id>.miru
//
//   <user>  anonymous per-person folder: HMAC(USER_SECRET, signed-in email), so the
//           public repository never shows an email address
//   <id>    random, chosen here, so nobody can overwrite someone else's board
//
// API
//   POST /v1/boards                 body: encrypted board ("MIRUSHR1" + ciphertext)
//     X-Miru-User: <email>          optional; empty = guest folder
//     X-Miru-Path, X-Miru-Edit      optional; update a board this device created before
//   -> 200 {"repo","branch","path","edit"}
//
//   GET  /v1/relay/r/<room>         WebSocket: live editing room (see Room below)
//
// Secrets (wrangler secret put ...): GITHUB_TOKEN, EDIT_SECRET, USER_SECRET
// Vars (wrangler.toml): BOARDS_REPO, BOARDS_BRANCH, ALLOWED_ORIGIN, MAX_BYTES

const MAGIC = new TextEncoder().encode('MIRUSHR1');
const PATH_RE = /^boards\/[a-z0-9]{16}\/[A-Za-z0-9_-]{16}\.miru$/;

const enc = new TextEncoder();
const b64url = (buf) => btoa(String.fromCharCode(...new Uint8Array(buf)))
  .replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');

async function hmac(secret, text) {
  const key = await crypto.subtle.importKey('raw', enc.encode(secret), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  return crypto.subtle.sign('HMAC', key, enc.encode(text));
}

function timingSafeEqual(a, b) {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

function base64(bytes) {
  let s = '';
  for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  return btoa(s);
}

function cors(env, request) {
  const origin = request.headers.get('Origin');
  const h = {
    'Access-Control-Allow-Methods': 'POST, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type, X-Miru-User, X-Miru-Path, X-Miru-Edit',
    'Access-Control-Max-Age': '86400',
    'Vary': 'Origin',
  };
  if (origin && origin === env.ALLOWED_ORIGIN) h['Access-Control-Allow-Origin'] = origin;
  return h;
}

function reply(env, request, status, obj) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { 'Content-Type': 'application/json', ...cors(env, request) },
  });
}

async function github(env, method, path, body) {
  return fetch(`https://api.github.com/repos/${env.BOARDS_REPO}/contents/${path}` +
               (method === 'GET' ? `?ref=${encodeURIComponent(env.BOARDS_BRANCH)}` : ''), {
    method,
    headers: {
      'Authorization': `Bearer ${env.GITHUB_TOKEN}`,
      'Accept': 'application/vnd.github+json',
      'X-GitHub-Api-Version': '2022-11-28',
      'User-Agent': 'miru-share-service',
      ...(body ? { 'Content-Type': 'application/json' } : {}),
    },
    body: body ? JSON.stringify(body) : undefined,
  });
}

const ROOM_RE = /^\/v1\/relay\/r\/([A-Za-z0-9_-]{8,64})$/;

async function handle(request, env) {
  const url = new URL(request.url);
  if (request.method === 'OPTIONS') return new Response(null, { status: 204, headers: cors(env, request) });

  // Live sessions: one Durable Object per room relays the (end-to-end encrypted) frames.
  const room = url.pathname.match(ROOM_RE);
  if (room) {
    if (request.headers.get('Upgrade') !== 'websocket') return new Response('Expected a WebSocket', { status: 426 });
    const origin = request.headers.get('Origin');
    if (origin && origin !== env.ALLOWED_ORIGIN) return new Response('Origin not allowed', { status: 403 });
    if (env.LIMITER) {
      const { success } = await env.LIMITER.limit({ key: 'ws:' + (request.headers.get('CF-Connecting-IP') || '') });
      if (!success) return new Response('Too many connections', { status: 429 });
    }
    return env.ROOMS.get(env.ROOMS.idFromName(room[1])).fetch(request);
  }
  if (url.pathname !== '/v1/boards' || request.method !== 'POST')
    return reply(env, request, 404, { error: 'Not found' });

  // Browsers may only call from the Miru web app; the desktop/phone app sends no Origin.
  const origin = request.headers.get('Origin');
  if (origin && origin !== env.ALLOWED_ORIGIN)
    return reply(env, request, 403, { error: 'Origin not allowed' });

  if (env.LIMITER) {
    const ip = request.headers.get('CF-Connecting-IP') || 'unknown';
    const { success } = await env.LIMITER.limit({ key: ip });
    if (!success) return reply(env, request, 429, { error: 'Too many uploads, try again in a minute' });
  }

  const max = Number(env.MAX_BYTES || 10 * 1024 * 1024);
  const declared = Number(request.headers.get('Content-Length') || 0);
  if (declared > max) return reply(env, request, 413, { error: 'Board too large to share' });
  const bytes = new Uint8Array(await request.arrayBuffer());
  if (bytes.length > max) return reply(env, request, 413, { error: 'Board too large to share' });
  if (bytes.length <= MAGIC.length || !MAGIC.every((b, i) => bytes[i] === b))
    return reply(env, request, 400, { error: 'Not a Miru board' });

  let path = request.headers.get('X-Miru-Path') || '';
  let sha;
  if (path) {
    // Update: only the device that created the board holds its edit key.
    if (!PATH_RE.test(path)) return reply(env, request, 400, { error: 'Bad board path' });
    const expected = b64url(await hmac(env.EDIT_SECRET, path));
    if (!timingSafeEqual(request.headers.get('X-Miru-Edit') || '', expected))
      return reply(env, request, 403, { error: 'Not allowed to change this board' });
    const cur = await github(env, 'GET', path);
    if (cur.status === 200) sha = (await cur.json()).sha;
    else if (cur.status !== 404) return reply(env, request, 502, { error: `GitHub error ${cur.status}` });
  } else {
    const email = (request.headers.get('X-Miru-User') || '').trim().toLowerCase();
    const user = Array.from(new Uint8Array(await hmac(env.USER_SECRET, email || 'guest')))
      .map((b) => b.toString(16).padStart(2, '0')).join('').slice(0, 16);
    path = `boards/${user}/${b64url(crypto.getRandomValues(new Uint8Array(12)))}.miru`;
  }

  const put = await github(env, 'PUT', path, {
    message: sha ? 'Update shared board' : 'Share board',
    branch: env.BOARDS_BRANCH,
    content: base64(bytes),
    ...(sha ? { sha } : {}),
  });
  if (put.status !== 200 && put.status !== 201) {
    console.log('GitHub PUT failed', put.status, await put.text());
    return reply(env, request, 502, { error: `GitHub error ${put.status}` });
  }
  return reply(env, request, 200, {
    repo: env.BOARDS_REPO,
    branch: env.BOARDS_BRANCH,
    path,
    edit: b64url(await hmac(env.EDIT_SECRET, path)),
  });
}

export default {
  async fetch(request, env) {
    try {
      return await handle(request, env);
    } catch (e) {
      console.log('Share service error', e && e.stack || e);
      return reply(env, request, 500, { error: 'Share service error' });
    }
  },
};

// ---------------------------------------------------------------------------
// Live editing room. Same protocol as the app's built-in relay
// (src/collab/RelayServer.cpp): every binary frame is 1 type byte + payload that
// the clients encrypt with the invite secret, so this relay never sees a board.
//   1 Ops       logged and forwarded
//   2 Snapshot  replaces the log (full state), not forwarded
//   3 Presence  forwarded only (cursors, names)
//   9 WantSnapshot  relay -> client: the log is long, please send a snapshot
// Newcomers first receive the log (latest snapshot + later ops), then live traffic.
const OPS = 1, SNAPSHOT = 2, PRESENCE = 3, WANT_SNAPSHOT = 9;
const MAX_LOG_FRAMES = 400;
const MAX_LOG_BYTES = 64 * 1024 * 1024;
const MAX_FRAME_BYTES = 32 * 1024 * 1024;
const MAX_CLIENTS = 50;

export class Room {
  constructor(state, env) {
    this.clients = new Set();
    this.reset();
  }

  reset() {
    this.log = [];
    this.logBytes = 0;
    this.snapshotRequested = false;
  }

  async fetch(request) {
    if (this.clients.size >= MAX_CLIENTS) return new Response('Room is full', { status: 429 });
    const [client, server] = Object.values(new WebSocketPair());
    server.accept();
    for (const frame of this.log) server.send(frame);   // catch the newcomer up
    this.clients.add(server);
    server.addEventListener('message', (ev) => this.onMessage(server, ev.data));
    const drop = () => {
      this.clients.delete(server);
      if (this.clients.size === 0) this.reset();   // nobody left: forget the (encrypted) log
    };
    server.addEventListener('close', drop);
    server.addEventListener('error', drop);
    return new Response(null, { status: 101, webSocket: client });
  }

  onMessage(from, data) {
    if (typeof data === 'string') return;            // binary frames only
    const frame = new Uint8Array(data);
    if (frame.length === 0) return;
    if (frame.length > MAX_FRAME_BYTES) { from.close(1009, 'Frame too large'); return; }
    const type = frame[0];
    if (type === SNAPSHOT) {
      this.log = [data];
      this.logBytes = frame.length;
      this.snapshotRequested = false;
      return;
    }
    if (type === OPS) {
      this.log.push(data);
      this.logBytes += frame.length;
      while (this.logBytes > MAX_LOG_BYTES && this.log.length > 1) {
        this.logBytes -= this.log[1].byteLength;
        this.log.splice(1, 1);                        // keep the base snapshot
      }
      if (this.log.length > MAX_LOG_FRAMES && !this.snapshotRequested) {
        this.snapshotRequested = true;
        from.send(new Uint8Array([WANT_SNAPSHOT]));
      }
    } else if (type !== PRESENCE) {
      return;
    }
    for (const peer of this.clients) {
      if (peer === from) continue;
      try { peer.send(data); } catch { this.clients.delete(peer); }
    }
  }
}
