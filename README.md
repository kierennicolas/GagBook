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
- View posts in different sections, e.g. Hot, Trending, Fresh, etc.
- Account login with your 9GAG email address (or username) and password
- View comments of each post
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

## Contacts

- **Me**: Alex217@t-online.de

## License

GagBook is licensed under the Simplified BSD License, see the [LICENSE](LICENSE) file for more details.

[VideoComponent.qml](sailfish/qml/VideoComponent.qml) was implemented by [@llelectronics](https://github.com/llelectronics) and is licensed under GPLv3.

## Credits

Thanks to [@and3rson](https://github.com/and3rson) for his work on [nineapi](https://github.com/and3rson/nineapi).
