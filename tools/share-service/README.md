# Miru share service

Lets anyone create a Miru share link — no GitHub account or token in the app.

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

## Set up (once, about 10 minutes)

1. **Boards repository.** On GitHub create a new **public** repository
   `vinothR-Dev/miru-boards` and tick *Add a README file* (it needs a first commit).

2. **Token.** GitHub → Settings → Developer settings → Fine-grained tokens →
   *Generate new token*:
   * Repository access: **Only select repositories → miru-boards**
   * Permissions: **Contents: Read and write** (nothing else)

3. **Cloudflare.** Create a free account at https://dash.cloudflare.com, then on any
   computer with Node.js:

   ```sh
   cd tools/share-service
   npx wrangler login
   npx wrangler secret put GITHUB_TOKEN    # paste the token from step 2
   npx wrangler secret put EDIT_SECRET     # paste a long random string
   npx wrangler secret put USER_SECRET     # paste another long random string
   npx wrangler deploy
   ```

   For the random strings you can use `openssl rand -base64 48`. Keep `USER_SECRET`
   the same forever (changing it moves everyone to a new folder).
   `deploy` prints the service address, e.g. `https://miru-share.<you>.workers.dev`.

4. **Tell the build.** In github.com/vinothR-Dev/BlackMiro → Settings → Secrets and
   variables → Actions → **Variables** → *New repository variable*:
   `MIRU_SHARE_URL` = the address from step 3. Then push (or re-run the workflow):
   the next Miru build shows **Create share link**.

Without `MIRU_SHARE_URL` the app still builds; the share box then says links aren't
available and offers *Download board* instead. Opening existing links always works.

## Test locally

The Worker has no dependencies. `npx wrangler dev` runs it on http://localhost:8787,
reading secrets from a `.dev.vars` file (never commit that file).
