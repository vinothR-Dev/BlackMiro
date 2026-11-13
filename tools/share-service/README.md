# Miru share service

Two things for Miru, on one free Cloudflare Worker:

* **Share links** — anyone can publish a board copy, no GitHub account or token in the app.
* **Live sessions** — a secure `wss://` relay, so people on different networks (and the
  web app) can edit together. Without it, live sessions only work on the same Wi-Fi.

```
Miru app ──(encrypted board)──▶ this Worker ──(token only here)──▶ github.com/vinothR-Dev/miru-boards
Browser/app opening a link ◀──────── raw.githubusercontent.com (ciphertext) ◀────────┘
```

* The app encrypts each board itself; the key is only in the link's `#` part, which is
  never sent to any server. This service and GitHub only ever see ciphertext.
* Boards go to a **separate** public repository, never the app's code repository, so
  the token can't touch Miru's code or releases.
* One folder per person: `boards/<id>/`, where `<id>` is an HMAC of the signed-in
  email (the address itself is never published). Board names are random, and only the
  device that created a board gets its edit key, so nobody can overwrite another
  person's board.
* Limits: 10 MB per board, 20 uploads per minute per IP address, only Miru boards
  accepted, browsers only from the Miru web app.

## Live sessions

`wss://<service>/v1/relay/r/<room>` — one Durable Object per room relays the frames the
app already sends to its built-in relay (`src/collab/RelayServer.cpp`). Frames are
end-to-end encrypted with the secret in the invite link, so the relay never sees a
board. A room keeps its log only while someone is connected. Invite links are normal
web links (`https://vinothr-dev.github.io/BlackMiro/app/#ws=…&room=…&key=…`): they open
the web app anywhere, or paste into the app's *Have a link?* box.

The free Workers plan includes Durable Objects (SQLite-backed, set up by
`wrangler.toml`); `wrangler deploy` creates them, nothing else to configure.

## Set up (once, about 10 minutes, no domain and nothing to install)

The service runs at a free `https://miru-share.<your-subdomain>.workers.dev` address and
is deployed by GitHub Actions (`.github/workflows/deploy-share-service.yml`).

1. **Cloudflare.** Sign up free at https://dash.cloudflare.com/sign-up (skip "add a
   website"). Open **Workers & Pages**, pick your workers.dev subdomain when asked, and
   copy the **Account ID** shown on that page. Then profile icon → **My Profile** →
   **API Tokens** → **Create Token** → template **Edit Cloudflare Workers** → create, copy.

2. **Boards repository.** On GitHub create a **public** repository `miru-boards` with
   *Add a README file* ticked. Then GitHub → Settings → Developer settings →
   **Fine-grained tokens** → *Generate new token*: repository access **Only select
   repositories → miru-boards**, permission **Contents: Read and write**, nothing else.

3. **Secrets.** In this repository → Settings → Secrets and variables → Actions →
   **New repository secret**:

   | Secret | Value |
   |---|---|
   | `CLOUDFLARE_API_TOKEN` | token from step 1 |
   | `CLOUDFLARE_ACCOUNT_ID` | account ID from step 1 |
   | `MIRU_BOARDS_TOKEN` | token from step 2 |
   | `MIRU_EDIT_SECRET` | random text, 32+ characters |
   | `MIRU_USER_SECRET` | other random text, 32+ characters — never change it (it names everyone's folder) |

4. **Deploy.** Actions → **Deploy share service** → **Run workflow**. The run summary
   shows the address.

5. **Connect the app.** Settings → Secrets and variables → Actions → **Variables** →
   `MIRU_SHARE_URL` = that address. Re-run **Build and release**: share links and live
   sessions are now on for Windows, Android and the web.

Re-run **Deploy share service** whenever `tools/share-service` changes.

*Without GitHub Actions:* `cd tools/share-service && npx wrangler login`, then
`npx wrangler secret put BOARDS_TOKEN` / `EDIT_SECRET` / `USER_SECRET`, then
`npx wrangler deploy`.

Without `MIRU_SHARE_URL` the app still builds; the share box then says links aren't
available and offers *Download board* instead. Opening existing links always works.

## Test locally

The Worker has no dependencies. `npx wrangler dev` runs it on http://localhost:8787,
reading `BOARDS_TOKEN`, `EDIT_SECRET` and `USER_SECRET` from a `.dev.vars` file (never commit it).
