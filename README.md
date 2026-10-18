<h1 align="center">
  <br>
  <img src="https://raw.githubusercontent.com/Alex217/GagBook/master/sailfish/harbour-gagbook.png" width="86px" height="86px" alt="GagBook">
  <br>
  <br>
  GagBook
  <br>
  <br>
</h1>

GagBook is an unofficial Sailfish OS app for the [9GAG.com](https://9gag.com) website.

## Features

- Simple, smooth and native Sailfish Silica UI
- View Hot, Trending and Fresh, and all of 9GAG's current interests (Humor, Memes, Gaming, ...)
- Search tags, with suggestions from 9GAG, and view the posts of any tag
- Account login with your 9GAG email address (or username) and password
- View comments of each post, write comments and replies, and delete your own
- Post an image from the phone, with a title, tags and the sensitive flag
- Save image and video files

## Development

This is a fork of the original GagBook app initially developed by [@dicksonleong](https://github.com/dicksonleong/GagBook) and later maintained by [@llelectronics](https://github.com/llelectronics/GagBook).

Initially GagBook used webscraping using QtWebKitWidgets to get its data. I started to work on GagBook back in December 2017 after the app was not working anymore due to a changed website, that was JS rendered since then.
Now GagBook gets its data from the undisclosed official 9GAG.com API.

### Login

Since version 1.5.0 GagBook logs in through the 9GAG app API (`api.9gag.com/v2/user-token`), the same endpoint the official Android app uses. The 9gag.com website, including its old `/login` form, sits behind a Cloudflare challenge that an app like this can't pass.

- Only email/username and password logins work. Accounts that only sign in with Facebook, Google or Apple can't log in.
- The app stores the session token, not your password. With "Stay logged in" switched on, it also stores your login name and the MD5 hash of your password that the API expects, so it can renew the session when the token expires. Logging out removes both.
- Voting is switched off for now. The old `9gag.com/vote` endpoint is behind the same Cloudflare challenge, and the vote call of the app API isn't publicly known.
- The API is undocumented, so 9GAG can change or close it at any time.

I will update GagBook to fix bugs and implement new features.

### Comments and posts

- Comments go to 9GAG's comment service with the comment key that comes with the login (`commentAuth`). Sessions from GagBook 1.6 and older don't have it: log out and in again once.
- Posts are sent in the three steps the 9GAG app uses (`/v2/post-submit`: image, details, creation). 9GAG may review a post before others see it, and refuses posts from accounts that are unverified or too new; GagBook shows 9GAG's reason.

### Sections and tags

9GAG replaced its numbered sections with named interests. GagBook loads the current list from `/v2/group-list` when the section page first opens (falling back to a built-in list), shows an interest with `/v2/post-list/interest/<name>`, tags with `/v2/tag-search` and suggests tags with `/v2/tag-suggest`. `tools/probe-9gag-api.sh` checks these endpoints from a phone.

### Building

The GitHub Actions workflow in `.github/workflows/sailfish-rpm.yml` builds an armv7hl RPM for Sailfish OS 5.1.0.11 (Sony Xperia XA2, Xperia 10 and other 32-bit devices) on every push. Run it from the Actions tab to pick another SDK release. The RPM is attached to the run as an artifact.

To build locally with the Sailfish SDK, run `sfdk build` (or `mb2 -t SailfishOS-5.1.0.11-armv7hl build`) inside the `sailfish/` folder.

To install on the phone, allow untrusted software (Settings → Untrusted software) and open the RPM, or with developer mode on run `devel-su pkcon install-local harbour-gagbook-*.armv7hl.rpm`.

## Contacts

- **Me**: Alex217@t-online.de

## License

GagBook is licensed under the Simplified BSD License, see the [LICENSE](LICENSE) file for more details.

[VideoComponent.qml](sailfish/qml/VideoComponent.qml) was implemented by [@llelectronics](https://github.com/llelectronics) and is licensed under GPLv3.

## Credits

Thanks to [@and3rson](https://github.com/and3rson) for his work on [nineapi](https://github.com/and3rson/nineapi).
