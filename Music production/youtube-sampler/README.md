# Phrase Sampler

Type a word or phrase, and Phrase Sampler finds YouTube videos where someone *says* it. For each hit you can play just that slice of audio, nudge the start and end, and download it as a WAV ready to drag into Ableton.

It runs on your own computer and opens in your web browser. Nothing is uploaded anywhere.

## Setup (once)

1. Unzip `phrase-sampler.zip` somewhere handy, for example your Music folder.
2. Double-click the starter for your computer:
   - **Mac:** `Start Sampler (Mac).command`
     The first time, macOS may say it "can't be opened because it is from an unidentified developer". Right-click (or Control-click) the file, choose **Open**, then click **Open** again. You only do this once.
   - **Windows:** `Start Sampler (Windows).bat`
     If a blue "Windows protected your PC" box appears, click **More info**, then **Run anyway**.
3. A black text window opens. The first start downloads what it needs (Python, the YouTube downloader and an audio converter, about 150 MB) and takes a minute or two. After that it starts in seconds.
4. Your browser opens on the Phrase Sampler page. If it doesn't, go to http://127.0.0.1:8765

To quit, close the black window. Keep it open while you use the page.

Each start also updates the YouTube downloader, which matters because YouTube changes things often. If searches suddenly stop working, quitting and starting again is the first fix.

## Using it

- **Spoken phrase to find:** what you want someone to say, e.g. `the future`, `jack your body`, `machine`.
- **Search YouTube for (optional):** what kind of videos to look in. Leave it blank to search for the phrase itself, or add topic words like `detroit techno interview`, `1970s documentary`, `NASA mission audio`, `sermon`. You can also paste a link to a channel, a playlist or a single video, and it will look inside that instead.
- **Videos to scan:** more videos means more hits but a slower search. 20 takes roughly half a minute. Results appear while it works, and **Stop** ends the search early.
- **Language:** `en` for English, or another language code (`fr`, `de`, `es`, `ja`…).

On each result:

- **▶ Play** fetches only a few seconds of audio around the phrase and plays it. The button shows a seconds counter while it loads, which can take 5 to 20 seconds the first time for each video. After that, nudging is instant.
- **⟸ start / start ⟹ / ⟸ end / end ⟹** move the start or end by 0.1 s and replay, so you can tighten the cut by ear.
- **⬇ WAV** downloads the clip (44.1 kHz, 16-bit stereo, with tiny fades so it doesn't click) and also keeps a copy in the `Samples` folder next to the app. **Open Samples folder** takes you there, and you can drag files straight into Live from it.
- **Pad before / after** at the top sets how much extra room new results get around the words.

## Good to know

- It finds phrases through YouTube's captions, mostly the automatic ones, so it only finds videos that have captions and only as well as the captions heard the words. Interviews, lectures, talks and documentaries work best. Songs mostly don't.
- Auto captions are timed word by word, so those cuts land close. "Human captions" are timed by the line, so the cut is an estimate and you'll nudge more.
- If you see "Sign in to confirm you're not a bot" or age-restricted videos fail, set **YouTube sign-in from** to the browser where you're logged into YouTube. The app then borrows that browser's YouTube login. On a Mac, Chrome may ask for your keychain password the first time; Safari needs Full Disk Access for the Terminal app, so Chrome or Firefox is easier.
- If YouTube returns lots of errors after heavy use, it is rate-limiting you. Wait a while, or scan fewer videos at a time.

## Clearing samples

This is a personal sampling tool. Downloading from YouTube is against YouTube's terms of service, and the voices in these clips belong to someone. That's fine for sketching and learning, but before you release a track that uses a clip, clear it with the rights holder, or swap it for something you have rights to (your own recordings, public-domain material such as many NASA and US government recordings, or a licensed sample pack).

## Troubleshooting

- **Something doesn't work:** look at the black window. It prints a line for every clip it fetches, and any problem in full. Copy that text and paste it to Claude.

- **Nothing happens when I double-click on Mac:** open the Terminal app, type `bash ` (with a space), drag the `.command` file into the Terminal window, and press Return.
- **"uv: command not found" or the window closes straight away:** start it again. The first run installs a helper and occasionally needs a second start to find it.
- **Everything says "no captions":** try more videos, different topic words, or check the language code.
